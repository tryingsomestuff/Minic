#pragma once

#include "attack.hpp"
#include "bitboardTools.hpp"
#include "definition.hpp"
#include "hash.hpp"
#include "logging.hpp"
#include "positionTools.hpp"
#include "timers.hpp"
#include "tools.hpp"

struct Searcher;

void applyNull(Searcher& context, Position& pN);

bool applyMove(Position& p, const Move& m, bool noValidation = false);

ScoreType randomMover(const Position& p, PVList& pv, bool isInCheck);

[[nodiscard]] bool isPseudoLegal(const Position& p, Move m);

namespace MoveGen {

enum GenPhase { GP_all = 0, GP_cap = 1, GP_quiet = 2 };

void addMove(Square from, Square to, MType type, MoveList& moves);

template<GenPhase phase = GP_all> void generateSquare(const Position& p, MoveList& moves, Square from) {
   assert(from >= 0 && from < 64);
#ifdef DEBUG_GENERATION
   if (from == INVALIDSQUARE) Logging::LogIt(Logging::logFatal) << "invalid square";
#endif
   const Color     side       = p.c;
   const BitBoard& myPieceBB  = p.allPieces[side];
   const BitBoard& oppPieceBB = p.allPieces[~side];
   const Piece     piece      = p.board_const(from);
   const Piece     ptype      = (Piece)std::abs(piece);
   assert(pieceValid(ptype));
#ifdef DEBUG_GENERATION
   if (!pieceValid(ptype)) {
      Logging::LogIt(Logging::logWarn) << BB::ToString(myPieceBB);
      Logging::LogIt(Logging::logWarn) << BB::ToString(oppPieceBB);
      Logging::LogIt(Logging::logWarn) << "piece " << (int)piece;
      Logging::LogIt(Logging::logWarn) << SquareNames[from];
      Logging::LogIt(Logging::logWarn) << ToString(p);
      Logging::LogIt(Logging::logWarn) << ToString(p.lastMove);
      Logging::LogIt(Logging::logFatal) << "invalid type " << ptype;
   }
#endif
   const BitBoard occupancy = p.occupancy();
   if (ptype != P_wp) {
      BitBoard bb = BBTools::pfCoverage[ptype - 1](from, occupancy, p.c) & ~myPieceBB;
      if (phase == GP_cap) bb &= oppPieceBB; // only target opponent piece
      else if (phase == GP_quiet)
         bb &= ~oppPieceBB; // do not target opponent piece
      while (bb) {
         const Square to    = BB::popBit(bb);
         const bool   isCap = (phase == GP_cap) || ((oppPieceBB & SquareToBitboard(to)) != emptyBitBoard);
         if (isCap) addMove(from, to, T_capture, moves);
         else
            addMove(from, to, T_std, moves);
      }

      // attack on castling king destination square will be checked by applyMove
      if (phase != GP_cap && ptype == P_wk) { // castling
         // Be carefull, here rooksInit are only accessed if the corresponding p.castling is set
         // This way rooksInit is not INVALIDSQUARE, and thus mask[] is not out of bound
         // Moreover, king[] also is assumed to be not INVALIDSQUARE which is verified in readFen      
         if (side == Co_White) {
            if ((p.castling & C_wqs) &&
                (((BBTools::mask[p.king[Co_White]].between[Sq_c1] | BB::BBSq_c1 | BBTools::mask[p.rootInfo().rooksInit[Co_White][CT_OOO]].between[Sq_d1] | BB::BBSq_d1) &
                  ~BBTools::mask[p.rootInfo().rooksInit[Co_White][CT_OOO]].bbsquare & ~BBTools::mask[p.king[Co_White]].bbsquare) & occupancy) == emptyBitBoard &&
                !isAttacked(p, BBTools::mask[p.king[Co_White]].between[Sq_c1] | SquareToBitboard(p.king[Co_White]) | BB::BBSq_c1))
               addMove(from, Sq_c1, T_wqs, moves); // wqs
            if ((p.castling & C_wks) &&
                (((BBTools::mask[p.king[Co_White]].between[Sq_g1] | BB::BBSq_g1 | BBTools::mask[p.rootInfo().rooksInit[Co_White][CT_OO]].between[Sq_f1] | BB::BBSq_f1) &
                  ~BBTools::mask[p.rootInfo().rooksInit[Co_White][CT_OO]].bbsquare & ~BBTools::mask[p.king[Co_White]].bbsquare) & occupancy) == emptyBitBoard &&
                !isAttacked(p, BBTools::mask[p.king[Co_White]].between[Sq_g1] | SquareToBitboard(p.king[Co_White]) | BB::BBSq_g1))
               addMove(from, Sq_g1, T_wks, moves); // wks
         }
         else {
            if ((p.castling & C_bqs) &&
                (((BBTools::mask[p.king[Co_Black]].between[Sq_c8] | BB::BBSq_c8 | BBTools::mask[p.rootInfo().rooksInit[Co_Black][CT_OOO]].between[Sq_d8] | BB::BBSq_d8) &
                  ~BBTools::mask[p.rootInfo().rooksInit[Co_Black][CT_OOO]].bbsquare & ~BBTools::mask[p.king[Co_Black]].bbsquare) & occupancy) == emptyBitBoard &&
                !isAttacked(p, BBTools::mask[p.king[Co_Black]].between[Sq_c8] | SquareToBitboard(p.king[Co_Black]) | BB::BBSq_c8))
               addMove(from, Sq_c8, T_bqs, moves); // wqs
            if ((p.castling & C_bks) &&
                (((BBTools::mask[p.king[Co_Black]].between[Sq_g8] | BB::BBSq_g8 | BBTools::mask[p.rootInfo().rooksInit[Co_Black][CT_OO]].between[Sq_f8] | BB::BBSq_f8) &
                  ~BBTools::mask[p.rootInfo().rooksInit[Co_Black][CT_OO]].bbsquare & ~BBTools::mask[p.king[Co_Black]].bbsquare) & occupancy) == emptyBitBoard &&
                !isAttacked(p, BBTools::mask[p.king[Co_Black]].between[Sq_g8] | SquareToBitboard(p.king[Co_Black]) | BB::BBSq_g8))
               addMove(from, Sq_g8, T_bks, moves); // wks
         }
      }
   }
   else {
      BitBoard              pawnmoves      = emptyBitBoard;
      static const BitBoard rank1_or_rank8 = BB::rank1 | BB::rank8;
      if (phase != GP_quiet) pawnmoves = BBTools::mask[from].pawnAttack[p.c] & ~myPieceBB & oppPieceBB;
      while (pawnmoves) {
         const Square to = BB::popBit(pawnmoves);
         if ((SquareToBitboard(to) & rank1_or_rank8) == emptyBitBoard) addMove(from, to, T_capture, moves);
         else {
            addMove(from, to, T_cappromq, moves); // pawn capture with promotion
            addMove(from, to, T_cappromr, moves); // pawn capture with promotion
            addMove(from, to, T_cappromb, moves); // pawn capture with promotion
            addMove(from, to, T_cappromn, moves); // pawn capture with promotion
         }
      }
      if (phase != GP_cap) pawnmoves |= BBTools::mask[from].push[p.c] & ~occupancy;
      if ((phase != GP_cap) && (BBTools::mask[from].push[p.c] & occupancy) == emptyBitBoard) pawnmoves |= BBTools::mask[from].dpush[p.c] & ~occupancy;
      while (pawnmoves) {
         const Square to = BB::popBit(pawnmoves);
         if ((SquareToBitboard(to) & rank1_or_rank8) == emptyBitBoard) addMove(from, to, T_std, moves);
         else {
            addMove(from, to, T_promq, moves); // promotion Q
            addMove(from, to, T_promr, moves); // promotion R
            addMove(from, to, T_promb, moves); // promotion B
            addMove(from, to, T_promn, moves); // promotion N
         }
      }
      if (p.ep != INVALIDSQUARE && phase != GP_quiet) pawnmoves = BBTools::mask[from].pawnAttack[p.c] & ~myPieceBB & SquareToBitboard(p.ep);
      while (pawnmoves) addMove(from, BB::popBit(pawnmoves), T_ep, moves);
   }
}

template<GenPhase phase = GP_all> void generate(const Position& p, MoveList& moves, bool doNotClear = false) {
   START_TIMER
   if (!doNotClear) moves.clear();
   BitBoard myPieceBBiterator = p.allPieces[p.c];
   while (myPieceBBiterator) generateSquare<phase>(p, moves, BB::popBit(myPieceBBiterator));
#ifdef DEBUG_GENERATION_LEGAL
   for (auto & m : moves) {
      if (!isPseudoLegal(p, m)) {
         Logging::LogIt(Logging::logError) << "Generation error, move not legal " << ToString(p);
         Logging::LogIt(Logging::logError) << "move " << ToString(m);
         Logging::LogIt(Logging::logFatal) << "previous " << ToString(p.lastMove);
         assert(false);
      }
   }
#endif
   STOP_AND_SUM_TIMER(Generate)
}

} // namespace MoveGen

