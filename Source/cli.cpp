#include "cli.hpp"

#include "com.hpp"
#include "dynamicConfig.hpp"
#include "evalDef.hpp"
#include "logging.hpp"
#include "material.hpp"
#include "moveApply.hpp"
#include "moveGen.hpp"
#include "option.hpp"
#include "position.hpp"
#include "searcher.hpp"
#include "tables.hpp"
#include "timeMan.hpp"
#include "tools.hpp"
#include "transposition.hpp"
#include "uci.hpp"
#include "xboard.hpp"

// see cli_perft.cpp
[[nodiscard]] bool perft_test(const std::string& fen, DepthType d, uint64_t expected);
Counter perft(const Position& p, DepthType depth, PerftAccumulator& acc);

// see cli_selfplay.cpp
void selfPlay(DepthType depth, uint64_t & nbPos);

// see cli_SEETest.cpp
bool TestSEE();

void analyze(const Position& p, DepthType depth, bool openBenchOutput = false) {
   static double  benchms    = 0;
   static Counter benchNodes = 0;

   const int oldOutLvl = DynamicConfig::minOutputLevel;
   if (openBenchOutput) { DynamicConfig::minOutputLevel = Logging::logOff; }

   TimeMan::isDynamic                   = false;
   TimeMan::nbMoveInTC                  = -1;
   TimeMan::msecPerMove                 = INFINITETIME;
   TimeMan::msecInTC                    = -1;
   TimeMan::msecInc                     = -1;
   TimeMan::msecUntilNextTC             = -1;
   ThreadPool::instance().currentMoveMs = TimeMan::getNextMSecPerMove(p);

   ///@todo support threading here by using ThinkAsync ?
   ThreadData d;
   d.p     = p;
   d.depth = depth;
   ThreadPool::instance().distributeData(d);
   //COM::position = p; // only need for display purpose
   ThreadPool::instance().main().stopFlag = false;
   ThreadPool::instance().main().searchDriver(false);
   d = ThreadPool::instance().main().getData();
   Logging::LogIt(Logging::logInfo) << "Best move is " << ToString(d.best) << " " << static_cast<int>(d.depth) << " " << d.score << " pv : " << ToString(d.pv);

   if (openBenchOutput) {
      Logging::LogIt(Logging::logInfo) << "Next two lines are for OpenBench";
      const TimeType ms = getTimeDiff(ThreadPool::instance().main().startTime);
      const Counter nodeCount =
          ThreadPool::instance().main().stats.counters[Stats::sid_nodes] + ThreadPool::instance().main().stats.counters[Stats::sid_qnodes];
      benchNodes += nodeCount;
      benchms += static_cast<decltype(benchms)>(ms) / 1000.;
      DynamicConfig::minOutputLevel = oldOutLvl;
      std::cerr << "NODES " << benchNodes << std::endl;
      std::cerr << "NPS " << static_cast<int>(static_cast<decltype(benchms)>(benchNodes) / benchms) << std::endl;
   }
}

// This struct is dedicated to display beta cut statistics in benchmark
struct BetaCutStat{
   Counter betaCuts   = 0ull;
   Counter betaCutsTT = 0ull;
   Counter betaCutsGC = 0ull;
   Counter betaCutsP  = 0ull;
   Counter betaCutsK1 = 0ull;
   Counter betaCutsK2 = 0ull;
   Counter betaCutsK3 = 0ull;
   Counter betaCutsK4 = 0ull;
   Counter betaCutsC  = 0ull;
   Counter betaCutsQ  = 0ull;
   Counter betaCutsBC = 0ull;

   void update(const Stats& stats){
      betaCuts   += stats.counters[Stats::sid_beta] + stats.counters[Stats::sid_ttbeta];
      betaCutsTT += stats.counters[Stats::sid_ttbeta];
      betaCutsGC += stats.counters[Stats::sid_beta_gc];
      betaCutsP  += stats.counters[Stats::sid_beta_p];
      betaCutsK1 += stats.counters[Stats::sid_beta_k1];
      betaCutsK2 += stats.counters[Stats::sid_beta_k2];
      betaCutsK3 += stats.counters[Stats::sid_beta_k3];
      betaCutsK4 += stats.counters[Stats::sid_beta_k4];
      betaCutsC  += stats.counters[Stats::sid_beta_c];
      betaCutsQ  += stats.counters[Stats::sid_beta_q];
      betaCutsBC += stats.counters[Stats::sid_beta_bc];
   }

