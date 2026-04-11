param(
    [string]$BaseUrl = "http://127.0.0.1:29011",
    [string]$BodyPath = "C:\dev\copilot\masodik\benchmarks\fixtures\manual-split-long-context-request.json"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $BodyPath)) {
    throw "A compiled manual split 128k smoke fixture nem talalhato: $BodyPath"
}

$payload = Get-Content $BodyPath -Raw | ConvertFrom-Json
$payload.stream = $false
$body = $payload | ConvertTo-Json -Depth 10

$health = Invoke-RestMethod -Method Get -Uri "$BaseUrl/health" -TimeoutSec 60
if ($health.split_mode -ne $true) {
    throw "A compiled manual split 128k smoke aktiv split profilt var."
}
if ($health.validated_context_tokens -lt 131072) {
    throw "A compiled manual split 128k smoke legalabb 131072 validated_context_tokens erteket var."
}
if ($health.fallback_detected -eq $true) {
    throw "A compiled manual split 128k smoke nem enged fallback allapotot."
}

$response = Invoke-RestMethod -Method Post -Uri "$BaseUrl/v1/chat/completions" -ContentType "application/json" -Body $body -TimeoutSec 600
if ($response.object -ne "chat.completion") {
    throw "A compiled manual split 128k smoke nem chat.completion objektumot kapott."
}
if (-not $response.choices -or $response.choices.Count -lt 1) {
    throw "A compiled manual split 128k smoke valaszban nincs choices tomb."
}
if (-not $response.choices[0].message -or [string]::IsNullOrWhiteSpace($response.choices[0].message.content)) {
    throw "A compiled manual split 128k smoke valaszban nincs assistant tartalom."
}
if ($response.choices[0].message.content -like "*Placeholder valasz*") {
    throw "A compiled manual split 128k smoke compiled ketstage-es valaszt var, placeholder nem elfogadhato."
}
if ($response.choices[0].message.content -like "*Compiled split infer sikeres*") {
    throw "A compiled manual split 128k smoke mar valodi detokenizalt modellvalaszt var, a technikai handoff-osszefoglalo nem eleg."
}

$postHealth = Invoke-RestMethod -Method Get -Uri "$BaseUrl/health" -TimeoutSec 60
if ($postHealth.request_path_mode -ne "compiled") {
    throw "A compiled manual split 128k smoke request_path_mode=compiled allapotot var."
}
if ($postHealth.handoff_state -ne "consumed") {
    throw "A compiled manual split 128k smoke handoff_state=consumed allapotot var."
}
if ($postHealth.validated_context_tokens -lt 131072) {
    throw "A compiled manual split 128k smoke a valasz utan is 131072 validated_context_tokens erteket var."
}
if ($postHealth.memory_summary.stage_1 -notmatch "required:" -or $postHealth.memory_summary.stage_1 -notmatch "budget:") {
    throw "A compiled manual split 128k smoke stage_1 memory_summary mezoben required es budget jelzest var."
}
if ($postHealth.memory_summary.stage_2 -notmatch "required:" -or $postHealth.memory_summary.stage_2 -notmatch "budget:") {
    throw "A compiled manual split 128k smoke stage_2 memory_summary mezoben required es budget jelzest var."
}
if ($postHealth.memory_summary.system -notmatch "mode=compiled") {
    throw "A compiled manual split 128k smoke system memory_summary mezoben mode=compiled jelzest var."
}

Write-Host "Compiled manual split 128k smoke PASS."
