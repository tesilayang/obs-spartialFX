from pathlib import Path
p=Path('installer/setup.cpp');s=p.read_text(encoding='utf-8')
anchor='DWORD drives=GetLogicalDrives();'
s=s.replace(anchor,'''// Running portable OBS provides its exact path even outside conventional folders.
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
 if(snapshot!=INVALID_HANDLE_VALUE){PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);for(BOOL ok=Process32FirstW(snapshot,&entry);ok;ok=Process32NextW(snapshot,&entry))if(_wcsicmp(entry.szExeFile,L"obs64.exe")==0){HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);if(process){wchar_t exe[32768];DWORD length=32768;if(QueryFullProcessImageNameW(process,0,exe,&length))add(list,seen,fs::path(std::wstring(exe,length)).parent_path().parent_path().parent_path());CloseHandle(process);}}CloseHandle(snapshot);}
 '''+anchor)
old='''if(obsRunning()){log(L"BLOCKED: OBS is running; no installation files modified");if(!silent)MessageBoxW(nullptr,L"请先关闭所有 OBS，再安装、更新或卸载。不会覆盖运行中的 OBS 文件。",L"OBS Spatial FX",MB_OK|MB_ICONWARNING);return 10;}'''
new='''while(obsRunning()){log(L"BLOCKED: OBS is running; no installation files modified");if(silent)return 10;if(MessageBoxW(nullptr,L"已识别运行中的 OBS 路径。请关闭所有 OBS，然后点击重试。\\n不会覆盖运行中的 OBS 文件。",L"OBS Spatial FX",MB_RETRYCANCEL|MB_ICONWARNING)!=IDRETRY)return 10;}'''
# new contains a C++ escaped newline, not a literal source newline.
new=new.replace('路径。请关闭所有 OBS，然后点击重试。\\n不会','路径。请关闭所有 OBS，然后点击重试。\\\\n不会')
assert old in s;s=s.replace(old,new);p.write_text(s,encoding='utf-8')
p=Path('README.md');s=p.read_text(encoding='utf-8-sig').replace('极不常见的便携目录可用 /OBSROOT=完整目录 指定。','对于深层便携目录，可先保持 OBS 打开再运行安装器：安装器记住运行路径，关闭 OBS 后点“重试”即可；也可用 /OBSROOT=完整目录 指定。');p.write_text(s,encoding='utf-8')
p=Path('CMakeLists.txt');s=p.read_text(encoding='utf-8-sig').replace('set(SPATIAL_PHASE 0 CACHE','set(SPATIAL_PHASE 4 CACHE')
start=s.index('file(WRITE "');end=s.index('\\nforeach(name obs',start)
s=s[:start]+'configure_file(src/obsconfig.h.in generated/obsconfig.h @ONLY)'+s[end:]
p.write_text(s,encoding='utf-8')
