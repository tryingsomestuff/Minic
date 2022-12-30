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

inline void sizeOf(){
   Logging::LogIt(Logging::logInfo) << "Size of Position " << sizeof(Position) << "b";
   Logging::LogIt(Logging::logInfo) << "Size of PVSData  " << sizeof(Searcher::PVSData) << "b";
}

// Initialize all the things that should be ...
inline void init(int argc, char** argv) {
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
   sizeOf();
}

inline void finalize() {
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
      const bool ret = x; \
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

bool help() {
   const std::string h1 = 
R"(
 Available options are :
 * -uci : starts in UCI mode (this is also default)
 * -xboard : starts in XBOARD move
)";
   const std::string h2 =
R"(
 Depending on compilation parameters you may use :
 * -tuning 
 * -pgn
 * -bin2plain
 * -plain2bin
 * -pgn2bin
 * -rescore
)";
#ifdef DEBUG_TOOL
   const std::string h3 =
R"(
 DEV / TEST options available are :
 * -selfplay [depth=15] [games=1] : run self play matches. Usefull to generate training data
 * -perft_test : run small perf test on 4 well-known positions
 * -perft_test_long_fischer : run a long perf test for FRC
 * -perft_test_long : run a long perf test
 * -see_test : run a SEE test (most positions taken from Vajolet by Marco Belli a.k.a elcabesa)
 * -evalSpeed [filename] : run an evaluation performance test
 * -timeTest [initial=50000] [incr=0] [moveInTC=-1] [guiLag=0] : run a TC simulation
 * bench [depth=16] : used for OpenBench output
 Next commands needs at least a position
 * -qsearch [pos]: run a qsearch
 * -see [pos] [move=\"e2e4\"] : run a SEE_GE on the given square
 * -attacked [pos] [square=0..63] : print BB attack
 * -cov [pos] [square=0..63] : print each piece type attack
 * -eval [pos] : run an evaluation (NNUE)
 * -evalHCE [pos] : run an evaluation (HCE)
 * -gen [pos] : generate available moves
 * -testmove [TODO] : test move application
 * -perft [pos] [depth=5] : run a perft on the given position for the given depth
 * -analyze [pos] [depth=15] : run an analysis for the given position to the given depth
 * -mateFinder [pos] [depth=10] : run an analysis in mate finder mode for the given position to the given depth
 * -probe [pos] : TB probe
 * -kpk [pos] : KPK probe
 Use --help_test do display available test suites (depends on compilation option)
)";
#endif
   Logging::LogIt(Logging::logGUI) << h1;
   Logging::LogIt(Logging::logGUI) << h2;
#ifdef DEBUG_TOOL
   Logging::LogIt(Logging::logGUI) << h3;
#endif
   return true;
}

#ifdef WITH_TEST_SUITE
bool helpTest(){
   const std::string h =
R"(
Available analysis tests
 * MEA
 * TTT
 * opening200
 * opening1000
 * middle200
 * middle1000
 * hard2020
 * BT2630
 * WAC
 * arasan
 * arasan_sym
 * CCROHT
 * BKTest
 * EETest
 * KMTest
 * LCTest
 * sbdTest
 * STS
 * ERET
 * MATE
)";
   Logging::LogIt(Logging::logGUI) << h;
   return 0;
}
#endif

inline bool minic(int argc, char** argv) {
   START_TIMER

   // initialization is **always** called
   // for sure this can be optimized in some situations 
   // but it may not worth the pain/risks
   init(argc, argv);

   std::string firstArg = argc > 1 ? argv[1] : "";
   const bool hasParam = argc > 2;
   const std::string secondArg = hasParam ? argv[2] : "";

#ifdef WITH_TEST_SUITE
   if (firstArg == "help_test" || firstArg == "-help_test" || firstArg == "--help_test") {
      return helpTest();
   }
#endif

   if (firstArg == "-h" || firstArg == "-help" || firstArg == "--help" || firstArg == "help") {
      return help();
   }

#ifndef __EMSCRIPTEN__ // if emscripten build, them everything is skipped

#ifdef WITH_TEST_SUITE
   // this will run a test suite if requiered
   if (test(firstArg)) RETURN(true)
#endif // WITH_TEST_SUITE

#ifdef WITH_EVAL_TUNING
   if (hasParam && std::string(firstArg) == "-tuning") {
      evaluationTuning(secondArg);
      RETURN(true)
   }
#endif // WITH_EVAL_TUNING

#ifdef WITH_PGN_PARSER
   if (hasParam && std::string(firstArg) == "-pgn") { RETURN(PGNParse(secondArg)) }
#endif // WITH_PGN_PARSER

#ifdef WITH_DATA2BIN
   if (hasParam && std::string(firstArg) == "-plain2bin") { 
      RETURN(convert_plain_to_bin({secondArg}, secondArg + ".bin", 1, 300)) 
   }
   if (hasParam && std::string(firstArg) == "-pgn2bin")   { 
      RETURN(convert_bin_from_pgn_extract({secondArg}, secondArg + ".bin", true, false)) 
   }
   if (hasParam && std::string(firstArg) == "-bin2plain") { 
      RETURN(convert_bin_to_plain({secondArg}, secondArg + ".plain")) 
   }
   if (hasParam && std::string(firstArg) == "-rescore")   { 
      RETURN(rescore({secondArg}, secondArg + ".rescored")) 
   }
#endif // WITH_DATA2BIN

#ifdef DEBUG_TOOL
   if (firstArg.empty()) {
      Logging::LogIt(Logging::logInfo) << "Hint: You can use -xboard command line option to enter xboard mode (default is uci)";
      firstArg = "-uci"; // default is uci
   }

   const bool ret = cliManagement(firstArg, argc, argv);
   STOP_AND_SUM_TIMER(Total)
   Timers::Display();
   finalize();
   return ret;

#else // DEBUG_TOOL
   // only init TimeMan if needed ...
   TimeMan::init();

#ifdef WITH_UCI // UCI is prefered default option
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

   RETURN(true)
#endif // DEBUG_TOOL

#endif // __EMSCRIPTEN__
   return true;
}
