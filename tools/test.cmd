@echo off
setlocal
cd /d "%~dp0.."
if exist msvc\setup_x64.bat call msvc\setup_x64.bat
if exist third_party\cmake\cmake-4.4.5-windows-x86_64\bin\cmake.exe set PATH=%CD%\third_party\cmake\cmake-4.4.5-windows-x86_64\bin;%PATH%
if not defined OBS_ROOT for /f "delims=" %%I in ('powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\find-obs.ps1') do set OBS_ROOT=%%I
set PATH=%OBS_ROOT%\bin\64bit;%PATH%
ctest --test-dir build --output-on-failure
exit /b %errorlevel%
