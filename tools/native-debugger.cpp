#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <cstdio>
#include <string>
#pragma comment(lib,"dbghelp.lib")
int wmain(int argc,wchar_t **argv){
 if(argc<3)return 2;
 FILE *out=nullptr;_wfopen_s(&out,argv[1],L"wt");if(!out)return 3;
 std::wstring cmd=argv[2];STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
 if(!CreateProcessW(nullptr,cmd.data(),nullptr,nullptr,FALSE,DEBUG_ONLY_THIS_PROCESS,nullptr,nullptr,&si,&pi)){fprintf(out,"create error %lu\n",GetLastError());fclose(out);return 4;}
 SymSetOptions(SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS|SYMOPT_LOAD_LINES);
 fprintf(out,"SymInitialize=%d error=%lu\n",SymInitialize(pi.hProcess,nullptr,TRUE),GetLastError());
 DEBUG_EVENT ev{};bool done=false;DWORD exit=0;
 while(!done){ if(!WaitForDebugEvent(&ev,1000)){if(GetLastError()==ERROR_SEM_TIMEOUT)continue;fprintf(out,"wait error=%lu\n",GetLastError());break;}
   DWORD status=DBG_CONTINUE;
   if(ev.dwDebugEventCode==EXCEPTION_DEBUG_EVENT){
     auto &ex=ev.u.Exception;DWORD code=ex.ExceptionRecord.ExceptionCode;
     if(code!=EXCEPTION_BREAKPOINT&&code!=0x406d1388){
       fprintf(out,"EXCEPTION code=0x%08lx first=%lu address=%p thread=%lu\n",code,ex.dwFirstChance,ex.ExceptionRecord.ExceptionAddress,ev.dwThreadId);
       if(code==EXCEPTION_ACCESS_VIOLATION||code==0xc0000374||code==0xc0000409||!ex.dwFirstChance){ MEMORY_BASIC_INFORMATION mbi{};VirtualQueryEx(pi.hProcess,ex.ExceptionRecord.ExceptionAddress,&mbi,sizeof(mbi));wchar_t fault[4096]{};GetModuleFileNameExW(pi.hProcess,reinterpret_cast<HMODULE>(mbi.AllocationBase),fault,4096);fwprintf(out,L"FAULT MODULE %s base=%p\n",fault,mbi.AllocationBase);
         HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,ev.dwThreadId);CONTEXT c{};c.ContextFlags=CONTEXT_FULL;
         if(thread&&GetThreadContext(thread,&c)){
           STACKFRAME64 sf{};sf.AddrPC={c.Rip,0,AddrModeFlat};sf.AddrFrame={c.Rbp,0,AddrModeFlat};sf.AddrStack={c.Rsp,0,AddrModeFlat};
           for(int i=0;i<48;++i){
             DWORD64 address=sf.AddrPC.Offset;char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};auto*sym=reinterpret_cast<SYMBOL_INFO*>(storage);sym->SizeOfStruct=sizeof(SYMBOL_INFO);sym->MaxNameLen=MAX_SYM_NAME;DWORD64 displacement=0;
             IMAGEHLP_MODULE64 mod{};mod.SizeOfStruct=sizeof(mod);SymGetModuleInfo64(pi.hProcess,address,&mod);
             if(SymFromAddr(pi.hProcess,address,&displacement,sym))fprintf(out,"  %02d %s!%s+0x%llx [%llx]\n",i,mod.ModuleName,sym->Name,displacement,address);
             else fprintf(out,"  %02d %s+0x%llx [%llx]\n",i,mod.ModuleName,address-mod.BaseOfImage,address);
             if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,pi.hProcess,thread,&sf,&c,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;
           }
         }if(thread)CloseHandle(thread);
       }
       status=DBG_EXCEPTION_NOT_HANDLED;fflush(out);
     }
   }else if(ev.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT){auto &d=ev.u.LoadDll;char name[4096]{};if(d.hFile)GetFinalPathNameByHandleA(d.hFile,name,sizeof(name),FILE_NAME_NORMALIZED);fprintf(out,"DLL %p %s\n",d.lpBaseOfDll,name);SymLoadModuleEx(pi.hProcess,d.hFile,name,nullptr,reinterpret_cast<DWORD64>(d.lpBaseOfDll),0,nullptr,0);if(d.hFile)CloseHandle(d.hFile);
   }else if(ev.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT){if(ev.u.CreateProcessInfo.hFile)CloseHandle(ev.u.CreateProcessInfo.hFile);
   }else if(ev.dwDebugEventCode==CREATE_THREAD_DEBUG_EVENT){CloseHandle(ev.u.CreateThread.hThread);
   }else if(ev.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT){exit=ev.u.ExitProcess.dwExitCode;fprintf(out,"EXIT code=0x%08lx\n",exit);done=true;}
   ContinueDebugEvent(ev.dwProcessId,ev.dwThreadId,status);
 }
 SymCleanup(pi.hProcess);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);fclose(out);return int(exit);
}


