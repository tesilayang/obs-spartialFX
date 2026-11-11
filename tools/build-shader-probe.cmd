@echo off
call msvc\setup_x64.bat
cl /nologo /utf-8 /EHsc /std:c++17 /Ithird_party\obs-studio-32.0.0\libobs /Ibuild\generated tools\shader_probe.cpp /Fe:build\shader-probe.exe /Fo:build\shader-probe.obj /link tools\sdk\obs.lib
