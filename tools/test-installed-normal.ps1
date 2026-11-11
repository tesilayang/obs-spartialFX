[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$python='C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$normal=Join-Path $env:APPDATA 'obs-studio'
$backup=Join-Path $root 'logs/normal-config-backup'
New-Item -ItemType Directory -Force $backup|Out-Null
$profile=Join-Path $normal 'basic/profiles/SpatialFXInstalledQA'
$collection=Join-Path $normal 'basic/scenes/Spatial_FX_Installed_QA.json'
if(Test-Path $profile){throw 'QA profile path already exists; refusing overwrite'}
if(Test-Path $collection){throw 'QA scene path already exists; refusing overwrite'}
$paths=@('user.ini','global.ini','plugin_config/obs-websocket/config.json','plugin_config/obs-spatial-fx')
$state=@()
foreach($relative in $paths){
 $source=Join-Path $normal $relative;$destination=Join-Path $backup $relative;$exists=Test-Path $source
 if($exists){New-Item -ItemType Directory -Force (Split-Path $destination -Parent)|Out-Null;Copy-Item -LiteralPath $source -Destination $destination -Recurse}
 $state+=@{path=$relative;existed=$exists}
}
$state|ConvertTo-Json|Set-Content -Encoding UTF8 (Join-Path $backup 'state.json')
try{
 if(Get-Process obs64 -ErrorAction SilentlyContinue){throw 'Close test OBS before normal-mode test'}
 Copy-Item (Join-Path $root 'runtime/obs32/config/obs-studio/basic/profiles/SpatialFXTest') $profile -Recurse
 $basic=Join-Path $profile 'basic.ini';(Get-Content $basic -Raw).Replace('Name=Spatial FX Test','Name=Spatial FX Installed QA')|Set-Content -Encoding UTF8 $basic
 $fixture=Get-Content (Join-Path $root 'runtime/obs32/config/obs-studio/basic/scenes/Spatial_FX_QA.json') -Raw|ConvertFrom-Json
 $fixture.name='Spatial FX Installed QA'
 foreach($source in $fixture.sources){if($source.id -eq 'image_source' -and $source.settings.file -and ![IO.Path]::IsPathRooted($source.settings.file)){$source.settings.file=[IO.Path]::GetFullPath((Join-Path (Join-Path $root 'runtime/obs32/bin/64bit') $source.settings.file))}}
 $fixture|ConvertTo-Json -Depth 100|Set-Content -Encoding UTF8 $collection
 foreach($relative in @('plugin_config/obs-websocket/config.json','plugin_config/obs-spatial-fx')){
  $dst=Join-Path $normal $relative;if(Test-Path $dst){Remove-Item -LiteralPath $dst -Recurse -Force};New-Item -ItemType Directory -Force (Split-Path $dst -Parent)|Out-Null
  Copy-Item (Join-Path $root ('runtime/obs32/config/obs-studio/'+$relative)) $dst -Recurse
 }
 $qa=Join-Path $root 'runtime/installed-normal-qa';New-Item -ItemType Directory -Force $qa|Out-Null;Copy-Item (Join-Path $root 'build/spatial-qa-driver.dll') $qa
 $env:OBS_PLUGINS_PATH=$qa;$env:OBS_PLUGINS_DATA_PATH=Join-Path $root 'runtime/plugin-data'
 $env:SPATIAL_QA_SCENARIO='hold';$env:SPATIAL_QA_HOLD_SECONDS='20';$env:SPATIAL_QA_OUTPUT=Join-Path $root 'logs/phase6-installed-normal'
 $exe='F:\Program Files (x86)\obs-studio new\bin\64bit\obs64.exe'
 $p=Start-Process $exe -WorkingDirectory (Split-Path $exe -Parent) -ArgumentList '--multi','--disable-updater','--disable-missing-files-check','--profile','"Spatial FX Installed QA"','--collection','"Spatial FX Installed QA"' -PassThru
 "Normal installed OBS PID=$($p.Id)"
 & $python -u (Join-Path $root 'tests/runtime_probe.py') (Join-Path $root 'logs/phase6-installed-normal')
 $probeExit=$LASTEXITCODE
 if(!$p.WaitForExit(45000)){throw 'Normal OBS did not exit via QA driver'}
 "Normal OBS exit=$($p.ExitCode)"
 if($p.ExitCode){throw 'OBS exited unsuccessfully'}
 $last=Get-ChildItem (Join-Path $normal 'logs/*.txt') | Sort-Object LastWriteTime | Select-Object -Last 1;Copy-Item $last.FullName (Join-Path $root 'logs/phase6-installed-normal-obs-runtime.log'); if($probeExit){throw 'Normal installed render probe failed'}
}finally{
 if($p -and !$p.HasExited){Stop-Process -Id $p.Id}
 foreach($entry in $state){$dst=Join-Path $normal $entry.path;if(Test-Path $dst){Remove-Item -LiteralPath $dst -Force -Recurse};if($entry.existed){Copy-Item (Join-Path $backup $entry.path) $dst -Recurse}}
 foreach($f in @($profile,$collection,($collection+'.bak'))){if(Test-Path $f){Remove-Item -LiteralPath $f -Recurse -Force}}
 $checks=@();foreach($relative in @('user.ini','global.ini')){if(Test-Path (Join-Path $backup $relative)){$a=(Get-FileHash (Join-Path $backup $relative)).Hash;$b=(Get-FileHash (Join-Path $normal $relative)).Hash;$checks+=@{path=$relative;restored=($a -eq $b)}}}
 $checks|ConvertTo-Json|Set-Content -Encoding UTF8 (Join-Path $root 'logs/normal-config-restoration.json')
}
