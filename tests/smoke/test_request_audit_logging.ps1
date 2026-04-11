param(
    [string]$BaseUrl = "http://127.0.0.1:29000",
    [string]$BodyPath = "tests\fixtures\chat_request_audit_tool_auto.json",
    [string]$LogPath = "build\logs\masodik-runtime.log"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\support\request_audit_assertions.ps1"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$contentOnlyBodyPath = Join-Path $repoRoot "tests\fixtures\chat_request_audit_content_only.json"
$requiredBodyPath = Join-Path $repoRoot "tests\fixtures\chat_request_audit_tool_required.json"
$invalidNamedBodyPath = Join-Path $repoRoot "tests\fixtures\chat_request_audit_invalid_named_tool_choice.json"
$streamingBodyPath = Join-Path $repoRoot "tests\fixtures\chat_request_tool_call_stream.json"

function Invoke-RequestAuditHttpRequest {
    param(
        [Parameter(Mandatory)]
        [string]$Uri,
        [Parameter(Mandatory)]
        [string]$Body
    )

    $bodyBytes = [System.Text.Encoding]::UTF8.GetBytes($Body)
    $request = [System.Net.HttpWebRequest]::Create($Uri)
    $request.Method = "POST"
    $request.ContentType = "application/json"
    $request.Accept = "text/event-stream, application/json"
    $request.Timeout = 300000
    $request.ReadWriteTimeout = 300000
    $request.KeepAlive = $false

    $requestStream = $request.GetRequestStream()
    try {
        $requestStream.Write($bodyBytes, 0, $bodyBytes.Length)
    } finally {
        $requestStream.Dispose()
    }

    $response = $null
    try {
        try {
            $response = [System.Net.HttpWebResponse]$request.GetResponse()
        } catch [System.Net.WebException] {
            if ($_.Exception.Response) {
                $response = [System.Net.HttpWebResponse]$_.Exception.Response
            } else {
                throw
            }
        }

        $reader = New-Object System.IO.StreamReader($response.GetResponseStream())
        try {
            return [pscustomobject]@{
                StatusCode  = [int]$response.StatusCode
                Headers     = $response.Headers
                ContentType = [string]$response.ContentType
                Body        = $reader.ReadToEnd()
            }
        } finally {
            $reader.Dispose()
        }
    } finally {
        if ($response) {
            $response.Dispose()
        }
    }
}

function Invoke-RequestAuditCheck {
    param(
        [Parameter(Mandatory)]
        [string]$TargetBodyPath,
        [Parameter(Mandatory)]
        [int]$ExpectedStatusCode,
        [Parameter(Mandatory)]
        [string]$ExpectedDecisionPath,
        [Parameter(Mandatory)]
        [string]$ExpectedValidationOutcome,
        [Parameter(Mandatory)]
        [string]$ExpectedToolChoiceKind,
        [string]$ExpectedToolChoiceName,
        [Parameter(Mandatory)]
        [bool]$ExpectedToolsPresent,
        [Parameter(Mandatory)]
        [int]$ExpectedToolCount,
        [Parameter(Mandatory)]
        [bool]$ExpectedStream,
        [string]$ExpectedContentType = "application/json"
    )

    $lineCountBefore = Get-LogLineCount -LogPath $LogPath
    $body = Get-Content $TargetBodyPath -Raw
    $response = Invoke-RequestAuditHttpRequest -Uri "$BaseUrl/v1/chat/completions" -Body $body

    if ($response.StatusCode -ne $ExpectedStatusCode) {
        throw "A request-audit smoke nem a vart status koddal tert vissza. Vart=$ExpectedStatusCode, tenyleges=$($response.StatusCode)"
    }
    if ($response.Headers["X-Request-Id"] -eq $null -or [string]::IsNullOrWhiteSpace([string]$response.Headers["X-Request-Id"])) {
        throw "A request-audit smoke valaszbol hianyzik az X-Request-Id fejlec."
    }
    if ($response.ContentType -notmatch [regex]::Escape($ExpectedContentType)) {
        throw "A request-audit smoke Content-Type mezoje nem vart. Vart mintazat=$ExpectedContentType, tenyleges=$($response.ContentType)"
    }

    $requestId = [string]$response.Headers["X-Request-Id"]
    $records = Get-RequestAuditRecords -LogPath $LogPath -StartLine $lineCountBefore
    $record = Find-RequestAuditRecord -Records $records -RequestId $requestId

    Assert-RequestAuditFieldsPresent -Record $record -FieldNames @(
        "request_id",
        "stream",
        "tools_present",
        "tool_count",
        "tool_choice_kind",
        "decision_path",
        "validation_outcome",
        "created_at"
    )
    Assert-RequestAuditFieldValue -Record $record -FieldName "request_id" -ExpectedValue $requestId
    Assert-RequestAuditFieldValue -Record $record -FieldName "tool_choice_kind" -ExpectedValue $ExpectedToolChoiceKind
    Assert-RequestAuditFieldValue -Record $record -FieldName "decision_path" -ExpectedValue $ExpectedDecisionPath
    Assert-RequestAuditFieldValue -Record $record -FieldName "validation_outcome" -ExpectedValue $ExpectedValidationOutcome
    Assert-RequestAuditFieldValue -Record $record -FieldName "tools_present" -ExpectedValue $ExpectedToolsPresent
    Assert-RequestAuditFieldValue -Record $record -FieldName "tool_count" -ExpectedValue $ExpectedToolCount
    Assert-RequestAuditFieldValue -Record $record -FieldName "stream" -ExpectedValue $ExpectedStream

    if (-not [string]::IsNullOrWhiteSpace($ExpectedToolChoiceName)) {
        Assert-RequestAuditFieldValue -Record $record -FieldName "tool_choice_name" -ExpectedValue $ExpectedToolChoiceName
    }
}

Invoke-RequestAuditCheck -TargetBodyPath $contentOnlyBodyPath -ExpectedStatusCode 200 -ExpectedDecisionPath "content_only" -ExpectedValidationOutcome "accepted" -ExpectedToolChoiceKind "absent" -ExpectedToolsPresent $false -ExpectedToolCount 0 -ExpectedStream $false
Invoke-RequestAuditCheck -TargetBodyPath $BodyPath -ExpectedStatusCode 200 -ExpectedDecisionPath "content_only" -ExpectedValidationOutcome "accepted" -ExpectedToolChoiceKind "auto" -ExpectedToolsPresent $true -ExpectedToolCount 1 -ExpectedStream $false
Invoke-RequestAuditCheck -TargetBodyPath $requiredBodyPath -ExpectedStatusCode 200 -ExpectedDecisionPath "tool_call_required" -ExpectedValidationOutcome "accepted" -ExpectedToolChoiceKind "required" -ExpectedToolsPresent $true -ExpectedToolCount 1 -ExpectedStream $false
Invoke-RequestAuditCheck -TargetBodyPath $streamingBodyPath -ExpectedStatusCode 200 -ExpectedDecisionPath "tool_call_auto" -ExpectedValidationOutcome "accepted" -ExpectedToolChoiceKind "auto" -ExpectedToolsPresent $true -ExpectedToolCount 1 -ExpectedStream $true -ExpectedContentType "text/event-stream"
Invoke-RequestAuditCheck -TargetBodyPath $invalidNamedBodyPath -ExpectedStatusCode 400 -ExpectedDecisionPath "tool_call_named" -ExpectedValidationOutcome "tool_schema_error" -ExpectedToolChoiceKind "named_function" -ExpectedToolChoiceName "workspace_inspector" -ExpectedToolsPresent $true -ExpectedToolCount 1 -ExpectedStream $false

Write-Host "Request-audit smoke PASS: $BaseUrl/v1/chat/completions"
