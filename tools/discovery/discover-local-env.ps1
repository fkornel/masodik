param()

$ErrorActionPreference = "Stop"

$vsRoot = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
$msvcRoot = Join-Path $vsRoot "VC\Tools\MSVC"
$openvinoDir = "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\OpenVINO\.venv\Lib\site-packages\openvino\cmake"
$openvinoIncludeDir = "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\OpenVINO\.venv\Lib\site-packages\openvino\include"
$openvinoRuntimeLibsDir = "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\OpenVINO\.venv\Lib\site-packages\openvino\libs"
$ovmsExe = "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\OpenVINO\ovms\ovms.exe"
$genAiDll = "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\OpenVINO\ovms\openvino_genai.dll"
$genAiCDll = "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\OpenVINO\ovms\openvino_genai_c.dll"
$genAiImportLib = Get-ChildItem "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\OpenVINO" -Recurse -Filter "openvino_genai.lib" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
$modelRoot = "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\models\LLM\openvino\OpenVINO---gpt-oss-20b-int4-ov"
$backendVersions = "C:\Users\jezus\AppData\Local\Programs\AI Playground\resources\backend-versions.json"

$toolset = Get-ChildItem $msvcRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
$clPath = if ($toolset) { Join-Path $toolset.FullName "bin\Hostx64\x64\cl.exe" } else { $null }
$backendInfo = $null
if (Test-Path $backendVersions) {
    $backendInfo = Get-Content $backendVersions -Raw | ConvertFrom-Json
}

[PSCustomObject]@{
    visualStudioBuildToolsRoot = $vsRoot
    msvcToolsetVersion = if ($toolset) { $toolset.Name } else { $null }
    clPath = $clPath
    clExists = [bool]($clPath -and (Test-Path $clPath))
    cmakePath = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    ctestPath = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
    ninjaPath = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    openvinoDir = $openvinoDir
    openvinoDirExists = Test-Path $openvinoDir
    openvinoIncludeDir = $openvinoIncludeDir
    openvinoIncludeDirExists = Test-Path $openvinoIncludeDir
    openvinoRuntimeLibsDir = $openvinoRuntimeLibsDir
    openvinoRuntimeLibsDirExists = Test-Path $openvinoRuntimeLibsDir
    ovmsExe = $ovmsExe
    ovmsExists = Test-Path $ovmsExe
    openvinoGenAiDll = $genAiDll
    openvinoGenAiDllExists = Test-Path $genAiDll
    openvinoGenAiCDll = $genAiCDll
    openvinoGenAiCDllExists = Test-Path $genAiCDll
    openvinoGenAiImportLib = $genAiImportLib
    openvinoGenAiImportLibExists = [bool]$genAiImportLib
    modelRoot = $modelRoot
    modelRootExists = Test-Path $modelRoot
    modelXmlPath = Join-Path $modelRoot "openvino_model.xml"
    modelBinPath = Join-Path $modelRoot "openvino_model.bin"
    graphPath = Join-Path $modelRoot "graph.pbtxt"
    backendVersion = $backendInfo.'openvino-backend'.version
    backendReleaseTag = $backendInfo.'openvino-backend'.releaseTag
} | ConvertTo-Json -Depth 4
