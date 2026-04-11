param(
    [string]$BaseUrl = "http://127.0.0.1:29011",
    [string]$StreamBodyPath = "tests\fixtures\chat_request_stream_abort.json",
    [string]$JsonBodyPath = "tests\fixtures\chat_request.json"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\support\stream_client.ps1"

$resolvedStreamBodyPath = (Resolve-Path $StreamBodyPath).Path
$resolvedJsonBodyPath = (Resolve-Path $JsonBodyPath).Path

$streamScript = @"
`$ErrorActionPreference = 'Stop'
. '$PSScriptRoot\support\stream_client.ps1'
Invoke-SseRequest -Uri '$BaseUrl/v1/chat/completions' -BodyPath '$resolvedStreamBodyPath' -TimeoutSec 300 -MaxEvents 4 | Out-Null
"@

$streamJob = Start-Job -ScriptBlock ([scriptblock]::Create($streamScript))
try {
    Start-Sleep -Milliseconds 250

    $jsonBody = Get-Content $resolvedJsonBodyPath -Raw
    $jsonJob = Start-Job -ScriptBlock {
        param($Uri, $Body)
        Invoke-RestMethod -Method Post -Uri $Uri -ContentType "application/json" -Body $Body -TimeoutSec 120 | Out-Null
    } -ArgumentList "$BaseUrl/v1/chat/completions", $jsonBody

    $observedHealthFields = $false
    $observedMixedSignal = $false
    for ($attempt = 0; $attempt -lt 60; $attempt++) {
        $health = Invoke-RestMethod -Method Get -Uri "$BaseUrl/health" -TimeoutSec 15
        if ($null -eq $health.active_streams) {
            throw "A health valasz nem tartalmaz active_streams mezot."
        }
        if ($null -eq $health.peak_active_streams) {
            throw "A health valasz nem tartalmaz peak_active_streams mezot."
        }
        if ($null -eq $health.total_stream_failures) {
            throw "A health valasz nem tartalmaz total_stream_failures mezot."
        }
        if ($null -eq $health.last_disconnect_error) {
            throw "A health valasz nem tartalmaz last_disconnect_error mezot."
        }

        $observedHealthFields = $true
        $observedMixedSignal = $observedMixedSignal -or
            ($health.active_streams -ge 1 -or $health.queue_depth -ge 1 -or $health.state -eq "degraded")

        if ($observedMixedSignal) {
            break
        }

        Start-Sleep -Milliseconds 500
    }

    if (-not $observedHealthFields) {
        throw "A mixed-load health mezok nem voltak elerhetok."
    }

    Wait-Job $jsonJob | Out-Null
    Receive-Job $jsonJob | Out-Null
    Remove-Job $jsonJob -Force

    Wait-Job $streamJob | Out-Null
    Receive-Job $streamJob | Out-Null

    $finalHealth = Invoke-RestMethod -Method Get -Uri "$BaseUrl/health" -TimeoutSec 15
    if ($null -eq $finalHealth.active_streams -or
        $null -eq $finalHealth.peak_active_streams -or
        $null -eq $finalHealth.total_stream_failures -or
        $null -eq $finalHealth.last_disconnect_error) {
        throw "A mixed-load vegi health nem tartalmazta a vart stream diagnosztikai mezoket."
    }
    if ($finalHealth.queue_depth -ne 0) {
        throw "A mixed-load vegi health queue_depth mezonel nem nullazodott."
    }
    if ($finalHealth.state -ne "ready") {
        throw "A mixed-load vegi health nem allt vissza ready allapotba."
    }

    Write-Host "Stream queue visibility smoke PASS: $BaseUrl/health"
} finally {
    if ($streamJob) {
        Remove-Job $streamJob -Force -ErrorAction SilentlyContinue
    }
}
