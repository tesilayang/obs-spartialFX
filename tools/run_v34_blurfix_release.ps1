param([ValidateSet(32,33)][int]$Major=32)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $root
$out='work/v3.4-blurfix-20261111/release'
$bridge="$out/bridge$Major"
$python='C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$ready=Get-Content "$bridge/native-ready.json" -Raw|ConvertFrom-Json
$owned=Get-Process -Id $ready.pid -ErrorAction Stop
$expected=Join-Path $root "runtime/obs$Major/bin/64bit/obs64.exe"
if(!$ready.ready -or $owned.Path -ne $expected){throw 'Owned frontend-ready OBS required'}
$codes=[ordered]@{}
foreach($entry in @(
 @('siblings','tests/v34_blur_siblings.py'),
 @('quality','tests/v34_blur_quality.py'),
 @('edge','tests/v34_blur_edgecases.py'),
 @('light','tests/v34_lighting_controls_probe.py'),
 @('patch','tests/v34_debug_probe.py'),
 @('gpu','tests/v34_render_probe.py')
)){
 $name=$entry[0]+$Major
 Add-Content "$out/commands$Major.log" "& '$python' $($entry[1]) $out/$name $bridge"
 & $python $entry[1] "$out/$name" $bridge *> "$out/$name.log"
 $codes[$name]=$LASTEXITCODE
 $codes|ConvertTo-Json|Set-Content "$out/exits$Major.json" -Encoding UTF8
 if($LASTEXITCODE -ne 0){throw "Failed $name; inspect log"}
 Write-Output "Completed $name"
}
& $python tests/v34_coordinate_probe.py "$out/gpu$Major" $bridge *> "$out/coordinates$Major.log"
$codes["coordinates$Major"]=$LASTEXITCODE
$codes|ConvertTo-Json|Set-Content "$out/exits$Major.json" -Encoding UTF8
if($LASTEXITCODE -ne 0){throw 'Coordinate regression failed'}
$latest=Get-ChildItem "runtime/obs$Major/config/obs-studio/logs" -Filter '*.txt'|Sort-Object LastWriteTime -Descending|Select-Object -First 1
if(!$latest){throw 'Runtime log missing'}
Copy-Item -LiteralPath $latest.FullName -Destination "$out/bridge$Major/obs-runtime.log" -Force
$bad=Select-String -LiteralPath $latest.FullName -Pattern 'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(failed|failure)'
if($bad){$bad|Set-Content "$out/render-errors$Major.log";throw 'Runtime render errors found'}
$null=$owned.CloseMainWindow()
if(!$owned.WaitForExit(15000)){throw 'QA OBS did not close normally'}
Copy-Item -LiteralPath $latest.FullName -Destination "$out/bridge$Major/obs-runtime.log" -Force
if((Select-String -LiteralPath $latest.FullName -Pattern 'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(failed|failure)')){throw 'Runtime errors on shutdown'}
foreach($pair in @(@('ui','v34'),@('persist','v34-persist'))){
 $name=$pair[0]+$Major
 & tools/run_v33_installed_qa.ps1 -Major $Major -Scenario $pair[1] -Output "$out/$name" -Wait *> "$out/$name.log"
 $codes[$name]=if($?){0}else{1}
 $codes|ConvertTo-Json|Set-Content "$out/exits$Major.json" -Encoding UTF8
 if($codes[$name] -ne 0){throw "Native QA failed $name"}
 Write-Output "Completed $name"
}
Write-Output "All installed OBS$Major release tests finished"
