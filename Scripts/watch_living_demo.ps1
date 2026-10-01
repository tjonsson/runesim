param([ValidateSet('MainLevel','LivingWorldDemo')][string]$Scene = 'MainLevel')
$ErrorActionPreference = 'Stop'
$ScenePath = if ($Scene -eq 'MainLevel') { '/Game/MainLevel' } else { '/Game/LivingWorld/Maps/LivingWorldDemo' }
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$SettingsArgument = ''
if ($Scene -eq 'MainLevel') {
    $UserSettingsPath = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'AirSim/settings.json'
    $UserSettings = if (Test-Path -LiteralPath $UserSettingsPath) { Get-Content -LiteralPath $UserSettingsPath -Raw | ConvertFrom-Json } else { $null }
    # AirSim's newly created, empty settings file prompts and then creates no vehicle.
    # Use an explicit simulation-only example only in that unconfigured case.
    if (-not $UserSettings.SimMode -and -not $UserSettings.Vehicles) {
        $ExampleSettings = Join-Path $ProjectRoot 'Samples/LivingWorld/MainLevel.settings.json'
        $SettingsArgument = ' -settings="' + $ExampleSettings + '"'
    }
}
$DemoExe = Join-Path $ProjectRoot 'Saved/LivingWorld/Packaged/Windows/RuneSim/Binaries/Win64/RuneSim.exe'
$LogPath = Join-Path $ProjectRoot 'Saved/LivingWorld/simulator-supervisor.log'
$Mutex = [System.Threading.Mutex]::new($false, 'Local\RuneSimLivingWorldDemoSupervisor')
$OwnsMutex = $false
try {
    try { $OwnsMutex = $Mutex.WaitOne(0) } catch [System.Threading.AbandonedMutexException] { $OwnsMutex = $true }
    if (-not $OwnsMutex) { exit 0 }
    $Crashes = [System.Collections.Generic.List[datetime]]::new()
    while ($true) {
        if (-not (Test-Path -LiteralPath $DemoExe)) { throw "Missing simulator: $DemoExe" }
        # Do not attach to, replace, or launch a second copy beside an existing app.
        if (Get-Process RuneSim -ErrorAction SilentlyContinue) { throw 'A simulator is already running.' }
        & (Join-Path $PSScriptRoot 'start_signalling.ps1')
        Add-Content -LiteralPath $LogPath -Value "$(Get-Date -Format o) Starting simulator (NVENC CUDA interop)"
        # Monitor the actual game binary so a bootstrap launcher cannot mask its exit code.
        # Interactive simulator is deliberately visible; this supervising shell is hidden.
        $SimProcess = Start-Process -FilePath $DemoExe -ArgumentList ($ScenePath + $SettingsArgument + ' -windowed -ResX=1280 -ResY=720 -nosplash -AVCodecs.NvEnc.D3D12UsesCUDA=true -ExecCmds="t.MaxFPS 30, PixelStreaming2.UseMediaCapture 0, PixelStreaming2.CaptureUseFence 0" -LogCmds="LogRenderer Warning"') -WindowStyle Normal -PassThru
        # Start-Process -Wait also waits for descendant processes; a helper may outlive a crash.
        $SimProcess.WaitForExit()
        $Result = $SimProcess.ExitCode
        Add-Content -LiteralPath $LogPath -Value "$(Get-Date -Format o) Simulator exited: $Result"
        if ($Result -eq 0) { break }
        $Now = Get-Date
        $Crashes.RemoveAll([Predicate[datetime]]{ param($When) ($Now - $When).TotalMinutes -gt 10 }) | Out-Null
        $Crashes.Add($Now)
        if ($Crashes.Count -ge 5) { throw 'Five unexpected exits in ten minutes; recovery stopped to avoid a crash loop.' }
        $Delay = [Math]::Min(40, 5 * [Math]::Pow(2, $Crashes.Count - 1))
        Add-Content -LiteralPath $LogPath -Value "$(Get-Date -Format o) Retrying in $Delay seconds"
        Start-Sleep -Seconds $Delay
    }
} catch {
    Add-Content -LiteralPath $LogPath -Value "$(Get-Date -Format o) ERROR: $($_.Exception.Message)"
    exit 1
} finally {
    if ($OwnsMutex) { $Mutex.ReleaseMutex() }
    $Mutex.Dispose()
}
