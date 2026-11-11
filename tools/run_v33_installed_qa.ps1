param([ValidateSet(32,33)][int]$Major=33,[string]$Scenario='v33',[string]$Output='work/v3.3/installed33',[switch]$Wait)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
function Write-Provenance($obs,$paths){
 $expected=(Get-FileHash (Join-Path $root 'build/obs-spatial-fx.dll')).Hash
 $installed=(Get-FileHash 'C:/ProgramData/obs-studio/plugins/obs-spatial-fx/obs-spatial-fx.dll').Hash
 $loaded=@($paths|ForEach-Object {@{Path=$_;Sha256=(Get-FileHash -LiteralPath $_).Hash}})
 if(!$loaded -or $installed -ne $expected -or @($loaded|Where-Object Sha256 -NE $expected).Count){throw 'Actual loaded DLL does not match final build'}
 $assets=@(Get-ChildItem 'C:/ProgramData/obs-studio/plugins/obs-spatial-fx/data/effects' -File|ForEach-Object {@{Name=$_.Name;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
 foreach($asset in $assets){
  if($asset.Sha256 -ne (Get-FileHash (Join-Path $root ('data/effects/'+$asset.Name))).Hash){throw 'Installed shader differs from current source'}
  foreach($prefix in @('portable-data/obs-spatial-fx/effects','portable33/obs-spatial-fx/data/effects')){
   if($asset.Sha256 -ne (Get-FileHash ('C:/ProgramData/obs-studio/plugins/obs-spatial-fx/'+$prefix+'/'+$asset.Name)).Hash){throw 'Portable shader copy differs'}
  }
 }
 @{LoadedPaths=$paths;LoadedModules=$loaded;ShaderAssets=$assets;Major=$Major;Scenario=$Scenario;Pid=$obs.Id;InstalledHash=$installed}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $env:SPATIAL_QA_OUTPUT 'installed-path.json') -Encoding UTF8
}
if(Get-Process obs64 -ErrorAction SilentlyContinue){throw 'Existing OBS must finish; no process will be terminated.'}
$runtime=Join-Path $root ('runtime/obs'+$Major)
$driver=if($Major -eq 33){Join-Path $runtime 'plugins/spatial-qa-driver/spatial-qa-driver.dll'}else{Join-Path $runtime 'obs-plugins/64bit/spatial-qa-driver.dll'}
New-Item (Split-Path $driver -Parent) -ItemType Directory -Force|Out-Null
Copy-Item (Join-Path $root 'build/spatial-qa-driver.dll') $driver -Force
$env:SPATIAL_QA_SCENARIO=$Scenario
$env:SPATIAL_QA_OUTPUT=Join-Path $root $Output
$env:SPATIAL_QA_AUDIO=Join-Path $root 'work/v3.2/audio/fixtures/点击_120_反相.wav'
New-Item $env:SPATIAL_QA_OUTPUT -ItemType Directory -Force|Out-Null
$readyPath=Join-Path $env:SPATIAL_QA_OUTPUT 'native-ready.json'
if(Test-Path -LiteralPath $readyPath){Remove-Item -LiteralPath $readyPath}
foreach($name in @('native-command.json','native-command.tmp','native-answer.json')){
 $commandPath=Join-Path $env:SPATIAL_QA_OUTPUT $name
 if(Test-Path -LiteralPath $commandPath){Remove-Item -LiteralPath $commandPath}
}
$reportPath=Join-Path $env:SPATIAL_QA_OUTPUT 'dock-smoke.json'
if(Test-Path -LiteralPath $reportPath){Remove-Item -LiteralPath $reportPath}
$started=[DateTime]::UtcNow
$launcher='C:/ProgramData/obs-studio/plugins/obs-spatial-fx/Uninstall.exe'
$proc=Start-Process $launcher -ArgumentList ('/LAUNCH="'+$runtime+'"') -PassThru
$proc.WaitForExit()
if($proc.ExitCode -ne 0){throw ('Installed launcher failed '+$proc.ExitCode)}
Start-Sleep -Seconds 2
$obs=Get-Process obs64 -ErrorAction Stop
# Retain a process handle before exit; Get-Process otherwise loses ExitCode.
$null=$obs.Handle
$watch=[Diagnostics.Stopwatch]::StartNew()
do{
 $obs.Refresh();$null=$obs.Handle
 $paths=@($obs.Modules|Where-Object ModuleName -EQ 'obs-spatial-fx.dll'|ForEach-Object FileName)
 if(-not $paths){Start-Sleep -Milliseconds 200}
}while(-not $paths -and $watch.Elapsed.TotalSeconds -lt 30)
if(!$paths){throw 'Installed DLL not observed; cannot certify runtime provenance'}
Write-Provenance $obs $paths
Write-Output ('Installed OBS'+$Major+' '+$Scenario+' PID '+$obs.Id+' output '+$env:SPATIAL_QA_OUTPUT)
if($Scenario -eq 'v34-hold'){
 $readyWatch=[Diagnostics.Stopwatch]::StartNew()
 do{
  if($obs.HasExited){throw 'OBS exited before frontend readiness.'}
  if(Test-Path -LiteralPath $readyPath){
   $ready=Get-Content -LiteralPath $readyPath -Raw -Encoding UTF8|ConvertFrom-Json
   if($ready.ready -eq $true -and $ready.pid -eq $obs.Id){break}
  }
  Start-Sleep -Milliseconds 200
 }while($readyWatch.Elapsed.TotalSeconds -lt 60)
 if(-not $ready -or $ready.ready -ne $true -or $ready.pid -ne $obs.Id){throw 'OBS frontend not ready after 60 seconds; do not send WebSocket mutations.'}
 Write-Output ('Frontend FINISHED_LOADING ready PID '+$obs.Id)
 $obs.Refresh()
 $paths=@($obs.Modules|Where-Object ModuleName -EQ 'obs-spatial-fx.dll'|ForEach-Object FileName)
 if(-not $paths){throw 'Frontend ready but installed plugin DLL missing.'}
 Write-Provenance $obs $paths
}


if($Wait){
 if(-not $obs.WaitForExit(60000)){throw 'QA timeout after 60 seconds; isolated OBS left intact for diagnosis.'}
 if($obs.ExitCode -ne 0){throw ('OBS exited abnormally: '+$obs.ExitCode)}
 $reportPath=Join-Path $env:SPATIAL_QA_OUTPUT 'dock-smoke.json'
 if(-not(Test-Path -LiteralPath $reportPath)){throw 'OBS exited without a completed QA report.'}
 $report=Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8|ConvertFrom-Json
 if((Get-Item -LiteralPath $reportPath).LastWriteTimeUtc -lt $started -or $report.scenario -ne $Scenario -or !$report.checks -or @($report.checks|Where-Object pass -NE $true).Count){throw 'Stale, wrong-scenario or incomplete native QA report'}
 $latest=Get-ChildItem (Join-Path $runtime 'config/obs-studio/logs') -Filter '*.txt'|Sort-Object LastWriteTime -Descending|Select-Object -First 1
 if($latest){Copy-Item -LiteralPath $latest.FullName -Destination (Join-Path $env:SPATIAL_QA_OUTPUT 'obs-runtime.log') -Force}
 if(!$latest -or (Select-String -LiteralPath $latest.FullName -Pattern 'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(failed|failure)')){throw 'Native QA runtime render errors or missing log'}
 Write-Output ('Checks='+$report.checks.Count+' Failures='+$report.failed)
 if($report.failed -ne 0){throw 'OBS QA reported failures; inspect dock-smoke.json.'}
}
