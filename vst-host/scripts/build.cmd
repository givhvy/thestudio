@echo off
REM ===================================================================
REM   Stratum VST Host build helper
REM
REM   Loads the MSVC Build Tools environment (so cl.exe can find its
REM   stdlib headers like <algorithm>) and then runs the ninja build.
REM   Forwards extra args to cmake --build.
REM ===================================================================

setlocal EnableDelayedExpansion

REM The path below contains parentheses; using delayed expansion plus
REM goto-based control flow avoids cmd's "(x86)" parsing issue.
set "VSDEVCMD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "!VSDEVCMD!" goto :no_vsdevcmd

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%.." >nul
set "PROJECT_ROOT=%CD%"
popd >nul
set "BUILD_DIR=%PROJECT_ROOT%\build-ninja"

if not exist "%BUILD_DIR%\build.ninja" goto :no_build_dir

call "!VSDEVCMD!" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 goto :vsdev_failed

echo [build.cmd] Building StratumVSTHost ...
cmake --build "%BUILD_DIR%" --target StratumVSTHost %*
exit /b %errorlevel%

:no_vsdevcmd
echo [build.cmd] VsDevCmd.bat not found at:
echo            !VSDEVCMD!
echo            Install "VS 2022 Build Tools" with the C++ workload.
exit /b 1

:no_build_dir
echo [build.cmd] No build-ninja\build.ninja - run:
echo            cmake -B build-ninja -G Ninja
echo            first, from a Developer Command Prompt.
exit /b 1

:vsdev_failed
echo [build.cmd] VsDevCmd.bat failed to initialize MSVC environment.
exit /b 1
