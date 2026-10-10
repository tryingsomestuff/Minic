#pragma once

#include "definition.hpp"
#include "hash.hpp"
#include "position.hpp"
#include "searchConfig.hpp"
#include "transposition.hpp"

/*!
 * This file defines classic tables management such as :
 *  - Killers
 *  - History
 *     - Color/from/to
 *     - Piece/to
 *     - Pawn hash bucket/piece/to
 *     - CMH history : previous moved piece/previous to/current moved piece & to
 *  - Counter : from/to
 */

inline constexpr array1d<unsigned int, SearchConfig::MAX_CMH_PLY> CMH_DISTANCE = {1, 2, 4, 6};
using CMHPtrArray = array1d<array1d<ScoreType, NbPiece*NbSquare> *, SearchConfig::MAX_CMH_PLY>;

struct KillerT {
   array2d<Move,MAX_DEPTH,2> killers;

   void initKillers();
   void update(Move m, DepthType height);
   [[nodiscard]] bool isKiller(const Move m, const DepthType height);
};

struct HistoryT {
   static_assert(SearchConfig::PAWN_HISTORY_BUCKETS > 0 &&
                 (SearchConfig::PAWN_HISTORY_BUCKETS & (SearchConfig::PAWN_HISTORY_BUCKETS - 1)) == 0);

   array3d<ScoreType,2,NbSquare,NbSquare> history;                       // Color, from, to
   array3d<ScoreType,NbPiece,NbSquare,PieceShift> historyCap;            // Piece moved (+color), to, piece taken
   array2d<ScoreType,NbPiece,NbSquare> historyP;                         // Piece, to
   array3d<ScoreType,SearchConfig::PAWN_HISTORY_BUCKETS,NbPiece,NbSquare> pawnHistory;
   array3d<ScoreType,NbPiece,NbSquare,NbPiece*NbSquare> counter_history; // Previous moved piece, previous to, current moved piece * boardsize + current to

   void initHistory();

   [[nodiscard]] FORCE_FINLINE const ScoreType& getFromToHistory(const Color color, const Square from, const Square to) const {
      return history[color][from][to];
   }

   [[nodiscard]] FORCE_FINLINE ScoreType& getFromToHistory(const Color color, const Square from, const Square to) {
      return history[color][from][to];
   }

   [[nodiscard]] FORCE_FINLINE const ScoreType& getPieceToHistory(const Piece piece, const Square to) const {
      return historyP[PieceIdx(piece)][to];
   }

   [[nodiscard]] FORCE_FINLINE ScoreType& getPieceToHistory(const Piece piece, const Square to) {
      return historyP[PieceIdx(piece)][to];
   }

   [[nodiscard]] FORCE_FINLINE const ScoreType& getCaptureHistory(const Piece piece, const Square to, const Piece capturedPiece) const {
      return historyCap[PieceIdx(piece)][to][Abs(capturedPiece) - 1];
   }

   [[nodiscard]] FORCE_FINLINE ScoreType& getCaptureHistory(const Piece piece, const Square to, const Piece capturedPiece) {
      return historyCap[PieceIdx(piece)][to][Abs(capturedPiece) - 1];
   }

   [[nodiscard]] FORCE_FINLINE const ScoreType& getPawnHistory(const Position& p, const Piece piece, const Square to) const {
      const size_t bucket = static_cast<size_t>(computePHash(p) & (SearchConfig::PAWN_HISTORY_BUCKETS - 1));
      return pawnHistory[bucket][PieceIdx(piece)][to];
   }

   [[nodiscard]] FORCE_FINLINE ScoreType& getPawnHistory(const Position& p, const Piece piece, const Square to) {
      const size_t bucket = static_cast<size_t>(computePHash(p) & (SearchConfig::PAWN_HISTORY_BUCKETS - 1));
      return pawnHistory[bucket][PieceIdx(piece)][to];
   }

   template<int S> FORCE_FINLINE void update(DepthType depth, Move m, const Position& p, CMHPtrArray& cmhPtr) {
      if (Move2Type(m) == T_std) {
         const Color  c    = p.c;
         const Square from = Move2From(m);
         assert(isValidSquare(from));
         const Square to = Move2To(m); // no need for correctedMove2ToKingDest here (std)
         assert(isValidSquare(to));
         const ScoreType s  = S * HSCORE(depth);
         const Piece     pp = p.board_const(from);

         ScoreType& fromToItem = getFromToHistory(c, from, to);
         fromToItem = HISTORY_UPDATE(fromToItem, s);

         ScoreType& pieceToItem = getPieceToHistory(pp, to);
         pieceToItem = HISTORY_UPDATE(pieceToItem, s);
         
         ScoreType& pawnItem = getPawnHistory(p, pp, to);
         pawnItem = HISTORY_UPDATE(pawnItem, s);
         
         const int cmhWeightBase = SearchConfig::CMHWeight[0];
         for (int i = 0; i < SearchConfig::MAX_CMH_PLY; ++i) {
            if (cmhPtr[i]) {
               ScoreType& CMHitem = (*cmhPtr[i])[PieceIdx(p.board_const(from)) * NbSquare + to];
               const int CMHbonus = s * SearchConfig::CMHWeight[i] / cmhWeightBase;
               CMHitem = HISTORY_UPDATE(CMHitem, CMHbonus);
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
         ScoreType& item = getCaptureHistory(pf, to, pt);
         item = HISTORY_UPDATE(item, s);
      }
   }
};

struct CounterT {
   array2d<ScoreType,NbSquare,NbSquare> counter;

   void initCounter();
   void update(Move m, const Position& p);
};

void updateTables(Searcher& context, const Position& p, DepthType depth, DepthType height, const Move m, TT::Bound bound, CMHPtrArray& cmhPtr);
