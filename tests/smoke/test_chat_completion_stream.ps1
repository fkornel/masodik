param(
    [string]$BaseUrl = "http://127.0.0.1:29000",
    [string]$BodyPath = "tests\fixtures\chat_request.json"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\support\stream_client.ps1"

$payload = Get-Content $BodyPath -Raw | ConvertFrom-Json
$payload.stream = $true
$bodyJson = $payload | ConvertTo-Json -Depth 10

$result = Invoke-SseRequest -Uri "$BaseUrl/v1/chat/completions" -BodyJson $bodyJson -TimeoutSec 300
Assert-SseResponse -Result $result -RequireDone

$hasChunkPayload = $false
$hasAssistantDelta = $false
$hasToolCallDelta = $false
$terminalFinishReason = $null
foreach ($event in $result.Events) {
    if ($event -match '"object":"chat\.completion\.chunk"') {
        $hasChunkPayload = $true
    }
    if ($event -match '"role":"assistant"') {
        $hasAssistantDelta = $true
    }
    if ($event -match '"tool_calls":\[') {
        $hasToolCallDelta = $true
    }
    if ($event -match '"finish_reason":"([^"]+)"') {
        $terminalFinishReason = $Matches[1]
    }
}

if (-not $hasChunkPayload) {
    throw "A streaming smoke valasz nem tartalmazott chat.completion.chunk payloadot."
}
if (-not $hasAssistantDelta) {
    throw "A streaming smoke valaszbol hianyzik a kezdo assistant delta."
}
if ($hasToolCallDelta) {
    throw "A content-only streaming smoke nem tartalmazhat tool_calls deltakat."
}
if ($terminalFinishReason -eq "tool_calls") {
    throw "A content-only streaming smoke finish_reason nem lehet tool_calls."
}

Write-Host "Streaming chat smoke PASS: $BaseUrl/v1/chat/completions"
