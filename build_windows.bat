@echo off
setlocal EnableExtensions

rem Usage:
rem   build_windows.bat hydra
rem   build_windows.bat demo
rem   build_windows.bat help
rem
rem Branches:
rem   hydra  Configure with ENABLE_HYDRA=ON, build vk_headless_KHR and hdRobot,
rem          then install the hdRobot component into the USD plugin directory.
rem   demo   Configure with ENABLE_HYDRA=OFF, build vk_headless_KHR, then run it.
rem
rem Environment overrides:
rem   BUILD_ROOT      Build directory prefix. Defaults to build.
rem   CONFIG          CMake configuration. Defaults to Release.
rem   DLSS_SDK_ROOT   DLSS SDK root. Defaults to D:\dev_env\DLSS.
rem   INSTALL_PREFIX  USD plugin install directory. Defaults to D:\dev_env\usd\plugin\usd.
rem   DEMO_EXE        Demo executable to run after build. Defaults to ..\bin_x64\%CONFIG%\vk_headless_KHR.exe.

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
) else if /i "%TARGET%"=="demo" (
  call :run_demo
) else (
  call :unknown_target "%TARGET%"
)

exit /b %errorlevel%

:set_defaults
if not defined BUILD_ROOT set "BUILD_ROOT=build"
if not defined CONFIG set "CONFIG=Release"
if not defined DLSS_SDK_ROOT set "DLSS_SDK_ROOT=D:\dev_env\DLSS"
if not defined INSTALL_PREFIX set "INSTALL_PREFIX=D:\dev_env\usd\plugin\usd"
if not defined DEMO_EXE set "DEMO_EXE=%~dp0..\bin_x64\%CONFIG%\vk_headless_KHR.exe"
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
call :build_targets vk_headless_KHR hdRobot
if errorlevel 1 exit /b %errorlevel%
call :install_hydra
exit /b %errorlevel%

:run_demo
set "BUILD_DIR=%BUILD_ROOT%_demo"
call :print_demo_settings
call :configure_demo
if errorlevel 1 exit /b %errorlevel%
call :build_targets vk_headless_KHR
if errorlevel 1 exit /b %errorlevel%
call :run_demo_exe
exit /b %errorlevel%

:print_hydra_settings
echo [build_windows] branch=hydra
echo [build_windows] config=%CONFIG%
echo [build_windows] build_dir=%BUILD_DIR%
echo [build_windows] install_prefix=%INSTALL_PREFIX%
exit /b 0

:print_demo_settings
echo [build_windows] branch=demo
echo [build_windows] config=%CONFIG%
echo [build_windows] build_dir=%BUILD_DIR%
echo [build_windows] demo_exe=%DEMO_EXE%
exit /b 0

:configure_hydra
call cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 ^
  -DENABLE_GL_VK_CONVERSION=ON ^
  -DENABLE_HYDRA=ON ^
  -DENABLE_DLSS_RR=ON ^
  -DDLSS_SDK_ROOT="%DLSS_SDK_ROOT%" ^
  -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%"
exit /b %errorlevel%

:configure_demo
call cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 ^
  -DENABLE_GL_VK_CONVERSION=ON ^
  -DENABLE_HYDRA=OFF ^
  -DENABLE_DLSS_RR=ON ^
  -DDLSS_SDK_ROOT="%DLSS_SDK_ROOT%"
exit /b %errorlevel%

:build_targets
call cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target %* --parallel
exit /b %errorlevel%

:install_hydra
call cmake --install "%BUILD_DIR%" --config "%CONFIG%" --component hdRobot --prefix "%INSTALL_PREFIX%"
exit /b %errorlevel%

:run_demo_exe
if not exist "%DEMO_EXE%" (
  echo [build_windows] demo executable not found: "%DEMO_EXE%"
  exit /b 1
)

echo [build_windows] running demo: "%DEMO_EXE%"
call "%DEMO_EXE%"
exit /b %errorlevel%

:print_usage
echo Usage: %~nx0 ^<hydra^|demo^>
echo.
echo Branches:
echo   hydra  Build Hydra plugin targets and install hdRobot to INSTALL_PREFIX.
echo   demo   Build vk_headless_KHR without Hydra and run DEMO_EXE.
echo.
echo Defaults:
echo   BUILD_ROOT=build
echo   CONFIG=Release
echo   DLSS_SDK_ROOT=D:\dev_env\DLSS
echo   INSTALL_PREFIX=D:\dev_env\usd\plugin\usd
echo   DEMO_EXE=..\bin_x64\%%CONFIG%%\vk_headless_KHR.exe
exit /b 0
