param(
    [string]$BaseUrl = "http://127.0.0.1:29012"
)

$ErrorActionPreference = "Stop"

Write-Host "=== 128k Deep Split Smoke Test ==="
Write-Host "BaseUrl: $BaseUrl"

# 1. Health check - 128k profil
Write-Host "`n--- Test 1: Health endpoint 128k profil ---"
$health = Invoke-RestMethod -Uri "$BaseUrl/health"
if ($health.active_profile -ne "b580-deep-split-128k") { throw "A profil nem b580-deep-split-128k: $($health.active_profile)" }
if ($health.stage_1_state -ne "ready") { throw "Stage-1 nem ready: $($health.stage_1_state)" }
if ($health.stage_2_state -ne "ready") { throw "Stage-2 nem ready: $($health.stage_2_state)" }
if ($health.validated_context_tokens -ne 131072) { throw "A validated_context_tokens nem 131072: $($health.validated_context_tokens)" }
Write-Host "PASS: 128k profil ready (context=$($health.validated_context_tokens), stage1=$($health.stage_1_device), stage2=$($health.stage_2_device))"

# 2. Rovid keres
Write-Host "`n--- Test 2: Rovid keres a 128k profilon ---"
$body = @{ model = "gpt-oss-20b-int4-ov"; messages = @(@{ role = "user"; content = "Szia" }); max_tokens = 128 } | ConvertTo-Json -Compress
$response = Invoke-RestMethod -Uri "$BaseUrl/v1/chat/completions" -Method Post -ContentType "application/json" -Body $body -TimeoutSec 120
if ($null -eq $response.choices -or $response.choices.Count -eq 0) { throw "Nincs valasz." }
Write-Host "PASS: Rovid keres sikeres ($($response.choices[0].message.content.Length) karakter)"

# 3. Embedding NPU
Write-Host "`n--- Test 3: Embedding NPU a 128k profilon ---"
$embedBody = @{ model = "Qwen3-Embedding-0.6B-int4-cw-ov"; input = "test" } | ConvertTo-Json -Compress
$embedResponse = Invoke-RestMethod -Uri "$BaseUrl/v1/embeddings" -Method Post -ContentType "application/json" -Body $embedBody -TimeoutSec 30
if ($embedResponse.data.Count -ne 1) { throw "Embedding valasz hibas." }
Write-Host "PASS: Embedding NPU mukodik (dim=$($embedResponse.data[0].embedding.Count))"

Write-Host "`n=== Minden 128k deep split smoke teszt PASS ==="
