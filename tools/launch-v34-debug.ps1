param([ValidateSet(32,33)][int]$Major=33,[string]$Scenario='v34',[string]$Output='work/v3.4-development/debug33')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$hostRoot=Join-Path $root ('runtime/obs'+$Major)
$exe=Join-Path $hostRoot 'bin/64bit/obs64.exe'
if(Get-Process obs64 -ErrorAction SilentlyContinue){throw 'Existing OBS is running; no process terminated.'}
$env:SPATIAL_QA_SCENARIO=$Scenario
$env:SPATIAL_QA_OUTPUT=Join-Path $root $Output
New-Item $env:SPATIAL_QA_OUTPUT -ItemType Directory -Force | Out-Null
$plugins=Join-Path $hostRoot 'plugins'
foreach($name in @('obs-spatial-fx','spatial-qa-driver')){
 $dest=Join-Path $plugins $name
 New-Item (Join-Path $dest 'data') -ItemType Directory -Force | Out-Null
 Copy-Item (Join-Path $root ('build/'+$name+'.dll')) (Join-Path $dest ($name+'.dll')) -Force
 Copy-Item (Join-Path $root ('build/'+$name+'.pdb')) $dest -Force -ErrorAction SilentlyContinue
}
Copy-Item (Join-Path $root 'data/*') (Join-Path $plugins 'obs-spatial-fx/data') -Recurse -Force
if($Major -eq 33){$env:OBS_PLUGINS_PATH=$plugins;$env:OBS_PLUGINS_DATA_PATH=$null}
else {$env:OBS_PLUGINS_PATH=(Join-Path $plugins '%module%');$env:OBS_PLUGINS_DATA_PATH=(Join-Path $plugins '%module%/data')}
$env:OBS_LEGACY_PLUGINS_PATH=$null
$env:OBS_LEGACY_PLUGINS_DATA_PATH=$null
$args=@(('"'+(Join-Path $env:SPATIAL_QA_OUTPUT 'debugger.log')+'"'),('"\"'+$exe+'\" --portable --multi --disable-updater --disable-missing-files-check --profile \"Spatial FX Test\""'))
$p=Start-Process (Join-Path $root 'build/spatial-debugger.exe') -WorkingDirectory (Split-Path $exe -Parent) -ArgumentList $args -PassThru
Write-Output ('Debugger PID '+$p.Id+' output '+$env:SPATIAL_QA_OUTPUT)
