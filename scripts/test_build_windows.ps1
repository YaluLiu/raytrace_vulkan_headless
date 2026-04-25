Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$script = Join-Path $repoRoot "build_windows.bat"
$rayTraceAppSource = Join-Path $repoRoot "headless\ray_trace_app.cpp"
$demoMainSource = Join-Path $repoRoot "demo\main.cpp"
$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) ("build-windows-test-" + [System.Guid]::NewGuid().ToString("N"))
$fakeCmake = Join-Path $tempDir "cmake.bat"
$fakeDemo = Join-Path $tempDir "vk_headless_KHR.bat"
$logFile = Join-Path $tempDir "cmake.log"
$demoLogFile = Join-Path $tempDir "demo.log"

function Assert-Contains {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Expected,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if (-not $Text.Contains($Expected)) {
        throw "Missing expected $Label`: $Expected`nActual:`n$Text"
    }
}

function Invoke-BuildScript {
    param([Parameter(Mandatory = $true)][string]$Target)

    Set-Content -Path $logFile -Value "" -Encoding ASCII
    $env:CMAKE_LOG = $logFile
    $oldPath = $env:PATH
    try {
        $env:PATH = "$tempDir;$oldPath"
        $env:DEMO_EXE = $fakeDemo
        & cmd /c "`"$script`" $Target" | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "build_windows.bat $Target exited with $LASTEXITCODE"
        }
    }
    finally {
        $env:PATH = $oldPath
        Remove-Item Env:CMAKE_LOG -ErrorAction SilentlyContinue
        Remove-Item Env:DEMO_EXE -ErrorAction SilentlyContinue
    }

    return (Get-Content -Raw $logFile)
}

New-Item -ItemType Directory -Path $tempDir | Out-Null
try {
    Set-Content -Path $fakeCmake -Encoding ASCII -Value @"
@echo off
echo %*>> "%CMAKE_LOG%"
exit /b 0
"@
    Set-Content -Path $fakeDemo -Encoding ASCII -Value @"
@echo off
echo demo>> "$demoLogFile"
exit /b 0
"@

    $demoLog = Invoke-BuildScript "demo"
    Assert-Contains $demoLog "-DENABLE_HYDRA=OFF" "demo configure flag"
    Assert-Contains $demoLog "--target vk_headless_KHR" "demo build target"
    Assert-Contains (Get-Content -Raw $demoLogFile) "demo" "demo executable invocation"

    $hydraLog = Invoke-BuildScript "hydra"
    Assert-Contains $hydraLog "-DENABLE_HYDRA=ON" "hydra configure flag"
    Assert-Contains $hydraLog "--target vk_headless_KHR hdRobot" "hydra build targets"
    Assert-Contains $hydraLog "--component hdRobot" "hydra install component"

    $rayTraceApp = Get-Content -Raw $rayTraceAppSource
    Assert-Contains $rayTraceApp '"VK_KHR_external_memory_win32"' "Windows external memory extension"
    Assert-Contains $rayTraceApp '"VK_KHR_external_semaphore_win32"' "Windows external semaphore extension"
    Assert-Contains $rayTraceApp '"VK_KHR_external_fence_win32"' "Windows external fence extension"
    Assert-Contains $rayTraceApp "No compatible Vulkan device" "compatible-device failure message"
    Assert-Contains $rayTraceApp 'fs::current_path() / "headless"' "repo-local shader search path"
    Assert-Contains $rayTraceApp "contextInfo.addDeviceExtension(extName, true)" "optional NGX device extensions"

    $demoMain = Get-Content -Raw $demoMainSource
    Assert-Contains $demoMain "m_materials.empty()" "USD material guard"
    Assert-Contains $demoMain "loadBeautyBallFallback" "beautyball fallback"
}
finally {
    Remove-Item -Recurse -Force $tempDir -ErrorAction SilentlyContinue
}
