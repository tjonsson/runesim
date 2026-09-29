$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
$Runtime = Join-Path $Root 'Saved/LivingWorld/Signalling'
$Node = Join-Path $Runtime 'node-v22.14.0-win-x64/node.exe'
$Server = Join-Path $Runtime 'Infrastructure/SignallingWebServer'
$Entry = Join-Path $Server 'dist/index.js'
if (-not (Test-Path -LiteralPath $Entry)) { throw 'Run Scripts/setup_signalling.ps1 first.' }
$PidFile = Join-Path $Runtime 'server.pid'
$Listeners = @(Get-NetTCPConnection -LocalPort 80,8888 -State Listen -ErrorAction SilentlyContinue)
if ($Listeners.Count) {
    $OwnedId = if (Test-Path -LiteralPath $PidFile) { [int](Get-Content -LiteralPath $PidFile) } else { 0 }
    $Owned = Get-CimInstance Win32_Process -Filter "ProcessId=$OwnedId" -ErrorAction SilentlyContinue
    if ($Owned -and $Owned.ExecutablePath -eq $Node.Replace('/','\') -and $Owned.CommandLine.Contains($Entry) -and @($Listeners | Where-Object OwningProcess -ne $OwnedId).Count -eq 0) {
        Write-Output "RuneSim signalling is already running (PID $OwnedId)."
        return
    }
    throw 'Port 80 or 8888 is already in use by another process. It has been left untouched.'
}
$Arguments = '"{0}" --no_config --serve --streamer_port 8888 --player_port 80 --http_root "{1}" --homepage player.html --log_folder "{2}"' -f $Entry, (Join-Path $Server 'www'), (Join-Path $Runtime 'logs')
$Process = Start-Process -FilePath $Node -ArgumentList $Arguments -WorkingDirectory $Server -WindowStyle Hidden -PassThru -RedirectStandardOutput "$Runtime/server.stdout.log" -RedirectStandardError "$Runtime/server.stderr.log"
$Process.Id | Set-Content -LiteralPath $PidFile
$Ready = $false
for ($Attempt=0; $Attempt -lt 30; $Attempt++) {
    $Process.Refresh()
    if ($Process.HasExited) { throw "Signalling exited. See $Runtime/server.stderr.log" }
    try {
        $Response = Invoke-WebRequest 'http://127.0.0.1/player.html' -TimeoutSec 1
        if ($Response.StatusCode -eq 200) { $Ready=$true; break }
    } catch { Start-Sleep -Milliseconds 500 }
}
if (-not $Ready) { throw 'Signalling did not become ready; process and logs retained for diagnosis.' }
Write-Output "RuneSim signalling started (PID $($Process.Id)); LAN player port 80, streamer port 8888."
