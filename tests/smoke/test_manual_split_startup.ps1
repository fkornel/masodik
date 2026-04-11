param(
    [string]$LauncherPath = "C:\dev\copilot\masodik\build\src\main\masodik-launcher.exe",
    [string]$ProfilePath = "C:\dev\copilot\masodik\config\profiles\b580-manual-split-cut23-tail-32k-u8.json",
    [string]$BaseUrl = "http://127.0.0.1:29011",
    [int]$TimeoutSeconds = 180,
    [string]$ModelRoot = $env:MASODIK_MANUAL_SPLIT_MODEL_ROOT,
    [string]$ExpectedSplitProfile = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ProfilePath)) {
    Write-Host "Manual split startup smoke SKIP: a split profil meg nincs letrehozva ($ProfilePath)."
    return
}

if (-not (Test-Path $LauncherPath)) {
    throw "A launcher nem talalhato: $LauncherPath"
}

if ([string]::IsNullOrWhiteSpace($ModelRoot)) {
    Write-Host "Manual split startup smoke SKIP: a valodi split model-root nincs megadva."
    return
}

if (-not (Test-Path $ModelRoot)) {
    throw "A manual split model-root nem talalhato: $ModelRoot"
}

$manifestPath = Join-Path $ModelRoot "split-manifest.json"
if (-not (Test-Path $manifestPath)) {
    throw "A manual split startup smoke valodi split artifactot var. Hianyzik: $manifestPath"
}

if ([string]::IsNullOrWhiteSpace($ExpectedSplitProfile)) {
    $profileJson = Get-Content $ProfilePath -Raw | ConvertFrom-Json
    $ExpectedSplitProfile = [string]$profileJson.profile_name
}

function Wait-SplitReady {
    param(
        [string]$HealthUrl,
        [int]$WaitSeconds
    )

    $deadline = (Get-Date).AddSeconds($WaitSeconds)
    do {
        $response = $null
        try {
            $response = Invoke-RestMethod -Method Get -Uri $HealthUrl -TimeoutSec 15
        } catch {
        }

        if ($null -ne $response) {
            if ($response.ready -eq $true -and $response.state -eq "ready") {
                return $response
            }
            if ($response.state -eq "failed") {
                throw ("A split startup failed allapotot jelentett: " + $response.status_message)
            }
        }

        Start-Sleep -Milliseconds 750
    } while ((Get-Date) -lt $deadline)

    throw "A split szolgaltatas nem valt ready allapotba a megadott ido alatt."
}

$process = Start-Process -FilePath $LauncherPath -ArgumentList @("--profile", $ProfilePath, "--model-root", $ModelRoot) -PassThru
try {
    $health = Wait-SplitReady -HealthUrl "$BaseUrl/health" -WaitSeconds $TimeoutSeconds
    if ($health.split_mode -ne $true) {
        throw "A split startup smoke split_mode=true allapotot var."
    }
    if ($health.split_profile -ne $ExpectedSplitProfile) {
        throw "A split startup smoke nem a vart split_profile erteket kapta."
    }
    if ($null -eq $health.stage_1_state -or $null -eq $health.stage_2_state) {
        throw "A split health valasz nem tartalmaz stage allapotokat."
    }
    if ($health.stage_1_state -ne "ready" -or $health.stage_2_state -ne "ready") {
        throw "A split startup smoke mindket stage-t ready allapotban varja."
    }
    if ($health.handoff_state -ne "ready") {
        throw "A split startup smoke handoff_state=ready allapotot var."
    }
    if (-not [string]::IsNullOrEmpty($health.last_failure_scope)) {
        throw "A split startup smoke nem var last_failure_scope erteket sikeres startupnal."
    }
    if ($health.fallback_detected -eq $true) {
        throw "A split startup smoke nem engedelyez fallback_detected=true allapotot."
    }
    if (-not $health.PSObject.Properties.Name.Contains("last_failure_summary")) {
        throw "A split startup smoke last_failure_summary mezot var."
    }
    if (-not [string]::IsNullOrEmpty($health.last_failure_summary)) {
        throw "A split startup smoke sikeres startupnal ures last_failure_summary mezot var."
    }

    Write-Host "Manual split startup smoke PASS."
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id
        $process.WaitForExit()
    }
}
