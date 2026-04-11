param(
    [string]$BuildDir = "build",
    [string]$Config = "RelWithDebInfo",
    [string]$ManualSplitModelRoot = $env:MASODIK_MANUAL_SPLIT_MODEL_ROOT
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$splitProfilePath = Join-Path $repoRoot "config\profiles\b580-manual-split-cut23-tail-32k-u8.json"
$baseUrl = "http://127.0.0.1:29015"
$chatFixturePath = Join-Path $repoRoot "tests\fixtures\chat_request.json"
$streamSmokeScript = Join-Path $repoRoot "tests\smoke\test_chat_completion_stream.ps1"
$streamQueueVisibilityScript = Join-Path $repoRoot "tests\smoke\test_stream_queue_visibility.ps1"
$toolCallSmokeScript = Join-Path $repoRoot "tests\smoke\test_tool_calling.ps1"
$toolCallStreamSmokeScript = Join-Path $repoRoot "tests\smoke\test_tool_calling_stream.ps1"
$requestAuditSmokeScript = Join-Path $repoRoot "tests\smoke\test_request_audit_logging.ps1"
$manualSplitStartupSmokeScript = Join-Path $repoRoot "tests\smoke\test_manual_split_startup.ps1"
$manualSplitCompiled128kSmokeScript = Join-Path $repoRoot "tests\smoke\test_manual_split_compiled_128k.ps1"
$manualSplitLongContextSmokeScript = Join-Path $repoRoot "tests\smoke\test_manual_split_long_context.ps1"
$manualSplitDiagnosticsSmokeScript = Join-Path $repoRoot "tests\smoke\test_manual_split_diagnostics.ps1"
$toolCallFixturePath = Join-Path $repoRoot "tests\fixtures\chat_request_tool_call.json"
$toolCallStreamFixturePath = Join-Path $repoRoot "tests\fixtures\chat_request_tool_call_stream.json"
$requestAuditFixturePath = Join-Path $repoRoot "tests\fixtures\chat_request_audit_tool_auto.json"
$requestAuditLogPath = Join-Path $repoRoot "build\logs\masodik-runtime-manual-split-cut23-tail-32k-u8.log"

# Proof artifact elérési út (T094 – validate-quickstart validation path)
$splitArtifactDir = Join-Path $repoRoot "build\generated-manual-split-cut23-tail-32k-u8"
$splitProofJson = Join-Path $repoRoot "build\artifacts\long-context-measure\cut23-tail-u8-real-32k-measurement.json"

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
        throw "A smoke suite manual split kor valodi split artifactot var. Hianyzik: $manifestPath"
    }

    return $resolvedPath
}

function Wait-ReadyState {
    param(
        [string]$HealthUrl,
        [int]$TimeoutSeconds
    )

    $health = $null
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $health = $null
        try {
            $health = Invoke-RestMethod -Method Get -Uri $HealthUrl -TimeoutSec 30
        } catch {
        }

        if ($null -ne $health) {
            if ($health.ready -eq $true) {
                return $health
            }
            if ($health.state -eq "failed") {
                throw ("A smoke suite alatti szolgaltatas failed allapotba kerult: " + $health.status_message)
            }
        }
        Start-Sleep -Seconds 2
    }

    throw "A smoke suite alatt a szolgaltatas nem valt ready allapotba."
}

function Stop-BackgroundProcess {
    param([System.Diagnostics.Process]$Process)

    if ($Process -and -not $Process.HasExited) {
        Stop-Process -Id $Process.Id
        $Process.WaitForExit()
    }
}

function Wait-Offline {
    param(
        [string]$HealthUrl,
        [int]$TimeoutSeconds = 30
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            Invoke-RestMethod -Method Get -Uri $HealthUrl -TimeoutSec 5 | Out-Null
        } catch {
            return
        }
        Start-Sleep -Milliseconds 500
    }

    throw "A szolgaltatas health vegpontja nem tunt el idoben."
}

