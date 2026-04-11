param(
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

$cmakeExe = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ninjaExe = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$vsDevCmd = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$msvcRoot = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC"
$openvinoDir = "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\OpenVINO\.venv\Lib\site-packages\openvino\cmake"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

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

if (-not (Test-Path $cmakeExe)) { throw "A cmake.exe nem talalhato: $cmakeExe" }
if (-not (Test-Path $ninjaExe)) { throw "A ninja.exe nem talalhato: $ninjaExe" }
if (-not (Test-Path $vsDevCmd)) { throw "A vcvars64.bat nem talalhato: $vsDevCmd" }

$toolset = Get-ChildItem $msvcRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
if (-not $toolset) { throw "Nem talalhato telepitett MSVC toolset a kovetkezo helyen: $msvcRoot" }

$clExe = Join-Path $toolset.FullName "bin\Hostx64\x64\cl.exe"
if (-not (Test-Path $clExe)) { throw "A cl.exe nem talalhato: $clExe" }

Import-VsDevEnvironment -VsDevCmdPath $vsDevCmd
Push-Location $repoRoot
try {
    & $cmakeExe -S . -B $BuildDir -G Ninja `
        -DCMAKE_BUILD_TYPE=RelWithDebInfo `
        -DCMAKE_CXX_COMPILER="$clExe" `
        -DCMAKE_MAKE_PROGRAM="$ninjaExe" `
        -DOpenVINO_DIR="$openvinoDir"
    if ($LASTEXITCODE -ne 0) { throw "A CMake konfiguracio sikertelen volt." }
} finally {
    Pop-Location
}

