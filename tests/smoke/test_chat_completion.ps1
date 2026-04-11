param(
    [string]$BaseUrl = "http://127.0.0.1:29000",
    [string]$BodyPath = "tests\fixtures\chat_request.json"
)

$ErrorActionPreference = "Stop"

$payload = Get-Content $BodyPath -Raw | ConvertFrom-Json
$payload.stream = $false
$body = $payload | ConvertTo-Json -Depth 10
$uri = "$BaseUrl/v1/chat/completions"
$response = Invoke-RestMethod -Method Post -Uri $uri -ContentType "application/json" -Body $body -TimeoutSec 300
if ($response.object -ne "chat.completion") {
    throw "A non-stream smoke nem chat.completion objektumot kapott."
}
if (-not $response.choices -or $response.choices.Count -lt 1) {
    throw "A valaszban nincs choices tomb."
}
$choice = $response.choices[0]
if (-not $choice.message -or [string]::IsNullOrWhiteSpace($choice.message.content)) {
    throw "A non-stream smoke valaszban nincs assistant tartalom."
}
if ($null -ne $choice.message.tool_calls) {
    throw "A content-only non-stream smoke nem tartalmazhat tool_calls tombot."
}
if ($choice.finish_reason -eq "tool_calls") {
    throw "A content-only non-stream smoke finish_reason nem lehet tool_calls."
}
if (($response | ConvertTo-Json -Depth 10) -match "data: \[DONE\]") {
    throw "A non-stream smoke valasz nem tartalmazhat SSE lezaro markert."
}
Write-Host "Chat smoke PASS: $uri"
