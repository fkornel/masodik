function Get-LogLineCount {
    param(
        [Parameter(Mandatory)]
        [string]$LogPath
    )

    if (-not (Test-Path $LogPath)) {
        return 0
    }

    return (Get-Content $LogPath).Count
}

function Get-RequestAuditLines {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$LogPath,
        [int]$StartLine = 0,
        [string]$Marker = "request_audit"
    )

    if (-not (Test-Path $LogPath)) {
        throw "A request-audit helper nem talalja a log fajlt: $LogPath"
    }

    $lines = Get-Content $LogPath
    if ($StartLine -gt 0 -and $StartLine -lt $lines.Count) {
        $lines = $lines[$StartLine..($lines.Count - 1)]
    } elseif ($StartLine -ge $lines.Count) {
        $lines = @()
    }

    return @($lines | Where-Object { $_ -match [regex]::Escape($Marker) })
}

function Convert-RequestAuditLineToRecord {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Line,
        [string]$Marker = "request_audit"
    )

    $firstBrace = $Line.IndexOf("{")
    $lastBrace = $Line.LastIndexOf("}")
    if ($firstBrace -ge 0 -and $lastBrace -gt $firstBrace) {
        $jsonText = $Line.Substring($firstBrace, $lastBrace - $firstBrace + 1)
        return ($jsonText | ConvertFrom-Json)
    }

    $markerIndex = $Line.IndexOf($Marker)
    if ($markerIndex -lt 0) {
        throw "A request-audit log sor nem tartalmazza a vart markert: $Marker"
    }

    $payload = $Line.Substring($markerIndex + $Marker.Length).Trim(" ", ":", "=")
    $record = [ordered]@{}
    foreach ($match in [regex]::Matches($payload, '([A-Za-z0-9_]+)=(".*?"|\S+)')) {
        $key = $match.Groups[1].Value
        $rawValue = $match.Groups[2].Value
        if ($rawValue.StartsWith('"') -and $rawValue.EndsWith('"')) {
            $rawValue = $rawValue.Substring(1, $rawValue.Length - 2)
        }
        $record[$key] = $rawValue
    }

    if ($record.Count -eq 0) {
        throw "A request-audit log sorbol nem sikerult mezoertekeket kinyerni: $Line"
    }

    return [pscustomobject]$record
}

function Get-RequestAuditRecords {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$LogPath,
        [int]$StartLine = 0,
        [string]$Marker = "request_audit"
    )

    $records = New-Object System.Collections.Generic.List[object]
    foreach ($line in (Get-RequestAuditLines -LogPath $LogPath -StartLine $StartLine -Marker $Marker)) {
        $records.Add((Convert-RequestAuditLineToRecord -Line $line -Marker $Marker))
    }

    return @($records.ToArray())
}

function Find-RequestAuditRecord {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [object[]]$Records,
        [string]$RequestId
    )

    if ([string]::IsNullOrWhiteSpace($RequestId)) {
        if ($Records.Count -lt 1) {
            throw "Nincs request-audit rekord a vizsgalt logreszben."
        }
        return $Records[-1]
    }

    foreach ($record in $Records) {
        if ($record.request_id -eq $RequestId) {
            return $record
        }
    }

    throw "A request-audit logban nem talalhato a kert request_id: $RequestId"
}

function Assert-RequestAuditFieldsPresent {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        $Record,
        [Parameter(Mandatory)]
        [string[]]$FieldNames
    )

    foreach ($fieldName in $FieldNames) {
        $property = $Record.PSObject.Properties[$fieldName]
        if ($null -eq $property) {
            throw "A request-audit rekordbol hianyzik a kovetkezo mezo: $fieldName"
        }

        $value = $property.Value
        if ($null -eq $value) {
            throw "A request-audit rekord null erteket adott a kovetkezo mezore: $fieldName"
        }

        if ($value -is [string] -and [string]::IsNullOrWhiteSpace($value)) {
            throw "A request-audit rekord ures stringet adott a kovetkezo mezore: $fieldName"
        }
    }
}

function Assert-RequestAuditFieldValue {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        $Record,
        [Parameter(Mandatory)]
        [string]$FieldName,
        [Parameter(Mandatory)]
        [string]$ExpectedValue
    )

    $property = $Record.PSObject.Properties[$FieldName]
    if ($null -eq $property) {
        throw "A request-audit rekordbol hianyzik a kovetkezo mezo: $FieldName"
    }

    $actualValue = [string]$property.Value
    if ($actualValue -ne $ExpectedValue) {
        throw "A request-audit rekord $FieldName mezje nem a vart ertek. Vart=$ExpectedValue, tenyleges=$actualValue"
    }
}
