#include "logging.hpp"
#include "searcher.hpp"

ScoreType Searcher::SEE(const Position& p, const Move& m) {
   if (!isValidMove(m)) return 0;

   START_TIMER

   const MType mtype = Move2Type(m);
   assert(isValidMoveType(mtype));
   Square from = Move2From(m);
   assert(isValidSquare(from));
   const Square to = correctedMove2ToKingDest(m);
   assert(isValidSquare(to));

   BitBoard   attackers        = BBTools::allAttackedBB(p, to);
   BitBoard   occupationMask   = 0xFFFFFFFFFFFFFFFF;
   ScoreType  currentTargetVal = 0;
   const bool promPossible     = PROMOTION_RANK(to);
   Color c = p.c;

   int nCapt = 0;
   array1d<int,64> swapList; // max 64 caps ... shall be ok (1B2R2B/2BnRnB1/2nBRBn1/RRRRBrrr/2NbrbN1/2bNrNb1/1b2r2b/b3r3 w - - 0 1)

   Piece pp = PieceTools::getPieceType(p, from);
   if (mtype == T_ep) {
      swapList[nCapt]  = valueSEE(P_wp);
      currentTargetVal = valueSEE(pp);
      occupationMask &= ~SquareToBitboard(p.ep);
   }
   else {
      const Piece ppTo = PieceTools::getPieceType(p, to);
      swapList[nCapt] = valueSEE(ppTo);
      if (promPossible && pp == P_wp) {
         swapList[nCapt] += valueSEE(promShift(mtype)) - valueSEE(P_wp);
         currentTargetVal = valueSEE(promShift(mtype));
      }
      else
         currentTargetVal = valueSEE(pp);
   }
   ++nCapt;
   assert(nCapt < 64);

   attackers &= ~SquareToBitboard(from);
   occupationMask &= ~SquareToBitboard(from);
   const BitBoard occupancy = p.occupancy();

   attackers |= BBTools::attack<P_wr>(to, p.allQueen() | p.allRook(), occupancy & occupationMask, c) |
                BBTools::attack<P_wb>(to, p.allQueen() | p.allBishop(), occupancy & occupationMask, c);
   attackers &= occupationMask;
   c = ~c;

   while (attackers) {
      if (!promPossible && (attackers & p.pieces_const<P_wp>(c)))
         from = BBTools::SquareFromBitBoard(attackers & p.pieces_const<P_wp>(c)), pp = P_wp;
      else if (attackers & p.pieces_const<P_wn>(c))
         from = BBTools::SquareFromBitBoard(attackers & p.pieces_const<P_wn>(c)), pp = P_wn;
      else if (attackers & p.pieces_const<P_wb>(c))
         from = BBTools::SquareFromBitBoard(attackers & p.pieces_const<P_wb>(c)), pp = P_wb;
      else if (attackers & p.pieces_const<P_wr>(c))
         from = BBTools::SquareFromBitBoard(attackers & p.pieces_const<P_wr>(c)), pp = P_wr;
      else if (promPossible && (attackers & p.pieces_const<P_wp>(c)))
         from = BBTools::SquareFromBitBoard(attackers & p.pieces_const<P_wp>(c)), pp = P_wp;
      else if (attackers & p.pieces_const<P_wq>(c))
         from = BBTools::SquareFromBitBoard(attackers & p.pieces_const<P_wq>(c)), pp = P_wq;
      else if ((attackers & p.pieces_const<P_wk>(c)) && !(attackers & p.allPieces[~c]))
         from = BBTools::SquareFromBitBoard(attackers & p.pieces_const<P_wk>(c)), pp = P_wk;
      else
         break;

      swapList[nCapt] = -swapList[nCapt - 1] + currentTargetVal;
      // pruning here does not influence the result
      //if (std::max(-swapList[nCapt-1], swapList[nCapt]) < 0) break; ///@todo ??
      if (promPossible && pp == P_wp) {
         swapList[nCapt] += valueSEE(P_wq) - valueSEE(P_wp);
         currentTargetVal = valueSEE(P_wq);
      }
      else
         currentTargetVal = valueSEE(pp);

      ++nCapt;
      attackers &= ~SquareToBitboard(from);
      occupationMask &= ~SquareToBitboard(from);

      attackers |= BBTools::attack<P_wr>(to, p.allQueen() | p.allRook(), occupancy & occupationMask, c) |
                   BBTools::attack<P_wb>(to, p.allQueen() | p.allBishop(), occupancy & occupationMask, c);
      attackers &= occupationMask;

      c = ~c;
   }

   while (--nCapt)
      if (swapList[nCapt] > -swapList[nCapt - 1]) swapList[nCapt - 1] = -swapList[nCapt];
   STOP_AND_SUM_TIMER(See)
   ///@todo assert fit in ScoreType
   return swapList[0];
}

bool Searcher::SEE_GE(const Position& p, const Move& m, ScoreType threshold) { return SEE(p, m) >= threshold; }
