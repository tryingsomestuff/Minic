#include "moveApply.hpp"

#include "attack.hpp"
#include "bitboard.hpp"
#include "bitboardTools.hpp"
#include "hash.hpp"
#include "material.hpp"
#include "movePseudoLegal.hpp"
#include "position.hpp"
#include "tools.hpp"

MoveInfo::MoveInfo(const Position& p, const Move _m):
    m(_m),
    from(Move2From(_m)),
    to(correctedMove2ToKingDest(_m)),
    king{p.king[Co_White], p.king[Co_Black]},
    ep(p.ep),
    type(Move2Type(_m)),
    fromP(p.board_const(from)),
    toP(p.board_const(to)), // won't be used for EP
    fromId(PieceIdx(fromP)),
    isCapNoEP(toP != P_none) {
    assert(isValidSquare(from));
    assert(isValidSquare(to));
    assert(isValidMoveType(type));
}

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


void movePiece(Position& p, const Square from, const Square to, const Piece fromP, const Piece toP, const bool isCapture = false, const Piece prom = P_none) {
   assert(isValidSquare(from));
   assert(isValidSquare(to));
   assert(isValidPieceNotNone(fromP));
   START_TIMER
   const int   fromId  = PieceIdx(fromP);
   const int   toId    = PieceIdx(toP);
   const Piece toPnew  = prom != P_none ? prom : fromP;
   const int   toIdnew = PieceIdx(toPnew);
   // update board
   p.board(from) = P_none;
   p.board(to)   = toPnew;
   // update bitboard
   BBTools::unSetBit(p, from, fromP);
   BB::_unSetBit(p.allPieces[p.c], from);
   if (toP != P_none) {
      BBTools::unSetBit(p, to, toP);
      BB::_unSetBit(p.allPieces[~p.c], to);
   }
   BBTools::setBit(p, to, toPnew);
   BB::_setBit(p.allPieces[p.c], to);
   // update Zobrist hash
   p.h ^= Zobrist::ZT[from][fromId]; // remove fromP at from
   p.h ^= Zobrist::ZT[to][toIdnew];  // add fromP (or prom) at to
   // piece that have influence on pawn hash
   static constexpr array1d<bool,NbPiece> helperPawnHash_ = {true, false, false, false, false, true, false, true, false, false, false, false, true};
   if (helperPawnHash_[fromId]) {
      p.ph ^= Zobrist::ZT[from][fromId];                    // remove fromP at from
      if (prom == P_none) p.ph ^= Zobrist::ZT[to][toIdnew]; // add fromP (if not prom) at to
   }
   if (isCapture) { // if capture remove toP at to
      p.h ^= Zobrist::ZT[to][toId];
      if (abs(toP) == P_wp || abs(toP) == P_wk) p.ph ^= Zobrist::ZT[to][toId];
   }
   // consequences on castling
   if (p.castling && (p.rootInfo().castlePermHashTable[from] ^ p.rootInfo().castlePermHashTable[to])) {
      p.h ^= Zobrist::ZTCastling[p.castling];
      p.castling &= (p.rootInfo().castlePermHashTable[from] & p.rootInfo().castlePermHashTable[to]);
      p.h ^= Zobrist::ZTCastling[p.castling];
   }

   // update king position
   if (fromP == P_wk) p.king[Co_White] = to;
   else if (fromP == P_bk)
      p.king[Co_Black] = to;

   // king capture : this can append in some chess variants
   if (toP == P_wk) p.king[Co_White] = INVALIDSQUARE;
   else if (toP == P_bk) p.king[Co_Black] = INVALIDSQUARE;

   STOP_AND_SUM_TIMER(MovePiece)
}

void applyNull(Searcher&, Position& pN) {
   START_TIMER
   pN.c = ~pN.c;
   pN.h ^= Zobrist::ZT[3][13];
   pN.h ^= Zobrist::ZT[4][13];
   if (pN.ep != INVALIDSQUARE) pN.h ^= Zobrist::ZT[pN.ep][13];
   pN.ep = INVALIDSQUARE;
   pN.lastMove = NULLMOVE;
   if (pN.c == Co_White) ++pN.moves;
   ++pN.halfmoves;
   STOP_AND_SUM_TIMER(Apply)
}

