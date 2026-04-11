param(
    [string]$BaseUrl = "http://127.0.0.1:29000",
    [string]$BodyPath = "tests\fixtures\chat_request_tool_call.json"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\support\tool_call_assertions.ps1"

$body = Get-Content $BodyPath -Raw
$bodyBytes = [System.Text.Encoding]::UTF8.GetBytes($body)
$request = [System.Net.HttpWebRequest]::Create("$BaseUrl/v1/chat/completions")
$request.Method = "POST"
$request.ContentType = "application/json"
$request.Accept = "application/json"
$request.Timeout = 300000
$request.ReadWriteTimeout = 300000
$request.KeepAlive = $false

$requestStream = $request.GetRequestStream()
try {
    $requestStream.Write($bodyBytes, 0, $bodyBytes.Length)
} finally {
    $requestStream.Dispose()
}

$httpResponse = $null
$requestId = $null
try {
    $httpResponse = [System.Net.HttpWebResponse]$request.GetResponse()
    $requestId = [string]$httpResponse.Headers["X-Request-Id"]
    $reader = New-Object System.IO.StreamReader($httpResponse.GetResponseStream())
    try {
        $response = $reader.ReadToEnd() | ConvertFrom-Json
    } finally {
        $reader.Dispose()
    }
} finally {
    if ($httpResponse) {
        $httpResponse.Dispose()
    }
}

if ([string]::IsNullOrWhiteSpace($requestId)) {
    throw "A non-stream tool-calling smoke valaszbol hianyzik az X-Request-Id fejlec."
}

Assert-NonStreamingToolCallResponse -Response $response

$choice = $response.choices[0]
if ($choice.finish_reason -ne "tool_calls") {
    throw "A non-stream tool-calling smoke finish_reason erteke nem tool_calls."
}
if ($choice.message.content -ne $null) {
    throw "A non-stream tool-calling smoke valaszban a content mezot nullkent varjuk."
}
if ($choice.message.tool_calls[0].function.name -ne "exec") {
    throw "A non-stream tool-calling smoke nem az elvart exec toolt adta vissza."
}
if ($choice.message.tool_calls[0].function.arguments -notmatch "gh pr list --json number,title,state") {
    throw "A non-stream tool-calling smoke nem a vart command payloadot adta vissza."
}

Write-Host "Tool-calling smoke PASS: $BaseUrl/v1/chat/completions"
