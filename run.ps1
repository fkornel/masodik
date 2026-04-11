# masodik TUI indito script
# Hasznalat: .\run.ps1 (a projekt gyokerebol)

$ErrorActionPreference = "Stop"
$repoRoot = $PSScriptRoot

$exePath = Join-Path $repoRoot "build\src\main\masodik-launcher.exe"

if (-not (Test-Path $exePath)) {
    Write-Host "HIBA: A masodik-launcher.exe nem talalhato: $exePath" -ForegroundColor Red
    Write-Host "Futtasd elobb a build-et: .\tools\scripts\build-and-test.ps1" -ForegroundColor Yellow
    exit 1
}

# A repo gyokerebol inditjuk, hogy a config/masodik.json relativ utvonalak mukodjenek
Push-Location $repoRoot
try {
    & $exePath @args
} finally {
    Pop-Location
}
