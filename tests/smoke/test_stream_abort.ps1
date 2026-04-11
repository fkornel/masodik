param(
    [string]$BaseUrl = "http://127.0.0.1:29000",
    [string]$AbortBodyPath = "tests\fixtures\chat_request_stream_abort.json",
    [string]$RecoveryBodyPath = "tests\fixtures\chat_request.json"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\support\stream_client.ps1"

$abortedResult = Invoke-SseRequest `
    -Uri "$BaseUrl/v1/chat/completions" `
    -BodyPath $AbortBodyPath `
    -TimeoutSec 300 `
    -MaxEvents 2

if (-not $abortedResult.Events -or $abortedResult.Events.Count -lt 1) {
    throw "Az abort smoke stream nem adott legalabb egy SSE esemenyt a megszakitas elott."
}

$recoveryBody = Get-Content $RecoveryBodyPath -Raw
$recoveryResponse = Invoke-RestMethod `
    -Method Post `
    -Uri "$BaseUrl/v1/chat/completions" `
    -ContentType "application/json" `
    -Body $recoveryBody `
    -TimeoutSec 60

if (-not $recoveryResponse.choices -or $recoveryResponse.choices.Count -lt 1) {
    throw "A kliensabort utani recovery keres nem adott hasznalhato valaszt."
}

Write-Host "Stream abort smoke PASS: $BaseUrl/v1/chat/completions"
