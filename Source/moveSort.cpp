#include "moveSort.hpp"

#include "logging.hpp"
#include "searcher.hpp"

/* Moves are sorted this way
 * 1°) previous best (from current thread previousBest)
 * 2°) TT move
 * --3°) king evasion is in check and king moves--
 * 4°) prom cap
 * 5°) good capture (not prom) based on SEE if not in check, based on MVV LVA if in check
 * 6°) prom
 * 7°) killer 0, then killer 1, then killer 0 from previous move, then counter
 * 8°) other quiet based on various history score (from/to, piece/to, CMH)
 * 9°) bad cap
 */

template<Color C> void MoveSorter::computeScore(Move& m) const {
   assert(isValidMove(m));
   if (Move2Score(m) != 0) return; // prob cut already computed captures score
   const MType t = Move2Type(m);
   assert(isValidMoveType(t));
   const Square from = Move2From(m);
   assert(isValidSquare(from));
   const Square to = Move2To(m);
   assert(isValidSquare(to));

   // apply default score based on type
   ScoreType s = MoveScoring[t];

   // previous root best at root
   if (height == 0 && sameMove(context.previousBest, m)) s += 15000; 
   // TT move
   else if (e && sameMove(e->m, m)) s += 15000;

   else {
      // king evasion ///@todo try again ??
      //if (isInCheck && from == p.king[C]) s += 10000; 
      
      // capture that are not promotion
      if (isCapture(t) && !isPromotion(t)) {
         const Piece pp    = PieceTools::getPieceType(p, from);
         const Piece ppOpp = (t != T_ep) ? PieceTools::getPieceType(p, to) : P_wp;
         assert(pp > 0);
         assert(ppOpp > 0);

         const EvalScore* const pst    = EvalConfig::PST[pp - 1];
         const EvalScore* const pstOpp = EvalConfig::PST[ppOpp - 1];
         // always use PST as a hint
         s += (ScaleScore(pst[ColorSquarePstHelper<C>(to)] - pst[ColorSquarePstHelper<C>(from)] + pstOpp[ColorSquarePstHelper<~C>(to)], gp))/2;
         
         if (useSEE && !isInCheck) { 
            const ScoreType see = context.SEE(p, m);
            s += see;
            // recapture bonus
            if (isValidMove(p.lastMove) && isCapture(p.lastMove) && to == correctedMove2To(p.lastMove)) s += 512;
            // too bad capture are ordered last (-7000)
            if (see < -80) s -= 2 * MoveScoring[T_capture];
         }
         else { // without SEE
            // recapture (> max MVVLVA value)
            if (isValidMove(p.lastMove) && isCapture(p.lastMove) && to == correctedMove2To(p.lastMove)) s += 512; 
            // MVVLVA [0 400] + cap history (HISTORY_MAX = 1024)
            s += (8 * SearchConfig::MvvLvaScores[ppOpp - 1][pp - 1] + context.historyT.historyCap[PieceIdx(p.board_const(from))][to][ppOpp-1]) / 4;
         }
      }

      // standard moves and castling
      else if (t == T_std || isCastling(m)) {
         if (sameMove(m, context.killerT.killers[height][0])) s += 1900; // quiet killer
         else if (sameMove(m, context.killerT.killers[height][1]))
            s += 1700; // quiet killer
         else if (height > 1 && sameMove(m, context.killerT.killers[height - 2][0]))
            s += 1500; // quiet killer
         else if (isValidMove(p.lastMove) && sameMove(context.counterT.counter[Move2From(p.lastMove)][correctedMove2To(p.lastMove)], m))
            s += 1300; // quiet counter
         else {
            ///@todo give another try to tune those ratio!
            // History
            const Piece pp = p.board_const(from);
            s += context.historyT.history[p.c][from][to] / 3;     // +/- HISTORY_MAX = 1024
            s += context.historyT.historyP[PieceIdx(pp)][to] / 3; // +/- HISTORY_MAX = 1024
            s += context.getCMHScore(p, from, to, cmhPtr) / 3;    // +/- HISTORY_MAX = 1024
            if (!isInCheck) {
               // move (safely) leaving threat square from null move search
               if (refutation != INVALIDMINIMOVE && from == correctedMove2To(refutation) && context.SEE_GE(p, m, -80)) s += 512; 
               // always use PST to compensate low value history
               const EvalScore* const pst = EvalConfig::PST[std::abs(pp) - 1];
               s += (ScaleScore(pst[ColorSquarePstHelper<C>(to)] - pst[ColorSquarePstHelper<C>(from)], gp))/2;
            }
         }
      }
   }
   m = ToMove(from, to, t, s);
}

void MoveSorter::score(const Searcher&    context,
                       MoveList&          moves,
                       const Position&    p,
                       float              gp,
                       DepthType          height,
                       const CMHPtrArray& cmhPtr,
                       bool               useSEE,
                       bool               isInCheck,
                       const TT::Entry*   e,
                       const MiniMove     refutation) {
   START_TIMER
   if (moves.size() < 2) return;
   const MoveSorter ms(context, p, gp, height, cmhPtr, useSEE, isInCheck, e, refutation);
   if (p.c == Co_White) {
      for (auto it = moves.begin(); it != moves.end(); ++it) { ms.computeScore<Co_White>(*it); }
   }
   else {
      for (auto it = moves.begin(); it != moves.end(); ++it) { ms.computeScore<Co_Black>(*it); }
   }
   STOP_AND_SUM_TIMER(MoveScoring)
}

void MoveSorter::scoreAndSort(const Searcher&    context,
                              MoveList&          moves,
                              const Position&    p,
                              float              gp,
                              DepthType          height,
                              const CMHPtrArray& cmhPtr,
                              bool               useSEE,
                              bool               isInCheck,
                              const TT::Entry*   e,
                              const MiniMove     refutation) {
   score(context, moves, p, gp, height, cmhPtr, useSEE, isInCheck, e, refutation);
   sort(moves);
}

void MoveSorter::sort(MoveList& moves) {
   START_TIMER
   std::sort(moves.begin(), moves.end(), MoveSortOperator());
   STOP_AND_SUM_TIMER(MoveSorting)
}

const Move* MoveSorter::pickNext(MoveList& moves, size_t& begin) {
   if (moves.begin() + begin == moves.end()) return nullptr;
   START_TIMER
   auto it = std::min_element(moves.begin() + begin, moves.end(), MoveSortOperator());
   std::iter_swap(moves.begin() + begin, it);
   STOP_AND_SUM_TIMER(MoveSorting)
   return &*(moves.begin() + (begin++)); // increment begin !
}
