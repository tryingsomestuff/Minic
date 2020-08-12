#include "definition.hpp"

#include "attack.hpp"
#include "bitboardTools.hpp"
#include "book.hpp"
#include "cli.hpp"
#include "com.hpp"
#include "dynamicConfig.hpp"
#include "egt.hpp"
#include "evalConfig.hpp"
#include "hash.hpp"
#include "kpk.hpp"
#include "logging.hpp"
#include "material.hpp"
#include "nn.hpp"
#include "nnue.hpp"
#include "option.hpp"
#include "pgnparser.hpp"
#include "smp.hpp"
#include "testSuite.hpp"
#include "texelTuning.hpp"
#include "timeMan.hpp"
#include "timers.hpp"
#include "tools.hpp"
#include "transposition.hpp"
#include "searchConfig.hpp"
#include "xboard.hpp"
#include "uci.hpp"

// Initialize all the things that should be ...
void init(int argc, char ** argv) {
    Logging::hellooo();
    Options::initOptions(argc, argv);
    Logging::init(); // after reading options
    Zobrist::initHash();
    TT::initTable();
    SearchConfig::initLMR();
    SearchConfig::initMvvLva();
    BBTools::initMask();
#ifdef WITH_MAGIC
    BBTools::MagicBB::initMagic();
#endif
    KPK::init();
    MaterialHash::MaterialHashInitializer::init();
    EvalConfig::initEval();
    ThreadPool::instance().setup();
    Book::initBook();
#ifdef WITH_SYZYGY
    SyzygyTb::initTB();
#endif
#ifdef WITH_MLP
    NN::loadNet();
#endif
#ifdef WITH_NNUE
    if ( !DynamicConfig::NNUEFile.empty()){
        nnue::init_NNUE();
        nnue::verify_NNUE();
    }
#endif
    COM::init(); // let's do this ... (usefull to reset position in case of NNUE)
}

int main(int argc, char ** argv) {
    START_TIMER

    init(argc, argv);

#ifdef WITH_TEST_SUITE
    if (argc > 1 && test(argv[1])) return EXIT_SUCCESS;
#endif

#ifdef WITH_TEXEL_TUNING
    if (argc > 1 && std::string(argv[1]) == "-texel") { TexelTuning(argv[2]); return EXIT_SUCCESS; }
#endif

#ifdef WITH_PGN_PARSER
    if (argc > 1 && std::string(argv[1]) == "-pgn") { return PGNParse(argv[2]); }
#endif

#ifdef WITH_DATA2BIN
    if ( argc > 1 && std::string(argv[1]) == "-plain2bin")  { return convert_bin({argv[2]},std::string(argv[2])+".bin",1,160,0); }
    if ( argc > 1 && std::string(argv[1]) == "-pgn2bin")    { return convert_bin_from_pgn_extract({argv[2]},std::string(argv[2])+".bin",true); }
    if ( argc > 1 && std::string(argv[1]) == "-bin2plain")  { return convert_plain({argv[2]},std::string(argv[2])+".plain"); }
#endif

#ifdef DEBUG_TOOL
    std::string firstOption;
    if (argc < 2) {
        Logging::LogIt(Logging::logInfo) << "Hint: You can use -xboard command line option to enter xboard mode";
        firstOption="-uci"; // default is uci
    }
    else firstOption=argv[1];
    int ret = cliManagement(firstOption,argc,argv);
    STOP_AND_SUM_TIMER(Total)
    #ifdef WITH_TIMER
        Timers::Display();
    #endif
    return ret;
#else
    // only init TimeMan if needed ...
    TimeMan::init();

#ifdef WITH_UCI // UCI is prefered option
    UCI::init();
    UCI::uci();
#else
#ifdef WITH_XBOARD
    XBoard::init();
    XBoard::xboard();
#endif

#endif

    STOP_AND_SUM_TIMER(Total)

#ifdef WITH_TIMER
    Timers::Display();
#endif

    return EXIT_SUCCESS;
#endif
}
