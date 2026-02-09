@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: LGTV Companion - Build Script
:: Usage: build.bat [options]
::   --config <Debug|Release>     Build configuration (default: Release)
::   --platform <x64|arm64>       Target platform (default: x64)
::   --help                       Show this help message
:: ============================================================================

set CONFIG=Release
set PLATFORM=x64

:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--config" (
    set CONFIG=%~2
    shift & shift
    goto :parse_args
)
if /i "%~1"=="--platform" (
    set PLATFORM=%~2
    shift & shift
    goto :parse_args
)
if /i "%~1"=="--help" (
    echo Usage: build.bat [options]
    echo   --config ^<Debug^|Release^>     Build configuration (default: Release)
    echo   --platform ^<x64^|arm64^>       Target platform (default: x64)
    echo   --help                       Show this help message
    exit /b 0
)
echo Unknown option: %~1
exit /b 1

:args_done

echo.
echo ============================================
echo  LGTV Companion Build
echo  Configuration: %CONFIG%
echo  Platform:      %PLATFORM%
echo ============================================
echo.

:: Check for vcpkg
where vcpkg >nul 2>&1
if %errorlevel% neq 0 (
    if defined VCPKG_ROOT (
        set "PATH=%VCPKG_ROOT%;%PATH%"
        where vcpkg >nul 2>&1
        if !errorlevel! neq 0 (
            echo [ERROR] vcpkg not found. Please install vcpkg and set VCPKG_ROOT.
            echo         See: https://github.com/microsoft/vcpkg#quick-start-windows
            exit /b 1
        )
    ) else (
        echo [ERROR] vcpkg not found. Please install vcpkg and set VCPKG_ROOT.
        echo         See: https://github.com/microsoft/vcpkg#quick-start-windows
        exit /b 1
    )
)

:: Check for MSBuild (try vswhere first, then PATH)
set MSBUILD=
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
    set "MSBUILD=%%i"
)
if not defined MSBUILD (
    where msbuild >nul 2>&1
    if %errorlevel% equ 0 (
        set MSBUILD=msbuild
    ) else (
        echo [ERROR] MSBuild not found. Please install Visual Studio 2022 with C++ workload.
        exit /b 1
    )
)

echo [1/3] Integrating vcpkg...
vcpkg integrate install
if %errorlevel% neq 0 (
    echo [ERROR] vcpkg integrate install failed.
    exit /b 1
)

echo.
echo [2/3] Restoring NuGet packages...
"%MSBUILD%" /t:Restore /p:VcpkgEnableManifest=true LGTVCompanion.sln /verbosity:minimal
if %errorlevel% neq 0 (
    echo [WARNING] NuGet restore had issues, continuing with build...
)

echo.
echo [3/3] Building solution (%CONFIG%^|%PLATFORM%)...
"%MSBUILD%" /p:VcpkgEnableManifest=true /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% LGTVCompanion.sln /verbosity:minimal
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed!
    exit /b 1
)

echo.
echo ============================================
echo  Build successful!
echo  Output: %PLATFORM%\%CONFIG%\
echo ============================================
echo.
