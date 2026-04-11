param(
    [string]$BaseUrl = "http://127.0.0.1:29000",
    [string]$BodyPath = "tests\fixtures\chat_request_tool_call_stream.json"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\support\stream_client.ps1"
. "$PSScriptRoot\support\tool_call_assertions.ps1"

$result = Invoke-SseRequest -Uri "$BaseUrl/v1/chat/completions" -BodyPath $BodyPath -TimeoutSec 300
Assert-StreamingToolCallResponse -Result $result

if ($result.Headers["X-Request-Id"] -eq $null -or [string]::IsNullOrWhiteSpace([string]$result.Headers["X-Request-Id"])) {
    throw "A streaming tool-calling smoke valaszbol hianyzik az X-Request-Id fejlec."
}

$payloads = Get-SseJsonPayloads -Result $result
$assembledArguments = New-Object System.Text.StringBuilder
$hasToolName = $false
foreach ($payload in $payloads) {
    foreach ($choice in $payload.choices) {
        if ($null -eq $choice.delta -or $null -eq $choice.delta.tool_calls) {
            continue
        }

        foreach ($toolCallDelta in $choice.delta.tool_calls) {
            if ($null -ne $toolCallDelta.function -and $toolCallDelta.function.name -eq "exec") {
                $hasToolName = $true
            }
            if ($null -ne $toolCallDelta.function -and $null -ne $toolCallDelta.function.arguments) {
                [void]$assembledArguments.Append([string]$toolCallDelta.function.arguments)
            }
        }
    }
}

if (-not $hasToolName) {
    throw "A streaming tool-calling smoke nem adott vissza exec function nevet."
}
if ($assembledArguments.ToString() -notmatch "gh pr list --json number,title,state") {
    throw "A streaming tool-calling smoke nem tudta osszeallitani a vart parancsot."
}

Write-Host "Streaming tool-calling smoke PASS: $BaseUrl/v1/chat/completions"
