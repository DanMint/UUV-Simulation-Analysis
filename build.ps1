# build.ps1 - PowerShell build script for UUV-Simulation-Analysis
# Usage: .\build.ps1 [Release|Debug] [Clean]

param(
    [string]$BuildType = "Release",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$Toolchain = "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
$BuildDir = "windows_build/build"

if ($Clean) {
    Write-Host "Cleaning build directory..."
    Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
    Write-Host "Clean complete."
    exit 0
}

Write-Host "Configuring CMake (build type: $BuildType)..."
Set-Location windows_build
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=$Toolchain -DCMAKE_BUILD_TYPE=$BuildType .. | Out-Null

Write-Host "Building..."
cmake --build build --config $BuildType --target uuv_sim test_attackerAgent test_simulation test_diveld_scenario test_logger | Out-Null

Write-Host "Running tests..."
ctest --test-dir build --output-on-failure

Write-Host ""
Write-Host "Build complete: $BuildDir/$BuildType/uuv_sim.exe"
