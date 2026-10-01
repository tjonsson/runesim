param([ValidateSet('MainLevel','LivingWorldDemo')][string]$Scene = 'MainLevel')
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$DemoExe = Join-Path $ProjectRoot 'Saved/LivingWorld/Packaged/Windows/RuneSim.exe'
if (-not (Test-Path -LiteralPath $DemoExe)) { throw 'Package the demo first with Scripts/package_living_demo.ps1.' }
& (Join-Path $PSScriptRoot 'start_signalling.ps1')
if (Get-Process RuneSim -ErrorAction SilentlyContinue) { Write-Output 'RuneSim is already running.'; return }
# A supervisor that is still shutting down holds its single-instance mutex; a new one would exit silently.
for ($Wait = 0; $Wait -lt 30; $Wait++) {
    if (-not @(Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -and $_.CommandLine.Contains('watch_living_demo') -and $_.ProcessId -ne $PID }).Count) { break }
    Start-Sleep -Seconds 1
}
$ShellExe = (Get-Process -Id $PID).Path
$Supervisor = Join-Path $PSScriptRoot 'watch_living_demo.ps1'
Start-Process -FilePath $ShellExe -ArgumentList @('-NoProfile', '-File', ('"{0}"' -f $Supervisor), '-Scene', $Scene) -WindowStyle Hidden
Write-Output 'RuneSim started with crash recovery. Closing its window normally stops it.'
