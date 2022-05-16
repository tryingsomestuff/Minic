#include "attack.hpp"
#include "bitboardTools.hpp"
#include "cli.hpp"
#include "com.hpp"
#include "definition.hpp"
#include "distributed.h"
#include "dynamicConfig.hpp"
#include "egt.hpp"
#include "evalConfig.hpp"
#include "hash.hpp"
#include "kpk.hpp"
#include "logging.hpp"
#include "material.hpp"
#include "nnue.hpp"
#include "option.hpp"
#include "pgnparser.hpp"
#include "searchConfig.hpp"
#include "searcher.hpp"
#include "testSuite.hpp"
#include "evaluationTuning.hpp"
#include "threading.hpp"
#include "timeMan.hpp"
#include "timers.hpp"
#include "tools.hpp"
#include "transposition.hpp"
#include "uci.hpp"
#include "xboard.hpp"

// Initialize all the things that should be ...
void init(int argc, char** argv) {
   Distributed::init();
   Logging::hellooo();
   Options::initOptions(argc, argv);
   Logging::init(); // after reading options
   Zobrist::initHash();
   TT::initTable();
   BBTools::initMask();
#ifdef WITH_MAGIC
   BBTools::MagicBB::initMagic();
#endif
   KPK::init();
   MaterialHash::MaterialHashInitializer::init();
   EvalConfig::initEval();
   ThreadPool::instance().setup();
   Distributed::lateInit();
#ifdef WITH_SYZYGY
   SyzygyTb::initTB();
#endif
#ifdef WITH_NNUE
   NNUEWrapper::init();
#endif
   COM::init(COM::p_uci); // let's do this ... (usefull to reset position in case of NNUE)
}

void finalize() {
   Logging::LogIt(Logging::logInfo) << "Stopping all threads";
   ThreadPool::instance().stop();
   Logging::LogIt(Logging::logInfo) << "Waiting all threads";
   ThreadPool::instance().wait(); ///@todo is this necessary ?
   Logging::LogIt(Logging::logInfo) << "Deleting all threads";
   ThreadPool::instance().resize(0);
   Logging::LogIt(Logging::logInfo) << "Syncing process...";
   Distributed::sync(Distributed::_commInput, __PRETTY_FUNCTION__);
   Logging::LogIt(Logging::logInfo) << "Bye bye...";
   Logging::finalize();
   Logging::LogIt(Logging::logInfo) << "See you soon...";
   Distributed::finalize();
}

#define RETURN(x)   \
   {                \
      bool ret = x; \
      finalize();   \
      return ret;   \
   }

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#ifdef __cplusplus
extern "C" {
#endif
EMSCRIPTEN_KEEPALIVE void processCommand(const char *str){
    const std::string command(str);
    UCI::processCommand(command);
}
#ifdef __cplusplus
}
#endif
#endif

int minic(int argc, char** argv) {
   START_TIMER

   // initialization is always called
   init(argc, argv);

#ifndef __EMSCRIPTEN__ // if emscripten build, them everything is skipped

#ifdef WITH_TEST_SUITE
   if (argc > 1 && test(argv[1])) RETURN(EXIT_SUCCESS)
#endif // WITH_TEST_SUITE

#ifdef WITH_EVAL_TUNING
   if (argc > 1 && std::string(argv[1]) == "-tuning") {
      evaluationTuning(argv[2]);
      RETURN(EXIT_SUCCESS)
   }
#endif // WITH_EVAL_TUNING

#ifdef WITH_PGN_PARSER
   if (argc > 1 && std::string(argv[1]) == "-pgn") { RETURN(PGNParse(argv[2])) }
#endif // WITH_PGN_PARSER

#ifdef WITH_DATA2BIN
   if (argc > 1 && std::string(argv[1]) == "-plain2bin") { RETURN(convert_plain_to_bin({argv[2]}, std::string(argv[2]) + ".bin", 1, 300)) }
   if (argc > 1 && std::string(argv[1]) == "-pgn2bin")   { RETURN(convert_bin_from_pgn_extract({argv[2]}, std::string(argv[2]) + ".bin", true, false)) }
   if (argc > 1 && std::string(argv[1]) == "-bin2plain") { RETURN(convert_bin_to_plain({argv[2]}, std::string(argv[2]) + ".plain")) }
   if (argc > 1 && std::string(argv[1]) == "-rescore")   { RETURN(rescore({argv[2]}, std::string(argv[2]) + ".rescored")) }
#endif // WITH_DATA2BIN

#ifdef DEBUG_TOOL
   std::string firstOption;
   if (argc < 2) {
      Logging::LogIt(Logging::logInfo) << "Hint: You can use -xboard command line option to enter xboard mode (default is uci)";
      firstOption = "-uci"; // default is uci
   }
   else
      firstOption = argv[1];

   int ret = cliManagement(firstOption, argc, argv);

   STOP_AND_SUM_TIMER(Total)
   Timers::Display();
   finalize();
   return ret;

#else // DEBUG_TOOL
   // only init TimeMan if needed ...
   TimeMan::init();

#ifdef WITH_UCI // UCI is prefered option
   UCI::init();
   UCI::uci();
#else // WITH_UCI
#ifdef WITH_XBOARD
   XBoard::init();
   XBoard::xboard();
#endif // WITH_XBOARD

#endif // WITH_UCI

   STOP_AND_SUM_TIMER(Total)

#ifdef WITH_TIMER
   Timers::Display();
#endif // WITH_TIMER

   RETURN(EXIT_SUCCESS)
#endif // DEBUG_TOOL

#endif // __EMSCRIPTEN__
   return 0;
}
