[CmdletBinding()]
param([string]$ObsRoot, [switch]$IncludeUnsupported)
$ErrorActionPreference='Stop'
$roots=[System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
function Add-Root([string]$Root){
    if($Root -and (Test-Path -LiteralPath (Join-Path $Root 'bin\64bit\obs64.exe'))){[void]$roots.Add([IO.Path]::GetFullPath($Root))}
}
if($ObsRoot){Add-Root $ObsRoot}
foreach($base in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall','HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall','HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall')){
    Get-ChildItem $base -ErrorAction SilentlyContinue | ForEach-Object {
        $entry=Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue
        if($entry.DisplayName -like '*OBS Studio*'){
            Add-Root $entry.InstallLocation
            if($entry.DisplayIcon){$icon=($entry.DisplayIcon -replace ',\d+$','').Trim('"'); if($icon -like '*\bin\64bit\obs64.exe'){Add-Root (Split-Path (Split-Path (Split-Path $icon -Parent) -Parent) -Parent)}}
        }
    }
}
foreach($drive in Get-PSDrive -PSProvider FileSystem){
    foreach($folder in @('','Program Files','Program Files (x86)','Apps','Tools')){
        $base=if($folder){Join-Path $drive.Root $folder}else{$drive.Root}
        Get-ChildItem -LiteralPath $base -Directory -ErrorAction SilentlyContinue | Where-Object Name -Like '*obs*' | ForEach-Object {Add-Root $_.FullName}
    }
}
Add-Root (Join-Path $PSScriptRoot '..\runtime\obs32')
foreach($root in $roots){
    $version=[Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $root 'bin\64bit\obs64.exe')).FileVersion
    $supported=$version -match '^(\d+)\.' -and [int]$Matches[1] -ge 32
    if($supported -or $IncludeUnsupported){
        [pscustomobject]@{Root=$root;Version=$version;Supported=$supported;Portable=(@('portable_mode','portable_mode.txt','obs_portable_mode','obs_portable_mode.txt')|Where-Object {Test-Path -LiteralPath (Join-Path $root $_)}).Count -gt 0}
    }
}
