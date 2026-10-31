param([switch]$OpenBrowser)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$guiRoot = Join-Path $projectRoot '.local/fairyground'
$serverFile = Join-Path $guiRoot 'server.js'
$controlFile = Join-Path $guiRoot '.petrified-control.json'
$guiUrl = 'http://127.0.0.1:5185/public/index.html?variant=petrified'
if (!(Test-Path -LiteralPath $serverFile)) { throw 'Fairyground is not configured in .local/fairyground.' }
$listeners = @(Get-NetTCPConnection -State Listen -LocalPort 5185,5186 -ErrorAction SilentlyContinue)
if ($listeners.Count) {
    if (!(Test-Path -LiteralPath $controlFile)) { throw 'Fairyground ports are already occupied by another process.' }
    $control = Get-Content -LiteralPath $controlFile -Raw | ConvertFrom-Json
    if (@($listeners | Where-Object OwningProcess -NE $control.pid).Count) { throw 'Fairyground ports are already occupied by another process.' }
    Invoke-WebRequest -Uri $guiUrl -UseBasicParsing -TimeoutSec 10 | Out-Null
} else {
    $nodeExe = (Get-Command node -CommandType Application | Select-Object -First 1).Source
    $started = Start-Process -FilePath $nodeExe -ArgumentList @('"' + $serverFile + '"') -WorkingDirectory $guiRoot -WindowStyle Hidden -RedirectStandardOutput (Join-Path $guiRoot 'petrified-server.stdout.log') -RedirectStandardError (Join-Path $guiRoot 'petrified-server.stderr.log') -PassThru
    $ready = $false
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        Start-Sleep -Milliseconds 500
        if ($started.HasExited) { throw 'Fairyground stopped during startup; see its stderr log.' }
        try { Invoke-WebRequest -Uri $guiUrl -UseBasicParsing -TimeoutSec 2 | Out-Null; $ready = $true; break } catch {}
    }
    if (!$ready) { throw 'Fairyground startup timed out; inspect the owned process and logs before retrying.' }
}
Write-Output $guiUrl
if ($OpenBrowser) { Start-Process $guiUrl }
