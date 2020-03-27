#pragma once

#include "definition.hpp"

/* A little logging facility
 * Can redirect non GUI output to file for debug purpose
 */

namespace Logging {
    enum COMType { CT_xboard = 0, CT_uci = 1 };
    extern COMType ct;
    enum LogLevel : unsigned char { logTrace = 0, logDebug = 1, logInfo = 2, logGUI = 3, logWarn = 4, logError = 5, logFatal = 6};
    const std::string _protocolComment[2] = { "# ", "info string " };
    const std::string _levelNames[7] = { "Trace ", "Debug ", "Info  ", "", "Warn  ", "Error ", "Fatal " };

    inline std::string backtrace() { return "@todo:: backtrace"; } ///@todo find a very simple portable implementation

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
