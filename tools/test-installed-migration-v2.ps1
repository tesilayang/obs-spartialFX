$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$base=Join-Path $env:ProgramData 'obs-studio/plugins/obs-spatial-fx'
$qa=Join-Path $base 'bin/64bit/spatial-qa-driver.dll'
$python='C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
if(Get-Process obs64 -ErrorAction SilentlyContinue){throw 'OBS running before migration test'}
& $python (Join-Path $root 'tools/prepare_v1_fixture.py');if($LASTEXITCODE){throw 'Fixture preparation failed'}
Copy-Item (Join-Path $root 'build/spatial-qa-driver.dll') $qa
try{
 foreach($run in @('first','restart')){
  $env:SPATIAL_QA_SCENARIO='v2migrate';$env:SPATIAL_QA_OUTPUT=Join-Path $root ('logs/v2/installed-migration-'+$run)
  $env:OBS_PLUGINS_PATH=$null;$env:OBS_PLUGINS_DATA_PATH=$null
  Start-Process -FilePath (Join-Path $env:ProgramData 'Microsoft/Windows/Start Menu/Programs/OBS Spatial FX/OBS Spatial FX - Portable 1.lnk')
  Start-Sleep -Seconds 3
  $p=Get-Process obs64 | Where-Object Path -EQ (Join-Path $root 'runtime/obs32/bin/64bit/obs64.exe')
  if(!$p){throw 'Installed portable shortcut did not start OBS'}
  if(!$p.WaitForExit(45000)){throw 'Migration OBS failed to close normally'}
  if($p.ExitCode){throw 'Migration OBS nonzero exit'}
  $report=Get-Content (Join-Path $env:SPATIAL_QA_OUTPUT 'dock-smoke.json') -Raw|ConvertFrom-Json
  if($report.failed){throw 'Native migration assertions failed'}
  $last=Get-ChildItem (Join-Path $root 'runtime/obs32/config/obs-studio/logs/*.txt')|Sort-Object LastWriteTime|Select-Object -Last 1
  Copy-Item $last.FullName (Join-Path $env:SPATIAL_QA_OUTPUT 'obs-runtime.log')
  $saved=Get-Content (Join-Path $root 'runtime/obs32/config/obs-studio/basic/scenes/Spatial_FX_QA.json') -Raw|ConvertFrom-Json
  $fixture=Get-Content (Join-Path $root 'logs/v2/legacy-fixture-prepared.json') -Raw|ConvertFrom-Json
  $source=$saved.sources|Where-Object name -EQ $fixture.stage
  $state=$source.private_settings.spatial_fx_model
  $ok=$state.schema_version -eq 2 -and $state.layers[0].z -gt 0 -and $state.animation_duration -eq 3
  @{run=$run;schema=$state.schema_version;layerDepth=$state.layers[0].z;duration=$state.animation_duration;pass=$ok}|ConvertTo-Json|Set-Content -Encoding UTF8 (Join-Path $env:SPATIAL_QA_OUTPUT 'saved-migration.json')
  if(!$ok){throw 'Migrated private model did not persist to disk'}
  Write-Output ('PASS installed v1 migration '+$run)
 }
}finally{
 $p=Get-Process obs64 -ErrorAction SilentlyContinue|Where-Object Path -EQ (Join-Path $root 'runtime/obs32/bin/64bit/obs64.exe');if($p){Stop-Process -Id $p.Id}
 if(Test-Path $qa){Remove-Item -LiteralPath $qa}
}
