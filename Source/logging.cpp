#include "logging.hpp"

#include "dynamicConfig.hpp"

namespace Logging {

    std::mutex LogIt::_mutex;
    std::unique_ptr<std::ofstream> LogIt::_of;

    COMType ct = CT_uci;

    std::string showDate() {
        std::stringstream str;
        auto msecEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch());
        char buffer[64];
        auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::time_point(msecEpoch));
        std::strftime(buffer, 63, "%Y-%m-%d %H:%M:%S", localtime(&tt));
        str << buffer << "-" << std::setw(3) << std::setfill('0') << msecEpoch.count() % 1000;
        return str.str();
    }

    LogIt::~LogIt() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_level != logGUI) {
           if ( ! DynamicConfig::quiet || _level > logGUI ){
              std::cout       << _protocolComment[ct] << _levelNames[_level] << showDate() << ": " << _buffer.str() << std::endl;
              if (_of) (*_of) << _protocolComment[ct] << _levelNames[_level] << showDate() << ": " << _buffer.str() << std::endl;
           }
        }
        else {
            std::cout       << _buffer.str() << std::flush << std::endl;
            if (_of) (*_of) << _buffer.str() << std::flush << std::endl;
        }
        if (_level >= logError) std::cout << backtrace() << std::endl;
        if (_level >= logFatal) {
            exit(1);
        }
    }

    void hellooo() {
      std::cout << Logging::_protocolComment[Logging::ct] << "This is Minic version " << MinicVersion << std::endl;
    }

    void init(){
        if ( DynamicConfig::debugMode ){
            if ( DynamicConfig::debugFile.empty()) DynamicConfig::debugFile = "minic.debug";
            LogIt::_of = std::unique_ptr<std::ofstream>(new std::ofstream(DynamicConfig::debugFile + "_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())));
        }

    }

}
