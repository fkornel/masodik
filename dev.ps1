# masodik fejlesztoi hot-reload script
# Figyeli a build konyvtarat. Ha uj exe keszul, leallitja a regit es inditja az ujat.
# SSH-n es lokalis terminalban is mukodik (nincs uj ablak nyitas).
#
# Hasznalat: .\dev.ps1

$ErrorActionPreference = "Stop"
$repoRoot = $PSScriptRoot
$exePath = Join-Path $repoRoot "build\src\main\masodik-launcher.exe"
$iterFile = Join-Path $repoRoot "build\iter.txt"

if (Test-Path $iterFile) {
    $iter = [int](Get-Content $iterFile -Raw).Trim()
} else {
    $iter = 0
}

$currentProcess = $null

function Stop-MasodikProcess {
    if ($script:currentProcess -and -not $script:currentProcess.HasExited) {
        Write-Host "[dev] Leallitas: masodik-launcher (PID $($script:currentProcess.Id))..." -ForegroundColor Yellow
        Stop-Process -Id $script:currentProcess.Id -Force -ErrorAction SilentlyContinue
        try { $script:currentProcess.WaitForExit(5000) | Out-Null } catch {}
    }
    # Biztonsagi halo — ha maradt masodik-launcher process
    Get-Process masodik-launcher -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}

function Start-MasodikProcess {
    param([int]$Iteration)
    if (-not (Test-Path $exePath)) {
        Write-Host "[dev] HIBA: $exePath nem talalhato." -ForegroundColor Red
        return $null
    }
    Write-Host "[dev] Inditas: iter #$Iteration" -ForegroundColor Green
    # Az alkalmazast hatter process-kent inditjuk UGYANABBAN a terminalban.
    # A -NoNewWindow biztositja, hogy SSH-n is mukodjon.
    # A -RedirectStandardInput NUL megakadalyozza, hogy az alkalmazas
    # stdin-je osszekeveredjen a dev.ps1 stdin-jevel.
    $proc = Start-Process -FilePath $exePath `
        -WorkingDirectory $repoRoot `
        -PassThru `
        -NoNewWindow `
        -Environment @{ MASODIK_ITER = "$Iteration" }
    return $proc
}

Write-Host "[dev] masodik fejlesztoi hot-reload" -ForegroundColor Cyan
Write-Host "[dev] Figyelt exe: $exePath" -ForegroundColor DarkGray
Write-Host "[dev] Az alkalmazasbol F10 a kilepes." -ForegroundColor DarkGray
Write-Host ""

if (-not (Test-Path $exePath)) {
    Write-Host "[dev] HIBA: Az exe nem letezik. Futtasd elobb: .\tools\scripts\build-and-test.ps1" -ForegroundColor Red
    exit 1
}

$lastWriteTime = (Get-Item $exePath).LastWriteTime
$iter++
$iter | Out-File $iterFile -Force
$currentProcess = Start-MasodikProcess -Iteration $iter

try {
    while ($true) {
        Start-Sleep -Seconds 2

        # Ha az exe frissult (uj build)
        if (Test-Path $exePath) {
            $currentWriteTime = (Get-Item $exePath).LastWriteTime
            if ($currentWriteTime -ne $lastWriteTime) {
                Write-Host ""
                Write-Host "[dev] Uj build eszlelve, varakozas a post-build befejezesere..." -ForegroundColor DarkGray
                Start-Sleep 5
                $lastWriteTime = (Get-Item $exePath).LastWriteTime
                Write-Host "[dev] Uj verzio: $lastWriteTime" -ForegroundColor Cyan

                Stop-MasodikProcess
                Start-Sleep 1
                Clear-Host

                $iter++
                $iter | Out-File $iterFile -Force
                $currentProcess = Start-MasodikProcess -Iteration $iter
            }
        }

        # Ha a process lealt
        if ($currentProcess -and $currentProcess.HasExited) {
            $exitCode = $currentProcess.ExitCode
            Clear-Host
            if ($exitCode -eq 0) {
                Write-Host "[dev] Az alkalmazas rendesen kilépett (F10). Viszlat!" -ForegroundColor Green
                exit 0
            } else {
                Write-Host "[dev] Az alkalmazas lealt (exit code: $exitCode)" -ForegroundColor Yellow
                Write-Host "[dev] Varakozas uj build-re..." -ForegroundColor DarkGray
            }
            $currentProcess = $null
        }
    }
} finally {
    Stop-MasodikProcess
    Write-Host "[dev] Kileptunk." -ForegroundColor DarkGray
}
