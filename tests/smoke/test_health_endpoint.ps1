param(
    [string]$BaseUrl = "http://127.0.0.1:29000"
)

$ErrorActionPreference = "Stop"

$response = Invoke-RestMethod -Method Get -Uri "$BaseUrl/health" -TimeoutSec 120
if ($null -eq $response.state -or [string]::IsNullOrWhiteSpace([string]$response.state)) {
    throw "A health valaszban nincs ervenyes state mezo."
}

if ($response.ready -ne $true) {
    throw "A health vegpont nem jelezte ready=true allapotot."
}

Write-Host "Health smoke PASS: $BaseUrl/health"
