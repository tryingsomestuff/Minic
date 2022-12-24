#pragma once

#include "definition.hpp"
#include "position.hpp"
#include "transposition.hpp"

/*!
 * This file defines classic tables management such as :
 *  - Killers
 *  - History
 *     - Color/from/to
 *     - Piece/to
 *     - CMH history : previous moved piece/previous to/current moved piece & to
 *  - Counter : from/to
 */

inline constexpr int MAX_CMH_PLY = 1;
using CMHPtrArray = array1d<array1d<ScoreType, NbPiece*NbSquare> *, MAX_CMH_PLY>;

struct KillerT {
   array2d<Move,MAX_DEPTH,2> killers;

   void initKillers();
   void update(Move m, DepthType height);
   [[nodiscard]] bool isKiller(const Move m, const DepthType height);
};

struct HistoryT {
   array3d<ScoreType,2,NbSquare,NbSquare> history;                       // Color, from, to
   array3d<ScoreType,NbPiece,NbSquare,PieceShift> historyCap;            // Piece moved (+color), to, piece taken
   array2d<ScoreType,NbPiece,NbSquare> historyP;                         // Piece, to
   array3d<ScoreType,NbPiece,NbSquare,NbPiece*NbSquare> counter_history; // Previous moved piece, previous to, current moved piece * boardsize + current to

   void initHistory();

   template<int S> FORCE_FINLINE void update(DepthType depth, Move m, const Position& p, CMHPtrArray& cmhPtr) {
      if (Move2Type(m) == T_std) {
         const Color  c    = p.c;
         const Square from = Move2From(m);
         assert(isValidSquare(from));
         const Square to = Move2To(m); // no need for correctedMove2ToKingDest here (std)
         assert(isValidSquare(to));
         const ScoreType s  = S * HSCORE(depth);
         const Piece     pp = p.board_const(from);
         history[c][from][to] += static_cast<ScoreType>(s - HISTORY_DIV(history[c][from][to] * std::abs(s)));
         historyP[PieceIdx(pp)][to] += static_cast<ScoreType>(s - HISTORY_DIV(historyP[PieceIdx(pp)][to] * std::abs(s)));
         for (int i = 0; i < MAX_CMH_PLY; ++i) {
            if (cmhPtr[i]) {
               ScoreType& item = (*cmhPtr[i])[PieceIdx(p.board_const(from)) * NbSquare + to];
               item += static_cast<ScoreType>(s - HISTORY_DIV(item * std::abs(s)));
            }
         }
      }
   }

   template<int S> FORCE_FINLINE void updateCap(DepthType depth, Move m, const Position& p) {
      if (isCaptureNoEP(m)) {
         const Square from = Move2From(m);
         assert(isValidSquare(from));
         const Square to = Move2To(m); // no need for correctedMove2ToKingDest here (std capture)
         assert(isValidSquare(to));
         const Piece pf = p.board_const(from);
         const Piece pt = p.board_const(to); // std capture, no ep
         const ScoreType s  = S * HSCORE(depth);
         historyCap[PieceIdx(pf)][to][Abs(pt)-1] += static_cast<ScoreType>(s - HISTORY_DIV(historyCap[PieceIdx(pf)][to][Abs(pt)-1] * std::abs(s)));
      }
   }
};

struct CounterT {
   array2d<ScoreType,NbSquare,NbSquare> counter;

   void initCounter();
   void update(Move m, const Position& p);
};

void updateTables(Searcher& context, const Position& p, DepthType depth, DepthType height, const Move m, TT::Bound bound, CMHPtrArray& cmhPtr);
