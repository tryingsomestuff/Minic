#include "moveGen.hpp"

#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "material.hpp"
#include "moveApply.hpp"
#include "positionTools.hpp"
#include "searcher.hpp"
#include "tools.hpp"

ScoreType randomMover(const Position& p, PVList& pv, const bool isInCheck) {
   MoveList moves;
   MoveGen::generate<MoveGen::GP_all>(p, moves, false);
   if (moves.empty()) return isInCheck ? matedScore(0) : 0;
   ///@todo this is slow because the static here implies a guard variable !!
   static std::random_device rd;
   static std::mt19937       g(rd()); // here really random
   std::ranges::shuffle(moves, g);
   for (const auto & it : moves) {
      Position p2 = p;
#if defined(WITH_NNUE) && defined(DEBUG_NNUE_UPDATE)
      NNUEEvaluator evaluator = p.evaluator();
      p2.associateEvaluator(evaluator);
#endif
      if (const MoveInfo moveInfo(p2, it); !applyMove(p2, moveInfo)) continue;
      pv.emplace_back(it); // updatePV
      const Square to = Move2To(it);
      // king capture (works only for most standard chess variants)
      if (p.c == Co_White && to == p.king[Co_Black]) return matingScore(0);
      if (p.c == Co_Black && to == p.king[Co_White]) return matingScore(0);
      return 0; // found one valid move
   }
   // no move is valid ...
   return isInCheck ? matedScore(0) : 0;
}
