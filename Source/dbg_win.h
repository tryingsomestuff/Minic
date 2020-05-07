#pragma once

#pragma warning( disable : 4091 )

#include <windows.h>
#include <stdio.h>
#include <intrin.h>
#include <dbghelp.h>
#include <vector>
#include <string>
#include <sstream>

// grrrrrrrrrrr Windows !
// it is a little more difficult to get a pretty stack under Windows OS
// this is making the function stack_trace available for logging purpose
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#pragma comment(lib, "dbghelp.lib")

namespace windbg{    

    inline
    std::string basename(const std::string& file){
        size_t i = file.find_last_of("\\/");
        if (i == std::string::npos){
            return file;
        }
        else{
            return file.substr(i + 1);
        }
    }

    struct StackFrame{
        DWORD64       address;
        std::string   name;
        std::string   module;
        unsigned int  line;
        std::string   file;
    };

    inline
    std::vector<StackFrame> stack_trace(){
        #if _WIN64
        DWORD machine = IMAGE_FILE_MACHINE_AMD64;
        #else
        DWORD machine = IMAGE_FILE_MACHINE_I386;
        #endif
        HANDLE process = GetCurrentProcess();
        HANDLE thread  = GetCurrentThread();
                
        if (SymInitialize(process, NULL, TRUE) == FALSE){
            return std::vector<StackFrame>(); 
        }

        SymSetOptions(SYMOPT_LOAD_LINES);
        
        CONTEXT    context = {};
        context.ContextFlags = CONTEXT_FULL;
        RtlCaptureContext(&context);

        #if _WIN64
        STACKFRAME frame = {};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;
        #else
        STACKFRAME frame = {};
        frame.AddrPC.Offset = context.Eip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Ebp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Esp;
        frame.AddrStack.Mode = AddrModeFlat;
        #endif
		      
        bool first = true;

        std::vector<StackFrame> frames;
        while (StackWalk(machine, process, thread, &frame, &context , NULL, SymFunctionTableAccess, SymGetModuleBase, NULL)){
            StackFrame f = {};
            f.address = frame.AddrPC.Offset;
            
            #if _WIN64
            DWORD64 moduleBase = 0;
            #else
            DWORD moduleBase = 0;
            #endif

            moduleBase = SymGetModuleBase(process, frame.AddrPC.Offset);

            char moduelBuff[MAX_PATH];            
            if (moduleBase && GetModuleFileNameA((HINSTANCE)moduleBase, moduelBuff, MAX_PATH)){
                f.module = basename(moduelBuff);
            }
            else{
                f.module = "Unknown Module";
            }
            #if _WIN64
            DWORD64 offset = 0;
            #else
            DWORD offset = 0;
            #endif
            char symbolBuffer[sizeof(IMAGEHLP_SYMBOL) + 255];
            PIMAGEHLP_SYMBOL symbol = (PIMAGEHLP_SYMBOL)symbolBuffer;
            symbol->SizeOfStruct = (sizeof IMAGEHLP_SYMBOL) + 255;
            symbol->MaxNameLength = 254;

            if (SymGetSymFromAddr(process, frame.AddrPC.Offset, &offset, symbol)){
                f.name = symbol->Name;
            }
            else{
                DWORD error = GetLastError();
                f.name = "Unknown Function";
            }
            
            IMAGEHLP_LINE line;
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE);
            
            DWORD offset_ln = 0;
            if (SymGetLineFromAddr(process, frame.AddrPC.Offset, &offset_ln, &line)){
                f.file = line.FileName;
                f.line = line.LineNumber;
            }
            else{
                DWORD error = GetLastError();
                f.line = 0;
            } 

            if (!first){ 
                frames.push_back(f);
            }
            first = false;
        }

        SymCleanup(process);

        return frames;
    }
}
