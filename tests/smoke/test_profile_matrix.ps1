param(
    [string]$LauncherPath = "C:\dev\copilot\masodik\build\src\main\masodik-launcher.exe",
    [string]$BaseUrl = "http://127.0.0.1:29015",
    [int]$PerProfileTimeoutSeconds = 240,
    [string]$ManualSplitModelRoot = $env:MASODIK_MANUAL_SPLIT_MODEL_ROOT,
    [string]$RequestBodyPath = "C:\dev\copilot\masodik\tests\fixtures\chat_request.json"
)

$ErrorActionPreference = "Stop"

function Resolve-ManualSplitModelRoot {
    param([string]$RawValue)

    if ([string]::IsNullOrWhiteSpace($RawValue)) {
        return ""
    }

    if (-not (Test-Path $RawValue)) {
        throw "A manual split model-root nem talalhato: $RawValue"
    }

    $resolvedPath = (Resolve-Path $RawValue).Path
    $manifestPath = Join-Path $resolvedPath "split-manifest.json"
    if (-not (Test-Path $manifestPath)) {
        throw "A manual split profile matrix valodi split artifactot var. Hianyzik: $manifestPath"
    }

    return $resolvedPath
}

$resolvedManualSplitModelRoot = Resolve-ManualSplitModelRoot -RawValue $ManualSplitModelRoot

# Egyetlen aktiv profil: cut23-tail
$profiles = @()
if (-not [string]::IsNullOrWhiteSpace($resolvedManualSplitModelRoot)) {
    $profiles += @{
        Name = "b580-manual-split-cut23-tail-32k-u8"
        Path = "C:\dev\copilot\masodik\config\profiles\b580-manual-split-cut23-tail-32k-u8.json"
        ExpectedDevice = ""
        ExpectedLog = "C:\dev\copilot\masodik\build\logs\masodik-runtime-manual-split-cut23-tail-32k-u8.log"
        RequestModelRoot = $resolvedManualSplitModelRoot
        SplitMode = $true
    }
} else {
    Write-Host "Profile matrix SKIP: a valodi split model-root nincs megadva. Add meg a ManualSplitModelRoot parametert."
    return
}

function Wait-ProfileReady {
    param(
        [string]$HealthUrl,
        [int]$TimeoutSeconds
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        $response = $null
        try {
            $response = Invoke-RestMethod -Method Get -Uri $HealthUrl -TimeoutSec 20
        } catch {
        }

        if ($null -ne $response) {
            if ($response.ready -eq $true -and $response.state -eq "ready") {
                return $response
            }
            if ($response.state -eq "failed") {
                throw ("A szolgaltatas failed allapotba kerult. Health status_message=" + $response.status_message)
            }
        }

        Start-Sleep -Milliseconds 750
    } while ((Get-Date) -lt $deadline)

    throw "A szolgaltatas nem valt ready allapotba $TimeoutSeconds masodpercen belul."
}

function Wait-ProfileOffline {
    param(
        [string]$HealthUrl,
        [int]$TimeoutSeconds = 30
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        try {
            Invoke-RestMethod -Method Get -Uri $HealthUrl -TimeoutSec 5 | Out-Null
        } catch {
            return
        }

        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $deadline)

    throw "Az elozo profil health vegpontja nem tunt el idoben, port-felszabadulasi gond gyanus."
}

function Stop-ProfileProcess {
    param([System.Diagnostics.Process]$Process)

    if ($null -ne $Process -and -not $Process.HasExited) {
        Stop-Process -Id $Process.Id
        $Process.WaitForExit()
    }
}

function Invoke-ProfileRequest {
    param(
        [string]$ChatUrl,
        [string]$BodyPath
    )

    $payload = Get-Content $BodyPath -Raw | ConvertFrom-Json
    $payload.stream = $false
    $body = $payload | ConvertTo-Json -Depth 10
    return Invoke-RestMethod -Method Post -Uri $ChatUrl -ContentType "application/json" -Body $body -TimeoutSec 180
}

$results = @()

