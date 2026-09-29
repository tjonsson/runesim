param([string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.8', [string]$PackageRoot = '')
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$ProjectFile = Join-Path $ProjectRoot 'RuneSim.uproject'
if (-not $PackageRoot) { $PackageRoot = Join-Path $ProjectRoot 'Saved/LivingWorld/Packaged' }
$PackageRoot = [IO.Path]::GetFullPath($PackageRoot)
$PackagePrefix = $PackageRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if (Get-Process RuneSim -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($PackagePrefix, [StringComparison]::OrdinalIgnoreCase) }) {
    throw 'Close the demo and wait for it to exit, or choose a separate -PackageRoot for a candidate build.'
}
& (Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat') BuildCookRun "-project=$ProjectFile" -noP4 -platform=Win64 -clientconfig=Development '-map=/Game/MainLevel+/Game/LivingWorld/Maps/LivingWorldDemo' -build -cook -stage -pak -archive "-archivedirectory=$PackageRoot" -unattended
if ($LASTEXITCODE -ne 0) { throw "Packaging failed with exit code $LASTEXITCODE" }
Write-Output "MainLevel and demo packaged at $PackageRoot/Windows. Launch Cesium with Scripts/run_living_demo.ps1 -Scene MainLevel."
