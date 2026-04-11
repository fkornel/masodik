param(
    [string]$BaseUrl = "http://127.0.0.1:29000",
    [string]$ExpectedModel = "gpt-oss-20b-int4-ov"
)

$ErrorActionPreference = "Stop"

$uri = "$BaseUrl/v1/models"
$response = Invoke-RestMethod -Method Get -Uri $uri -TimeoutSec 120
if (-not $response.data) {
    throw "A valaszban nincs data tomb."
}
$match = $response.data | Where-Object { $_.id -eq $ExpectedModel }
if (-not $match) {
    throw "A vart modellazonosito nem talalhato."
}
Write-Host "Models smoke PASS: $uri"
