$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent;$exe=Join-Path $root 'build/OBS_Spatial_FX_V1.0_Setup.exe';$base=Join-Path $env:ProgramData 'obs-studio/plugins/obs-spatial-fx'
$checks=@()
function Check([bool]$ok,[string]$name){$script:checks+=@{name=$name;pass=$ok};Write-Output (('FAIL ','PASS ')[[int]$ok]+$name);if(!$ok){throw $name}}
function Run([string]$CommandLine){Add-Content (Join-Path $root 'logs/installer-test-commands.log') $CommandLine;$p=Start-Process $exe -ArgumentList $CommandLine -PassThru -Wait;return $p.ExitCode}
function Hashes{Get-ChildItem $base -File -Recurse|ForEach-Object { "$($_.FullName):$((Get-FileHash $_.FullName).Hash)" }}
if(Get-Process obs64 -ErrorAction SilentlyContinue){throw 'OBS running before installer tests'}
$before=@(Hashes)
$lock=[IO.File]::Open((Join-Path $base 'data/effects/spatial.effect'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try{$code=Run ('/SILENT /OBSROOT="'+(Join-Path $root 'runtime/obs32')+'" /LOG="'+(Join-Path $root 'logs/installer-rollback.log')+'"')}finally{$lock.Dispose()}
Check ($code -eq 1) 'locked owned file rejects update'
$after=@(Hashes);Check (!(Compare-Object $before $after)) 'failed update rolls back every installed file byte-for-byte'
$presets=Join-Path $root 'runtime/obs32/config/obs-studio/plugin_config/obs-spatial-fx/presets.json';$presetHash=(Get-FileHash $presets).Hash
$sentinel=Join-Path $base 'qa-preserve.txt';Set-Content -Encoding UTF8 $sentinel 'Unknown file must survive uninstall'
$uninstall=Join-Path $base 'Uninstall.exe';$p=Start-Process $uninstall -ArgumentList '/UNINSTALL','/SILENT' -PassThru -Wait;Check ($p.ExitCode -eq 0) 'installed uninstaller starts real helper'
for($i=0;$i -lt 60 -and (Test-Path (Join-Path $base 'install-manifest.txt'));$i++){Start-Sleep -Milliseconds 200}
Check (!(Test-Path (Join-Path $base 'bin/64bit/obs-spatial-fx.dll'))) 'uninstaller removes product DLL'
Check (!(Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Spatial FX')) 'uninstaller removes Windows uninstall entry'
Check (Test-Path $sentinel) 'uninstaller preserves unowned files'
Check ((Get-FileHash $presets).Hash -eq $presetHash) 'uninstaller preserves OBS configuration and presets'
Remove-Item -LiteralPath $sentinel
$code=Run ('/SILENT /OBSROOT="'+(Join-Path $root 'runtime/obs32')+'" /LOG="'+(Join-Path $root 'logs/installer-final-install.log')+'"')
Check ($code -eq 0) 'release installer installs successfully after uninstall'
Check ((Get-FileHash (Join-Path $root 'build/obs-spatial-fx.dll')).Hash -eq (Get-FileHash (Join-Path $base 'bin/64bit/obs-spatial-fx.dll')).Hash) 'installed DLL exactly matches release build'
Check (@(Get-ChildItem $base -Recurse -Filter 'Qt*.dll').Count -eq 0) 'package contains zero Qt DLLs'
Check (@(Get-ChildItem $base -Recurse -Filter '*qa-driver*').Count -eq 0) 'package contains zero QA modules'
$checks|ConvertTo-Json|Set-Content -Encoding UTF8 (Join-Path $root 'logs/installer-tests.json')
