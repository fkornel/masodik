param(
    [string]$LauncherPath,
    [string]$ProfilePath = "config\profiles\b580-manual-split-cut23-tail-32k-u8.json",
    [string]$ModelRoot = $env:MASODIK_MANUAL_SPLIT_MODEL_ROOT,
    [string]$BaseUrl = "http://127.0.0.1:29011",
    [string]$RequestBodyPath = "benchmarks\fixtures\manual-split-long-context-request.json",
    [int]$SampleIntervalMillis = 300,
    [int]$StartupTimeoutSeconds = 600,
    [int]$RequestTimeoutSeconds = 900,
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Resolve-OptionalPath {
    param(
        [string]$PathValue,
        [string]$Description,
        [switch]$AllowEmpty
    )

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        if ($AllowEmpty) {
            return ""
        }
        throw "$Description nincs megadva."
    }

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        $candidate = $PathValue
    } else {
        $candidate = Join-Path $repoRoot $PathValue
    }

    if (-not (Test-Path $candidate)) {
        throw "$Description nem talalhato: $candidate"
    }
    return (Resolve-Path $candidate).Path
}

function Resolve-LauncherPath {
    param([string]$RawPath)

    if (-not [string]::IsNullOrWhiteSpace($RawPath)) {
        return Resolve-OptionalPath -PathValue $RawPath -Description "Launcher path"
    }

    $candidates = @(
        (Join-Path $repoRoot "build\src\main\RelWithDebInfo\masodik-launcher.exe"),
        (Join-Path $repoRoot "build\src\main\Debug\masodik-launcher.exe"),
        (Join-Path $repoRoot "build\src\main\masodik-launcher.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "A launcher nem talalhato. Add meg expliciten a -LauncherPath parametert."
}

function Wait-ReadyState {
    param(
        [string]$HealthUrl,
        [int]$TimeoutSeconds
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            $health = Invoke-RestMethod -Method Get -Uri $HealthUrl -TimeoutSec 15
            if ($health.ready -eq $true) {
                return $health
            }
            if ($health.state -eq "failed") {
                throw "A szolgaltatas failed allapotba kerult: $($health.status_message)"
            }
        } catch {
        }
        Start-Sleep -Seconds 2
    }

    throw "A szolgaltatas nem valt ready allapotba a proof capture alatt."
}

function Wait-Offline {
    param([string]$HealthUrl)

    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        try {
            Invoke-RestMethod -Method Get -Uri $HealthUrl -TimeoutSec 5 | Out-Null
        } catch {
            return
        }
        Start-Sleep -Milliseconds 500
    }

    throw "A szolgaltatas health vegpontja nem allt le idoben."
}

function Parse-LuidFromCounterName {
    param([string]$CounterName)

    $match = [regex]::Match($CounterName, 'luid_(0x[0-9A-Fa-f]+)_(0x[0-9A-Fa-f]+)')
    if (-not $match.Success) {
        return ""
    }
    return ("luid_{0}_{1}" -f $match.Groups[1].Value.ToLowerInvariant(), $match.Groups[2].Value.ToLowerInvariant())
}

function Get-ProofSample {
    param([int]$TargetPid)

    $memoryRows = Get-CimInstance -Namespace root\cimv2 -ClassName Win32_PerfFormattedData_GPUPerformanceCounters_GPUProcessMemory |
        Where-Object { $_.Name -like "pid_${TargetPid}_*" }
    $engineRows = Get-CimInstance -Namespace root\cimv2 -ClassName Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine |
        Where-Object { $_.Name -like "pid_${TargetPid}_*" }

    $byLuid = @{}
    foreach ($row in $memoryRows) {
        $luid = Parse-LuidFromCounterName -CounterName $row.Name
        if ([string]::IsNullOrWhiteSpace($luid)) {
            continue
        }
        if (-not $byLuid.ContainsKey($luid)) {
            $byLuid[$luid] = [ordered]@{
                luid = $luid
                max_dedicated_usage = [uint64]0
                max_local_usage = [uint64]0
                max_non_local_usage = [uint64]0
                max_shared_usage = [uint64]0
                max_total_committed = [uint64]0
                max_engine_utilization = [uint64]0
            }
        }

        $entry = $byLuid[$luid]
        $entry.max_dedicated_usage = [Math]::Max($entry.max_dedicated_usage, [uint64]$row.DedicatedUsage)
        $entry.max_local_usage = [Math]::Max($entry.max_local_usage, [uint64]$row.LocalUsage)
        $entry.max_non_local_usage = [Math]::Max($entry.max_non_local_usage, [uint64]$row.NonLocalUsage)
        $entry.max_shared_usage = [Math]::Max($entry.max_shared_usage, [uint64]$row.SharedUsage)
        $entry.max_total_committed = [Math]::Max($entry.max_total_committed, [uint64]$row.TotalCommitted)
    }

    foreach ($row in $engineRows) {
        $luid = Parse-LuidFromCounterName -CounterName $row.Name
        if ([string]::IsNullOrWhiteSpace($luid)) {
            continue
        }
        if (-not $byLuid.ContainsKey($luid)) {
            $byLuid[$luid] = [ordered]@{
                luid = $luid
                max_dedicated_usage = [uint64]0
                max_local_usage = [uint64]0
                max_non_local_usage = [uint64]0
                max_shared_usage = [uint64]0
                max_total_committed = [uint64]0
                max_engine_utilization = [uint64]0
            }
        }

        $entry = $byLuid[$luid]
        $entry.max_engine_utilization = [Math]::Max($entry.max_engine_utilization, [uint64]$row.UtilizationPercentage)
    }

    return @($byLuid.Values)
}

