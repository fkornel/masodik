param(
    [string]$BuildDir = "build",
    [string]$Config = "RelWithDebInfo",
    [string]$ManualSplitModelRoot = $env:MASODIK_MANUAL_SPLIT_MODEL_ROOT,
    [string]$NativeTestFilter = "split"
)

$ErrorActionPreference = "Stop"

$cmakeExe = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ctestExe = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
$vsDevCmd = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

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
        throw "A build-and-test manual split kor valodi split artifactot var. Hianyzik: $manifestPath"
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

if (-not (Test-Path $BuildDir)) {
    & "$PSScriptRoot\configure-local.ps1" -BuildDir $BuildDir
}

Import-VsDevEnvironment -VsDevCmdPath $vsDevCmd
Push-Location $repoRoot
try {
    & $cmakeExe --build $BuildDir --config $Config
    if ($LASTEXITCODE -ne 0) { throw "A build sikertelen volt." }

    $nativeTestsExe = Join-Path $repoRoot "$BuildDir\tests\masodik-tests.exe"
    if (Test-Path $nativeTestsExe) {
        if ([string]::IsNullOrWhiteSpace($NativeTestFilter)) {
            Write-Host "A natív tesztkör kihagyva, mert a NativeTestFilter üres."
        } else {
            Write-Host ("Natív split tesztkör indul. Filter: " + $NativeTestFilter)
            & $nativeTestsExe $NativeTestFilter
        }
    } elseif (Test-Path $ctestExe) {
        if ([string]::IsNullOrWhiteSpace($NativeTestFilter)) {
            Write-Host "A CTest kör kihagyva, mert a NativeTestFilter üres."
        } else {
            Write-Host ("CTest split kör indul. Regex filter: " + $NativeTestFilter)
            & $ctestExe --test-dir $BuildDir --output-on-failure -C $Config -R $NativeTestFilter
        }
    }
} finally {
    Pop-Location
}

$resolvedManualSplitModelRoot = Resolve-ManualSplitModelRoot -RawValue $ManualSplitModelRoot
if ([string]::IsNullOrWhiteSpace($resolvedManualSplitModelRoot)) {
    Write-Host "Manual split regression SKIP: a valodi split model-root nincs megadva."
} else {
    Write-Host ("Manual split regression indul. Model-root: " + $resolvedManualSplitModelRoot)
    $env:MASODIK_MANUAL_SPLIT_MODEL_ROOT = $resolvedManualSplitModelRoot
    if ([string]::IsNullOrWhiteSpace($env:MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT)) {
        $env:MASODIK_BENCHMARK_MANUAL_SPLIT_MODEL_ROOT = $resolvedManualSplitModelRoot
    }

    # T095 - cut23-tail proof aggregation
    $splitArtifactDir = Join-Path $repoRoot "build\generated-manual-split-cut23-tail-32k-u8"
    $splitProofJson   = Join-Path $repoRoot "build\artifacts\long-context-measure\cut23-tail-u8-real-32k-measurement.json"
    if (Test-Path $splitArtifactDir) {
        Write-Host "T095 proof aggregation: split artifact OK - $splitArtifactDir"
    } else {
        Write-Host "T095 proof aggregation: split artifact SKIP (meg nem generalt) - $splitArtifactDir"
    }
    if (Test-Path $splitProofJson) {
        $proof = Get-Content $splitProofJson -Raw | ConvertFrom-Json
        if ($proof.request_succeeded -eq $true) {
            Write-Host ("T095 proof aggregation: 32k measurement PASS - request_succeeded=true, peak_working_set_bytes=" + $proof.peak_working_set_bytes)
        } else {
            Write-Host "T095 proof aggregation: 32k measurement WARN - request_succeeded nem true"
        }
    } else {
        Write-Host "T095 proof aggregation: 32k measurement SKIP (meg nem all rendelkezesre) - $splitProofJson"
    }

    & "$PSScriptRoot\validate-quickstart.ps1" -BuildDir $BuildDir -Config $Config -ManualSplitModelRoot $resolvedManualSplitModelRoot
    & "$repoRoot\tools\profiling\export-benchmark-results.ps1" -BuildDir $BuildDir -Config $Config -ManualSplitModelRoot $resolvedManualSplitModelRoot
}

