@echo off
call msvc\setup_x64.bat
cl /nologo /utf-8 /EHsc /std:c++17 /W4 tools\native-debugger.cpp /Fe:build\spatial-debugger.exe /Fo:build\native-debugger.obj /link dbghelp.lib

