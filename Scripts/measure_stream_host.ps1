param(
    [ValidateRange(10,86400)][int]$Seconds = 1200,
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$')][string]$Report = 'stream-host',
    [ValidateRange(1,60)][int]$IntervalSeconds = 10
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$GamePath = [IO.Path]::GetFullPath((Join-Path $ProjectRoot 'Saved/LivingWorld/Packaged/Windows/RuneSim/Binaries/Win64/RuneSim.exe'))
$OutputPath = Join-Path $ProjectRoot "Saved/LivingWorld/$Report.json"
if (Test-Path -LiteralPath $OutputPath) { throw "Report already exists: $OutputPath" }
$Initial = @(Get-Process RuneSim -ErrorAction SilentlyContinue | Where-Object Path -eq $GamePath)
if ($Initial.Count -ne 1) { throw 'Expected one running packaged RuneSim process.' }
$InitialPid = $Initial[0].Id
$GpuTool = Get-Command nvidia-smi -ErrorAction SilentlyContinue
$Samples = [System.Collections.Generic.List[object]]::new()
$Clock = [Diagnostics.Stopwatch]::StartNew()
do {
    $Current = @(Get-Process RuneSim -ErrorAction SilentlyContinue | Where-Object Path -eq $GamePath)
    $Game = if ($Current.Count -eq 1) { $Current[0] } else { $null }
    $GpuRows = @()
    if ($GpuTool) {
        $GpuRows = @(& $GpuTool.Source '--query-gpu=index,memory.used,memory.total,utilization.gpu,utilization.encoder' '--format=csv,noheader,nounits' 2>$null)
        if ($LASTEXITCODE -ne 0) { $GpuRows = @() }
    }
    $Sample = [ordered]@{
        time = (Get-Date).ToUniversalTime().ToString('o')
        elapsed_seconds = [Math]::Round($Clock.Elapsed.TotalSeconds,2)
        process_id = if ($Game) { $Game.Id } else { $null }
        process_unchanged = ($null -ne $Game -and $Game.Id -eq $InitialPid)
        private_bytes = if ($Game) { $Game.PrivateMemorySize64 } else { $null }
        working_set_bytes = if ($Game) { $Game.WorkingSet64 } else { $null }
        cpu_seconds = if ($Game) { $Game.CPU } else { $null }
        gpu_device_csv = $GpuRows
    }
    $Samples.Add($Sample)
    $Valid = @($Samples | Where-Object { $null -ne $_.private_bytes })
    $Completed = $Clock.Elapsed.TotalSeconds -ge $Seconds
    $Result = [ordered]@{
        completed = $Completed
        duration_seconds = $Sample.elapsed_seconds
        initial_process_id = $InitialPid
        process_interruptions = @($Samples | Where-Object { -not $_.process_unchanged }).Count
        private_bytes_start = if ($Valid.Count) { $Valid[0].private_bytes } else { $null }
        private_bytes_end = if ($Valid.Count) { $Valid[-1].private_bytes } else { $null }
        gpu_csv_columns = 'index,memory_used_mib,memory_total_mib,gpu_utilization_percent,encoder_utilization_percent'
        gpu_scope = 'Whole GPU device; not attributed solely to RuneSim.'
        samples = $Samples.ToArray()
    }
    $Result.passed = $Completed -and $Result.process_interruptions -eq 0
    $Result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$OutputPath.tmp"
    Move-Item -LiteralPath "$OutputPath.tmp" -Destination $OutputPath -Force
    if (-not $Completed) { Start-Sleep -Seconds ([Math]::Min($IntervalSeconds, [Math]::Max(1, $Seconds - $Clock.Elapsed.TotalSeconds))) }
} while (-not $Completed)
if (-not $Result.passed) { exit 1 }
