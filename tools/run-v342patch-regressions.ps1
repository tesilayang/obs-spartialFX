param([string]$Output='work/v3.4.2-fxpatch-20261112')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $root
$python='C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$env:PYTHONIOENCODING='utf-8'
$bridge="$Output/final-bridge"
$codes=[ordered]@{}
function Probe($name,$script,$destination,$extra=@()){
 Add-Content "$Output/final-commands.log" "& '$python' $script $destination $bridge $extra"
 & $python $script $destination $bridge @extra *> "$Output/final-$name.log"
 $script:codes[$name]=$LASTEXITCODE
 $script:codes|ConvertTo-Json|Set-Content "$Output/final-exits.json" -Encoding UTF8
 if($LASTEXITCODE -ne 0){throw "Failed $name; inspect final-$name.log"}
 Write-Output "Completed $name"
}
foreach($job in @(
 @('patch','tests/v342patch_probe.py'),
 @('fx342','tests/v342_fx_probe.py'),
 @('quality','tests/v341_quality_probe.py'),
 @('colors','tests/v34_live_colors_probe.py'),
 @('siblings','tests/v34_blur_siblings.py'),
 @('blurquality','tests/v34_blur_quality.py'),
 @('edge','tests/v34_blur_edgecases.py'),
 @('light','tests/v34_lighting_controls_probe.py'),
 @('composition','tests/v34_debug_probe.py'),
 @('gpu','tests/v34_render_probe.py')
)){Probe $job[0] $job[1] "$Output/final-$($job[0])"}
Probe 'coordinates' 'tests/v34_coordinate_probe.py' "$Output/final-gpu"
Probe 'encode' 'tests/v341_encode_probe.py' "$Output/final-encode" @("$Output/final-quality/fixture.png")
$ready=Get-Content "$bridge/native-ready.json" -Raw|ConvertFrom-Json
$owned=Get-Process -Id $ready.pid -ErrorAction Stop
if($owned.Path -ne (Join-Path $root 'runtime/obs33/bin/64bit/obs64.exe')){throw 'Unexpected OBS'}
$latest=Get-ChildItem 'runtime/obs33/config/obs-studio/logs' -Filter '*.txt'|Sort-Object LastWriteTime -Descending|Select-Object -First 1
$null=$owned.CloseMainWindow()
if(!$owned.WaitForExit(15000)){throw 'QA OBS did not close normally'}
Copy-Item -LiteralPath $latest.FullName -Destination "$bridge/obs-runtime.log" -Force
if(Select-String -LiteralPath $latest.FullName -Pattern 'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(failed|failure)'){throw 'Runtime render error'}
foreach($pair in @(@('ui34','v34'),@('persist34','v34-persist'),@('ui341','v341'),@('persist341','v341-persist'),@('ui342','v342'),@('persist342','v342-persist'),@('ui-patch','v342patch'),@('persist-patch','v342patch-persist'))){
 $name=$pair[0]
 Add-Content "$Output/final-commands.log" "& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario $($pair[1]) -Output $Output/final-$name -Wait"
 & tools/run_v33_installed_qa.ps1 -Major 33 -Scenario $pair[1] -Output "$Output/final-$name" -Wait *> "$Output/final-$name.log"
 $codes[$name]=if($?){0}else{1}
 $codes|ConvertTo-Json|Set-Content "$Output/final-exits.json" -Encoding UTF8
 if($codes[$name]){throw "Native QA failed $name"}
 Write-Output "Completed $name"
}
