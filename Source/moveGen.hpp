#pragma once

#include "attack.hpp"
#include "bitboardTools.hpp"
#include "definition.hpp"
#include "dynamicConfig.hpp"
#include "hash.hpp"
#include "logging.hpp"
#include "positionTools.hpp"
#include "timers.hpp"
#include "tools.hpp"

struct Searcher;

template<MType MT>
struct CastlingTraits{
};

template<>
struct CastlingTraits<T_wqs>{
   static const CastlingTypes ct = CT_OOO;
   static const MType mt = T_wqs;
   static const CastlingRights cr = C_wqs;
   static const Color c = Co_White;
   static const Square kingLanding = Sq_c1;
   static const Square rookLanding = Sq_d1;
   static const BitBoard kingLandingBB = BB::BBSq_c1;
   static const BitBoard rookLandingBB = BB::BBSq_d1;
   static const Piece fromP = P_wk;
   static const Piece toP = P_wr;
};

template<>
struct CastlingTraits<T_wks>{
   static const CastlingTypes ct = CT_OO;
   static const MType mt = T_wks;
   static const CastlingRights cr = C_wks;
   static const Color c = Co_White;
   static const Square kingLanding = Sq_g1;
   static const Square rookLanding = Sq_f1;
   static const BitBoard kingLandingBB = BB::BBSq_g1;
   static const BitBoard rookLandingBB = BB::BBSq_f1;
   static const Piece fromP = P_wk;
   static const Piece toP = P_wr;
};

template<>
struct CastlingTraits<T_bqs>{
   static const CastlingTypes ct = CT_OOO;
   static const MType mt = T_bqs;
   static const CastlingRights cr = C_bqs;
   static const Color c = Co_Black;
   static const Square kingLanding = Sq_c8;
   static const Square rookLanding = Sq_d8;
   static const BitBoard kingLandingBB = BB::BBSq_c8;
   static const BitBoard rookLandingBB = BB::BBSq_d8;
   static const Piece fromP = P_bk;
   static const Piece toP = P_br;
};

template<>
struct CastlingTraits<T_bks>{
   static const CastlingTypes ct = CT_OO;
   static const MType mt = T_bks;
   static const CastlingRights cr = C_bks;
   static const Color c = Co_Black;
   static const Square kingLanding = Sq_g8;
   static const Square rookLanding = Sq_f8;
   static const BitBoard kingLandingBB = BB::BBSq_g8;
   static const BitBoard rookLandingBB = BB::BBSq_f8;
   static const Piece fromP = P_bk;
   static const Piece toP = P_br;
};

void applyNull(Searcher& context, Position& pN);

bool applyMove(Position& p, const Position::MoveInfo & moveInfo, const bool noNNUEUpdate = false);

void applyMoveNNUEUpdate(Position & p, const Position::MoveInfo & moveInfo);

[[nodiscard]] ScoreType randomMover(const Position& p, PVList& pv, const bool isInCheck);

[[nodiscard]] bool isPseudoLegal(const Position& p, const Move m);

