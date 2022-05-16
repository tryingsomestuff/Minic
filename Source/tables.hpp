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
typedef std::array<ScoreType*, MAX_CMH_PLY> CMHPtrArray;

struct KillerT {
   Move killers[MAX_DEPTH][2];

   void initKillers();
   void update(Move m, DepthType ply);
   [[nodiscard]] bool isKiller(const Move m, const DepthType height);
};

struct HistoryT {
   ScoreType history[2][NbSquare][NbSquare];                         // Color, from, to
   ScoreType historyCap[NbPiece][NbSquare][PieceShift];              // Piece moved (+color), to, piece taken
   ScoreType historyP[NbPiece][NbSquare];                            // Piece, to
   ScoreType counter_history[NbPiece][NbSquare][NbPiece * NbSquare]; // Previous moved piece, previous to, current moved piece * boardsize + current to

   void initHistory();

   template<int S> inline void update(DepthType depth, Move m, const Position& p, CMHPtrArray& cmhPtr) {
      if (Move2Type(m) == T_std) {
         const Color  c    = p.c;
         const Square from = Move2From(m);
         assert(isValidSquare(from));
         const Square to = correctedMove2ToKingDest(m);
         assert(isValidSquare(to));
         const ScoreType s  = S * HSCORE(depth);
         const Piece     pp = p.board_const(from);
         history[c][from][to] += static_cast<ScoreType>(s - HISTORY_DIV(history[c][from][to] * std::abs(s)));
         historyP[PieceIdx(pp)][to] += static_cast<ScoreType>(s - HISTORY_DIV(historyP[PieceIdx(pp)][to] * std::abs(s)));
         for (int i = 0; i < MAX_CMH_PLY; ++i) {
            if (cmhPtr[i]) {
               ScoreType& item = cmhPtr[i][PieceIdx(p.board_const(from)) * NbSquare + to];
               item += static_cast<ScoreType>(s - HISTORY_DIV(item * std::abs(s)));
            }
         }
      }
   }

   template<int S> inline void updateCap(DepthType depth, Move m, const Position& p) {
      if (isCapture(m)) {
         const Square from = Move2From(m);
         assert(isValidSquare(from));
         const Square to = correctedMove2ToKingDest(m);
         assert(isValidSquare(to));
         const Piece pf = p.board_const(from);
         const Piece pt = p.board_const(to);
         const ScoreType s  = S * HSCORE(depth);
         historyCap[PieceIdx(pf)][to][Abs(pt)-1] += static_cast<ScoreType>(s - HISTORY_DIV(historyCap[PieceIdx(pf)][to][Abs(pt)-1] * std::abs(s)));
      }
   }
};

struct CounterT {
   ScoreType counter[NbSquare][NbSquare];

   void initCounter();
   void update(Move m, const Position& p);
};

void updateTables(Searcher& context, const Position& p, DepthType depth, DepthType height, const Move m, TT::Bound bound, CMHPtrArray& cmhPtr);
