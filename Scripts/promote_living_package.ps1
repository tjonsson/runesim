param(
    [string]$Candidate = 'PackagedCombat',
    [string]$Backup = 'PackagedBeforeCombat',
    [int]$SensorStreams = 2
)
# Promote a candidate package: keep the current archive as a rollback copy, carry saved
# preferences (graphics + Living World) forward and enable the new Living World options.
$ErrorActionPreference = 'Stop'
$Root = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/LivingWorld'
if (Get-Process RuneSim -ErrorAction SilentlyContinue) { throw 'Close RuneSim and wait for it to exit first.' }
if (-not (Test-Path "$Root/$Candidate/Windows/RuneSim.exe")) { throw "Candidate $Candidate is not a complete package." }
if (Test-Path "$Root/$Backup") { throw "Backup $Backup already exists; choose another name." }
Move-Item "$Root/Packaged" "$Root/$Backup"
Move-Item "$Root/$Candidate" "$Root/Packaged"
$OldConfig = "$Root/$Backup/Windows/RuneSim/Saved/Config"
$NewConfig = "$Root/Packaged/Windows/RuneSim/Saved/Config"
if (Test-Path $OldConfig) { New-Item -ItemType Directory -Force (Split-Path $NewConfig) | Out-Null; Copy-Item $OldConfig $NewConfig -Recurse -Force }
$Settings = "$NewConfig/Windows/GameUserSettings.ini"
if (Test-Path $Settings) {
    $Text = Get-Content $Settings -Raw
    $Text = [regex]::Replace($Text, '(Options=\([^\r\n]*?),?(bCombatTargets=\w+,?|SensorStreams=\d+,?|bRuntimePerches=\w+,?)*\)', {
        param($M) $M.Groups[1].Value.TrimEnd(',') + ",bCombatTargets=True,SensorStreams=$SensorStreams,bRuntimePerches=True)" })
    Set-Content -Path $Settings -Value $Text -Encoding UTF8 -NoNewline
}
$Hash = (Get-FileHash "$Root/Packaged/Windows/RuneSim/Binaries/Win64/RuneSim.exe" -Algorithm SHA256).Hash
"Promoted $Candidate (RuneSim.exe SHA-256 $Hash); previous archive kept as $Backup."
if (Test-Path $Settings) { Select-String -Path $Settings -Pattern '^Options=' | ForEach-Object { $_.Line } }
