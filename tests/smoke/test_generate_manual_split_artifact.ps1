param(
    [string]$BuildDir = "build",
    [string]$Config = "RelWithDebInfo",
    [string]$SourceModelRoot = $env:MASODIK_SOURCE_MODEL_ROOT,
    [string]$OutputRoot = "",
    [string]$ProfilePath = "C:\dev\copilot\masodik\config\profiles\b580-manual-split-cut23-tail-32k-u8.json"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repoRoot "build\generated-manual-split-smoke"
}

& "$repoRoot\tools\scripts\generate-manual-split-artifact.ps1" `
    -BuildDir $BuildDir `
    -Config $Config `
    -SourceModelRoot $SourceModelRoot `
    -OutputRoot $OutputRoot

$manifestPath = Join-Path $OutputRoot "split-manifest.json"
$stage1XmlPath = Join-Path $OutputRoot "stage-1\openvino_model.xml"
$stage2XmlPath = Join-Path $OutputRoot "stage-2\openvino_model.xml"

if (-not (Test-Path $manifestPath)) {
    throw "A generated split artifact smoke split-manifest.json fajlt vart: $manifestPath"
}
if (-not (Test-Path $stage1XmlPath)) {
    throw "A generated split artifact smoke stage-1 IR-t vart: $stage1XmlPath"
}
if (-not (Test-Path $stage2XmlPath)) {
    throw "A generated split artifact smoke stage-2 IR-t vart: $stage2XmlPath"
}

$launcherPath = Join-Path $repoRoot "$BuildDir\src\main\masodik-launcher.exe"
if (-not (Test-Path $launcherPath)) {
    throw "A launcher nem talalhato: $launcherPath"
}

& $launcherPath --profile $ProfilePath --model-root $OutputRoot --validate-and-stop
if ($LASTEXITCODE -ne 0) {
    throw "A generated split artifact smoke szerint a launcher validate-and-stop kore sikertelen."
}

Write-Host "Generated manual split artifact smoke PASS."
