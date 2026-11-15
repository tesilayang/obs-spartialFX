param([Parameter(Mandatory=$true)][string]$Label,[Parameter(Mandatory=$true)][int[]]$ProcessIds,[int]$Seconds=30)
$ErrorActionPreference='Stop'
$taskOut=Join-Path $PSScriptRoot ($Label+'.json')
$taskCores=[Environment]::ProcessorCount
$taskPrevious=@{}
foreach($taskId in $ProcessIds){$taskP=Get-Process -Id $taskId;$taskPrevious[$taskId]=@{cpu=$taskP.TotalProcessorTime.TotalSeconds;t=[Diagnostics.Stopwatch]::GetTimestamp()}}
$taskSamples=@()
$taskWatch=[Diagnostics.Stopwatch]::StartNew()
while($taskWatch.Elapsed.TotalSeconds -lt $Seconds){
 $taskCounterErrors=@()
 $taskCounters=@((Get-Counter '\GPU Engine(*)\Utilization Percentage','\GPU Process Memory(*)\Dedicated Usage','\GPU Process Memory(*)\Shared Usage','\Process(*)\ID Process','\Process(*)\Working Set - Private','\Processor(_Total)\% Processor Time' -SampleInterval 1 -MaxSamples 1 -ErrorAction SilentlyContinue -ErrorVariable taskCounterErrors).CounterSamples | Where-Object {$_.Status -in @(0,1)})
 $taskRows=@()
 foreach($taskId in $ProcessIds){
  $taskP=Get-Process -Id $taskId
  $taskNow=[Diagnostics.Stopwatch]::GetTimestamp();$taskCpu=$taskP.TotalProcessorTime.TotalSeconds
  $taskDt=($taskNow-$taskPrevious[$taskId].t)/[Diagnostics.Stopwatch]::Frequency
  $taskUsage=100*($taskCpu-$taskPrevious[$taskId].cpu)/$taskDt/$taskCores
  $taskPrevious[$taskId]=@{cpu=$taskCpu;t=$taskNow}
  $taskGpu=@($taskCounters | Where-Object InstanceName -Like "pid_${taskId}_*" | ForEach-Object {@{instance=$_.InstanceName;path=$_.Path;value=$_.CookedValue}})
  if(-not @($taskGpu | Where-Object path -Like '*\utilization percentage').Count){throw 'Missing valid GPU counter for target process'}
  $taskInstance=@($taskCounters | Where-Object {$_.Path -Like '*\id process' -and [int]$_.CookedValue -eq $taskId})
  if($taskInstance.Count -ne 1){throw 'Missing unique private working-set process counter'}
  $taskPrivate=@($taskCounters | Where-Object {$_.InstanceName -eq $taskInstance[0].InstanceName -and $_.Path -Like '*\working set - private'})
  if($taskPrivate.Count -ne 1){throw 'Missing private working-set value'}
  $taskRows+=@{pid=$taskId;name=$taskP.ProcessName;cpu_system_percent=$taskUsage;working_set_bytes=$taskP.WorkingSet64;private_working_set_bytes=$taskPrivate[0].CookedValue;private_bytes=$taskP.PrivateMemorySize64;gpu=$taskGpu}
 }
 $taskOtherGpu=@($taskCounters|Where-Object {$_.Path -like '*\utilization percentage' -and $_.InstanceName -match '^pid_(\d+)_' -and [int]$Matches[1] -notin $ProcessIds}|ForEach-Object {@{instance=$_.InstanceName;value=$_.CookedValue}})
 $taskTotalCpu=@($taskCounters|Where-Object Path -Like '*\processor(_total)\% processor time')
 $taskSamples+=@{utc=[DateTime]::UtcNow.ToString('o');non_target_counter_error_count=$taskCounterErrors.Count;processes=$taskRows;other_gpu=$taskOtherGpu;system_cpu_percent=if($taskTotalCpu.Count){$taskTotalCpu[0].CookedValue}else{$null}}
 @{label=$Label;logical_processors=$taskCores;requested_seconds=$Seconds;elapsed_seconds=$taskWatch.Elapsed.TotalSeconds;samples=$taskSamples}|ConvertTo-Json -Depth 9 | Set-Content -LiteralPath $taskOut -Encoding utf8
}
Write-Output ($Label+': '+$taskSamples.Count+' samples')
