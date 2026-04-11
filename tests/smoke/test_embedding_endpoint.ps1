param(
    [string]$BaseUrl = "http://127.0.0.1:29011",
    [string]$Model = "Qwen3-Embedding-0.6B-int4-cw-ov"
)

$ErrorActionPreference = "Stop"

Write-Host "=== Embedding Endpoint Smoke Test ==="
Write-Host "BaseUrl: $BaseUrl"
Write-Host "Model: $Model"

# 1. Egyedi embedding keres
Write-Host "`n--- Test 1: Egyedi embedding keres ---"
$singleBody = @{ model = $Model; input = "Hello vilag" } | ConvertTo-Json -Compress
$singleResponse = Invoke-RestMethod -Uri "$BaseUrl/v1/embeddings" -Method Post -ContentType "application/json" -Body $singleBody

if ($singleResponse.object -ne "list") { throw "Az object mezo nem 'list'." }
if ($singleResponse.data.Count -ne 1) { throw "Pontosan 1 embedding vart, kapott: $($singleResponse.data.Count)" }
if ($singleResponse.data[0].embedding.Count -lt 100) { throw "Az embedding vektor tul rovid: $($singleResponse.data[0].embedding.Count)" }
if ($singleResponse.data[0].index -ne 0) { throw "Az index nem 0." }
if ($singleResponse.usage.prompt_tokens -le 0) { throw "A prompt_tokens nullanal nagyobb kell legyen." }
Write-Host "PASS: Egyedi embedding OK (dimenzio=$($singleResponse.data[0].embedding.Count), tokens=$($singleResponse.usage.prompt_tokens))"

# 2. Batch embedding keres
Write-Host "`n--- Test 2: Batch embedding keres ---"
$batchBody = @{ model = $Model; input = @("Budapest", "Pecs", "Debrecen") } | ConvertTo-Json -Compress
$batchResponse = Invoke-RestMethod -Uri "$BaseUrl/v1/embeddings" -Method Post -ContentType "application/json" -Body $batchBody

if ($batchResponse.data.Count -ne 3) { throw "3 embedding vart, kapott: $($batchResponse.data.Count)" }
for ($i = 0; $i -lt 3; $i++) {
    if ($batchResponse.data[$i].index -ne $i) { throw "Az index $i nem vart: $($batchResponse.data[$i].index)" }
}
if ($batchResponse.usage.prompt_tokens -le 0) { throw "A batch prompt_tokens nullanal nagyobb kell legyen." }
Write-Host "PASS: Batch embedding OK (count=3, tokens=$($batchResponse.usage.prompt_tokens))"

# 3. Ismeretlen modell → 404
Write-Host "`n--- Test 3: Ismeretlen modell 404 ---"
try {
    $errorBody = @{ model = "nonexistent"; input = "hello" } | ConvertTo-Json -Compress
    Invoke-RestMethod -Uri "$BaseUrl/v1/embeddings" -Method Post -ContentType "application/json" -Body $errorBody
    throw "404 vart, de nem kaptunk hibat."
} catch {
    if ($_.Exception.Response.StatusCode.value__ -eq 404) {
        Write-Host "PASS: Ismeretlen modell 404-et ad."
    } else {
        throw "404 vart, kapott: $($_.Exception.Response.StatusCode.value__)"
    }
}

# 4. Health endpoint embedding mezok
Write-Host "`n--- Test 4: Health endpoint embedding mezok ---"
$health = Invoke-RestMethod -Uri "$BaseUrl/health"
if ($health.embedding_state -ne "ready") { throw "Az embedding_state nem 'ready': $($health.embedding_state)" }
if ($health.embedding_model -ne $Model) { throw "Az embedding_model nem vart: $($health.embedding_model)" }
if ([string]::IsNullOrEmpty($health.embedding_device)) { throw "Az embedding_device ures." }
Write-Host "PASS: Health embedding mezok OK (state=$($health.embedding_state), device=$($health.embedding_device))"

# 5. Models endpoint embedding modell
Write-Host "`n--- Test 5: Models endpoint embedding modell ---"
$models = Invoke-RestMethod -Uri "$BaseUrl/v1/models"
$embeddingModel = $models.data | Where-Object { $_.type -eq "embedding" }
if ($null -eq $embeddingModel) { throw "Az embedding modell nem jelenik meg a /v1/models valaszban." }
if ($embeddingModel.id -ne $Model) { throw "Az embedding modell id nem vart: $($embeddingModel.id)" }
Write-Host "PASS: Models endpoint embedding modell OK"

Write-Host "`n=== Minden embedding smoke teszt PASS ==="
