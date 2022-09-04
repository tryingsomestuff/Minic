#include "definition.hpp"

#include "logging.hpp"
#include "moveGen.hpp"
#include "position.hpp"
#include "positionTools.hpp"

void PerftAccumulator::Display() {
   Logging::LogIt(Logging::logInfo) << "pseudoNodes   " << pseudoNodes;
   Logging::LogIt(Logging::logInfo) << "validNodes    " << validNodes;
   Logging::LogIt(Logging::logInfo) << "captureNodes  " << captureNodes;
   Logging::LogIt(Logging::logInfo) << "epNodes       " << epNodes;
   Logging::LogIt(Logging::logInfo) << "castling      " << castling;
   Logging::LogIt(Logging::logInfo) << "promotion     " << promotion;
   Logging::LogIt(Logging::logInfo) << "checkNode     " << checkNode;
   Logging::LogIt(Logging::logInfo) << "checkMateNode " << checkMateNode;
}

Counter perft(const Position& p, DepthType depth, PerftAccumulator& acc) {
   if (depth == 0) return 0;
   MoveList         moves;
   PerftAccumulator accLoc;
#ifndef DEBUG_PERFT
   if (isPosInCheck(p)) MoveGen::generate<MoveGen::GP_evasion>(p, moves);
   else
      MoveGen::generate<MoveGen::GP_all>(p, moves);
   const size_t n = moves.size();
   for (size_t k = 0; k < n; ++k) {
      const Move& m = moves[k];
#else
   for (MiniMove m = std::numeric_limits<MiniMove>::min(); m < std::numeric_limits<MiniMove>::max(); ++m) {
      if (!isPseudoLegal(p, m)) continue;
#endif
      ++accLoc.pseudoNodes;
      Position p2 = p;
#if defined(WITH_NNUE) && defined(DEBUG_NNUE_UPDATE)
      NNUEEvaluator evaluator;
      p2.associateEvaluator(evaluator);
      p2.resetNNUEEvaluator(p2.evaluator());
#endif
      const Position::MoveInfo moveInfo(p2, m);
      if (!applyMove(p2, moveInfo)) continue;
      ++accLoc.validNodes;
      MType t = Move2Type(m);
      if (t == T_ep) ++accLoc.epNodes;
      if (isCapture(t)) ++accLoc.captureNodes;
      if (isCastling(t)) ++accLoc.castling;
      if (isPromotion(t)) ++accLoc.promotion;
      perft(p2, depth - 1, acc);
   }
   if (depth == 1) acc += accLoc;
   return acc.validNodes;
}

void perft_test(const std::string& fen, DepthType d, uint64_t expected) {
   RootPosition p;
#ifdef WITH_NNUE
   NNUEEvaluator evaluator;
   p.associateEvaluator(evaluator);
#endif
   readFEN(fen, p);
   Logging::LogIt(Logging::logInfo) << ToString(p);
   PerftAccumulator acc;
   uint64_t         n = perft(p, d, acc);
   acc.Display();
   if (n != expected) Logging::LogIt(Logging::logFatal) << "Error !! " << fen << " " << expected;
   Logging::LogIt(Logging::logInfo) << "#########################";
}