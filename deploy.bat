@echo off
setlocal

rem Build, test, package, back up, and deploy the current committed candidate.
rem The PowerShell deployment script preserves halomccvr.cfg and installs the
rem same manifest-verified files into every detected MCC edition.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\package-candidate.ps1" %*
exit /b %ERRORLEVEL%