foreach ($profile in $profiles) {
    Write-Host ""
    Write-Host ("=== Profil indul: {0} ===" -f $profile.Name)

    $startupWatch = [System.Diagnostics.Stopwatch]::StartNew()
    $process = $null

    try {
        $arguments = @("--profile", $profile.Path)
        if (-not [string]::IsNullOrWhiteSpace($profile.RequestModelRoot)) {
            $arguments += @("--model-root", $profile.RequestModelRoot)
        }

        $process = Start-Process -FilePath $LauncherPath -ArgumentList $arguments -PassThru
        $health = Wait-ProfileReady -HealthUrl "$BaseUrl/health" -TimeoutSeconds $PerProfileTimeoutSeconds
        $startupWatch.Stop()

        Write-Host ("[{0}] Ready {1:N1} mp alatt | state={2}" -f
            $profile.Name,
            $startupWatch.Elapsed.TotalSeconds,
            $health.state)

        if ($profile.SplitMode) {
            if ($health.split_mode -ne $true) {
                throw ("A manual split profil health payloadja split_mode=true allapotot var. Profil={0}" -f $profile.Name)
            }
            if ($health.stage_1_state -ne "ready" -or $health.stage_2_state -ne "ready") {
                throw ("A manual split profil mindket stage-t ready allapotban varja. Profil={0}" -f $profile.Name)
            }
            if ($health.fallback_detected -eq $true) {
                throw ("A manual split profil fallback_detected=true allapotba kerult. Profil={0}" -f $profile.Name)
            }
        }

        $requestWatch = [System.Diagnostics.Stopwatch]::StartNew()
        $response = Invoke-ProfileRequest -ChatUrl "$BaseUrl/v1/chat/completions" -BodyPath $RequestBodyPath
        $requestWatch.Stop()

        if ($response.object -ne "chat.completion") {
            throw ("A chat valasz nem OpenAI kompatibilis. Profil={0}" -f $profile.Name)
        }
        if (-not $response.choices -or $response.choices.Count -lt 1) {
            throw ("A chat valasz choices tombje hianyzik. Profil={0}" -f $profile.Name)
        }
        if (-not $response.choices[0].message -or [string]::IsNullOrWhiteSpace($response.choices[0].message.content)) {
            throw ("A chat valasz assistant tartalma hianyzik. Profil={0}" -f $profile.Name)
        }
        if ($profile.SplitMode -and $response.choices[0].message.content -like "*Placeholder valasz*") {
            throw ("A manual split profil placeholder valaszt adott, ez nem tekintheto valodi ketstage-es kiszolgalasnak. Profil={0}" -f $profile.Name)
        }

        Write-Host ("[{0}] Chat OK {1:N1} mp alatt | finish_reason={2}" -f
            $profile.Name,
            $requestWatch.Elapsed.TotalSeconds,
            $response.choices[0].finish_reason)

        $results += [pscustomobject]@{
            profile = $profile.Name
            startup_seconds = [Math]::Round($startupWatch.Elapsed.TotalSeconds, 1)
            request_seconds = [Math]::Round($requestWatch.Elapsed.TotalSeconds, 1)
            effective_device = "manual-split GPU.1+GPU.0"
            status = "PASS"
        }
    } catch {
        Write-Host ("[{0}] HIBA: {1}" -f $profile.Name, $_.Exception.Message)
        if (Test-Path $profile.ExpectedLog) {
            Write-Host ("[{0}] Utolso runtime log sorok:" -f $profile.Name)
            Get-Content $profile.ExpectedLog -Tail 20 | ForEach-Object { Write-Host $_ }
        }
        throw
    } finally {
        Stop-ProfileProcess -Process $process
        Wait-ProfileOffline -HealthUrl "$BaseUrl/health"
        Start-Sleep -Seconds 2
    }
}

Write-Host ""
Write-Host "=== Profil smoke osszegzes ==="
$results | Format-Table -AutoSize | Out-String | Write-Host
Write-Host "Profil smoke PASS."


function Wait-ProfileReady {
    param(
        [string]$HealthUrl,
        [int]$TimeoutSeconds
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        $response = $null
        try {
            $response = Invoke-RestMethod -Method Get -Uri $HealthUrl -TimeoutSec 20
        } catch {
        }

        if ($null -ne $response) {
            if ($response.ready -eq $true -and $response.state -eq "ready") {
                return $response
            }
            if ($response.state -eq "failed") {
                throw ("A szolgaltatas failed allapotba kerult. Health status_message=" + $response.status_message)
            }
        }

        Start-Sleep -Milliseconds 750
    } while ((Get-Date) -lt $deadline)

    throw "A szolgaltatas nem valt ready allapotba $TimeoutSeconds masodpercen belul."
}

function Wait-ProfileOffline {
    param(
        [string]$HealthUrl,
        [int]$TimeoutSeconds = 30
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        try {
            Invoke-RestMethod -Method Get -Uri $HealthUrl -TimeoutSec 5 | Out-Null
        } catch {
            return
        }

        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $deadline)

    throw "Az elozo profil health vegpontja nem tunt el idoben, port-felszabadulasi gond gyanus."
}

function Stop-ProfileProcess {
    param([System.Diagnostics.Process]$Process)

    if ($null -ne $Process -and -not $Process.HasExited) {
        Stop-Process -Id $Process.Id
        $Process.WaitForExit()
    }
}

function Invoke-ProfileRequest {
    param(
        [string]$ChatUrl,
        [string]$BodyPath
    )

    $payload = Get-Content $BodyPath -Raw | ConvertFrom-Json
    $payload.stream = $false
    $body = $payload | ConvertTo-Json -Depth 10
    return Invoke-RestMethod -Method Post -Uri $ChatUrl -ContentType "application/json" -Body $body -TimeoutSec 180
}

$results = @()