   float percent(const Counter c) const { return c*100.f/betaCuts; }
   
   void show() const {
#ifdef WITH_BETACUTSTATS
      Logging::LogIt(Logging::logInfo) << "betaCuts   " << betaCuts;
      Logging::LogIt(Logging::logInfo) << "betaCutsTT " << betaCutsTT << " (" << percent(betaCutsTT) << "%)";
      Logging::LogIt(Logging::logInfo) << "betaCutsGC " << betaCutsGC << " (" << percent(betaCutsGC) << "%)";
      Logging::LogIt(Logging::logInfo) << "betaCutsP  " << betaCutsP  << " (" << percent(betaCutsP)  << "%)";
      Logging::LogIt(Logging::logInfo) << "betaCutsK1 " << betaCutsK1 << " (" << percent(betaCutsK1) << "%)";
      Logging::LogIt(Logging::logInfo) << "betaCutsK2 " << betaCutsK2 << " (" << percent(betaCutsK2) << "%)";
      Logging::LogIt(Logging::logInfo) << "betaCutsK3 " << betaCutsK3 << " (" << percent(betaCutsK3) << "%)";
      Logging::LogIt(Logging::logInfo) << "betaCutsK4 " << betaCutsK4 << " (" << percent(betaCutsK4) << "%)";
      Logging::LogIt(Logging::logInfo) << "betaCutsC  " << betaCutsC  << " (" << percent(betaCutsC)  << "%)";
      Logging::LogIt(Logging::logInfo) << "betaCutsQ  " << betaCutsQ  << " (" << percent(betaCutsQ)  << "%)";
      Logging::LogIt(Logging::logInfo) << "betaCutsBC " << betaCutsBC << " (" << percent(betaCutsBC) << "%)";
#endif
   }
};

bool bench(DepthType depth) {
   RootPosition p;
#ifdef WITH_NNUE
   NNUEEvaluator evaluator;
   p.associateEvaluator(evaluator);
#endif

   BetaCutStat betaStats;

   auto pos = {startPosition, fine70, shirov, shirov2, mate4};

   for(const auto & fen : pos){
      readFEN(std::string{fen}, p);
      analyze(p, depth, true);
      betaStats.update(ThreadPool::instance().main().stats);
   }

   betaStats.show();
   
   return true;
}

bool spsaInputs(){
   Options::displayOptionsSPSA();
   return true;
}

