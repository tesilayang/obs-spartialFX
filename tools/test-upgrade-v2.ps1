$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent;$exe=Join-Path $root 'build/OBS_Spatial_FX_V2.0_Setup.exe';$base=Join-Path $env:ProgramData 'obs-studio/plugins/obs-spatial-fx';$reg='HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Spatial FX';$checks=@()
function Check([bool]$ok,[string]$name){$script:checks+=@{name=$name;pass=$ok};Write-Output (('FAIL ','PASS ')[[int]$ok]+$name);if(!$ok){throw $name}}
function HashTree([string]$dir){@(Get-ChildItem $dir -File -Recurse|Sort-Object FullName|ForEach-Object{$_.FullName+':'+(Get-FileHash $_.FullName).Hash})}
function Run([string]$line){Add-Content (Join-Path $root 'logs/v2/upgrade-commands.log') $line;$p=Start-Process $exe -ArgumentList $line -WindowStyle Hidden -PassThru -Wait;return $p.ExitCode}
if(Get-Process obs64 -ErrorAction SilentlyContinue){throw 'OBS running before upgrade test'}
$beforeVersion=(Get-ItemProperty $reg).DisplayVersion;Check ($beforeVersion -eq '1.0.0') 'real installed baseline is v1.0.0'
$payload=HashTree $base;$links=Join-Path $env:ProgramData 'Microsoft/Windows/Start Menu/Programs/OBS Spatial FX';$linkHashes=HashTree $links
$lock=[IO.File]::Open((Join-Path $links 'Uninstall OBS Spatial FX.lnk'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try{$code=Run ('/SILENT /OBSROOT="'+(Join-Path $root 'runtime/obs32')+'" /LOG="'+(Join-Path $root 'logs/v2/upgrade-shortcut-rollback.log')+'"')}finally{$lock.Dispose()}
Check ($code -eq 1) 'locked shortcut rejects v1 update after payload stage'
Check (!(Compare-Object $payload (HashTree $base))) 'shortcut failure restores v1 payload byte-for-byte'
Check (!(Compare-Object $linkHashes (HashTree $links))) 'shortcut failure restores prior shortcut bytes'
Check ((Get-ItemProperty $reg).DisplayVersion -eq $beforeVersion) 'shortcut failure preserves v1 registry version'
$paths=@((Join-Path $root 'runtime/obs32/config/obs-studio/plugin_config/obs-spatial-fx/presets.json'),(Join-Path $root 'runtime/obs32/config/obs-studio/basic/scenes/Spatial_FX_QA.json'),(Join-Path $env:APPDATA 'obs-studio/plugin_config/obs-spatial-fx/presets.json'))
$before=@{};foreach($p in $paths){if(Test-Path $p){$before[$p]=(Get-FileHash $p).Hash}}
$sentinel=Join-Path $base 'qa-v2-upgrade-preserve.txt';Set-Content -Encoding UTF8 $sentinel 'Unowned file preserved through v1 to v2 upgrade'
$code=Run ('/SILENT /OBSROOT="'+(Join-Path $root 'runtime/obs32')+'" /LOG="'+(Join-Path $root 'logs/v2/upgrade-install.log')+'"')
Check ($code -eq 0) 'v2 installer upgrades actual installed v1 successfully'
Check ((Get-ItemProperty $reg).DisplayVersion -eq '2.0.0') 'same uninstall key now reports v2.0.0'
Check ([IO.Path]::GetFullPath((Get-ItemProperty $reg).InstallLocation).TrimEnd('\') -eq [IO.Path]::GetFullPath($base).TrimEnd('\')) 'upgrade retains original ProgramData install directory'
Check ((Get-FileHash (Join-Path $root 'build/obs-spatial-fx.dll')).Hash -eq (Get-FileHash (Join-Path $base 'bin/64bit/obs-spatial-fx.dll')).Hash) 'upgraded DLL exactly matches v2 release'
Check (Test-Path $sentinel) 'upgrade preserves unowned installed file'
foreach($p in $before.Keys){Check ((Get-FileHash $p).Hash -eq $before[$p]) ('upgrade preserves configuration bytes: '+$p)}
Remove-Item -LiteralPath $sentinel
@{baselineVersion=$beforeVersion;upgradedVersion=(Get-ItemProperty $reg).DisplayVersion;checks=$checks;failed=@($checks|Where-Object {!$_.pass}).Count}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 (Join-Path $root 'logs/v2/upgrade-tests.json')