foreach ($profile in $profiles) {
    Write-Host ""
    Write-Host ("=== Profil indul: {0} ===" -f $profile.Name)

    $startupWatch = [System.Diagnostics.Stopwatch]::StartNew()
    $process = $null

    try {
        $arguments = @("--profile", $profile.Path)
        if (-not [string]::IsNullOrWhiteSpace($profile.RequestModelRoot)) {
            $arguments += @("--model-root", $profile.RequestModelRoot)
        }

        $process = Start-Process -FilePath $LauncherPath -ArgumentList $arguments -PassThru
        $health = Wait-ProfileReady -HealthUrl "$BaseUrl/health" -TimeoutSeconds $PerProfileTimeoutSeconds
        $startupWatch.Stop()

        Write-Host ("[{0}] Ready {1:N1} mp alatt | state={2} | effective_device={3}" -f
            $profile.Name,
            $startupWatch.Elapsed.TotalSeconds,
            $health.state,
            $health.effective_device)

        if (-not [string]::IsNullOrWhiteSpace($profile.ExpectedDevice) -and $health.effective_device -ne $profile.ExpectedDevice) {
            throw ("A health effective_device nem vart. Profil={0} vart={1} kapott={2}" -f
                $profile.Name,
                $profile.ExpectedDevice,
                $health.effective_device)
        }
        if ($profile.SplitMode) {
            if ($health.split_mode -ne $true) {
                throw ("A manual split profil health payloadja split_mode=true allapotot var. Profil={0}" -f $profile.Name)
            }
            if ($health.stage_1_state -ne "ready" -or $health.stage_2_state -ne "ready") {
                throw ("A manual split profil mindket stage-t ready allapotban varja. Profil={0}" -f $profile.Name)
            }
            if ($health.fallback_detected -eq $true) {
                throw ("A manual split profil fallback_detected=true allapotba kerult. Profil={0}" -f $profile.Name)
            }
        }

        $requestWatch = [System.Diagnostics.Stopwatch]::StartNew()
        $response = Invoke-ProfileRequest -ChatUrl "$BaseUrl/v1/chat/completions" -BodyPath $RequestBodyPath
        $requestWatch.Stop()

        if ($response.object -ne "chat.completion") {
            throw ("A chat valasz nem OpenAI kompatibilis. Profil={0}" -f $profile.Name)
        }
        if (-not $response.choices -or $response.choices.Count -lt 1) {
            throw ("A chat valasz choices tombje hianyzik. Profil={0}" -f $profile.Name)
        }
        if (-not $response.choices[0].message -or [string]::IsNullOrWhiteSpace($response.choices[0].message.content)) {
            throw ("A chat valasz assistant tartalma hianyzik. Profil={0}" -f $profile.Name)
        }
        if ($profile.SplitMode -and $response.choices[0].message.content -like "*Placeholder valasz*") {
            throw ("A manual split profil placeholder valaszt adott, ez nem tekintheto valodi ketstage-es kiszolgalasnak. Profil={0}" -f $profile.Name)
        }

        Write-Host ("[{0}] Chat OK {1:N1} mp alatt | finish_reason={2}" -f
            $profile.Name,
            $requestWatch.Elapsed.TotalSeconds,
            $response.choices[0].finish_reason)

        if ($startupWatch.Elapsed.TotalSeconds -gt $PerProfileTimeoutSeconds) {
            throw ("A startup tullepte a keretet. Profil={0} ido={1:N1}s limit={2}s" -f
                $profile.Name,
                $startupWatch.Elapsed.TotalSeconds,
                $PerProfileTimeoutSeconds)
        }
        if ($requestWatch.Elapsed.TotalSeconds -gt $PerProfileTimeoutSeconds) {
            throw ("A chat keres tullepte a keretet. Profil={0} ido={1:N1}s limit={2}s" -f
                $profile.Name,
                $requestWatch.Elapsed.TotalSeconds,
                $PerProfileTimeoutSeconds)
        }

        $effectiveDeviceLabel = if ($profile.SplitMode) { "manual-split" } else { $health.effective_device }
        $results += [pscustomobject]@{
            profile = $profile.Name
            startup_seconds = [Math]::Round($startupWatch.Elapsed.TotalSeconds, 1)
            request_seconds = [Math]::Round($requestWatch.Elapsed.TotalSeconds, 1)
            effective_device = $effectiveDeviceLabel
            status = "PASS"
        }
    } catch {
        Write-Host ("[{0}] HIBA: {1}" -f $profile.Name, $_.Exception.Message)
        if (Test-Path $profile.ExpectedLog) {
            Write-Host ("[{0}] Utolso runtime log sorok:" -f $profile.Name)
            Get-Content $profile.ExpectedLog -Tail 20 | ForEach-Object { Write-Host $_ }
        }
        throw
    } finally {
        Stop-ProfileProcess -Process $process
        Wait-ProfileOffline -HealthUrl "$BaseUrl/health"
        Start-Sleep -Seconds 2
    }
}

Write-Host ""
Write-Host "=== Profil smoke osszegzes ==="
$results | Format-Table -AutoSize | Out-String | Write-Host
Write-Host "Profil smoke PASS."
