#include "logging.hpp"

#include "distributed.h"
#include "dynamicConfig.hpp"

#ifdef __ANDROID__
std::string backtrace([[maybe_unused]] int skip) {
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

std::string backtrace([[maybe_unused]] int skip) {
   void *    callstack[128];
   const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
   char      buf[1024];
   int       nFrames = backtrace(callstack, nMaxFrames);
   char **   symbols = backtrace_symbols(callstack, nFrames);

   std::ostringstream trace_buf;
   for (int i = skip; i < nFrames; i++) {
      Dl_info info;
      if (dladdr(callstack[i], &info)) {
         char *demangled = nullptr;
         int   status;
         demangled = abi::__cxa_demangle(info.dli_sname, nullptr, 0, &status);
         std::snprintf(buf, sizeof(buf), "%-3d %*p %s + %zd\n", i, static_cast<int>(2 + sizeof(void *) * 2), callstack[i],
                       status == 0 ? demangled : info.dli_sname, (char *)callstack[i] - (char *)info.dli_saddr);
         free(demangled);
      }
      else {
         std::snprintf(buf, sizeof(buf), "%-3d %*p\n", i, static_cast<int>(2 + sizeof(void *) * 2), callstack[i]);
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
std::string backtrace([[maybe_unused]] int skip) {
   ////@todo backtrace for mingw
   return "";
}

#elif defined _WIN32
//#include "dbg_win.h"

std::string backtrace([[maybe_unused]] int skip) {
/*
   std::stringstream               buff;
   std::vector<windbg::StackFrame> stack = windbg::stack_trace();
   buff << "Callstack: \n";
   for (unsigned int i = 0; i < stack.size(); ++i) {
      buff << "0x" << std::hex << stack[i].address << ": " << stack[i].name << "(" << std::dec << stack[i].line << ") in " << stack[i].module << "\n";
   }
   return buff.str();
*/
   return "";
}

#elif defined __CYGWIN__
std::string backtrace([[maybe_unused]] int skip) {
   ///@todo backtrace for cygwin
   return "";
}

#endif

namespace Logging {

std::mutex                     LogIt::_mutex;
std::unique_ptr<std::ofstream> LogIt::_of;

COMType ct = CT_uci;

#if defined(__MINGW32__) || defined(__MINGW64__)
#   define localtime_r(T,Tm) (localtime_s(Tm,T) ? NULL : Tm)
#endif

[[nodiscard]] std::string showDate() {
   std::stringstream str;
   const auto n = std::chrono::system_clock::now();
   str << n;
   return str.str();
}

LogIt::~LogIt() {
   std::lock_guard lock(_mutex);
   if (_level != logGUI) { // those are "comments" and are prefixed with _protocolComment[ct] ("info string" for UCI)
      const std::string rankStr = Distributed::moreThanOneProcess() ? (std::string("(") + std::to_string(Distributed::rank) + ") ") : "";
      const std::string str = std::string(_protocolComment[ct]) + std::string(_levelNames[_level]) + showDate() + ": " + rankStr + _buffer.str();
      if ( _level >= DynamicConfig::minOutputLevel ) {
#ifdef WITH_FMTLIB
         if ( ct == CT_pretty ){
            std::cout << fmt::format(_levelStyles[_level], str) << std::endl;
         }
         else
#endif
         std::cout << str << std::endl;
      }
      // debug file output is *not* depending on DynamicConfig::minOutputLevel
      if (_of) (*_of) << str << std::endl;
   }
   else { // those are direct GUI outputs (like bestmove, feature, option, ...)
      if ( _level >= DynamicConfig::minOutputLevel ) {
         std::cout << _buffer.str() << std::flush << std::endl;
      }
      // debug file output is *not* depending on DynamicConfig::minOutputLevel
      if (_of) (*_of) << _buffer.str() << std::flush << std::endl;
   }
   if (_level >= logError) {
#ifdef DEBUG_BACKTRACE
      std::cout << backtrace() << std::endl;
#endif
   }
   if (_level >= logFatal) {
      Distributed::finalize();
      exit(1);
   }
}

void hellooo() {
   if (Distributed::isMainProcess()) {
#ifdef WITH_NNUE
      std::cout << Logging::_protocolComment[Logging::ct] << "This is Minic version " << MinicVersion << " (NNUE available)" << std::endl;
#else
      std::cout << Logging::_protocolComment[Logging::ct] << "This is Minic version " << MinicVersion << std::endl;
#endif
      if (Distributed::moreThanOneProcess()) {
         std::cout << Logging::_protocolComment[Logging::ct] << "MPI version running on " << Distributed::worldSize << " process" << std::endl;
      }
   }
}

void init() {
   if (DynamicConfig::debugMode) {
      if (DynamicConfig::debugFile.empty()) DynamicConfig::debugFile = "minic.debug";
      LogIt::_of = std::unique_ptr<std::ofstream>(
          new std::ofstream(DynamicConfig::debugFile + "_" + std::to_string(Distributed::rank) + "_" +
                            std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count())));
   }
}

void finalize() {
   if (LogIt::_of) { LogIt::_of.reset(); }
}

} // namespace Logging
