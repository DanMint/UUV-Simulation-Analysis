@echo off
REM build.bat - Windows build script for UUV-Simulation-Analysis
REM Usage: build.bat [Release|Debug] [Clean]

setlocal
set BUILD_TYPE=Release
set CMAKE_TOOLCHAIN=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

if "%1"=="" goto :configure
if /i "%1"=="Debug" set BUILD_TYPE=Debug
if /i "%1"=="Release" set BUILD_TYPE=Release

if /i "%2"=="Clean" goto :clean

:configure
echo Configuring CMake (build type: %BUILD_TYPE%)...
cd windows_build
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%CMAKE_TOOLCHAIN% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% .. || exit /b 1

echo Building...
cmake --build build --config %BUILD_TYPE% --target uuv_sim test_attackerAgent test_simulation test_diveld_scenario test_logger || exit /b 1

echo Running tests...
ctest --test-dir build --output-on-failure || exit /b 1

echo.
echo Build complete: windows_build\build\%BUILD_TYPE%\uuv_sim.exe
exit /b 0

:clean
echo Cleaning build directory...
rmdir /s /q windows_build\build
echo Clean complete.
exit /b 0
