@echo off
setlocal EnableExtensions

rem Usage:
rem   build_windows.bat hydra
rem   build_windows.bat help
rem
rem Branches:
rem   hydra  Configure, build hdRobot, then install the hdRobot component into
rem          the USD plugin directory.
rem
rem Environment overrides:
rem   BUILD_ROOT      Build directory prefix. Defaults to build.
rem   CONFIG          CMake configuration. Defaults to Release.
rem   INSTALL_PREFIX  USD plugin install directory. Defaults to D:\dev_env\usd\plugin\usd.

cd /d "%~dp0"

set "TARGET=%~1"
call :set_defaults

if "%TARGET%"=="" (
  call :usage_error
) else if /i "%TARGET%"=="help" (
  call :usage_ok
) else if /i "%TARGET%"=="-h" (
  call :usage_ok
) else if /i "%TARGET%"=="--help" (
  call :usage_ok
) else if /i "%TARGET%"=="/?" (
  call :usage_ok
) else if /i "%TARGET%"=="hydra" (
  call :run_hydra
) else (
  call :unknown_target "%TARGET%"
)

exit /b %errorlevel%

:set_defaults
if not defined BUILD_ROOT set "BUILD_ROOT=build"
if not defined CONFIG set "CONFIG=Release"
if not defined INSTALL_PREFIX set "INSTALL_PREFIX=D:\dev_env\usd\plugin\usd"
exit /b 0

:usage_ok
call :print_usage
exit /b 0

:usage_error
call :print_usage
exit /b 2

:unknown_target
echo [build_windows] unknown target: %~1
echo.
call :usage_error
exit /b %errorlevel%

:run_hydra
set "BUILD_DIR=%BUILD_ROOT%_hydra"
call :print_hydra_settings
call :configure_hydra
if errorlevel 1 exit /b %errorlevel%
call :build_targets hdRobot
if errorlevel 1 exit /b %errorlevel%
call :install_hydra
exit /b %errorlevel%

:print_hydra_settings
echo [build_windows] branch=hydra
echo [build_windows] config=%CONFIG%
echo [build_windows] build_dir=%BUILD_DIR%
echo [build_windows] install_prefix=%INSTALL_PREFIX%
exit /b 0

:configure_hydra
call cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%"
exit /b %errorlevel%

:build_targets
call cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target %* --parallel
exit /b %errorlevel%

:install_hydra
call cmake --install "%BUILD_DIR%" --config "%CONFIG%" --component hdRobot --prefix "%INSTALL_PREFIX%"
exit /b %errorlevel%

:print_usage
echo Usage: %~nx0 ^<hydra^>
echo.
echo Branches:
echo   hydra  Build Hydra plugin targets and install hdRobot to INSTALL_PREFIX.
echo.
echo Defaults:
echo   BUILD_ROOT=build
echo   CONFIG=Release
echo   INSTALL_PREFIX=D:\dev_env\usd\plugin\usd
exit /b 0