namespace MoveGen {

enum GenPhase { GP_all = 0, GP_cap = 1, GP_quiet = 2, GP_evasion = 3 };

FORCE_FINLINE void addMove(Square from, Square to, MType type, MoveList& moves) {
   assert(isValidSquare(from));
   assert(isValidSquare(to));
   moves.push_back(ToMove(from, to, type, 0));
}

template<GenPhase phase = GP_all> 
void generateSquare(const Position& p, MoveList& moves, const Square from) {
   assert(isValidSquare(from));
#ifdef DEBUG_GENERATION
   if (from == INVALIDSQUARE) Logging::LogIt(Logging::logFatal) << "invalid square";
#endif
   const Color     side       = p.c;
   const BitBoard& myPieceBB  = p.allPieces[side];
   const BitBoard& oppPieceBB = p.allPieces[~side];
   const Piece     piece      = p.board_const(from);
   const Piece     ptype      = (Piece)std::abs(piece);
   assert(isValidPieceNotNone(ptype));
#ifdef DEBUG_GENERATION
   if (!isValidPieceNotNone(ptype)) {
      Logging::LogIt(Logging::logWarn) << ToString(myPieceBB);
      Logging::LogIt(Logging::logWarn) << ToString(oppPieceBB);
      Logging::LogIt(Logging::logWarn) << "piece " << static_cast<int>(piece);
      Logging::LogIt(Logging::logWarn) << SquareNames[from];
      Logging::LogIt(Logging::logWarn) << ToString(p);
      Logging::LogIt(Logging::logWarn) << ToString(p.lastMove);
      Logging::LogIt(Logging::logFatal) << "invalid type " << ptype;
   }
#endif
   const BitBoard occupancy = p.occupancy();
   BitBoard sliderRay = emptyBitBoard;
   BitBoard attacker = emptyBitBoard;
   if (phase == GP_evasion){
      const BitBoard slider = BBTools::attack<P_wb>(kingSquare(p), p.pieces_const<P_wb>(~p.c) | p.pieces_const<P_wq>(~p.c), occupancy)|
                              BBTools::attack<P_wr>(kingSquare(p), p.pieces_const<P_wr>(~p.c) | p.pieces_const<P_wq>(~p.c), occupancy);
      attacker = slider;
      BB::applyOn(slider, [&](const Square & k){
         sliderRay |= BBTools::between(k,kingSquare(p));
      });
      attacker |= BBTools::attack<P_wn>(kingSquare(p), p.pieces_const<P_wn>(~p.c));
      attacker |= BBTools::attack<P_wp>(kingSquare(p), p.pieces_const<P_wp>(~p.c), occupancy, p.c);
      if (!attacker){
         std::cout << "Error ! no attaker but evasion requested !" << std::endl;
         std::cout << ToString(p) << std::endl;
      }
      //std::cout << ToString(attacker) << std::endl;
      //std::cout << ToString(sliderRay) << std::endl;
   }
   if (ptype != P_wp) {
      BitBoard bb = BBTools::pfCoverage[ptype - 1](from, occupancy, p.c) & ~myPieceBB;
      if (phase == GP_cap){
         // only target opponent piece
         bb &= oppPieceBB;
      }
      else if (phase == GP_quiet){
         // do not target opponent piece
         bb &= ~oppPieceBB;
      }
      else if (phase == GP_evasion){
         // king don't want to stay in check
         if ( ptype == P_wk )
            bb &= ~sliderRay;
         // other pieces only target king attakers and sliders path
         else
            bb &= attacker | sliderRay;
      }
      BB::applyOn(bb, [&](const Square & to){
         const bool isCap = (phase == GP_cap) || ((oppPieceBB & SquareToBitboard(to)) != emptyBitBoard);
         if (isCap) addMove(from, to, T_capture, moves);
         else
            addMove(from, to, T_std, moves);
      });

      // attack on castling king destination square will be checked by applyMove
      if (phase != GP_cap && phase != GP_evasion && ptype == P_wk) { // add castling
         // Be carefull, here rooksInit are only accessed if the corresponding p.castling is set
         // This way rooksInit is not INVALIDSQUARE, and thus mask[] is not out of bound
         // Moreover, king[] also is assumed to be not INVALIDSQUARE which is verified in readFen
         /*
         std::cout << int(p.rootInfo().kingInit[Co_White]) << std::endl;
         std::cout << int(p.rootInfo().kingInit[Co_Black]) << std::endl;
         std::cout << int(p.rootInfo().rooksInit[Co_White][CT_OOO]) << std::endl;
         std::cout << int(p.rootInfo().rooksInit[Co_White][CT_OO]) << std::endl;
         std::cout << int(p.rootInfo().rooksInit[Co_Black][CT_OOO]) << std::endl;
         std::cout << int(p.rootInfo().rooksInit[Co_Black][CT_OO]) << std::endl;         
         */
         
         auto addIfCastlingOk = [&]<MType mt>() mutable {
            const Color c = CastlingTraits<mt>::c;
            const Square to = p.rootInfo().rooksInit[c][CastlingTraits<mt>::ct];
            if ((p.castling & CastlingTraits<mt>::cr) &&
               (((BBTools::between(p.king[c], CastlingTraits<mt>::kingLanding) | CastlingTraits<mt>::kingLandingBB | BBTools::between(to, CastlingTraits<mt>::rookLanding) | CastlingTraits<mt>::rookLandingBB) 
                   & ~BBTools::mask[to].bbsquare & ~BBTools::mask[p.king[c]].bbsquare) & occupancy) == emptyBitBoard &&
               !isAttacked(p, BBTools::between(p.king[c], CastlingTraits<mt>::kingLanding) | SquareToBitboard(p.king[c]) | CastlingTraits<mt>::kingLandingBB)){
              addMove(from, to, mt, moves);
            }
         };

         if (side == Co_White) {
            addIfCastlingOk.template operator()<T_wqs>();
            addIfCastlingOk.template operator()<T_wks>();
         }
         else {
            addIfCastlingOk.template operator()<T_bqs>();
            addIfCastlingOk.template operator()<T_bks>();
         }
      }
   }
   else {
      BitBoard pawnmoves = emptyBitBoard;
      if (phase != GP_quiet) pawnmoves = BBTools::mask[from].pawnAttack[p.c] & ~myPieceBB & oppPieceBB;
      if (phase == GP_evasion) pawnmoves &= attacker;
      BB::applyOn(pawnmoves, [&](const Square & to){
         if ((SquareToBitboard(to) & BB::rank1_or_rank8) == emptyBitBoard) addMove(from, to, T_capture, moves);
         else {
            addMove(from, to, T_cappromq, moves); // pawn capture with promotion
            addMove(from, to, T_cappromr, moves); // pawn capture with promotion
            addMove(from, to, T_cappromb, moves); // pawn capture with promotion
            addMove(from, to, T_cappromn, moves); // pawn capture with promotion
         }
      });
      
      pawnmoves = emptyBitBoard;
      if (phase != GP_cap) pawnmoves |= BBTools::mask[from].push[p.c] & ~occupancy;
      if ((phase != GP_cap) && (BBTools::mask[from].push[p.c] & occupancy) == emptyBitBoard) pawnmoves |= BBTools::mask[from].dpush[p.c] & ~occupancy;
      if (phase == GP_evasion) pawnmoves &= sliderRay;
      BB::applyOn(pawnmoves, [&](const Square & to){
         if ((SquareToBitboard(to) & BB::rank1_or_rank8) == emptyBitBoard) addMove(from, to, T_std, moves);
         else {
            addMove(from, to, T_promq, moves); // promotion Q
            addMove(from, to, T_promr, moves); // promotion R
            addMove(from, to, T_promb, moves); // promotion B
            addMove(from, to, T_promn, moves); // promotion N
         }
      });
      
      ///@todo evasion ep special case ?
      pawnmoves = emptyBitBoard;
      if (p.ep != INVALIDSQUARE && phase != GP_quiet) pawnmoves = BBTools::mask[from].pawnAttack[p.c] & ~myPieceBB & SquareToBitboard(p.ep);
      BB::applyOn(pawnmoves, [&](const Square & to){
         addMove(from, to, T_ep, moves);
      });
   }
}

template<GenPhase phase = GP_all> 
void generate(const Position& p, MoveList& moves, const bool doNotClear = false) {
   START_TIMER
   if (!doNotClear) moves.clear();
   BitBoard myPieceBBiterator = p.allPieces[p.c];
   BB::applyOn(myPieceBBiterator, [&](const Square & k){ generateSquare<phase>(p, moves, k); });
#ifdef DEBUG_GENERATION_LEGAL
   for (auto m : moves) {
      if (!isPseudoLegal(p, m)) {
         Logging::LogIt(Logging::logError) << "Generation error, move not legal " << ToString(p);
         Logging::LogIt(Logging::logError) << "move " << ToString(m);
         Logging::LogIt(Logging::logFatal) << "previous " << ToString(p.lastMove);
         assert(false);
      }
   }
#endif
   // in anarchy chess, EP is mandatory
   if ( DynamicConfig::anarchy && p.ep != INVALIDSQUARE ){ // will be slow ... ///@todo better
      bool foundEP = false;
      MoveList eps;
      for (auto m : moves){
         if ( Move2Type(m) == T_ep ){
            foundEP = true;
            eps.push_back(m);
         }
      }
      if ( foundEP ){
         moves = eps;
      }
   }
   // in antichess, capture is mandatory
   if ( DynamicConfig::antichess ){
     bool foundCap = false;
      MoveList caps;
      for (auto m : moves){
         if ( isCapture(m) ){
            foundCap = true;
            caps.push_back(m);
         }
      }
      if ( foundCap ){
         moves = caps;
      }     
   }
   STOP_AND_SUM_TIMER(Generate)
}

} // namespace MoveGen

void movePiece(Position& p, const Square from, const Square to, const Piece fromP, const Piece toP, const bool isCapture = false, const Piece prom = P_none);

template<Color c> 
FORCE_FINLINE void movePieceCastle(Position& p, const CastlingTypes ct, const Square kingDest, const Square rookDest) {
   START_TIMER
   constexpr Piece          pk = c == Co_White ? P_wk  : P_bk;
   constexpr Piece          pr = c == Co_White ? P_wr  : P_br;
   constexpr CastlingRights ks = c == Co_White ? C_wks : C_bks;
   constexpr CastlingRights qs = c == Co_White ? C_wqs : C_bqs;
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
