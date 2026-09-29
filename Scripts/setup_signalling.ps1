param([string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
$Runtime = Join-Path $Root 'Saved/LivingWorld/Signalling'
$Source = Join-Path $EngineRoot 'Engine/Plugins/Media/PixelStreaming2/Resources/WebServers'
if (-not (Test-Path -LiteralPath "$Source/package-lock.json")) { throw "Missing Epic infrastructure: $Source" }
New-Item -ItemType Directory -Force -Path $Runtime | Out-Null
$Version = 'v22.14.0' # Matches the infrastructure bundled with UE 5.8.
$Archive = "node-$Version-win-x64.zip"
$NodeRoot = Join-Path $Runtime "node-$Version-win-x64"
if (-not (Test-Path -LiteralPath "$NodeRoot/npm.cmd")) {
    Invoke-WebRequest "https://nodejs.org/dist/$Version/$Archive" -OutFile "$Runtime/$Archive"
    $Sums = (Invoke-WebRequest "https://nodejs.org/dist/$Version/SHASUMS256.txt").Content
    $Expected = ($Sums -split "`n" | Where-Object { $_ -match "  $([regex]::Escape($Archive))\s*$" }) -split '\s+' | Select-Object -First 1
    if (-not $Expected -or (Get-FileHash "$Runtime/$Archive" -Algorithm SHA256).Hash -ne $Expected) { throw 'Node archive checksum mismatch' }
    Expand-Archive -LiteralPath "$Runtime/$Archive" -DestinationPath $Runtime -Force
}
$Infrastructure = Join-Path $Runtime 'Infrastructure'
if (-not (Test-Path -LiteralPath "$Infrastructure/package-lock.json")) {
    New-Item -ItemType Directory -Force -Path $Infrastructure | Out-Null
    Get-ChildItem -LiteralPath $Source | Copy-Item -Destination $Infrastructure -Recurse
}
$PreviousPath = $env:PATH
try {
    $env:PATH = "$NodeRoot;$PreviousPath"
    Push-Location $Infrastructure
    try {
        & "$NodeRoot/npm.cmd" ci --workspace=Common --workspace=Signalling --workspace=SignallingWebServer --include-workspace-root --ignore-scripts --no-audit --no-fund
        if ($LASTEXITCODE -ne 0) { throw 'Signalling dependency installation failed' }
        foreach ($Workspace in @('Common', 'Signalling', 'SignallingWebServer')) {
            $Build = if ($Workspace -eq 'SignallingWebServer') { 'build' } else { 'build:cjs' }
            & "$NodeRoot/npm.cmd" run $Build "--workspace=$Workspace"
            if ($LASTEXITCODE -ne 0) { throw "Signalling build failed: $Workspace" }
        }
    } finally { Pop-Location }
} finally { $env:PATH = $PreviousPath }
Write-Output 'Signalling is ready. Run Scripts/start_signalling.ps1 before launching the demo.'
