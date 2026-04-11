param(
    [string]$BuildDir = "build",
    [string]$Config = "RelWithDebInfo",
    [string]$SourceModelRoot = $env:MASODIK_SOURCE_MODEL_ROOT,
    [string]$OutputRoot = "",
    [string]$CutAfterLayer = "layers.17 -> layers.18",
    [string]$ValidatedForProfile = "b580-manual-split-cut23-tail-32k-u8",
    [int]$MaxContextTokens = 131072,
    [int]$ValidatedContextTokens = 131072,
    [double]$Stage1MemoryBudgetGb = 12.0,
    [double]$Stage2MemoryBudgetGb = 24.0,
    [double]$Stage1KvBudgetGb = 3.5,
    [double]$Stage2KvBudgetGb = 18.0,
    [switch]$CompressToFp16,
    [switch]$NoCleanOutputRoot
)

$ErrorActionPreference = "Stop"

$cmakeExe = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$vsDevCmd = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Import-VsDevEnvironment {
    param([string]$VsDevCmdPath)

    $cmdString = 'call "' + $VsDevCmdPath + '" >nul && set'
    $envDump = cmd /c $cmdString
    foreach ($line in $envDump) {
        $idx = $line.IndexOf("=")
        if ($idx -gt 0) {
            Set-Item -Path ("Env:" + $line.Substring(0, $idx)) -Value $line.Substring($idx + 1)
        }
    }
}

function Resolve-SourceModelRoot {
    param([string]$RawValue)

    $candidate = $RawValue
    if ([string]::IsNullOrWhiteSpace($candidate)) {
        $candidate = "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\models\LLM\openvino\OpenVINO---gpt-oss-20b-int4-ov"
    }

    if (-not (Test-Path $candidate)) {
        throw "A source model-root nem talalhato: $candidate"
    }

    $resolved = (Resolve-Path $candidate).Path
    $modelXmlPath = Join-Path $resolved "openvino_model.xml"
    if (-not (Test-Path $modelXmlPath)) {
        throw "A forrasmodell openvino_model.xml fajlja hianyzik: $modelXmlPath"
    }

    return $resolved
}

function Resolve-OutputRoot {
    param([string]$RawValue, [string]$RepoRoot)

    if ([string]::IsNullOrWhiteSpace($RawValue)) {
        return (Join-Path $RepoRoot "build\generated-manual-split")
    }

    return $RawValue
}

Import-VsDevEnvironment -VsDevCmdPath $vsDevCmd
Push-Location $repoRoot
try {
    & $cmakeExe --build $BuildDir --config $Config --target masodik-split-artifact-tool
    if ($LASTEXITCODE -ne 0) {
        throw "A split artifact tool buildje sikertelen volt."
    }

    $resolvedSourceModelRoot = Resolve-SourceModelRoot -RawValue $SourceModelRoot
    $resolvedOutputRoot = Resolve-OutputRoot -RawValue $OutputRoot -RepoRoot $repoRoot
    $toolPath = Join-Path $repoRoot "$BuildDir\src\main\masodik-split-artifact-tool.exe"
    if (-not (Test-Path $toolPath)) {
        throw "A split artifact tool nem talalhato: $toolPath"
    }

    $argumentList = @(
        "--source-model-root", $resolvedSourceModelRoot,
        "--output-root", $resolvedOutputRoot,
        "--cut-after-layer", $CutAfterLayer,
        "--validated-for-profile", $ValidatedForProfile,
        "--max-context-tokens", $MaxContextTokens.ToString(),
        "--validated-context-tokens", $ValidatedContextTokens.ToString(),
        "--stage-1-memory-budget-gb", $Stage1MemoryBudgetGb.ToString([System.Globalization.CultureInfo]::InvariantCulture),
        "--stage-2-memory-budget-gb", $Stage2MemoryBudgetGb.ToString([System.Globalization.CultureInfo]::InvariantCulture),
        "--stage-1-kv-budget-gb", $Stage1KvBudgetGb.ToString([System.Globalization.CultureInfo]::InvariantCulture),
        "--stage-2-kv-budget-gb", $Stage2KvBudgetGb.ToString([System.Globalization.CultureInfo]::InvariantCulture)
    )
    if ($CompressToFp16) {
        $argumentList += "--compress-to-fp16"
    }
    if ($NoCleanOutputRoot) {
        $argumentList += "--no-clean-output-root"
    }

    & $toolPath @argumentList
    if ($LASTEXITCODE -ne 0) {
        throw "A split artifact generalas sikertelen volt."
    }
} finally {
    Pop-Location
}