bool benchBig(DepthType depth){
   // use here same postitions set as Ethereal or Berserk
   static const std::vector<std::string> positions = {
      "r3k2r/2pb1ppp/2pp1q2/p7/1nP1B3/1P2P3/P2N1PPP/R2QK2R w KQkq a6 0 14",
      "4rrk1/2p1b1p1/p1p3q1/4p3/2P2n1p/1P1NR2P/PB3PP1/3R1QK1 b - - 2 24",
      "r3qbrk/6p1/2b2pPp/p3pP1Q/PpPpP2P/3P1B2/2PB3K/R5R1 w - - 16 42",
      "6k1/1R3p2/6p1/2Bp3p/3P2q1/P7/1P2rQ1K/5R2 b - - 4 44",
      "8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8 w - - 3 54",
      "7r/2p3k1/1p1p1qp1/1P1Bp3/p1P2r1P/P7/4R3/Q4RK1 w - - 0 36",
      "r1bq1rk1/pp2b1pp/n1pp1n2/3P1p2/2P1p3/2N1P2N/PP2BPPP/R1BQ1RK1 b - - 2 10",
      "3r3k/2r4p/1p1b3q/p4P2/P2Pp3/1B2P3/3BQ1RP/6K1 w - - 3 87",
      "2r4r/1p4k1/1Pnp4/3Qb1pq/8/4BpPp/5P2/2RR1BK1 w - - 0 42",
      "4q1bk/6b1/7p/p1p4p/PNPpP2P/KN4P1/3Q4/4R3 b - - 0 37",
      "2q3r1/1r2pk2/pp3pp1/2pP3p/P1Pb1BbP/1P4Q1/R3NPP1/4R1K1 w - - 2 34",
      "1r2r2k/1b4q1/pp5p/2pPp1p1/P3Pn2/1P1B1Q1P/2R3P1/4BR1K b - - 1 37",
      "r3kbbr/pp1n1p1P/3ppnp1/q5N1/1P1pP3/P1N1B3/2P1QP2/R3KB1R b KQkq b3 0 17",
      "8/6pk/2b1Rp2/3r4/1R1B2PP/P5K1/8/2r5 b - - 16 42",
      "1r4k1/4ppb1/2n1b1qp/pB4p1/1n1BP1P1/7P/2PNQPK1/3RN3 w - - 8 29",
      "8/p2B4/PkP5/4p1pK/4Pb1p/5P2/8/8 w - - 29 68",
      "3r4/ppq1ppkp/4bnp1/2pN4/2P1P3/1P4P1/PQ3PBP/R4K2 b - - 2 20",
      "5rr1/4n2k/4q2P/P1P2n2/3B1p2/4pP2/2N1P3/1RR1K2Q w - - 1 49",
      "1r5k/2pq2p1/3p3p/p1pP4/4QP2/PP1R3P/6PK/8 w - - 1 51",
      "q5k1/5ppp/1r3bn1/1B6/P1N2P2/BQ2P1P1/5K1P/8 b - - 2 34",
      "r1b2k1r/5n2/p4q2/1ppn1Pp1/3pp1p1/NP2P3/P1PPBK2/1RQN2R1 w - - 0 22",
      "r1bqk2r/pppp1ppp/5n2/4b3/4P3/P1N5/1PP2PPP/R1BQKB1R w KQkq - 0 5",
      "r1bqr1k1/pp1p1ppp/2p5/8/3N1Q2/P2BB3/1PP2PPP/R3K2n b Q - 1 12",
      "r1bq2k1/p4r1p/1pp2pp1/3p4/1P1B3Q/P2B1N2/2P3PP/4R1K1 b - - 2 19",
      "r4qk1/6r1/1p4p1/2ppBbN1/1p5Q/P7/2P3PP/5RK1 w - - 2 25",
      "r7/6k1/1p6/2pp1p2/7Q/8/p1P2K1P/8 w - - 0 32",
      "r3k2r/ppp1pp1p/2nqb1pn/3p4/4P3/2PP4/PP1NBPPP/R2QK1NR w KQkq - 1 5",
      "3r1rk1/1pp1pn1p/p1n1q1p1/3p4/Q3P3/2P5/PP1NBPPP/4RRK1 w - - 0 12",
      "5rk1/1pp1pn1p/p3Brp1/8/1n6/5N2/PP3PPP/2R2RK1 w - - 2 20",
      "8/1p2pk1p/p1p1r1p1/3n4/8/5R2/PP3PPP/4R1K1 b - - 3 27",
      "8/4pk2/1p1r2p1/p1p4p/Pn5P/3R4/1P3PP1/4RK2 w - - 1 33",
      "8/5k2/1pnrp1p1/p1p4p/P6P/4R1PK/1P3P2/4R3 b - - 1 38",
      "8/8/1p1kp1p1/p1pr1n1p/P6P/1R4P1/1P3PK1/1R6 b - - 15 45",
      "8/8/1p1k2p1/p1prp2p/P2n3P/6P1/1P1R1PK1/4R3 b - - 5 49",
      "8/8/1p4p1/p1p2k1p/P2npP1P/4K1P1/1P6/3R4 w - - 6 54",
      "8/8/1p4p1/p1p2k1p/P2n1P1P/4K1P1/1P6/6R1 b - - 6 59",
      "8/5k2/1p4p1/p1pK3p/P2n1P1P/6P1/1P6/4R3 b - - 14 63",
      "8/1R6/1p1K1kp1/p6p/P1p2P1P/6P1/1Pn5/8 w - - 0 67",
      "1rb1rn1k/p3q1bp/2p3p1/2p1p3/2P1P2N/PP1RQNP1/1B3P2/4R1K1 b - - 4 23",
      "4rrk1/pp1n1pp1/q5p1/P1pP4/2n3P1/7P/1P3PB1/R1BQ1RK1 w - - 3 22",
      "r2qr1k1/pb1nbppp/1pn1p3/2ppP3/3P4/2PB1NN1/PP3PPP/R1BQR1K1 w - - 4 12",
      "2r2k2/8/4P1R1/1p6/8/P4K1N/7b/2B5 b - - 0 55",
      "6k1/5pp1/8/2bKP2P/2P5/p4PNb/B7/8 b - - 1 44",
      "2rqr1k1/1p3p1p/p2p2p1/P1nPb3/2B1P3/5P2/1PQ2NPP/R1R4K w - - 3 25",
      "r1b2rk1/p1q1ppbp/6p1/2Q5/8/4BP2/PPP3PP/2KR1B1R b - - 2 14",
      "6r1/5k2/p1b1r2p/1pB1p1p1/1Pp3PP/2P1R1K1/2P2P2/3R4 w - - 1 36",
      "rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2",
      "2rr2k1/1p4bp/p1q1p1p1/4Pp1n/2PB4/1PN3P1/P3Q2P/2RR2K1 w - f6 0 20",
      "3br1k1/p1pn3p/1p3n2/5pNq/2P1p3/1PN3PP/P2Q1PB1/4R1K1 w - - 0 23",
      "2r2b2/5p2/5k2/p1r1pP2/P2pB3/1P3P2/K1P3R1/7R w - - 23 93"      
   };

   RootPosition p;
#ifdef WITH_NNUE
   NNUEEvaluator evaluator;
   p.associateEvaluator(evaluator);
#endif

   BetaCutStat betaStats;

   for( const auto & fen : positions){
      readFEN(fen, p);
      analyze(p, depth, true);
      betaStats.update(ThreadPool::instance().main().stats);
   }

   betaStats.show();
   
   return true;
}

