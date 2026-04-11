param(
    [string]$LauncherPath = "C:\dev\copilot\masodik\build\src\main\masodik-launcher.exe",
    [string]$ProfilePath = "C:\dev\copilot\masodik\config\profiles\b580-manual-split-cut23-tail-32k-u8.json",
    [string]$BaseUrl = "http://127.0.0.1:29015",
    [string]$ManualSplitProfilePath = "C:\dev\copilot\masodik\config\profiles\b580-manual-split-cut23-tail-32k-u8.json",
    [string]$ManualSplitModelRoot = $env:MASODIK_MANUAL_SPLIT_MODEL_ROOT
)

$ErrorActionPreference = "Stop"

function Stop-BackgroundProcess {
    param([System.Diagnostics.Process]$Process)

    if ($null -ne $Process -and -not $Process.HasExited) {
        Stop-Process -Id $Process.Id
        $Process.WaitForExit()
    }
}

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
        throw "A no-silent-degradation manual split kor valodi split artifactot var. Hianyzik: $manifestPath"
    }

    return $resolvedPath
}

$resolvedManualSplitModelRoot = Resolve-ManualSplitModelRoot -RawValue $ManualSplitModelRoot
if ([string]::IsNullOrWhiteSpace($resolvedManualSplitModelRoot)) {
    Write-Host "Manual split no-silent-degradation SKIP: a valodi split model-root nincs megadva."
    return
}

# A cut23-tail split profil nem engedhet csendes CPU fallbackot (reject_cpu_fallback=true).
# Ready allapotban mindket stage-nek aktiv kell lenni es fallback_detected=false.
$splitProcess = Start-Process -FilePath $LauncherPath -ArgumentList @("--profile", $ManualSplitProfilePath, "--model-root", $resolvedManualSplitModelRoot) -PassThru
try {
    $deadline = (Get-Date).AddMinutes(10)
    $health = $null
    while ((Get-Date) -lt $deadline) {
        try {
            $health = Invoke-RestMethod -Method Get -Uri "$BaseUrl/health" -TimeoutSec 30
            if ($health.state -eq "ready" -or $health.state -eq "failed") {
                break
            }
        } catch {
        }

        Start-Sleep -Seconds 2
    }

    if ($null -eq $health) {
        throw "A split profil nem adott ertelmezheto health valaszt."
    }

    if ($health.state -eq "failed") {
        if ($health.ready -eq $true) {
            throw "A split profil failed allapot mellett nem maradhat ready=true."
        }
        Write-Host "Split loud-failure path PASS."
        return
    }

    if ($health.state -ne "ready") {
        throw "A split profil csak loud failed vagy teljes ready allapotban elfogadhato. Kapott state=$($health.state)"
    }
    if ($health.split_mode -ne $true) {
        throw "A split profil ready allapotban split_mode=true mezot var."
    }
    if ($health.fallback_detected -eq $true) {
        throw "A split profil ready allapotban nem lehet fallback_detected=true."
    }
    if ($health.stage_1_state -ne "ready" -or $health.stage_2_state -ne "ready") {
        throw "A split profil ready allapotban mindket stage-t ready-nek kell mutassa."
    }

    $requestBody = '{"model":"gpt-oss-20b-int4-ov","messages":[{"role":"user","content":"Szia"}],"stream":false,"max_tokens":32}'
    $response = Invoke-RestMethod -Method Post -Uri "$BaseUrl/v1/chat/completions" -ContentType "application/json" -Body $requestBody -TimeoutSec 300
    if ($response.choices[0].message.content -like "*Placeholder valasz*") {
        throw "A split profil ready allapotban placeholder valaszt adott, ez csendes funkcionalis degradacio."
    }

    Write-Host "Split no silent degradation PASS."
} finally {
    Stop-BackgroundProcess -Process $splitProcess
}

