$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$out=Join-Path $root 'work/v3.4-debug-20261111/installer'
New-Item $out -ItemType Directory -Force|Out-Null
$exe=Join-Path $root 'build/OBS_Spatial_FX_V3.4_Setup.exe'
$base=Join-Path $env:ProgramData 'obs-studio/plugins/obs-spatial-fx'
$checks=@()
function Check([bool]$ok,[string]$name){$script:checks+=@{name=$name;pass=$ok};$script:checks|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 (Join-Path $out 'installer-tests.json');Write-Output (('FAIL ','PASS ')[[int]$ok]+$name);if(!$ok){throw $name}}
function Run([string]$binary,[string]$label){$args='/SILENT /OBSROOT="'+(Join-Path $root 'runtime/obs32')+'" /DETECTEDROOT="'+(Join-Path $root 'runtime/obs33')+'" /LOG="'+(Join-Path $out ($label+'.log'))+'"';Add-Content (Join-Path $out 'commands.log') ('"'+$binary+'" '+$args);$p=Start-Process $binary -ArgumentList $args -PassThru -Wait;return $p.ExitCode}
function Hashes{Get-ChildItem $base -File -Recurse|Sort-Object FullName|ForEach-Object { "$($_.FullName):$((Get-FileHash -LiteralPath $_.FullName).Hash)" }}
function ConfigHashes{foreach($major in @(32,33)){foreach($folder in @('basic/scenes','plugin_config/obs-spatial-fx')){Get-ChildItem (Join-Path $root ('runtime/obs'+$major+'/config/obs-studio/'+$folder)) -Filter '*.json' -File -Recurse -ErrorAction SilentlyContinue|Sort-Object FullName|ForEach-Object { "$($_.FullName):$((Get-FileHash -LiteralPath $_.FullName).Hash)" }}}}
if(Get-Process obs64 -ErrorAction SilentlyContinue){throw 'OBS must finish before upgrade tests.'}
if(!(Test-Path (Join-Path $out 'installed-before'))){Copy-Item -LiteralPath $base -Destination (Join-Path $out 'installed-before') -Recurse}
Check ((Run (Join-Path $root 'work/v3.4-debug-20261111/baseline/OBS_Spatial_FX_V3.4_Setup.exe') 'install-original-v34') -eq 0) 'real original v3.4 baseline installer succeeds'
$configBefore=@(ConfigHashes)
$before=@(Hashes)
$lock=[IO.File]::Open((Join-Path $base 'data/effects/spatial.effect'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try{$code=Run $exe 'upgrade-locked'}finally{$lock.Dispose()}
Check ($code -eq 1) 'locked payload rejects v3.4 upgrade'
Check (!(Compare-Object $before @(Hashes))) 'failed upgrade restores every baseline file byte-for-byte'
Check ((Run $exe 'upgrade-v34') -eq 0) 'same-version v3.4 patch upgrade succeeds'
Check (!(Compare-Object $configBefore @(ConfigHashes))) 'upgrade preserves OBS32 and33 scene JSON and plugin configuration'
Check ((Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Spatial FX').DisplayVersion -eq '3.4.0') 'Windows control panel registers version3.4.0'
Check ((Get-FileHash (Join-Path $root 'build/obs-spatial-fx.dll')).Hash -eq (Get-FileHash (Join-Path $base 'bin/64bit/obs-spatial-fx.dll')).Hash) 'installed DLL equals tested build'
Check (@(Get-ChildItem $base -Recurse -Filter 'Qt*.dll').Count -eq 0) 'installer packages no Qt DLL'
Check (@(Get-ChildItem $base -Recurse -Filter '*qa-driver*').Count -eq 0) 'installer packages no QA driver'
Check (@(Get-ChildItem (Join-Path $base 'data/effects') -Filter '*.body').Count -eq 15) 'installer deploys all15 FX shader bodies'
$preset=Join-Path $root 'runtime/obs32/config/obs-studio/plugin_config/obs-spatial-fx/presets.json'
$presetHash=if(Test-Path $preset){(Get-FileHash $preset).Hash}else{''}
$sentinel=Join-Path $base 'qa-v34-unowned-preserve.txt'
if(Test-Path $sentinel){throw 'sentinel already exists'}
Set-Content -Encoding UTF8 $sentinel 'QA unowned file must survive uninstall'
$uninstallArgs='/UNINSTALL /SILENT /LOG="'+(Join-Path $out 'uninstall.log')+'"'
$p=Start-Process (Join-Path $base 'Uninstall.exe') -ArgumentList $uninstallArgs -PassThru -Wait
Check ($p.ExitCode -eq 0) 'installed uninstaller starts helper'
for($i=0;$i -lt 100 -and (Test-Path (Join-Path $base 'install-manifest.txt'));$i++){Start-Sleep -Milliseconds 200}
Check (!(Test-Path (Join-Path $base 'bin/64bit/obs-spatial-fx.dll'))) 'uninstaller removes owned plugin DLL'
Check (!(Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Spatial FX')) 'uninstaller removes Windows entry'
Check (Test-Path $sentinel) 'uninstaller preserves unowned user files'
Check (($presetHash -eq '') -or ((Get-FileHash $preset).Hash -eq $presetHash)) 'uninstaller preserves existing presets'
Check (!(Compare-Object $configBefore @(ConfigHashes))) 'uninstaller preserves OBS32 and33 scene JSON and plugin configuration'
Remove-Item -LiteralPath $sentinel
Check ((Run $exe 'final-install') -eq 0) 'final v3.4 reinstall succeeds'
Check ((Get-FileHash $exe).Hash -eq (Get-FileHash (Join-Path $base 'Uninstall.exe')).Hash) 'installed uninstaller equals final setup'