bool cliManagement(const std::string & firstArg, int argc, char** argv) {
   
   // first we will look for options that do not need extra parameters

#ifdef WITH_UCI
   if (firstArg == "-uci") {
      UCI::init();
      TimeMan::init();
      UCI::uci();
      return true;
   }
   Logging::LogIt(Logging::logInfo) << "You can use -uci command line option to enter uci mode";
#endif
#ifdef WITH_XBOARD
   if (firstArg == "-xboard") {
      XBoard::init();
      TimeMan::init();
      XBoard::xboard();
      return true;
   }
   Logging::LogIt(Logging::logInfo) << "You can use -xboard command line option to enter xboard mode";
#endif

#ifdef WITH_FMTLIB   
   // let's switch to "pretty mode"
   Pretty::init();
#endif

   const auto args {std::span(argv, size_t(argc))};

   if (firstArg == "-selfplay") {
      DepthType d = 15; // this is "search depth", not genFenDepth !
      if (argc > 2) d = clampDepth(atoi(args[2]));
      int64_t n = 1;
      if (argc > 3) n = atoll(args[3]);
      Logging::LogIt(Logging::logInfo) << "Let's go for " << n << " selfplay games ...";
      const auto startTime = Clock::now();
      uint64_t nbGames = 0;
      uint64_t nbPos = 0;
      while (n-- > 0) {
         if (n % 1000 == 0) Logging::LogIt(Logging::logInfo) << "Remaining games " << n;
         uint64_t nbPosLoc = 0;
         selfPlay(d, nbPosLoc);
         nbPos += nbPosLoc;
         const auto ms = getTimeDiff(startTime);
         ++nbGames;
         Logging::LogIt(Logging::logInfoPrio) << "Nb games : " << nbGames << ", Nb Pos : " << nbPos << ", elapsed s : " << ms/1000;
         Logging::LogIt(Logging::logInfoPrio) << "Game speed : " << static_cast<double>(nbGames) / (static_cast<double>(ms) / 1000 / 60) << " games/min";
         Logging::LogIt(Logging::logInfoPrio) << "Pos speed : " << static_cast<double>(nbPos) / (static_cast<double>(ms) / 1000) << " pos/s";         
      }
      return true;
   }

   if (firstArg == "-perft_test") {
      bool ok = true;
      ok |= perft_test(std::string(startPosition), 5, 4865609);
      ok |= perft_test("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ", 4, 4085603);
      ok |= perft_test("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ", 6, 11030083);
      ok |= perft_test("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5, 15833292);
      return ok;
   }

   if (firstArg == "-perft_test_long_fischer") {
      bool ok = true;
      DynamicConfig::FRC = true;
      std::ifstream infile("Book_and_Test/TestSuite/fischer.txt");
      std::string   line;
      while (std::getline(infile, line)) {
         std::size_t       found = line.find_first_of(",");
         const std::string fen   = line.substr(0, found);
         while (found != std::string::npos) {
            const std::size_t start = found + 1;
            found                   = line.find_first_of(',', found + 1);
            const uint64_t ull      = std::stoull(line.substr(start, found - start));
            ok |= perft_test(fen, 6, ull);
         }
      }
      return ok;
   }

   if (firstArg == "-perft_test_long") {
      bool ok = true;
      std::ifstream infile("Book_and_Test/TestSuite/perft.txt");
      std::string   line;
      while (std::getline(infile, line)) {
         std::size_t       found = line.find_first_of(",");
         const std::string fen   = line.substr(0, found);
         DepthType         i     = 0;
         while (found != std::string::npos) {
            const std::size_t start = found + 1;
            found                   = line.find_first_of(',', found + 1);
            const uint64_t ull      = std::stoull(line.substr(start, found - start));
            ok |= perft_test(fen, ++i, ull);
         }
      }
      return ok;
   }

   if (firstArg == "-see_test") {
      return TestSEE();
   }

   if (firstArg == "bench" || firstArg == "-bench") {
      DepthType d = 16;
      if (argc > 2) d = clampDepth(atoi(args[2]));
      return bench(d);
   }

   if (firstArg == "spsa" || firstArg == "-spsa") {
      return spsaInputs();
   }

   if (firstArg == "benchBig" || firstArg == "-benchBig") {
      DepthType d = 16;
      if (argc > 2) d = clampDepth(atoi(args[2]));
      return benchBig(d);
   }

   if (firstArg == "-evalSpeed") {
      DynamicConfig::disableTT = true;
      std::string filename     = "Book_and_Test/TestSuite/evalSpeed.epd";
      if (argc > 2) filename = args[2];
      std::vector<RootPosition> data;
      Logging::LogIt(Logging::logInfo) << "Running eval speed with file (including NNUE evaluator reset)" << filename;
      std::vector<std::string> positions;
      DISCARD                  readEPDFile(filename, positions);
      for (size_t k = 0; k < positions.size(); ++k) {
         data.emplace_back(positions[k], false);
         if (k % 50000 == 0) Logging::LogIt(Logging::logInfo) << k << " position read";
      }
      Logging::LogIt(Logging::logInfo) << "Data size : " << data.size();

      std::chrono::time_point<Clock> startTime = Clock::now();
      const int loops = 10;
      for (int k = 0; k < loops; ++k)
         for (auto& p : data) {
#ifdef WITH_NNUE
            NNUEEvaluator evaluator;
            p.associateEvaluator(evaluator);
            p.resetNNUEEvaluator(evaluator);
#endif
            EvalData d;
            DISCARD eval(p, d, ThreadPool::instance().main(), true);
         }
      auto ms = getTimeDiff(startTime);
      Logging::LogIt(Logging::logInfo) << "Eval speed (with EG material hash): " << static_cast<double>(data.size()) * loops / (static_cast<double>(ms) * 1000) << " Meval/s";

      startTime = Clock::now();
      for (int k = 0; k < loops; ++k)
         for (auto& p : data) {
#ifdef WITH_NNUE
            NNUEEvaluator evaluator;
            p.associateEvaluator(evaluator);
            p.resetNNUEEvaluator(evaluator);
#endif
            EvalData d;
            DISCARD eval(p, d, ThreadPool::instance().main(), false);
         }
      ms = getTimeDiff(startTime);
      Logging::LogIt(Logging::logInfo) << "Eval speed : " << static_cast<double>(data.size()) * loops / (static_cast<double>(ms) * 1000) << " Meval/s";

      ThreadPool::instance().displayStats();
      return true;
   }

   if (firstArg == "-timeTest") {
      TimeMan::TCType tcType      = TimeMan::TC_suddendeath;
      TimeType        initialTime = 50000;
      TimeType        increment   = 0;
      int             movesInTC   = -1;
      TimeType        guiLag      = 0;
      if (argc > 2) initialTime = atoi(args[2]);
      if (argc > 3) increment   = atoi(args[3]);
      if (argc > 4) movesInTC   = atoi(args[4]);
      if (argc > 5) guiLag      = atoi(args[5]);
      TimeMan::simulate(tcType, initialTime, increment, movesInTC, guiLag);
      return true;
   }

   // in all other cases, args[2] is always the fen string
   if(argc < 3){
      Logging::LogIt(Logging::logError) << "your command is either wrong or needs a fen string";
      return true;
   }
   std::string fen = args[2];

   // some "short cuts" !
   if (fen == "start") fen = startPosition;
   if (fen == "fine70") fen = fine70;
   if (fen == "shirov") fen = shirov;
   if (fen == "shirov2") fen = shirov2;
   if (fen == "mate4") fen = mate4;

   // validate the supposed fen string ...
   const std::regex lookLikeFen(
    "^"
    "([rnbqkpRNBQKP1-8]+\\/){7}([rnbqkpRNBQKP1-8]+)\\s"
    "[bw]\\s"
    "((-|(K|[A-H])?(Q|[A-H])?(k|[a-h])?(q|[a-h])?)\\s)"
    "((-|[a-h][36])\\s)"
    "((\\d+)\\s(\\d+)\\s?|(\\d+)\\s?)?"
    "$"
   );
   if( ! std::regex_match(fen, lookLikeFen)){
      Logging::LogIt(Logging::logError) << "fen does not look good... : " << fen;
      return true;
   }

   // instantiate the position
   RootPosition p;
#ifdef WITH_NNUE
   NNUEEvaluator evaluator;
   p.associateEvaluator(evaluator);
#endif
   if (!readFEN(fen, p, false, true)) {
      Logging::LogIt(Logging::logError) << "when reading fen";
      return true;
   }

   Logging::LogIt(Logging::logInfo) << ToString(p);

   if (firstArg == "-qsearch") {
      DepthType seldepth = 0;
      ScoreType s        = ThreadPool::instance().main().qsearchNoPruning(-10000, 10000, p, 1, seldepth);
      Logging::LogIt(Logging::logInfo) << "QScore " << s;
      return true;
   }

#ifdef WITH_SYZYGY
   if (firstArg == "-probe"){
      Logging::LogIt(Logging::logInfo) << "Probing TB";
      if ((BB::countBit(p.allPieces[Co_White] | p.allPieces[Co_Black])) <= SyzygyTb::MAX_TB_MEN) {
         ScoreType tbScore = 0;
         Logging::LogIt(Logging::logInfo) << "Probing root";
         if (MoveList movesTB; SyzygyTb::probe_root(ThreadPool::instance().main(), p, tbScore, movesTB) >= 0) { // only good moves if TB success
            for (auto m : movesTB ){
               Logging::LogIt(Logging::logInfo) << "TB move " << ToString(m);
            }
            Logging::LogIt(Logging::logInfo) << "Score " << tbScore;
         }
         else {
            Logging::LogIt(Logging::logInfo) << "TB failed";
         }
         Logging::LogIt(Logging::logInfo) << "Probing wdl";
         if (SyzygyTb::probe_wdl(p, tbScore, false) > 0) {
            Logging::LogIt(Logging::logInfo) << "Score " << tbScore;
         }
         else {
            Logging::LogIt(Logging::logInfo) << "TB failed";
         }
      }
      else {
         Logging::LogIt(Logging::logInfo) << "No TB hit";
      }
      return true;
   }
#endif

   if ( firstArg == "-kpk"){
      Logging::LogIt(Logging::logInfo) << "Probing KPK";
      const Color winningSide = p.pieces_const<P_wp>(Co_White) == emptyBitBoard ? Co_Black : Co_White;
      std::cout << "KPK score : " << MaterialHash::helperKPK(p, winningSide, 0, 1) << std::endl;
      return true;
   }

   if (firstArg == "-see") {
      Square from  = INVALIDSQUARE;
      Square to    = INVALIDSQUARE;
      MType  mtype = T_std;
      const std::string move  = argc > 3 ? args[3] : "e2e4";
      readMove(p, move, from, to, mtype);
      Move      m = ToMove(from, to, mtype);
      ScoreType t = clampInt<ScoreType>(atoi(args[4]));
      bool      b = Searcher::SEE_GE(p, m, t);
      Logging::LogIt(Logging::logInfo) << "SEE ? " << (b ? "ok" : "not");
      return true;
   }

   if (firstArg == "-attacked") {
      Square k = Sq_e4;
      if (argc > 3) k = clampIntU<Square>(atoi(args[3]));
      Logging::LogIt(Logging::logInfo) << SquareNames[k];
      Logging::LogIt(Logging::logInfo) << ToString(BBTools::allAttackedBB(p, k, p.c));
      return true;
   }

   if (firstArg == "-cov") {
      Square k = Sq_e4;
      if (argc > 3) k = clampIntU<Square>(atoi(args[3]));
      switch (p.board_const(k)) {
         case P_wp: Logging::LogIt(Logging::logInfo) << ToString((BBTools::coverage<P_wp>(k, p.occupancy(), p.c) + BBTools::mask[k].push[p.c]) &~p.allPieces[Co_White]); break;
         case P_wn: Logging::LogIt(Logging::logInfo) << ToString(BBTools::coverage<P_wn>(k, p.occupancy(), p.c) & ~p.allPieces[Co_White]); break;
         case P_wb: Logging::LogIt(Logging::logInfo) << ToString(BBTools::coverage<P_wb>(k, p.occupancy(), p.c) & ~p.allPieces[Co_White]); break;
         case P_wr: Logging::LogIt(Logging::logInfo) << ToString(BBTools::coverage<P_wr>(k, p.occupancy(), p.c) & ~p.allPieces[Co_White]); break;
         case P_wq: Logging::LogIt(Logging::logInfo) << ToString(BBTools::coverage<P_wq>(k, p.occupancy(), p.c) & ~p.allPieces[Co_White]); break;
         case P_wk: Logging::LogIt(Logging::logInfo) << ToString(BBTools::coverage<P_wk>(k, p.occupancy(), p.c) & ~p.allPieces[Co_White]); break;
         case P_bk: Logging::LogIt(Logging::logInfo) << ToString(BBTools::coverage<P_wk>(k, p.occupancy(), p.c) & ~p.allPieces[Co_Black]); break;
         case P_bq: Logging::LogIt(Logging::logInfo) << ToString(BBTools::coverage<P_wq>(k, p.occupancy(), p.c) & ~p.allPieces[Co_Black]); break;
         case P_br: Logging::LogIt(Logging::logInfo) << ToString(BBTools::coverage<P_wr>(k, p.occupancy(), p.c) & ~p.allPieces[Co_Black]); break;
         case P_bb: Logging::LogIt(Logging::logInfo) << ToString(BBTools::coverage<P_wb>(k, p.occupancy(), p.c) & ~p.allPieces[Co_Black]); break;
         case P_bn: Logging::LogIt(Logging::logInfo) << ToString(BBTools::coverage<P_wn>(k, p.occupancy(), p.c) & ~p.allPieces[Co_Black]); break;
         case P_bp:
            Logging::LogIt(Logging::logInfo) << ToString((BBTools::coverage<P_wp>(k, p.occupancy(), p.c) + BBTools::mask[k].push[p.c]) &
                                                         ~p.allPieces[Co_Black]);
            break;
         default: Logging::LogIt(Logging::logInfo) << ToString(emptyBitBoard);
      }
      return true;
   }

   if (firstArg == "-eval") {
      EvalData data;
      if (DynamicConfig::useNNUE) DynamicConfig::forceNNUE = true;
      const ScoreType score = eval(p, data, ThreadPool::instance().main(), true, true);
      Logging::LogIt(Logging::logInfo) << "eval " << score << " phase " << data.gp;
      return true;
   }

   if (firstArg == "-evalHCE") {
      EvalData data;
      if (DynamicConfig::useNNUE) DynamicConfig::useNNUE = false;
      const ScoreType score = eval(p, data, ThreadPool::instance().main(), true, true);
      Logging::LogIt(Logging::logInfo) << "eval " << score << " phase " << data.gp;
      return true;
   }

   if (firstArg == "-gen") {
      MoveList moves;
      MoveGen::generate<MoveGen::GP_all>(p, moves);
      CMHPtrArray cmhPtr = {nullptr};
      MoveSorter::scoreAndSort(ThreadPool::instance().main(), moves, p, 0.f, 0, cmhPtr);
      Logging::LogIt(Logging::logInfo) << "nb moves : " << moves.size();
      for (const auto & it : moves) { Logging::LogIt(Logging::logInfo) << ToString(it, true); }
      return true;
   }

   if (firstArg == "-testmove") {
      ///@todo
      constexpr Move m  = ToMove(8, 16, T_std);
      Position p2 = p;
#if defined(WITH_NNUE) && defined(DEBUG_NNUE_UPDATE)
      p2.associateEvaluator(evaluator);
      p2.resetNNUEEvaluator(p2.evaluator());
#endif
      if (const MoveInfo moveInfo(p2, m); !applyMove(p2, moveInfo)) { 
         Logging::LogIt(Logging::logError) << "Cannot apply this move";
      }
      Logging::LogIt(Logging::logInfo) << ToString(p2);
      return true;
   }

   if (firstArg == "-perft") {
      DepthType d = 5;
      if (argc > 3) d = clampIntU<DepthType>(atoi(args[3]));
      PerftAccumulator acc;
      auto start = Clock::now();
      perft(p, d, acc);
      auto elapsed = getTimeDiff(start);
      Logging::LogIt(Logging::logInfo) << "Perft done in " << elapsed << "ms";
      acc.Display();
      Logging::LogIt(Logging::logInfo) << "Speed " << static_cast<int>(acc.validNodes / elapsed) << "KNPS";
      return true;
   }

   if (firstArg == "-analyze") {
      DepthType depth = 15;
      if (argc > 3) depth = clampIntU<DepthType>(atoi(args[3]));
      auto start = Clock::now();
      analyze(p, depth);
      auto elapsed = getTimeDiff(start);
      Logging::LogIt(Logging::logInfo) << "Analysis done in " << elapsed << "ms";
      return true;
   }

   if (firstArg == "-mateFinder") {
      DynamicConfig::mateFinder = true;
      DepthType depth = 10;
      if (argc > 3) depth = clampIntU<DepthType>(atoi(args[3]));
      auto start = Clock::now();
      analyze(p, depth);
      auto elapsed = getTimeDiff(start);
      Logging::LogIt(Logging::logInfo) << "Analysis done in " << elapsed << "ms";
      return true;
   }

   Logging::LogIt(Logging::logError) << "unknown command line : " << firstArg;
   return false;
}
