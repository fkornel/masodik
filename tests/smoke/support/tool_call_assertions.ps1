function Assert-ToolCallObjectShape {
    param(
        [Parameter(Mandatory)]
        $ToolCall
    )

    if ($null -eq $ToolCall) {
        throw "A tool call objektum nem lehet null."
    }
    if ([string]::IsNullOrWhiteSpace($ToolCall.type)) {
        throw "A tool call objektumbol hianyzik a type mezo."
    }
    if ($ToolCall.type -ne "function") {
        throw "A tool call type mezonel csak a function tamogatott. Ertek: $($ToolCall.type)"
    }
    if ($null -eq $ToolCall.function) {
        throw "A tool call objektumbol hianyzik a function alobjektum."
    }
    if ([string]::IsNullOrWhiteSpace($ToolCall.function.name)) {
        throw "A tool call function.name mezoje hianyzik."
    }
    if ($null -eq $ToolCall.function.arguments) {
        throw "A tool call function.arguments mezoje hianyzik."
    }
}

function Assert-NonStreamingToolCallResponse {
    param(
        [Parameter(Mandatory)]
        $Response
    )

    if ($Response.object -ne "chat.completion") {
        throw "A tool-calling non-stream valasz nem chat.completion objektum."
    }
    if (-not $Response.choices -or $Response.choices.Count -lt 1) {
        throw "A tool-calling non-stream valaszban nincs choices tomb."
    }

    $choice = $Response.choices[0]
    if ([string]::IsNullOrWhiteSpace($choice.finish_reason)) {
        throw "A tool-calling non-stream valaszbol hianyzik a finish_reason."
    }
    if ($null -eq $choice.message) {
        throw "A tool-calling non-stream valaszbol hianyzik a message objektum."
    }
    if ($null -eq $choice.message.tool_calls -or $choice.message.tool_calls.Count -lt 1) {
        throw "A tool-calling non-stream valasz nem tartalmaz tool_calls tombot."
    }

    foreach ($toolCall in $choice.message.tool_calls) {
        Assert-ToolCallObjectShape -ToolCall $toolCall
    }
}

function Get-SseJsonPayloads {
    param(
        [Parameter(Mandatory)]
        $Result
    )

    $payloads = New-Object System.Collections.Generic.List[object]
    foreach ($event in $Result.Events) {
        if ($event -match '^data: \[DONE\]$') {
            continue
        }

        $jsonLines = @()
        foreach ($line in ($event -split "`n")) {
            if ($line.StartsWith("data: ")) {
                $jsonLines += $line.Substring(6)
            }
        }

        if ($jsonLines.Count -gt 0) {
            $jsonText = [string]::Join("`n", $jsonLines)
            $payloads.Add(($jsonText | ConvertFrom-Json))
        }
    }

    return @($payloads.ToArray())
}

function Assert-StreamingToolCallResponse {
    param(
        [Parameter(Mandatory)]
        $Result
    )

    Assert-SseResponse -Result $Result -RequireDone
    $payloads = Get-SseJsonPayloads -Result $Result
    if (-not $payloads -or $payloads.Count -lt 1) {
        throw "A tool-calling stream nem tartalmazott JSON payloadot."
    }

    $hasToolCallDelta = $false
    $assembledArguments = New-Object System.Text.StringBuilder
    $finishReason = $null
    foreach ($payload in $payloads) {
        if ($null -eq $payload.choices) {
            continue
        }

        foreach ($choice in $payload.choices) {
            if ($null -ne $choice.finish_reason) {
                $finishReason = [string]$choice.finish_reason
            }
            if ($null -ne $choice.delta -and $null -ne $choice.delta.tool_calls) {
                $hasToolCallDelta = $true
                foreach ($toolCallDelta in $choice.delta.tool_calls) {
                    if ($null -ne $toolCallDelta.function -and $null -ne $toolCallDelta.function.arguments) {
                        [void]$assembledArguments.Append([string]$toolCallDelta.function.arguments)
                    }
                }
            }
        }
    }

    if (-not $hasToolCallDelta) {
        throw "A tool-calling stream nem tartalmazott tool_calls delta payloadot."
    }
    if ([string]::IsNullOrWhiteSpace($assembledArguments.ToString())) {
        throw "A tool-calling stream nem tartalmazott osszeallithato arguments deltakat."
    }
    if ($finishReason -ne "tool_calls") {
        throw "A tool-calling stream finish_reason erteke nem tool_calls. Ertek: $finishReason"
    }
}
