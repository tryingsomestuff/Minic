#pragma once

#include "definition.hpp"

#ifdef __ANDROID__
inline std::string backtrace(int skip = 1) {
   ////@todo backtrace for android
   return "";
}

#elif __linux__
#include <cxxabi.h> // for __cxa_demangle
#include <dlfcn.h>  // for dladdr
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

inline std::string backtrace(int skip = 1) {
   void *    callstack[128];
   const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
   char      buf[1024];
   int       nFrames = backtrace(callstack, nMaxFrames);
   char **   symbols = backtrace_symbols(callstack, nFrames);

   std::ostringstream trace_buf;
   for (int i = skip; i < nFrames; i++) {
      Dl_info info;
      if (dladdr(callstack[i], &info)) {
         char *demangled = NULL;
         int   status;
         demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
         std::snprintf(buf, sizeof(buf), "%-3d %*p %s + %zd\n", i, (int)(2 + sizeof(void *) * 2), callstack[i],
                       status == 0 ? demangled : info.dli_sname, (char *)callstack[i] - (char *)info.dli_saddr);
         free(demangled);
      }
      else {
         std::snprintf(buf, sizeof(buf), "%-3d %*p\n", i, (int)(2 + sizeof(void *) * 2), callstack[i]);
      }
      trace_buf << buf;
      std::snprintf(buf, sizeof(buf), "%s\n", symbols[i]);
      trace_buf << buf;
   }
   free(symbols);
   if (nFrames == nMaxFrames) trace_buf << "[truncated]\n";
   return trace_buf.str();
}

#elif defined __MINGW32__
inline std::string backtrace(int skip = 1) {
   ////@todo backtrace for mingw
   return "";
}

#elif defined _WIN32
#include "dbg_win.h"

inline std::string backtrace(int skip = 1) {
   std::stringstream               buff;
   std::vector<windbg::StackFrame> stack = windbg::stack_trace();
   buff << "Callstack: \n";
   for (unsigned int i = 0; i < stack.size(); ++i) {
      buff << "0x" << std::hex << stack[i].address << ": " << stack[i].name << "(" << std::dec << stack[i].line << ") in " << stack[i].module << "\n";
   }
   return buff.str();
}

#elif defined __CYGWIN__
inline std::string backtrace(int skip = 1) {
   ///@todo backtrace for cygwin
   return "";
}

#endif

/* A little logging facility
 * Can redirect non GUI output to file for debug purpose
 */
namespace Logging {
enum COMType { CT_xboard = 0, CT_uci = 1 };
extern COMType ct;
enum LogLevel : uint8_t { logTrace = 0, logDebug = 1, logInfo = 2, logInfoPrio = 3, logWarn = 4, logGUI = 5, logError = 6, logFatal = 7, logOff = 8, logMax = 9 };
const std::string _protocolComment[2] = {"# ", "info string "};
const std::string _levelNames[logMax] = {"Trace ", "Debug ", "Info  ", "Info  ", "Warn  ", "",  "Error ", "Fatal ", "Off..."};

class LogIt {
   friend void init();
   friend void finalize();

  public:
   LogIt(LogLevel loglevel): _level(loglevel) {}
   template<typename T> inline Logging::LogIt& operator<<(T const& value) {
      _buffer << value;
      return *this;
   }
   ~LogIt();

  private:
   static std::mutex  _mutex;
   std::ostringstream _buffer;
   LogLevel           _level;
   static std::unique_ptr<std::ofstream> _of;
};

void hellooo();

void init();

void finalize();
} // namespace Logging
