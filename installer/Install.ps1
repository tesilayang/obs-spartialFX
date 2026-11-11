[CmdletBinding(SupportsShouldProcess)]
param([string]$ObsRoot,[string]$PackageRoot=(Join-Path $PSScriptRoot '..\package\obs-spatial-fx'))
$ErrorActionPreference='Stop'
if(Get-Process obs64 -ErrorAction SilentlyContinue){throw 'OBS is running. Close OBS before installing or updating.'}
$instances=@(& (Join-Path $PSScriptRoot 'Detect-OBS.ps1') -ObsRoot $ObsRoot)
if(!$instances.Count){throw 'OBS32+ not detected. Specify -ObsRoot for an unusual portable location.'}
if(!(Test-Path -LiteralPath (Join-Path $PackageRoot 'bin\64bit\obs-spatial-fx.dll'))){throw 'Compiled package is missing.'}
$target=Join-Path $env:ProgramData 'obs-studio\plugins\obs-spatial-fx'
if($PSCmdlet.ShouldProcess($target,'Install OBS Spatial FX')){
    New-Item -ItemType Directory -Force $target|Out-Null
    Copy-Item -Path (Join-Path $PackageRoot '*') -Destination $target -Recurse -Force
    Write-Output ('Installed: '+$target)
}
$instances
