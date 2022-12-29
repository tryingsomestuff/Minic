#include "cli.hpp"

#include "com.hpp"
#include "dynamicConfig.hpp"
#include "evalDef.hpp"
#include "logging.hpp"
#include "material.hpp"
#include "moveGen.hpp"
#include "position.hpp"
#include "searcher.hpp"
#include "tables.hpp"
#include "timeMan.hpp"
#include "tools.hpp"
#include "transposition.hpp"
#include "uci.hpp"
#include "xboard.hpp"

// see cli_perft.cpp
void perft_test(const std::string& fen, DepthType d, uint64_t expected);
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

bool bench(DepthType depth) {
   RootPosition p;
#ifdef WITH_NNUE
   NNUEEvaluator evaluator;
   p.associateEvaluator(evaluator);
#endif
   readFEN(startPosition, p);
   analyze(p, depth, true);
   readFEN(fine70, p);
   analyze(p, depth, true);
   readFEN(shirov, p);
   analyze(p, depth, true);
   return 0;
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
         Logging::LogIt(Logging::logInfo) << "Nb games : " << nbGames << ", Nb Pos : " << nbPos << ", elapsed s : " << ms/1000;
         Logging::LogIt(Logging::logInfo) << "Game speed : " << static_cast<double>(nbGames) / (static_cast<double>(ms) / 1000 / 60) << " games/min";
         Logging::LogIt(Logging::logInfo) << "Pos speed : " << static_cast<double>(nbPos) / (static_cast<double>(ms) / 1000) << " pos/s";         
      }
      return true;
   }

   if (firstArg == "-perft_test") {
      perft_test(startPosition, 5, 4865609);
      perft_test("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ", 4, 4085603);
      perft_test("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ", 6, 11030083);
      perft_test("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5, 15833292);
      return true;
   }

   if (firstArg == "-perft_test_long_fischer") {
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
            perft_test(fen, 6, ull);
         }
      }
      return true;
   }

   if (firstArg == "-perft_test_long") {
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
            perft_test(fen, ++i, ull);
         }
      }
      return true;
   }

   if (firstArg == "-see_test") {
      return TestSEE();
   }

   if (firstArg == "bench" || firstArg == "-bench") {
      DepthType d = 16;
      if (argc > 2) d = clampDepth(atoi(args[2]));
      return bench(d);
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
   std::string fen = args[2];

   // some "short cuts" !
   if (fen == "start") fen = startPosition;
   if (fen == "fine70") fen = fine70;
   if (fen == "shirov") fen = shirov;

   // instantiate the position
   RootPosition p;
#ifdef WITH_NNUE
   NNUEEvaluator evaluator;
   p.associateEvaluator(evaluator);
#endif
   if (!readFEN(fen, p, false, true)) {
      Logging::LogIt(Logging::logInfo) << "Error reading fen";
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
      if (const Position::MoveInfo moveInfo(p2, m); !applyMove(p2, moveInfo)) { 
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

   Logging::LogIt(Logging::logInfo) << "Error : unknown command line";
   return false;
}
