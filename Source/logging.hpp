#pragma once

#include "definition.hpp"

std::string backtrace(int skip = 1);

/* A little logging facility
 * Can redirect non GUI output to file for debug purpose
 */
namespace Logging {
enum COMType { CT_xboard = 0, CT_uci = 1, CT_pretty = 2 };
extern COMType ct;

enum LogLevel : uint8_t { 
   logTrace = 0, 
   logDebug = 1, 
   logInfo = 2, 
   logInfoPrio = 3, 
   logWarn = 4, 
   logGUI = 5, 
   logError = 6, 
   logFatal = 7, 
   logOff = 8, 
   logMax = 9 
};

inline constexpr array1d<std::string_view,3> _protocolComment = {"# ", "info string ", ""};

inline constexpr array1d<std::string_view,logMax> _levelNames = {
   "Trace ", 
   "Debug ", 
   "Info  ", 
   "Info  ", 
   "Warn  ", 
   "",  
   "Error ", 
   "Fatal ", 
   "Off..."
};

#ifdef WITH_FMTLIB   
inline constexpr array1d<fmt::text_style,logMax> _levelStyles = {
         fmt::fg(fmt::color::light_gray) | fmt::emphasis::faint,
         fmt::fg(fmt::color::dark_green) | fmt::emphasis::italic,
         fmt::fg(fmt::color::sky_blue) ,
         fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
         fmt::fg(fmt::color::orange_red) | fmt::emphasis::bold,
         {},
         fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
         fmt::fg(fmt::color::pale_violet_red) | fmt::emphasis::bold,
         {}
         };
#endif

class LogIt {
   friend void init();
   friend void finalize();

  public:
   explicit LogIt(LogLevel loglevel): _level(loglevel) {}
   template<typename T> FORCE_FINLINE Logging::LogIt& operator<<(T const& value) {
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
