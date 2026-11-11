@echo off
setlocal
cd /d "%~dp0.."
call msvc\setup_x64.bat
dumpbin /dependents build\obs-spatial-fx.dll
dumpbin /dependents build\OBS_Spatial_FX_V1.0_Setup.exe
dumpbin /headers build\obs-spatial-fx.dll
