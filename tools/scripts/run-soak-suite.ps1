param(
    [string]$BuildDir = "build",
    [string]$Config = "RelWithDebInfo"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

& "$repoRoot\tools\profiling\export-benchmark-results.ps1" `
    -BuildDir $BuildDir `
    -Config $Config `
    -Scenarios @(
        "benchmark_cold_start",
        "benchmark_first_response",
        "benchmark_restart_stability",
        "benchmark_parallel_requests",
        "benchmark_long_queue",
        "benchmark_32768_context",
        "benchmark_compare_gpu_vs_hybrid_32768"
    )

Write-Host "Soak suite PASS."
