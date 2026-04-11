param(
    [string]$BuildDir = "build",
    [string]$Config = "RelWithDebInfo",
    [string]$ManualSplitModelRoot = $env:MASODIK_MANUAL_SPLIT_MODEL_ROOT
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

# T094 – cut23-tail proof artifact elérési utak
$splitArtifactDir = Join-Path $repoRoot "build\generated-manual-split-cut23-tail-32k-u8"
$splitProofJson   = Join-Path $repoRoot "build\artifacts\long-context-measure\cut23-tail-u8-real-32k-measurement.json"

Write-Host "Quickstart validacio indul: build, split smoke, tool-calling, request-audit regressziok."
Write-Host "Aktiv profil: b580-manual-split-cut23-tail-32k-u8 (GPU.1 dGPU stage-1, GPU.0 iGPU stage-2, 32k kontextus, u8 KV-cache)."
if ([string]::IsNullOrWhiteSpace($ManualSplitModelRoot)) {
    Write-Host "Valodi split model-root nincs megadva; placeholder mod smoke-kal folytatunk."
} else {
    Write-Host ("Split model-root megadva: " + $ManualSplitModelRoot)
}

# Proof artifact ellenőrzés (nem blokkoló – a build-and-test már előállítja)
if (Test-Path $splitArtifactDir) {
    Write-Host "Split artifact mappa megtalalhato: $splitArtifactDir"
} else {
    Write-Host "WARN: Split artifact mappa hianzik (meg nem lett generalva): $splitArtifactDir"
}
if (Test-Path $splitProofJson) {
    $proof = Get-Content $splitProofJson -Raw | ConvertFrom-Json
    if ($proof.request_succeeded -eq $true) {
        Write-Host "32k proof artifact PASS: request_succeeded=true – $splitProofJson"
    } else {
        Write-Host "WARN: Proof fajl letezik, de request_succeeded != true: $splitProofJson"
    }
} else {
    Write-Host "WARN: 32k proof artifact meg nem all rendelkezesre: $splitProofJson"
}

& "$PSScriptRoot\run-smoke-suite.ps1" -BuildDir $BuildDir -Config $Config -ManualSplitModelRoot $ManualSplitModelRoot

Write-Host "Quickstart validacio PASS."

