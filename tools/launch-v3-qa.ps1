param([ValidateSet(32,33)][int]$Major=33,[string]$Scenario='v3',[string]$Output='work/v3/obs33-ui-final',[int]$HoldSeconds=300)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$hostRoot=Join-Path $root ('runtime/obs'+$Major)
$exe=Join-Path $hostRoot 'bin/64bit/obs64.exe'
if(Get-Process obs64 -ErrorAction SilentlyContinue){throw 'Close the prior QA instance first; no existing OBS is terminated.'}
$env:SPATIAL_QA_SCENARIO=$Scenario
$env:SPATIAL_QA_OUTPUT=Join-Path $root $Output
$env:SPATIAL_QA_HOLD_SECONDS=$HoldSeconds
New-Item $env:SPATIAL_QA_OUTPUT -ItemType Directory -Force | Out-Null
if($Major -eq 32){
  & (Join-Path $PSScriptRoot 'launch-test-obs.ps1')
  exit
}
$plugins=Join-Path $hostRoot 'plugins'
foreach($name in @('obs-spatial-fx','spatial-qa-driver')){
  $dest=Join-Path $plugins $name
  New-Item (Join-Path $dest 'data') -ItemType Directory -Force | Out-Null
  Copy-Item (Join-Path $root ('build/'+$name+'.dll')) (Join-Path $dest ($name+'.dll')) -Force
}
Copy-Item (Join-Path $root 'data/*') (Join-Path $plugins 'obs-spatial-fx/data') -Recurse -Force
# OBS33 reuses the plural variable name with a different structured-root meaning.
$env:OBS_PLUGINS_PATH=$plugins
$env:OBS_PLUGINS_DATA_PATH=$null
$env:OBS_LEGACY_PLUGINS_PATH=$null
$env:OBS_LEGACY_PLUGINS_DATA_PATH=$null
$p=Start-Process $exe -WorkingDirectory (Split-Path $exe -Parent) -ArgumentList '--portable','--multi','--disable-updater','--disable-missing-files-check','--profile','"Spatial FX Test"' -PassThru
Write-Output ('OBS'+$Major+' QA PID '+$p.Id+' output '+$env:SPATIAL_QA_OUTPUT)
