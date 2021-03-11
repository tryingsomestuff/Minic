#include "logging.hpp"

#include "distributed.h"
#include "dynamicConfig.hpp"

namespace Logging {

    std::mutex LogIt::_mutex;
    std::unique_ptr<std::ofstream> LogIt::_of;

    COMType ct = CT_uci;

    std::string showDate() {
        std::stringstream str;
        auto msecEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch());
        char buffer[64];
        auto tt = Clock::to_time_t(Clock::time_point(msecEpoch));
        std::strftime(buffer, 63, "%Y-%m-%d %H:%M:%S", localtime(&tt));
        str << buffer << "-" << std::setw(3) << std::setfill('0') << msecEpoch.count() % 1000;
        return str.str();
    }

    LogIt::~LogIt() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_level != logGUI) {
           if ( ! DynamicConfig::quiet || _level > logGUI ){
              if ( ! DynamicConfig::silent) std::cout << _protocolComment[ct] << _levelNames[_level] << showDate() << ": " << _buffer.str() << std::endl;
           }
           if (_of) (*_of) << _protocolComment[ct] << _levelNames[_level] << showDate() << ": " << _buffer.str() << std::endl;
        }
        else {
            if ( ! DynamicConfig::silent) std::cout << _buffer.str() << std::flush << std::endl;
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
      if ( Distributed::isMainProcess()){
#ifdef WITH_NNUE
         std::cout << Logging::_protocolComment[Logging::ct] << "This is Minic version " << MinicVersion << " (NNUE available)" << std::endl;
#else
         std::cout << Logging::_protocolComment[Logging::ct] << "This is Minic version " << MinicVersion << std::endl;
#endif
         if ( Distributed::worldSize > 1 ){
           std::cout << Logging::_protocolComment[Logging::ct] << "MPI version running on " << Distributed::worldSize << " process" << std::endl;
         }
      }
    }

    void init(){
        if ( DynamicConfig::debugMode ){
            if ( DynamicConfig::debugFile.empty()) DynamicConfig::debugFile = "minic.debug";
            LogIt::_of = std::unique_ptr<std::ofstream>(new std::ofstream(DynamicConfig::debugFile + "_" + std::to_string(Distributed::rank) + "_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count())));
        }
    }

    void finalize(){
        if ( LogIt::_of ){
            LogIt::_of.reset();
        }
    }

}
