$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$base=Join-Path $env:ProgramData 'obs-studio/plugins/obs-spatial-fx'
$qa=Join-Path $base 'bin/64bit/spatial-qa-driver.dll'
$python='C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
if(Get-Process obs64 -ErrorAction SilentlyContinue){throw 'OBS running before portable release test'}
Copy-Item (Join-Path $root 'build/spatial-qa-driver.dll') $qa
try{
 $env:SPATIAL_QA_SCENARIO='hold';$env:SPATIAL_QA_HOLD_SECONDS='85';$env:SPATIAL_QA_OUTPUT=Join-Path $root 'logs/release-installed-performance'
 $env:OBS_PLUGINS_PATH=$null;$env:OBS_PLUGINS_DATA_PATH=$null
 Start-Process -FilePath (Join-Path $env:ProgramData 'Microsoft/Windows/Start Menu/Programs/OBS Spatial FX/OBS Spatial FX - Portable 1.lnk')
 Start-Sleep -Seconds 3
 $setup=Join-Path $root 'build/OBS_Spatial_FX_V1.0_Setup.exe'
 $dllHash=(Get-FileHash (Join-Path $base 'bin/64bit/obs-spatial-fx.dll')).Hash
 $commands=@(
  ('/DIAGNOSE="'+(Join-Path $root 'logs/installer-running-discovery.json')+'" /LOG="'+(Join-Path $root 'logs/installer-running-discovery.log')+'"'),
  ('/SILENT /LOG="'+(Join-Path $root 'logs/release-installer-running-guard.log')+'"'),
  ('/UNINSTALL /SILENT /LOG="'+(Join-Path $root 'logs/release-installer-running-guard.log')+'"')
 )
 $codes=@();foreach($argsLine in $commands){$p=Start-Process $setup -ArgumentList $argsLine -PassThru -Wait;$codes+=$p.ExitCode}
 $guard=@{diagnoseExit=$codes[0];installExit=$codes[1];uninstallExit=$codes[2];dllUnchanged=((Get-FileHash (Join-Path $base 'bin/64bit/obs-spatial-fx.dll')).Hash -eq $dllHash)}
 $guard|ConvertTo-Json|Set-Content -Encoding UTF8 (Join-Path $root 'logs/release-installer-guard.json')
 if($codes[0] -ne 0 -or $codes[1] -ne 10 -or $codes[2] -ne 10 -or !$guard.dllUnchanged){throw 'Running OBS guard failed'}
 & $python -u (Join-Path $root 'tests/performance_probe.py') (Join-Path $root 'logs/release-installed-performance')
 $probeExit=$LASTEXITCODE
 $obs=Get-Process obs64|Where-Object Path -EQ (Join-Path $root 'runtime/obs32/bin/64bit/obs64.exe')
 if($obs -and !$obs.WaitForExit(45000)){throw 'QA did not close installed portable OBS'}
 if($obs -and $obs.ExitCode){throw 'OBS nonzero exit'}
 $last=Get-ChildItem (Join-Path $root 'runtime/obs32/config/obs-studio/logs/*.txt') | Sort-Object LastWriteTime | Select-Object -Last 1;Copy-Item $last.FullName (Join-Path $root 'logs/release-installed-performance-obs-runtime.log')
 if($probeExit){throw 'Installed release performance probe failed'}
 'PASS installed portable OBS exited normally'
}finally{
 $obs=Get-Process obs64 -ErrorAction SilentlyContinue|Where-Object Path -EQ (Join-Path $root 'runtime/obs32/bin/64bit/obs64.exe');if($obs){Stop-Process -Id $obs.Id}
 if(Test-Path $qa){Remove-Item -LiteralPath $qa}
}