function Convert-ToActiveAdapterSummary {
    param([object[]]$Samples)

    $active = @($Samples | Where-Object {
        $_.max_dedicated_usage -gt 0 -or
        $_.max_local_usage -gt 0 -or
        $_.max_non_local_usage -gt 0 -or
        $_.max_shared_usage -gt 0 -or
        $_.max_engine_utilization -gt 0
    })

    $ordered = @($active | Sort-Object -Property max_dedicated_usage, max_local_usage, max_shared_usage -Descending)
    $dgpuCandidate = $null
    $igpuCandidate = $null
    if ($ordered.Count -gt 0) {
        $dgpuCandidate = $ordered[0]
    }
    if ($ordered.Count -gt 1) {
        $igpuCandidate = @($ordered | Sort-Object -Property max_shared_usage, max_non_local_usage, max_local_usage -Descending | Select-Object -First 1)[0]
        if ($igpuCandidate.luid -eq $dgpuCandidate.luid -and $ordered.Count -gt 1) {
            $igpuCandidate = $ordered[1]
        }
    }

    return [PSCustomObject]@{
        active_adapter_count = $active.Count
        active_adapters = @($ordered | ForEach-Object { [PSCustomObject]$_ })
        dgpu_candidate = if ($dgpuCandidate) { [PSCustomObject]$dgpuCandidate } else { $null }
        igpu_candidate = if ($igpuCandidate) { [PSCustomObject]$igpuCandidate } else { $null }
    }
}

$resolvedLauncherPath = Resolve-LauncherPath -RawPath $LauncherPath
$resolvedProfilePath = Resolve-OptionalPath -PathValue $ProfilePath -Description "Profile path"
$resolvedModelRoot = Resolve-OptionalPath -PathValue $ModelRoot -Description "Model root"
$resolvedRequestBodyPath = Resolve-OptionalPath -PathValue $RequestBodyPath -Description "Request body path"