void movePiece(Position& p, Square from, Square to, Piece fromP, Piece toP, bool isCapture = false, Piece prom = P_none);

template<Color c> inline void movePieceCastle(Position& p, CastlingTypes ct, Square kingDest, Square rookDest) {
   START_TIMER
   const Piece          pk = c == Co_White ? P_wk  : P_bk;
   const Piece          pr = c == Co_White ? P_wr  : P_br;
   const CastlingRights ks = c == Co_White ? C_wks : C_bks;
   const CastlingRights qs = c == Co_White ? C_wqs : C_bqs;
   BBTools::unSetBit(p, p.king[c]);
   BB::_unSetBit(p.allPieces[c], p.king[c]);
   BBTools::unSetBit(p, p.rootInfo().rooksInit[c][ct]);
   BB::_unSetBit(p.allPieces[c], p.rootInfo().rooksInit[c][ct]);
   BBTools::setBit(p, kingDest, pk);
   BB::_setBit(p.allPieces[c], kingDest);
   BBTools::setBit(p, rookDest, pr);
   BB::_setBit(p.allPieces[c], rookDest);
   p.board(p.king[c]) = P_none;
   p.board(p.rootInfo().rooksInit[c][ct]) = P_none;
   p.board(kingDest) = pk;
   p.board(rookDest) = pr;
   p.h  ^= Zobrist::ZT[p.king[c]][pk + PieceShift];
   p.ph ^= Zobrist::ZT[p.king[c]][pk + PieceShift];
   p.h  ^= Zobrist::ZT[p.rootInfo().rooksInit[c][ct]][pr + PieceShift];
   p.h  ^= Zobrist::ZT[kingDest][pk + PieceShift];
   p.ph ^= Zobrist::ZT[kingDest][pk + PieceShift];
   p.h  ^= Zobrist::ZT[rookDest][pr + PieceShift];
   p.king[c] = kingDest;
   p.h  ^= Zobrist::ZTCastling[p.castling];
   p.castling &= ~(ks | qs);
   p.h  ^= Zobrist::ZTCastling[p.castling];
   STOP_AND_SUM_TIMER(MovePiece)
}
