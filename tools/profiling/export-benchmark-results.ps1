param(
    [string]$BuildDir = "build",
    [string]$Config = "RelWithDebInfo",
    [string]$ManualSplitModelRoot = $env:MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT,
    [string[]]$Scenarios = @(
        "benchmark_cold_start",
        "benchmark_first_response",
        "benchmark_restart_stability",
        "benchmark_parallel_requests",
        "benchmark_long_queue",
        "benchmark_32768_context",
        "benchmark_compare_gpu_vs_hybrid_32768"
    )
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$cmakeExe = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$vsDevCmd = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$benchmarkExe = Join-Path $repoRoot "$BuildDir\benchmarks\masodik-benchmarks.exe"
$outputDir = Join-Path $repoRoot "$BuildDir\artifacts\benchmarks"
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$outputPath = Join-Path $outputDir "benchmark-results-$timestamp.json"

function Resolve-ManualSplitModelRoot {
    param([string]$RawValue)

    if ([string]::IsNullOrWhiteSpace($RawValue)) {
        return ""
    }

    if (-not (Test-Path $RawValue)) {
        throw "A manual split benchmark model-root nem talalhato: $RawValue"
    }

    $resolvedPath = (Resolve-Path $RawValue).Path
    $manifestPath = Join-Path $resolvedPath "split-manifest.json"
    if (-not (Test-Path $manifestPath)) {
        throw "A manual split benchmark export valodi split artifactot var. Hianyzik: $manifestPath"
    }

    return $resolvedPath
}

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

Import-VsDevEnvironment -VsDevCmdPath $vsDevCmd
Push-Location $repoRoot
try {
    & $cmakeExe --build $BuildDir --config $Config --target masodik-benchmarks
    if ($LASTEXITCODE -ne 0) { throw "A benchmark target build sikertelen volt." }
} finally {
    Pop-Location
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$resolvedManualSplitModelRoot = Resolve-ManualSplitModelRoot -RawValue $ManualSplitModelRoot
if (-not [string]::IsNullOrWhiteSpace($resolvedManualSplitModelRoot)) {
    $env:MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT = $resolvedManualSplitModelRoot
    $Scenarios += @(
        "benchmark_manual_split_cold_start",
        "benchmark_manual_split_long_context",
        "benchmark_manual_split_handoff_latency",
        "benchmark_manual_split_diagnostics_overhead",
        "benchmark_manual_split_stability_proof"
    )
}

$results = @()
foreach ($scenario in $Scenarios) {
    $outputLines = & $benchmarkExe $scenario 2>&1
    $success = $LASTEXITCODE -eq 0
    $metrics = @{}
    foreach ($line in $outputLines) {
        if ($line -match "^(?<key>[^=]+)=(?<value>.*)$") {
            $metrics[$Matches["key"]] = $Matches["value"]
        }
    }

    $results += [PSCustomObject]@{
        scenario = $scenario
        success = $success
        metrics = $metrics
        raw_output = @($outputLines)
    }
}

$splitManifestMetadata = $null
if (-not [string]::IsNullOrWhiteSpace($resolvedManualSplitModelRoot)) {
    $manifestPath = Join-Path $resolvedManualSplitModelRoot "split-manifest.json"
    if (Test-Path $manifestPath) {
        $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
        $splitManifestMetadata = [PSCustomObject]@{
            artifact_version = $manifest.artifact_version
            cut_after_layer = $manifest.cut_after_layer
            validated_context_tokens = $manifest.validated_context_tokens
            stage_1_layer_range = $manifest.stage_1.layer_range
            stage_2_layer_range = $manifest.stage_2.layer_range
        }
    }
}

$latestProofCapture = $null
$proofCaptureDir = Join-Path $repoRoot "$BuildDir\artifacts\manual-split-proof"
if (Test-Path $proofCaptureDir) {
    $latestProofCapture = Get-ChildItem -Path $proofCaptureDir -Filter "*.json" |
        Sort-Object -Property LastWriteTimeUtc -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

$payload = [PSCustomObject]@{
    exported_at = (Get-Date).ToString("o")
    manual_split_model_root = $resolvedManualSplitModelRoot
    manual_split_manifest = $splitManifestMetadata
    latest_manual_split_proof_capture = $latestProofCapture
    results = $results
}

$payload | ConvertTo-Json -Depth 8 | Set-Content -Path $outputPath -Encoding UTF8
Write-Host "Benchmark export kesz: $outputPath"
