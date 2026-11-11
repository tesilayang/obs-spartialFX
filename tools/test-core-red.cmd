@echo off
setlocal
cd /d "%~dp0.."
call msvc\setup_x64.bat
cl /nologo /utf-8 /std:c++17 /EHsc /MD /I src tests\core_tests.cpp src\core.cpp /Fe:build\spatial-core-red.exe /Fo:build\ /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1
build\spatial-core-red.exe
exit /b %errorlevel%
