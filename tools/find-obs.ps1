$root=Split-Path $PSScriptRoot -Parent
$instances=@(& (Join-Path $root 'installer/Detect-OBS.ps1'))
$found=$instances | Where-Object Supported | Sort-Object Portable,@{Expression={[version]$_.Version};Descending=$true} | Select-Object -First 1
if($found){Write-Output $found.Root}else{exit 1}