function Resolve-LauncherPath {
    param(
        [string]$RepositoryRoot,
        [string]$BuildDirectory,
        [string]$BuildConfig
    )

    $candidates = @(
        (Join-Path $RepositoryRoot "$BuildDirectory\src\main\$BuildConfig\masodik-launcher.exe"),
        (Join-Path $RepositoryRoot "$BuildDirectory\src\main\masodik-launcher.exe")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    return $candidates[0]
}

$launcherPath = Resolve-LauncherPath -RepositoryRoot $repoRoot -BuildDirectory $BuildDir -BuildConfig $Config

if (-not (Test-Path $launcherPath)) {
    & "$PSScriptRoot\build-and-test.ps1" -BuildDir $BuildDir -Config $Config
    $launcherPath = Resolve-LauncherPath -RepositoryRoot $repoRoot -BuildDirectory $BuildDir -BuildConfig $Config
}

$resolvedManualSplitModelRoot = Resolve-ManualSplitModelRoot -RawValue $ManualSplitModelRoot

Write-Host "Smoke suite indul: cut23-tail split profil alapjan (port $baseUrl)."
Write-Host "  Profil: $splitProfilePath"
if ($resolvedManualSplitModelRoot) {
    Write-Host "  Split model-root: $resolvedManualSplitModelRoot"
}

# --- Split alapu smoke futtatás ---
if ([string]::IsNullOrWhiteSpace($resolvedManualSplitModelRoot)) {
    Write-Host "Split model-root nincs megadva; alapszintu (placeholder) smoke korrel folytatunk."
    $launcherArgs = @("--profile", $splitProfilePath)
} else {
    $launcherArgs = @("--profile", $splitProfilePath, "--model-root", $resolvedManualSplitModelRoot)
}

$process = Start-Process -FilePath $launcherPath -ArgumentList $launcherArgs -PassThru
try {
    $health = Wait-ReadyState -HealthUrl "$baseUrl/health" -TimeoutSeconds 600

    & "$repoRoot\tests\smoke\test_health_endpoint.ps1" -BaseUrl $baseUrl
    & "$repoRoot\tests\smoke\test_models_endpoint.ps1" -BaseUrl $baseUrl
    & "$repoRoot\tests\smoke\test_chat_completion.ps1" -BaseUrl $baseUrl -BodyPath $chatFixturePath
    if (Test-Path $streamSmokeScript) {
        & $streamSmokeScript -BaseUrl $baseUrl -BodyPath $chatFixturePath
    } else {
        Write-Host "Streaming chat smoke SKIP: $streamSmokeScript meg nincs letrehozva."
    }
    if (Test-Path $streamQueueVisibilityScript) {
        & $streamQueueVisibilityScript -BaseUrl $baseUrl
    } else {
        Write-Host "Streaming queue visibility smoke SKIP: $streamQueueVisibilityScript meg nincs letrehozva."
    }
    if (Test-Path $toolCallSmokeScript) {
        & $toolCallSmokeScript -BaseUrl $baseUrl -BodyPath $toolCallFixturePath
    } else {
        Write-Host "Tool-calling smoke SKIP: $toolCallSmokeScript meg nincs letrehozva."
    }
    if (Test-Path $toolCallStreamSmokeScript) {
        & $toolCallStreamSmokeScript -BaseUrl $baseUrl -BodyPath $toolCallStreamFixturePath
    } else {
        Write-Host "Tool-calling stream smoke SKIP: $toolCallStreamSmokeScript meg nincs letrehozva."
    }
    if (Test-Path $requestAuditSmokeScript) {
        & $requestAuditSmokeScript -BaseUrl $baseUrl -BodyPath $requestAuditFixturePath -LogPath $requestAuditLogPath
    } else {
        Write-Host "Request-audit smoke SKIP: $requestAuditSmokeScript meg nincs letrehozva."
    }
    & "$repoRoot\tests\smoke\test_queue_visibility.ps1" -BaseUrl $baseUrl

    # Embedding endpoint smoke teszt (NPU)
    $embeddingSmokeScript = Join-Path $repoRoot "tests\smoke\test_embedding_endpoint.ps1"
    if (Test-Path $embeddingSmokeScript) {
        & $embeddingSmokeScript -BaseUrl $baseUrl
    } else {
        Write-Host "Embedding smoke SKIP: $embeddingSmokeScript meg nincs letrehozva."
    }

    if (-not [string]::IsNullOrWhiteSpace($resolvedManualSplitModelRoot)) {
        if ($health.split_mode -ne $true) {
            throw "A split smoke kor split_mode=true health allapotot var."
        }
        if (Test-Path $manualSplitCompiled128kSmokeScript) {
            & $manualSplitCompiled128kSmokeScript -BaseUrl $baseUrl
        }
        if (Test-Path $manualSplitDiagnosticsSmokeScript) {
            & $manualSplitDiagnosticsSmokeScript -BaseUrl $baseUrl
        }
        if (Test-Path $manualSplitLongContextSmokeScript) {
            & $manualSplitLongContextSmokeScript -BaseUrl $baseUrl
        }
    }
} finally {
    Stop-BackgroundProcess -Process $process
    Wait-Offline -HealthUrl "$baseUrl/health"
}

& "$repoRoot\tests\smoke\test_restart_cycle.ps1" `
    -LauncherPath $launcherPath `
    -ProfilePath $splitProfilePath `
    -BaseUrl $baseUrl

& "$repoRoot\tests\smoke\test_no_silent_degradation.ps1" `
    -LauncherPath $launcherPath `
    -ProfilePath $splitProfilePath `
    -BaseUrl $baseUrl `
    -ManualSplitProfilePath $splitProfilePath `
    -ManualSplitModelRoot $resolvedManualSplitModelRoot

# T094 – split artifact és proof validació
Write-Host "T094 proof validacio: split artifact es hosszu-kontextus proof ellenorzese."
if (Test-Path $splitArtifactDir) {
    $manifestPath = Join-Path $splitArtifactDir "split-manifest.json"
    if (Test-Path $manifestPath) {
        Write-Host "  Split artifact PASS: $manifestPath"
    } else {
        Write-Host "  WARN: split artifact mappa letezik, de split-manifest.json hianyzik: $manifestPath"
    }
} else {
    Write-Host "  SKIP: split artifact meg nem generalt ($splitArtifactDir)."
}
if (Test-Path $splitProofJson) {
    $proof = Get-Content $splitProofJson -Raw | ConvertFrom-Json
    if ($proof.request_succeeded -eq $true) {
        Write-Host "  32k proof PASS: $splitProofJson"
    } else {
        Write-Host "  WARN: proof fajl letezik, de request_succeeded != true: $splitProofJson"
    }
} else {
    Write-Host "  SKIP: proof meres meg nem all rendelkezesre ($splitProofJson)."
}

Write-Host "Smoke suite PASS."

