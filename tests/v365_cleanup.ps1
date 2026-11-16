param([string]$Bridge='work/v3.6.5/delivery-bridge')
$ErrorActionPreference='Stop'
$taskRoot=Join-Path $PSScriptRoot '../work/v3.6.5'
$runtimeRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../runtime/obs33'))
$ready=Get-Content (Join-Path $Bridge 'native-ready.json') -Raw -Encoding UTF8|ConvertFrom-Json
$obsTask=Get-Process -Id $ready.pid
if($obsTask.Path -ne (Join-Path $runtimeRoot 'bin/64bit/obs64.exe')){throw 'Unexpected OBS process path'}
$finish=@{id=[guid]::NewGuid().ToString();action='finish'}|ConvertTo-Json
[IO.File]::WriteAllText((Join-Path (Resolve-Path $Bridge) 'native-command.json'),$finish)
Wait-Process -Id $obsTask.Id -Timeout 15
$latest=Get-ChildItem (Join-Path $runtimeRoot 'config/obs-studio/logs') -Filter '*.txt'|Sort-Object LastWriteTime -Descending|Select-Object -First 1
Copy-Item -LiteralPath $latest.FullName -Destination (Join-Path $Bridge 'obs-runtime.log')
if(Get-Process obs64 -ErrorAction SilentlyContinue){throw 'OBS process remains; stop cleanup'}
$config=Join-Path $runtimeRoot 'config'
$backup=Join-Path $taskRoot 'before/config'
$saved=Join-Path $taskRoot 'test-config'
if(Test-Path -LiteralPath $saved){throw 'Evidence destination exists'}
if([IO.Path]::GetFullPath($config) -ne (Join-Path $runtimeRoot 'config')){throw 'Unexpected config path'}
Move-Item -LiteralPath $config -Destination $saved
Copy-Item -LiteralPath $backup -Destination $config -Recurse
$plugins=Join-Path $runtimeRoot 'plugins'
$qaArchive=Join-Path $taskRoot 'removed-qa-plugins'
New-Item -ItemType Directory -Path $qaArchive|Out-Null
foreach($name in @('spatial-qa-driver','win-spout','distroav')){
 $target=Join-Path $plugins $name
 if(Test-Path -LiteralPath $target){
  if([IO.Path]::GetFullPath($target) -ne (Join-Path $runtimeRoot ('plugins/'+$name))){throw 'Unexpected plugin path'}
  Move-Item -LiteralPath $target -Destination (Join-Path $qaArchive $name)
 }
}
$original=Join-Path $taskRoot 'before/runtime-spatial'
$restore=Join-Path $plugins 'obs-spatial-fx'
if(Test-Path -LiteralPath $restore){throw 'Refuse to overwrite runtime plugin'}
Copy-Item -LiteralPath $original -Destination $restore -Recurse
function Inventory([string]$folder){
 $result=@{}
 Get-ChildItem -LiteralPath $folder -File -Recurse|ForEach-Object{
  $relative=$_.FullName.Substring([IO.Path]::GetFullPath($folder).Length).TrimStart('\','/')
  $result[$relative]=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
 }
 return $result
}
$before=Inventory $backup;$after=Inventory $config
if($before.Count -ne $after.Count){throw 'Config inventory mismatch'}
foreach($key in $before.Keys){if($after[$key] -ne $before[$key]){throw ('Config hash mismatch: '+$key)}}
$originalPlugin=Inventory $original;$restoredPlugin=Inventory $restore
if($originalPlugin.Count -ne $restoredPlugin.Count){throw 'Plugin inventory mismatch'}
foreach($key in $originalPlugin.Keys){if($restoredPlugin[$key] -ne $originalPlugin[$key]){throw ('Runtime plugin mismatch: '+$key)}}
$listening=@(Get-NetTCPConnection -LocalPort 4457 -State Listen -ErrorAction SilentlyContinue)
if($listening.Count){throw 'Test listener remains'}
$result=@{status='PASS';normalExit=$true;configFilesRestored=$after.Count;runtimeFilesRestored=$restoredPlugin.Count;testListenerClosed=$true;qaPluginsRemoved=$true;installedManifest=(Get-Content 'C:/ProgramData/obs-studio/plugins/obs-spatial-fx/install-manifest.txt' -TotalCount 2);installedDllHash=(Get-FileHash 'C:/ProgramData/obs-studio/plugins/obs-spatial-fx/portable33/obs-spatial-fx/obs-spatial-fx.dll').Hash}
$result|ConvertTo-Json -Depth 5|Set-Content (Join-Path $taskRoot 'cleanup.json') -Encoding UTF8
$result|ConvertTo-Json -Depth 5