$manifestPath = Join-Path $resolvedModelRoot "split-manifest.json"
if (-not (Test-Path $manifestPath)) {
    throw "A proof capture valodi split artifactot var. Hianyzik: $manifestPath"
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $outputDir = Join-Path $repoRoot "build\artifacts\manual-split-proof"
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
    $OutputPath = Join-Path $outputDir ("manual-split-proof-{0}.json" -f (Get-Date -Format "yyyyMMdd-HHmmss"))
} elseif (-not [System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $repoRoot $OutputPath
}

$requestBody = Get-Content $resolvedRequestBodyPath -Raw
$process = Start-Process -FilePath $resolvedLauncherPath -ArgumentList @("--profile", $resolvedProfilePath, "--model-root", $resolvedModelRoot) -PassThru
try {
    $healthBefore = Wait-ReadyState -HealthUrl "$BaseUrl/health" -TimeoutSeconds $StartupTimeoutSeconds
    $requestJob = Start-Job -ScriptBlock {
        param($ChatUrl, $Body, $TimeoutSeconds)
        Invoke-RestMethod -Method Post -Uri $ChatUrl -Body $Body -ContentType "application/json" -TimeoutSec $TimeoutSeconds
    } -ArgumentList "$BaseUrl/v1/chat/completions", $requestBody, $RequestTimeoutSeconds

    $aggregated = @{}
    $peakWorkingSet = [uint64]0
    $peakPrivateBytes = [uint64]0
    while ($requestJob.State -eq "Running") {
        $processSnapshot = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
        if ($processSnapshot) {
            $peakWorkingSet = [Math]::Max($peakWorkingSet, [uint64]$processSnapshot.WorkingSet64)
            $peakPrivateBytes = [Math]::Max($peakPrivateBytes, [uint64]$processSnapshot.PrivateMemorySize64)
        }

        foreach ($sample in Get-ProofSample -TargetPid $process.Id) {
            if (-not $aggregated.ContainsKey($sample.luid)) {
                $aggregated[$sample.luid] = $sample
                continue
            }
            $entry = $aggregated[$sample.luid]
            $entry.max_dedicated_usage = [Math]::Max($entry.max_dedicated_usage, $sample.max_dedicated_usage)
            $entry.max_local_usage = [Math]::Max($entry.max_local_usage, $sample.max_local_usage)
            $entry.max_non_local_usage = [Math]::Max($entry.max_non_local_usage, $sample.max_non_local_usage)
            $entry.max_shared_usage = [Math]::Max($entry.max_shared_usage, $sample.max_shared_usage)
            $entry.max_total_committed = [Math]::Max($entry.max_total_committed, $sample.max_total_committed)
            $entry.max_engine_utilization = [Math]::Max($entry.max_engine_utilization, $sample.max_engine_utilization)
        }
        Start-Sleep -Milliseconds $SampleIntervalMillis
    }

    $response = Receive-Job -Job $requestJob -Wait -AutoRemoveJob
    $healthAfter = Invoke-RestMethod -Method Get -Uri "$BaseUrl/health" -TimeoutSec 30
    $adapterSummary = Convert-ToActiveAdapterSummary -Samples @($aggregated.Values)

    $proofPassed =
        ($healthAfter.request_path_mode -eq "compiled") -and
        (-not [string]::IsNullOrWhiteSpace([string]$healthAfter.stage_1_execution_devices)) -and
        (-not [string]::IsNullOrWhiteSpace([string]$healthAfter.stage_2_execution_devices)) -and
        ($adapterSummary.active_adapter_count -ge 2) -and
        ($null -ne $adapterSummary.dgpu_candidate) -and
        ($null -ne $adapterSummary.igpu_candidate)

    $payload = [PSCustomObject]@{
        captured_at = (Get-Date).ToString("o")
        launcher_pid = $process.Id
        model_root = $resolvedModelRoot
        profile_path = $resolvedProfilePath
        request_body_path = $resolvedRequestBodyPath
        request_path_mode = [string]$healthAfter.request_path_mode
        split_artifact_version = [string]$healthAfter.split_artifact_version
        validated_context_tokens = [int]$healthAfter.validated_context_tokens
        stage_1_device = [string]$healthAfter.stage_1_device
        stage_2_device = [string]$healthAfter.stage_2_device
        stage_1_execution_devices = [string]$healthAfter.stage_1_execution_devices
        stage_2_execution_devices = [string]$healthAfter.stage_2_execution_devices
        last_split_request_id = [string]$healthAfter.last_split_request_id
        last_split_sequence_id = [string]$healthAfter.last_split_sequence_id
        handoff_state = [string]$healthAfter.handoff_state
        peak_working_set_bytes = $peakWorkingSet
        peak_private_bytes = $peakPrivateBytes
        active_adapter_count = $adapterSummary.active_adapter_count
        active_adapters = $adapterSummary.active_adapters
        dgpu_candidate = $adapterSummary.dgpu_candidate
        igpu_candidate = $adapterSummary.igpu_candidate
        response_preview = (($response.choices[0].message.content | Out-String).Trim())
        proof_passed = $proofPassed
    }

    New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($OutputPath)) | Out-Null
    $payload | ConvertTo-Json -Depth 8 | Set-Content -Path $OutputPath -Encoding UTF8
    Write-Host ("Manual split proof capture kesz: " + $OutputPath)

    if (-not $proofPassed) {
        throw "A hardware proof capture nem talalt ket aktiv GPU adaptert a launcher process alatt, vagy a health nem compiled split allapotot mutatott."
    }
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id
        $process.WaitForExit()
    }
    Wait-Offline -HealthUrl "$BaseUrl/health"
}

