param([ValidateSet(32,33)][int]$Major=32,[string]$StartAt='quality')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $root
$out='work/v3.4.1/release'
$bridge="$out/bridge${Major}final"
$python='C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$codes=[ordered]@{}
if(Test-Path "$out/exits$Major.json"){foreach($property in (Get-Content "$out/exits$Major.json" -Raw|ConvertFrom-Json).PSObject.Properties){$codes[$property.Name]=$property.Value}}
$started=$false
foreach($entry in @(
 @('quality','tests/v341_quality_probe.py'),
 @('colors','tests/v34_live_colors_probe.py'),
 @('siblings','tests/v34_blur_siblings.py'),
 @('blurquality','tests/v34_blur_quality.py'),
 @('edge','tests/v34_blur_edgecases.py'),
 @('light','tests/v34_lighting_controls_probe.py'),
 @('patch','tests/v34_debug_probe.py'),
 @('gpu','tests/v34_render_probe.py')
)){
 if($entry[0] -eq $StartAt){$started=$true}
 if(!$started){continue}
 $name=$entry[0]+$Major
 Add-Content "work/v3.4.1/logs/commands$Major.log" "& '$python' $($entry[1]) $out/$name $bridge"
 & $python $entry[1] "$out/$name" $bridge *> "work/v3.4.1/logs/$name.log"
 $codes[$name]=$LASTEXITCODE
 $codes|ConvertTo-Json|Set-Content "$out/exits$Major.json" -Encoding UTF8
 if($LASTEXITCODE -ne 0){throw "Failed $name; inspect log"}
 Write-Output "Completed $name"
}
& $python tests/v34_coordinate_probe.py "$out/gpu$Major" $bridge *> "work/v3.4.1/logs/coordinates$Major.log"
$codes["coordinates$Major"]=$LASTEXITCODE
if($LASTEXITCODE -ne 0){throw 'Coordinate regression failed'}
& $python tests/v341_encode_probe.py "$out/encode$Major" $bridge "$out/quality$Major/fixture.png" *> "work/v3.4.1/logs/encode$Major.log"
$codes["encode$Major"]=$LASTEXITCODE
$codes|ConvertTo-Json|Set-Content "$out/exits$Major.json" -Encoding UTF8
if($LASTEXITCODE -ne 0){throw 'Encode test failed'}
$ready=Get-Content "$bridge/native-ready.json" -Raw|ConvertFrom-Json
$owned=Get-Process -Id $ready.pid -ErrorAction Stop
if($owned.Path -ne (Join-Path $root "runtime/obs$Major/bin/64bit/obs64.exe")){throw 'Unexpected OBS'}
$latest=Get-ChildItem "runtime/obs$Major/config/obs-studio/logs" -Filter '*.txt'|Sort-Object LastWriteTime -Descending|Select-Object -First 1
if(!$latest){throw 'Runtime log missing'}
$null=$owned.CloseMainWindow()
if(!$owned.WaitForExit(15000)){throw 'QA OBS did not close normally'}
Copy-Item -LiteralPath $latest.FullName -Destination "$bridge/obs-runtime.log" -Force
if((Select-String -LiteralPath $latest.FullName -Pattern 'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(failed|failure)')){throw 'Runtime render error'}
foreach($pair in @(@('ui','v34'),@('persist','v34-persist'),@('ui341','v341'),@('persist341','v341-persist'))){
 $name=$pair[0]+$Major
 & tools/run_v33_installed_qa.ps1 -Major $Major -Scenario $pair[1] -Output "$out/$name" -Wait *> "work/v3.4.1/logs/$name.log"
 $codes[$name]=if($?){0}else{1}
 $codes|ConvertTo-Json|Set-Content "$out/exits$Major.json" -Encoding UTF8
 if($codes[$name] -ne 0){throw "Native QA failed $name"}
 Write-Output "Completed $name"
}
Write-Output "All OBS$Major tests finished"
