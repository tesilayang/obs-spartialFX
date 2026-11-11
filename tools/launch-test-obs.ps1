[CmdletBinding()]
param([switch]$Template,[switch]$Baseline)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $root 'runtime\obs32\bin\64bit\obs64.exe'
if(Get-Process obs64 -ErrorAction SilentlyContinue|Where-Object Path -EQ $exe){throw 'The isolated OBS test instance is still running.'}
$plugins=Join-Path $root 'runtime\test-plugins'
Get-ChildItem -LiteralPath $plugins -Filter '*.dll'|Remove-Item
$name=if($Template){'obs-template-smoke'}else{'obs-spatial-fx'}
if(!$Baseline){Copy-Item -LiteralPath (Join-Path $root ('build\'+$name+'.dll')) -Destination $plugins}
if($env:SPATIAL_QA_OUTPUT){Copy-Item (Join-Path $root 'build/spatial-qa-driver.dll') $plugins}
if(!$Template){Copy-Item -Path (Join-Path $root 'data\*') -Destination (Join-Path $root 'runtime\plugin-data\obs-spatial-fx') -Recurse -Force}
$env:OBS_PLUGINS_PATH=$plugins
$env:OBS_PLUGINS_DATA_PATH=Join-Path $root 'runtime\plugin-data'
$p=Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -ArgumentList '--portable','--multi','--disable-updater','--disable-missing-files-check','--profile','"Spatial FX Test"' -PassThru
Write-Output ('Isolated OBS process '+$p.Id)
