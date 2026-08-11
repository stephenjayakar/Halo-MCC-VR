@echo off
setlocal

if "%~1"=="/h" goto :usage
if "%~1"=="/help" goto :usage
if "%~1"=="--help" goto :usage

set "CLEAN="
if "%~1"=="--clean" set "CLEAN=-Clean"
if "%~1"=="-Clean" set "CLEAN=-Clean"
if not "%~1"=="" if "%CLEAN%"=="" goto :bad_option
if not "%~2"=="" goto :bad_option

where powershell.exe >nul 2>nul
if errorlevel 1 (
    echo Windows PowerShell was not found.
    exit /b 127
)

echo Running the verified Halo MCC VR build and deployment flow...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\package-candidate.ps1" %CLEAN%
set "STATUS=%ERRORLEVEL%"
endlocal
exit /b %STATUS%

:bad_option
echo Unknown option. Use deploy.bat or deploy.bat --clean.
exit /b 2

:usage
echo Usage: deploy.bat [--clean]
echo.
echo Builds Release, runs tests, creates a manifest-backed candidate, and
echo deploys the exact verified DLL and launcher to every detected MCC edition.
echo MCC must be closed and the intended changes must already be committed.
echo Use --clean for a from-scratch rebuild.
exit /b 0
