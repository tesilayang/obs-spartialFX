param([ValidateSet(32,33)][int]$Major=33,[string]$Scenario='camera-remake',[string]$Output='work/v3.2/installed33-final')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(Get-Process obs64 -ErrorAction SilentlyContinue){throw 'Existing OBS must finish; no process will be terminated.'}
$runtime=Join-Path $root ('runtime/obs'+$Major)
$driver=if($Major -eq 33){Join-Path $runtime 'plugins/spatial-qa-driver/spatial-qa-driver.dll'}else{Join-Path $runtime 'obs-plugins/64bit/spatial-qa-driver.dll'}
New-Item (Split-Path $driver -Parent) -ItemType Directory -Force|Out-Null
Copy-Item (Join-Path $root 'build/spatial-qa-driver.dll') $driver -Force
$env:SPATIAL_QA_SCENARIO=$Scenario
$env:SPATIAL_QA_OUTPUT=Join-Path $root $Output
$env:SPATIAL_QA_AUDIO=Join-Path $root 'work/v3.2/audio/fixtures/点击_120_反相.wav'
New-Item $env:SPATIAL_QA_OUTPUT -ItemType Directory -Force|Out-Null
$launcher='C:/ProgramData/obs-studio/plugins/obs-spatial-fx/Uninstall.exe'
$proc=Start-Process $launcher -ArgumentList ('/LAUNCH="'+$runtime+'"') -PassThru
$proc.WaitForExit()
if($proc.ExitCode -ne 0){throw ('Installed launcher failed '+$proc.ExitCode)}
Start-Sleep -Seconds 2
$obs=Get-Process obs64 -ErrorAction Stop
$watch=[Diagnostics.Stopwatch]::StartNew()
do{$obs=Get-Process obs64 -ErrorAction Stop;$paths=@($obs.Modules|Where-Object ModuleName -EQ 'obs-spatial-fx.dll'|ForEach-Object FileName);if(-not $paths){Start-Sleep -Milliseconds 200}}while(-not $paths -and $watch.Elapsed.TotalSeconds -lt 30)
@{LoadedPaths=$paths;Major=$Major;Scenario=$Scenario;InstalledHash=(Get-FileHash 'C:/ProgramData/obs-studio/plugins/obs-spatial-fx/obs-spatial-fx.dll').Hash}|ConvertTo-Json|Set-Content (Join-Path $env:SPATIAL_QA_OUTPUT 'installed-path.json') -Encoding UTF8
Write-Output ('Installed OBS'+$Major+' '+$Scenario+' PID '+$obs.Id+' output '+$env:SPATIAL_QA_OUTPUT)

