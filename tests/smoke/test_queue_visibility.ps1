param(
    [string]$BaseUrl = "http://127.0.0.1:29000"
)

$ErrorActionPreference = "Stop"
$response = Invoke-RestMethod -Method Get -Uri "$BaseUrl/health" -TimeoutSec 120

if ($null -eq $response.queue_depth) {
    throw "A health valasz nem tartalmaz queue_depth mezot."
}

if ($null -eq $response.gpu_offload_active) {
    throw "A health valasz nem tartalmaz gpu_offload_active mezot."
}

Write-Host "Queue visibility smoke PASS: queue_depth=$($response.queue_depth)"
