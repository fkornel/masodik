param(
    [string]$LauncherPath,
    [string]$ProfilePath = "config\profiles\b580-manual-split-cut23-tail-32k-u8.json",
    [string]$ModelRoot = $env:MASODIK_MANUAL_SPLIT_MODEL_ROOT,
    [string]$BaseUrl = "http://127.0.0.1:29011",
    [string]$OutputPath = "build\artifacts\manual-split-proof\manual-split-proof-smoke.json"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$captureScript = Join-Path $repoRoot "tools\scripts\capture-manual-split-proof.ps1"

if (-not (Test-Path $captureScript)) {
    throw "A hardware proof smoke nem talalja a capture scriptet: $captureScript"
}

& $captureScript `
    -LauncherPath $LauncherPath `
    -ProfilePath $ProfilePath `
    -ModelRoot $ModelRoot `
    -BaseUrl $BaseUrl `
    -OutputPath $OutputPath

$resolvedOutputPath = if ([System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath
} else {
    Join-Path $repoRoot $OutputPath
}

if (-not (Test-Path $resolvedOutputPath)) {
    throw "A hardware proof smoke nem talalja a capture eredmenyt: $resolvedOutputPath"
}

$proof = Get-Content $resolvedOutputPath -Raw | ConvertFrom-Json
if (-not $proof.proof_passed) {
    throw "A hardware proof smoke szerint a proof_passed=false."
}
if ($proof.request_path_mode -ne "compiled") {
    throw "A hardware proof smoke compiled request_path_mode-ot vart, tenyleges=$($proof.request_path_mode)"
}
if ([string]::IsNullOrWhiteSpace([string]$proof.stage_1_execution_devices)) {
    throw "A hardware proof smoke szerint hianyzik a stage_1_execution_devices mezo."
}
if ([string]::IsNullOrWhiteSpace([string]$proof.stage_2_execution_devices)) {
    throw "A hardware proof smoke szerint hianyzik a stage_2_execution_devices mezo."
}
if ([int]$proof.active_adapter_count -lt 2) {
    throw "A hardware proof smoke legalabb ket aktiv GPU adaptert vart, tenyleges=$($proof.active_adapter_count)"
}

Write-Host "Manual split hardware proof PASS: $resolvedOutputPath"

