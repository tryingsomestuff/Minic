#pragma once

#include "definition.hpp"

#ifdef __ANDROID__
inline std::string backtrace(){
    ////@todo backtrace for android
    return "";
}

#elif __linux__
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

inline std::string backtrace() {
  void *array[64];
  size_t size = backtrace(array, 64);
  char ** bk = backtrace_symbols(array, size);
  std::string ret;
  for(size_t k = 0 ; k < size ; ++k){
      ret += std::string(bk[k]) + "\n";
  }
  return ret;
}

#elif defined __MINGW32__
inline std::string backtrace(){
    ////@todo backtrace for mingw
    return "";
}

#elif defined _WIN32
#include "dbg_win.h"

inline
std::string backtrace() {
    std::stringstream buff;
    std::vector<windbg::StackFrame> stack = windbg::stack_trace();
    buff << "Callstack: \n";
    for (unsigned int i = 0; i < stack.size(); ++i)	{
        buff << "0x" << std::hex << stack[i].address << ": " << stack[i].name << "(" << std::dec << stack[i].line << ") in " << stack[i].module << "\n";
    }
    return buff.str();
}

#elif defined __CYGWIN__
inline
std::string backtrace() {
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
    enum LogLevel : unsigned char { logTrace = 0, logDebug = 1, logInfo = 2, logGUI = 3, logWarn = 4, logError = 5, logFatal = 6};
    const std::string _protocolComment[2] = { "# ", "info string " };
    const std::string _levelNames[7] = { "Trace ", "Debug ", "Info  ", "", "Warn  ", "Error ", "Fatal " };

    class LogIt {
        friend void init();
    public:
        LogIt(LogLevel loglevel):_level(loglevel){}
        template <typename T> inline Logging::LogIt & operator<<(T const & value) { _buffer << value; return *this; }
        ~LogIt();
    private:
        static std::mutex      _mutex;
        std::ostringstream     _buffer;
        LogLevel               _level;
        static std::unique_ptr<std::ofstream> _of;
    };

    void hellooo();

    void init();
}
