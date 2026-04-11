param(
    [string]$BuildDir = "build",
    [string]$Config = "RelWithDebInfo",
    [string]$ManualSplitModelRoot = $env:MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT
)

$ErrorActionPreference = "Stop"

$cmakeExe = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$vsDevCmd = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$benchmarkExe = Join-Path $BuildDir "benchmarks\masodik-benchmarks.exe"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$manualSplitBenchmarkEntryPoints = @(
    "benchmark_manual_split_cold_start",
    "benchmark_manual_split_long_context",
    "benchmark_manual_split_handoff_latency",
    "benchmark_manual_split_diagnostics_overhead",
    "benchmark_manual_split_stability_proof"
)

function Import-VsDevEnvironment {
    param([string]$VsDevCmdPath)

    $cmdString = 'call "' + $VsDevCmdPath + '" >nul && set'
    $envDump = cmd /c $cmdString
    foreach ($line in $envDump) {
        $idx = $line.IndexOf("=")
        if ($idx -gt 0) {
            $name = $line.Substring(0, $idx)
            $value = $line.Substring($idx + 1)
            Set-Item -Path "Env:$name" -Value $value
        }
    }
}

if (-not (Test-Path $BuildDir)) {
    & "$PSScriptRoot\configure-local.ps1" -BuildDir $BuildDir
}

Import-VsDevEnvironment -VsDevCmdPath $vsDevCmd
Push-Location $repoRoot
try {
    & $cmakeExe --build $BuildDir --config $Config --target masodik-benchmarks
    if ($LASTEXITCODE -ne 0) { throw "A benchmark target build sikertelen volt." }
} finally {
    Pop-Location
}

if (Test-Path $benchmarkExe) {
    & $benchmarkExe benchmark_cold_start
    & $benchmarkExe benchmark_first_response
    & $benchmarkExe benchmark_restart_stability
    & $benchmarkExe benchmark_parallel_requests
    & $benchmarkExe benchmark_long_queue
    & $benchmarkExe benchmark_32768_context
    & $benchmarkExe benchmark_compare_gpu_vs_hybrid_32768
    if ($ManualSplitModelRoot) {
        $env:MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT = $ManualSplitModelRoot
        & $benchmarkExe benchmark_manual_split_cold_start
        & $benchmarkExe benchmark_manual_split_long_context
        & $benchmarkExe benchmark_manual_split_handoff_latency
        & $benchmarkExe benchmark_manual_split_diagnostics_overhead
        & $benchmarkExe benchmark_manual_split_stability_proof
    }
    Write-Host ("Manual split benchmark entry points: " + ($manualSplitBenchmarkEntryPoints -join ", "))
} else {
    Write-Host "A benchmark futtathato allomany meg nem keszult el: $benchmarkExe"
    Write-Host ("Manual split benchmark entry points: " + ($manualSplitBenchmarkEntryPoints -join ", "))
}

