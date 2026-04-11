# Az OpenVINO GenAI SDK letoltese es kicsomagolasa.
# Ha az SDK mar letezik a projekt konyvtarban, nem csinal semmit.
#
# Hasznalat: .\tools\scripts\init-sdk.ps1
# Forras: https://storage.openvinotoolkit.org/repositories/openvino_genai/packages/2026.1/windows/

param(
    [string]$SdkVersion = "2026.1.0.0",
    [string]$TargetDir = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

$sdkDirName = "openvino_genai_windows_${SdkVersion}_x86_64"
if ([string]::IsNullOrWhiteSpace($TargetDir)) {
    $TargetDir = Join-Path $repoRoot $sdkDirName
}

if (Test-Path (Join-Path $TargetDir "runtime")) {
    Write-Host "Az OpenVINO GenAI SDK mar letezik: $TargetDir"
    Write-Host "Nincs teendo."
    exit 0
}

$zipUrl = "https://storage.openvinotoolkit.org/repositories/openvino_genai/packages/2026.1/windows/openvino_genai_windows_${SdkVersion}_x86_64.zip"
$zipPath = Join-Path $repoRoot "$sdkDirName.zip"

Write-Host "OpenVINO GenAI SDK letoltese..."
Write-Host "  Verzio: $SdkVersion"
Write-Host "  URL: $zipUrl"
Write-Host "  Cel: $TargetDir"

try {
    Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing
    Write-Host "Letoltes kesz. Kicsomagolas..."

    Expand-Archive -Path $zipPath -DestinationPath $repoRoot -Force
    Remove-Item $zipPath -Force

    if (Test-Path (Join-Path $TargetDir "runtime")) {
        Write-Host "OpenVINO GenAI SDK sikeresen telepitve: $TargetDir"
    } else {
        throw "A kicsomagolas sikertelen — a runtime konyvtar nem talalhato."
    }
} catch {
    Write-Host "HIBA: $($_.Exception.Message)" -ForegroundColor Red
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force -ErrorAction SilentlyContinue }
    exit 1
}
