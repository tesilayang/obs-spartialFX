@echo off
setlocal
cd /d "%~dp0.."
if exist msvc\setup_x64.bat call msvc\setup_x64.bat
where cl.exe >nul 2>nul
if errorlevel 1 (
 echo ERROR: Run from VS2022 x64 Native Tools Command Prompt, or provision local msvc.
 exit /b 1
)
if exist third_party\cmake\cmake-4.4.5-windows-x86_64\bin\cmake.exe set PATH=%CD%\third_party\cmake\cmake-4.4.5-windows-x86_64\bin;%PATH%
if exist third_party\ninja\ninja.exe set PATH=%CD%\third_party\ninja;%PATH%
if not defined PYTHON (
 set PYTHON=python
 if exist "%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" set PYTHON=%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe
)
if not defined OBS_ROOT for /f "delims=" %%I in ('powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\find-obs.ps1') do set OBS_ROOT=%%I
if not defined OBS_ROOT (
 echo ERROR: OBS32+ not detected. Set OBS_ROOT to the installation root.
 exit /b 1
)
if not exist tools\sdk\obs.lib "%PYTHON%" tools\prepare_sdk.py "%OBS_ROOT%"
if errorlevel 1 exit /b 1
set PHASE=%1
if "%PHASE%"=="" set PHASE=4
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSPATIAL_PHASE=%PHASE% "-DOBS_ROOT=%OBS_ROOT%" %2 %3 %4
if errorlevel 1 exit /b 1
cmake --build build --parallel 6
exit /b %errorlevel%
