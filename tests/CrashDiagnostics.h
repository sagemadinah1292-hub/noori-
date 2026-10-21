#pragma once
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#pragma comment(lib,"dbghelp.lib")
inline LONG WINAPI reportWindowsTestCrash(EXCEPTION_POINTERS* exception){
 auto process=GetCurrentProcess();SymSetOptions(SYMOPT_UNDNAME|SYMOPT_LOAD_LINES);SymInitialize(process,nullptr,TRUE);
 CONTEXT context=*exception->ContextRecord;STACKFRAME64 frame{};frame.AddrPC.Offset=context.Rip;frame.AddrFrame.Offset=context.Rbp;frame.AddrStack.Offset=context.Rsp;frame.AddrPC.Mode=frame.AddrFrame.Mode=frame.AddrStack.Mode=AddrModeFlat;
 std::fprintf(stderr,"Windows exception 0x%lx at %p\n",exception->ExceptionRecord->ExceptionCode,exception->ExceptionRecord->ExceptionAddress);
 for(int i=0;i<48&&frame.AddrPC.Offset;++i){alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};auto* symbol=reinterpret_cast<SYMBOL_INFO*>(storage);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=MAX_SYM_NAME;DWORD64 displacement=0;const bool named=SymFromAddr(process,frame.AddrPC.Offset,&displacement,symbol)!=FALSE;std::fprintf(stderr,"  0x%llx %s + 0x%llx\n",(unsigned long long)frame.AddrPC.Offset,named?symbol->Name:"?",(unsigned long long)displacement);
  IMAGEHLP_LINE64 line{};line.SizeOfStruct=sizeof(line);DWORD lineOffset=0;if(SymGetLineFromAddr64(process,frame.AddrPC.Offset,&lineOffset,&line))std::fprintf(stderr,"    %s:%lu\n",line.FileName,line.LineNumber);
  if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,GetCurrentThread(),&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;
 }
 std::fflush(stderr);return EXCEPTION_EXECUTE_HANDLER;
}
inline void installTestCrashDiagnostics(){SetUnhandledExceptionFilter(reportWindowsTestCrash);}
#else
inline void installTestCrashDiagnostics(){}
#endif
