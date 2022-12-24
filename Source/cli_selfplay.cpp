#include "definition.hpp"
#include "dynamicConfig.hpp"
#include "position.hpp"
#include "positionTools.hpp"
#include "searcher.hpp"
#include "threading.hpp"

// see cli.cpp
void analyze(const Position& p, DepthType depth, bool openBenchOutput = false);

void selfPlay(DepthType depth, uint64_t & nbPos) {
   //DynamicConfig::genFen = true; // this can be forced here but shall be set by CLI option in fact.

   assert(!DynamicConfig::armageddon);
   assert(!DynamicConfig::antichess);

   const std::string startfen = 
#if !defined(WITH_SMALL_MEMORY)
                                  DynamicConfig::DFRC ? chess960::getDFRCXFEN() : 
                                  DynamicConfig::FRC ? chess960::positions[std::rand() % 960] : 
#endif
                                  startPosition;
   RootPosition      p(startfen);
#ifdef WITH_NNUE
   NNUEEvaluator evaluator;
   p.associateEvaluator(evaluator);
   p.resetNNUEEvaluator(p.evaluator()); // this is needed as the RootPosition CTOR with a fen hasn't fill the evaluator yet ///@todo a CTOR with evaluator given ...
#endif

   if (DynamicConfig::pgnOut && !ThreadPool::instance().main().pgnStream.is_open()) {
      ThreadPool::instance().main().pgnStream.open("selfplay_" + std::to_string(GETPID()) + "_" + std::to_string(0) + ".pgn", std::ofstream::app);
   }

#ifdef WITH_GENFILE
   if (DynamicConfig::genFen && !ThreadPool::instance().main().genStream.is_open()) {
      ThreadPool::instance().main().genStream.open("genfen_" + std::to_string(GETPID()) + "_" + std::to_string(0) + ".epd", std::ofstream::app);
   }
#endif
   Position  p2           = p;
   bool      ended        = false;
   bool      justBegin    = true;
   int       result       = 0;
   int       drawCount    = 0;
   int       winCount     = 0;
   const int minAdjMove   = 40;
   const int minAdjCount  = 10;
   const int minDrawScore = 8;
   const int minWinScore  = 800;

   nbPos = 0;

   while (true) {
      ThreadPool::instance().main().subSearch = true;
      const uint64_t maxNodes = TimeMan::maxNodes;
      TimeMan::maxNodes = 0;
      analyze(p2, depth); // search using a specific depth
      ThreadPool::instance().main().subSearch = false;
      ThreadData d = ThreadPool::instance().main().getData();
      TimeMan::maxNodes = maxNodes;

      if (justBegin){
         if (DynamicConfig::pgnOut){
            ThreadPool::instance().main().pgnStream << "[Event \"Minic self play\"]\n";
            ThreadPool::instance().main().pgnStream << "[FEN \"" + GetFEN(p) + "\"]\n";
         }
         justBegin = false;
      }

      if (std::abs(d.score) < minDrawScore) ++drawCount;
      else
         drawCount = 0;

      if (std::abs(d.score) > minWinScore) ++winCount;
      else
         winCount = 0;

      if (p2.halfmoves > minAdjMove && winCount > minAdjCount) {
         Logging::LogIt(Logging::logInfoPrio) << "End of game (adjudication win) " << GetFEN(p2);
         ended  = true;
         result = (d.score * (p2.c == Co_White ? 1 : -1)) > 0 ? 1 : -1;
      }
      else if (p2.halfmoves > minAdjMove && drawCount > minAdjCount) {
         Logging::LogIt(Logging::logInfoPrio) << "End of game (adjudication draw) " << GetFEN(p2);
         ended  = true;
         result = 0;
      }
      else if (d.best == INVALIDMOVE) {
         Logging::LogIt(Logging::logInfoPrio) << "End of game " << GetFEN(p2);
         ended = true;
         ///@todo this is only working in classic chess (no armageddon or antichess)
         if (isPosInCheck(p2)) result = p2.c == Co_Black ? 1 : -1; // checkmated (cannot move and attaked)
         else
            result = 0; // pat (cannot move)
      }
      else if (p2.halfmoves > MAX_PLY / 4) {
         Logging::LogIt(Logging::logInfoPrio) << "Too long game " << GetFEN(p2);
         ended  = true;
         result = 0; // draw
      }

      if (ended){
         if (DynamicConfig::pgnOut) ThreadPool::instance().main().pgnStream << (result == 0 ? "1/2-1/2" : result > 0 ? "1-0" : "0-1") << "\n";
         justBegin = true;
      }
      else {
         if (DynamicConfig::pgnOut) ThreadPool::instance().main().pgnStream << (p2.halfmoves%2?(std::to_string(p2.moves)+". ") : "") << showAlgAbr(d.best,p2) << " ";
         ++nbPos;
      }

#ifdef WITH_GENFILE
      const bool getQuietPos = true;
      if (DynamicConfig::genFen) {
         // writeToGenFile using genFenDepth from this root position
         if (!ended) ThreadPool::instance().main().writeToGenFile(p2, getQuietPos, d, {}); // bufferized
         else {
            ThreadPool::instance().main().writeToGenFile(p2, getQuietPos, d, result); // write to file using result
         }
      }
#else
      DISCARD ended;
      DISCARD result;
#endif

      if (ended) break;

      // update position using best move
      Position p3 = p2;
      if (const Position::MoveInfo moveInfo(p3, d.best); !applyMove(p3, moveInfo)) break;
      p2 = p3;
   }
}