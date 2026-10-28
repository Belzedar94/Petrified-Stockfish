$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$controlFile = Join-Path $projectRoot '.local/fairyground/.petrified-control.json'
if (!(Test-Path -LiteralPath $controlFile)) { Write-Output 'Fairyground is not running.'; exit 0 }
$control = Get-Content -LiteralPath $controlFile -Raw | ConvertFrom-Json
$listener = Get-NetTCPConnection -State Listen -LocalPort 5185 -ErrorAction SilentlyContinue
if (!$listener) { Write-Output 'Fairyground is not running.'; exit 0 }
if ($listener.OwningProcess -ne $control.pid) { throw 'Port 5185 belongs to a different process; refusing to stop it.' }
Invoke-RestMethod -Method Post -Uri 'http://127.0.0.1:5185/petrified-stop' -Headers @{ 'x-petrified-stop' = $control.token } -TimeoutSec 10 | Out-Null
Write-Output 'Fairyground is shutting down with its own engines.'