bool applyMove(Position& p, const MoveInfo & moveInfo, [[maybe_unused]] const bool noNNUEUpdate) {
   assert(isValidMove(moveInfo.m));
   START_TIMER
#ifdef DEBUG_MATERIAL
   Position previous = p;
#endif
#ifdef DEBUG_APPLY
   if (!isPseudoLegal(p, moveInfo.m)) {
      Logging::LogIt(Logging::logError) << ToString(moveInfo.m) << " " << moveInfo.type;
      Logging::LogIt(Logging::logFatal) << "Apply error, not legal " << ToString(p);
      assert(false);
   }
#endif

   switch (moveInfo.type) {
      case T_reserved:
         Logging::LogIt(Logging::logError) << ToString(moveInfo.m) << " " << static_cast<int>(moveInfo.type);
         Logging::LogIt(Logging::logFatal) << "Apply error, move is not legal (reserved). " << ToString(p);
         break; 
      case T_std:
      case T_capture:
         // update material
         if (moveInfo.isCapNoEP) { --p.mat[~p.c][Abs(moveInfo.toP)]; }
         // update hash, BB, board and castling rights
         movePiece(p, moveInfo.from, moveInfo.to, moveInfo.fromP, moveInfo.toP, moveInfo.type == T_capture);
         break;
      case T_ep:
         {
            assert(p.ep != INVALIDSQUARE);
            assert(SQRANK(p.ep) == EPRank[p.c]);
            const auto epCapSq = static_cast<Square>(p.ep + (p.c == Co_White ? -8 : +8));
            assert(isValidSquare(epCapSq));
            BBTools::unSetBit(p, epCapSq, ~moveInfo.fromP); // BEFORE setting p.b new shape !!!
            BB::_unSetBit(p.allPieces[~p.c], epCapSq);
            BBTools::unSetBit(p, moveInfo.from, moveInfo.fromP);
            BB::_unSetBit(p.allPieces[p.c], moveInfo.from);
            BBTools::setBit(p, moveInfo.to, moveInfo.fromP);
            BB::_setBit(p.allPieces[p.c], moveInfo.to);
            p.board(moveInfo.from)    = P_none;
            p.board(moveInfo.to)      = moveInfo.fromP;
            p.board(epCapSq) = P_none;

            p.h ^= Zobrist::ZT[moveInfo.from][moveInfo.fromId];                     // remove fromP at from
            p.h ^= Zobrist::ZT[epCapSq][PieceIdx(p.c == Co_White ? P_bp : P_wp)];   // remove captured pawn
            p.h ^= Zobrist::ZT[moveInfo.to][moveInfo.fromId];                       // add fromP at to

            p.ph ^= Zobrist::ZT[moveInfo.from][moveInfo.fromId];                    // remove fromP at from
            p.ph ^= Zobrist::ZT[epCapSq][PieceIdx(p.c == Co_White ? P_bp : P_wp)];  // remove captured pawn
            p.ph ^= Zobrist::ZT[moveInfo.to][moveInfo.fromId];                      // add fromP at to

            --p.mat[~p.c][M_p];
         }
         break;
      case T_promq:
      case T_cappromq:
         MaterialHash::updateMaterialProm(p, moveInfo.to, moveInfo.type);
         movePiece(p, moveInfo.from, moveInfo.to, moveInfo.fromP, moveInfo.toP, moveInfo.type == T_cappromq, (p.c == Co_White ? P_wq : P_bq));
         break;
      case T_promr:
      case T_cappromr:
         MaterialHash::updateMaterialProm(p, moveInfo.to, moveInfo.type);
         movePiece(p, moveInfo.from, moveInfo.to, moveInfo.fromP, moveInfo.toP, moveInfo.type == T_cappromr, (p.c == Co_White ? P_wr : P_br));
         break;
      case T_promb:
      case T_cappromb:
         MaterialHash::updateMaterialProm(p, moveInfo.to, moveInfo.type);
         movePiece(p, moveInfo.from, moveInfo.to, moveInfo.fromP, moveInfo.toP, moveInfo.type == T_cappromb, (p.c == Co_White ? P_wb : P_bb));
         break;
      case T_promn:
      case T_cappromn:
         MaterialHash::updateMaterialProm(p, moveInfo.to, moveInfo.type);
         movePiece(p, moveInfo.from, moveInfo.to, moveInfo.fromP, moveInfo.toP, moveInfo.type == T_cappromn, (p.c == Co_White ? P_wn : P_bn));
         break;
      case T_wks: movePieceCastle<Co_White>(p, CT_OO,  Sq_g1, Sq_f1); break;
      case T_wqs: movePieceCastle<Co_White>(p, CT_OOO, Sq_c1, Sq_d1); break;
      case T_bks: movePieceCastle<Co_Black>(p, CT_OO,  Sq_g8, Sq_f8); break;
      case T_bqs: movePieceCastle<Co_Black>(p, CT_OOO, Sq_c8, Sq_d8); break;
      case T_max: break;
   }

   if (/*!noValidation &&*/ isPosInCheck(p)) {
      STOP_AND_SUM_TIMER(Apply)
      return false; // this is the only legal move validation needed
   }

   const bool pawnMove = abs(moveInfo.fromP) == P_wp;
   // update EP
   if (p.ep != INVALIDSQUARE) p.h ^= Zobrist::ZT[p.ep][13];
   p.ep = INVALIDSQUARE;
   if (pawnMove && abs(moveInfo.to - moveInfo.from) == 16 && (BBTools::adjacent(SquareToBitboard(moveInfo.to)) & p.pieces_const(~p.c,P_wp)) != emptyBitBoard) {
      p.ep = static_cast<Square>((moveInfo.from + moveInfo.to) / 2);
      p.h ^= Zobrist::ZT[p.ep][13];
   }
   assert(p.ep == INVALIDSQUARE || SQRANK(p.ep) == EPRank[~p.c]);

   // update Color
   p.c = ~p.c;
   p.h ^= Zobrist::ZT[3][13];
   p.h ^= Zobrist::ZT[4][13];

   // update game state
   if (moveInfo.isCapNoEP || pawnMove) p.fifty = 0; // E.p covered by pawn move here ...
   else
      ++p.fifty;
   if (p.c == Co_White) ++p.moves;
   ++p.halfmoves;

   if (isCaptureOrProm(moveInfo.type)) MaterialHash::updateMaterialOther(p);

#ifdef WITH_NNUE
   if (!noNNUEUpdate){
      applyMoveNNUEUpdate(p, moveInfo);
   }
#endif

#ifdef DEBUG_MATERIAL
   const Position::Material mat = p.mat;
   MaterialHash::initMaterial(p);
   if (p.mat != mat) {
      Logging::LogIt(Logging::logWarn) << "Material update error";
      Logging::LogIt(Logging::logWarn) << "Material previous " << ToString(previous) << ToString(previous.mat);
      Logging::LogIt(Logging::logWarn) << "Material computed " << ToString(p) << ToString(p.mat);
      Logging::LogIt(Logging::logWarn) << "Material incrementally updated " << ToString(mat);
      Logging::LogIt(Logging::logFatal) << "Last move " << ToString(p.lastMove) << " current move " << ToString(moveInfo.m);
   }
#endif

#ifdef DEBUG_BITBOARD
   int count_bb1   = BB::countBit(p.occupancy());
   int count_bb2   = 0;
   int count_board = 0;
   for (Square s = Sq_a1; s <= Sq_h8; ++s) {
      if (p.board_const(s) != P_none) ++count_board;
   }
   for (Piece pp = P_bk; pp <= P_wk; ++pp) {
      if (pp == P_none) continue;
      BitBoard       b  = p.pieces_const(pp);
      const BitBoard bb = b;
      applyOn(b, [&](const Square & k){ {
         ++count_bb2;
         if (p.board_const(k) != pp) {
            Logging::LogIt(Logging::logWarn) << SquareNames[k];
            Logging::LogIt(Logging::logWarn) << ToString(p);
            Logging::LogIt(Logging::logWarn) << ToString(bb);
            Logging::LogIt(Logging::logWarn) << static_cast<int>(pp);
            Logging::LogIt(Logging::logWarn) << static_cast<int>(p.board_const(k));
            Logging::LogIt(Logging::logWarn) << "last move " << ToString(p.lastMove);
            Logging::LogIt(Logging::logWarn) << " current move " << ToString(moveInfo.m);
            Logging::LogIt(Logging::logFatal) << "Wrong bitboard ";
         }
      });
   }
   if (count_bb1 != count_bb2) {
      Logging::LogIt(Logging::logFatal) << "Wrong bitboard count (all/piece)" << count_bb1 << " " << count_bb2;
      Logging::LogIt(Logging::logWarn) << ToString(p);
      Logging::LogIt(Logging::logWarn) << ToString(p.occupancy());
      for (Piece pp = P_bk; pp <= P_wk; ++pp) {
         if (pp == P_none) continue;
         Logging::LogIt(Logging::logWarn) << ToString(p.pieces_const(pp));
      }
   }
   if (count_board != count_bb1) {
      Logging::LogIt(Logging::logFatal) << "Wrong bitboard count (board)" << count_board << " " << count_bb1;
      Logging::LogIt(Logging::logWarn) << ToString(p);
      Logging::LogIt(Logging::logWarn) << ToString(p.occupancy());
   }
#endif
   p.lastMove = Move2MiniMove(moveInfo.m);
   STOP_AND_SUM_TIMER(Apply)
   return true;
}

void applyMoveNNUEUpdate([[maybe_unused]] Position & p, [[maybe_unused]] const MoveInfo & moveInfo){
#ifdef WITH_NNUE
   if (!DynamicConfig::useNNUE) return;
   // if king is not moving, update nnue evaluator
   // this is based on initial position state (most notably ep square), 
   // all is available in the previously built moveInfo)
   if (Abs(moveInfo.fromP) != P_wk) {
      // ***be carefull here***, p.c has already been updated !!!
      if (p.c == Co_Black) updateNNUEEvaluator<Co_White>(p.evaluator(), moveInfo);
      else updateNNUEEvaluator<Co_Black>(p.evaluator(), moveInfo);
   }
   // if a king was moved (including castling!!), reset nnue evaluator
   // this is based on new position state !
   else { 
      p.resetNNUEEvaluator(p.evaluator(), ~p.c);
      if (isCastling(moveInfo.type)){
         p.resetNNUEEvaluator(p.evaluator(), p.c);
      }
      else{
         // ***be carefull here***, p.c has already been updated !!!
         if (p.c == Co_Black) updateNNUEEvaluatorThemOnly<Co_White>(p.evaluator(), moveInfo);
         else updateNNUEEvaluatorThemOnly<Co_Black>(p.evaluator(), moveInfo);
      }
   }

#ifdef DEBUG_NNUE_UPDATE
   Position      p2 = p;
   NNUEEvaluator evaluator;
   p2.associateEvaluator(evaluator);
   p2.resetNNUEEvaluator(p2.evaluator());
   if (p2.evaluator() != p.evaluator()) {
      Logging::LogIt(Logging::logWarn) << "evaluator update error";
      Logging::LogIt(Logging::logWarn) << ToString(p);
      Logging::LogIt(Logging::logWarn) << ToString(moveInfo.m);
      Logging::LogIt(Logging::logWarn) << ToString(p.lastMove);
      Logging::LogIt(Logging::logWarn) << p2.evaluator().white.active();
      Logging::LogIt(Logging::logWarn) << p2.evaluator().black.active();
      Logging::LogIt(Logging::logWarn) << "--------------------";
      Logging::LogIt(Logging::logWarn) << p.evaluator().white.active();
      Logging::LogIt(Logging::logWarn) << p.evaluator().black.active();
      Logging::LogIt(Logging::logWarn) << backtrace();
   }
#endif // DEBUG_NNUE_UPDATE

#endif // WITH_NNUE


}