function Get-StreamingRequestBody {
    param(
        [string]$BodyPath,
        [string]$BodyJson
    )

    if (-not [string]::IsNullOrWhiteSpace($BodyJson)) {
        return $BodyJson
    }

    if ([string]::IsNullOrWhiteSpace($BodyPath)) {
        throw "A streaming helperhez BodyPath vagy BodyJson szukseges."
    }

    return Get-Content $BodyPath -Raw
}

function Invoke-SseRequest {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Uri,
        [string]$BodyPath,
        [string]$BodyJson,
        [int]$TimeoutSec = 300,
        [int]$MaxEvents = 0
    )

    $requestBody = Get-StreamingRequestBody -BodyPath $BodyPath -BodyJson $BodyJson
    $bodyBytes = [System.Text.Encoding]::UTF8.GetBytes($requestBody)

    $request = [System.Net.HttpWebRequest]::Create($Uri)
    $request.Method = "POST"
    $request.ContentType = "application/json"
    $request.Accept = "text/event-stream, application/json"
    $request.Timeout = $TimeoutSec * 1000
    $request.ReadWriteTimeout = $TimeoutSec * 1000
    $request.KeepAlive = $false
    $request.AllowReadStreamBuffering = $false

    $requestStream = $request.GetRequestStream()
    try {
        $requestStream.Write($bodyBytes, 0, $bodyBytes.Length)
    } finally {
        $requestStream.Dispose()
    }

    $response = $null
    $reader = $null
    $rawLines = New-Object System.Collections.Generic.List[string]
    $events = New-Object System.Collections.Generic.List[string]
    $currentEvent = New-Object System.Collections.Generic.List[string]

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
        while (-not $reader.EndOfStream) {
            $line = $reader.ReadLine()
            $rawLines.Add($line)

            if ([string]::IsNullOrEmpty($line)) {
                if ($currentEvent.Count -gt 0) {
                    $events.Add([string]::Join("`n", $currentEvent))
                    $currentEvent.Clear()
                    if ($MaxEvents -gt 0 -and $events.Count -ge $MaxEvents) {
                        break
                    }
                }
                continue
            }

            $currentEvent.Add($line)
        }

        if ($currentEvent.Count -gt 0) {
            $events.Add([string]::Join("`n", $currentEvent))
        }

        return [pscustomobject]@{
            StatusCode        = [int]$response.StatusCode
            StatusDescription = $response.StatusDescription
            ContentType       = $response.ContentType
            Headers           = $response.Headers
            Events            = @($events.ToArray())
            RawLines          = @($rawLines.ToArray())
            Body              = [string]::Join("`n", $rawLines.ToArray())
        }
    } finally {
        if ($reader) {
            $reader.Dispose()
        }
        if ($response) {
            $response.Dispose()
        }
    }
}

function Assert-SseResponse {
    param(
        [Parameter(Mandatory)]
        $Result,
        [switch]$RequireDone
    )

    if ($Result.StatusCode -ne 200) {
        throw "A streaming valasz nem 200-as. StatusCode=$($Result.StatusCode)"
    }

    if ($Result.ContentType -notmatch "text/event-stream") {
        throw "A streaming valasz Content-Type mezoje nem text/event-stream. Ertek: $($Result.ContentType)"
    }

    if (-not $Result.Events -or $Result.Events.Count -lt 1) {
        throw "A streaming valasz nem tartalmaz SSE esemenyt."
    }

    if ($RequireDone) {
        $hasDone = $false
        foreach ($event in $Result.Events) {
            if ($event -match "^data: \[DONE\]$") {
                $hasDone = $true
                break
            }
        }

        if (-not $hasDone) {
            throw "A streaming valasz nem tartalmazta a [DONE] lezaro esemenyt."
        }
    }
}

function Get-SseEventPayloadText {
    param(
        [Parameter(Mandatory)]
        [string]$Event
    )

    $payloadLines = @()
    foreach ($line in ($Event -split "`n")) {
        if ($line.StartsWith("data: ")) {
            $payloadLines += $line.Substring(6)
        }
    }

    return [string]::Join("`n", $payloadLines)
}

function Get-SseEventPayloads {
    param(
        [Parameter(Mandatory)]
        $Result,
        [switch]$SkipDone
    )

    $payloads = New-Object System.Collections.Generic.List[string]
    foreach ($event in $Result.Events) {
        $payload = Get-SseEventPayloadText -Event $event
        if ($SkipDone -and $payload -eq "[DONE]") {
            continue
        }
        $payloads.Add($payload)
    }

    return @($payloads.ToArray())
}
