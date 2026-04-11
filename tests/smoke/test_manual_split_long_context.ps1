param(
    [string]$BaseUrl = "http://127.0.0.1:29011",
    [string]$BodyPath = "C:\dev\copilot\masodik\benchmarks\fixtures\manual-split-long-context-request.json",
    [switch]$AllowPlaceholder
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $BodyPath)) {
    throw "A manual split long-context fixture nem talalhato: $BodyPath"
}

$payload = Get-Content $BodyPath -Raw | ConvertFrom-Json
$payload.stream = $false
$body = $payload | ConvertTo-Json -Depth 10

$health = Invoke-RestMethod -Method Get -Uri "$BaseUrl/health" -TimeoutSec 60
if ($health.split_mode -ne $true) {
    throw "A manual split long-context smoke aktiv split profilt var."
}
if ($health.fallback_detected -eq $true) {
    throw "A manual split long-context smoke nem enged fallback allapotot."
}

$response = Invoke-RestMethod -Method Post -Uri "$BaseUrl/v1/chat/completions" -ContentType "application/json" -Body $body -TimeoutSec 600
if ($response.object -ne "chat.completion") {
    throw "A manual split long-context smoke nem chat.completion objektumot kapott."
}
if (-not $response.choices -or $response.choices.Count -lt 1) {
    throw "A manual split long-context smoke valaszban nincs choices tomb."
}
if (-not $response.choices[0].message -or [string]::IsNullOrWhiteSpace($response.choices[0].message.content)) {
    throw "A manual split long-context smoke valaszban nincs assistant tartalom."
}
if (-not $AllowPlaceholder.IsPresent -and $response.choices[0].message.content -like "*Placeholder valasz*") {
    throw "A manual split long-context smoke valodi ketstage-es kiszolgalast var, placeholder valasz nem elfogadhato."
}

$postHealth = Invoke-RestMethod -Method Get -Uri "$BaseUrl/health" -TimeoutSec 60
if ($postHealth.stage_1_state -ne "ready" -or $postHealth.stage_2_state -ne "ready") {
    throw "A manual split long-context smoke a valasz utan is ready stage allapotokat var."
}
if ($postHealth.handoff_state -ne "consumed") {
    throw "A manual split long-context smoke handoff_state=consumed allapotot var a sikeres keres utan."
}
if (-not [string]::IsNullOrEmpty($postHealth.last_failure_scope)) {
    throw "A manual split long-context smoke sikeres futasnal ures last_failure_scope mezot var."
}
if ($postHealth.fallback_detected -eq $true) {
    throw "A manual split long-context smoke nem varhat fallback_detected=true allapotot."
}
if (-not $postHealth.PSObject.Properties.Name.Contains("last_failure_summary")) {
    throw "A manual split long-context smoke last_failure_summary mezot var."
}
if (-not [string]::IsNullOrEmpty($postHealth.last_failure_summary)) {
    throw "A manual split long-context smoke sikeres keres utan ures last_failure_summary mezot var."
}
if ($postHealth.memory_summary.stage_1 -notmatch "required:" -or $postHealth.memory_summary.stage_1 -notmatch "budget:") {
    throw "A manual split long-context smoke stage_1 memory_summary mezoben required es budget jelzest var."
}
if ($postHealth.memory_summary.stage_2 -notmatch "required:" -or $postHealth.memory_summary.stage_2 -notmatch "budget:") {
    throw "A manual split long-context smoke stage_2 memory_summary mezoben required es budget jelzest var."
}
if ($postHealth.memory_summary.system -notmatch "prompt_tokens=" -or $postHealth.memory_summary.system -notmatch "total_tokens=") {
    throw "A manual split long-context smoke system memory_summary mezoben prompt_tokens es total_tokens jelzest var."
}

Write-Host "Manual split long-context smoke PASS."
