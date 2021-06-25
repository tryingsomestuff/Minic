#include "moveGen.hpp"

#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "material.hpp"
#include "positionTools.hpp"
#include "searcher.hpp"
#include "tools.hpp"

namespace MoveGen {

void addMove(Square from, Square to, MType type, MoveList& moves) {
   assert(from >= 0 && from < NbSquare);
   assert(to >= 0 && to < NbSquare);
   moves.push_back(ToMove(from, to, type, 0));
}

} // namespace MoveGen

namespace {
// piece that have influence on pawn hash
const bool _helperPawnHash[NbPiece] = {true, false, false, false, false, true, false, true, false, false, false, false, true};
} // namespace

void movePiece(Position& p, Square from, Square to, Piece fromP, Piece toP, bool isCapture, Piece prom) {
   START_TIMER
   const int   fromId  = PieceIdx(fromP);
   const int   toId    = PieceIdx(toP);
   const Piece toPnew  = prom != P_none ? prom : fromP;
   const int   toIdnew = prom != P_none ? PieceIdx(prom) : fromId;
   assert(squareOK(from));
   assert(squareOK(to));
   assert(pieceValid(fromP));
   // update board
   p.board(from) = P_none;
   p.board(to)   = toPnew;
   // update bitboard
   BBTools::unSetBit(p, from, fromP);
   BB::_unSetBit(p.allPieces[fromP > 0 ? Co_White : Co_Black], from);
   if (toP != P_none) {
      BBTools::unSetBit(p, to, toP);
      BB::_unSetBit(p.allPieces[toP > 0 ? Co_White : Co_Black], to);
   }
   BBTools::setBit(p, to, toPnew);
   BB::_setBit(p.allPieces[fromP > 0 ? Co_White : Co_Black], to);
   // update Zobrist hash
   p.h ^= Zobrist::ZT[from][fromId]; // remove fromP at from
   p.h ^= Zobrist::ZT[to][toIdnew];  // add fromP (or prom) at to
   if (_helperPawnHash[fromId]) {
      p.ph ^= Zobrist::ZT[from][fromId];                    // remove fromP at from
      if (prom == P_none) p.ph ^= Zobrist::ZT[to][toIdnew]; // add fromP (if not prom) at to
   }
   if (isCapture) { // if capture remove toP at to
      p.h ^= Zobrist::ZT[to][toId];
      if ((abs(toP) == P_wp || abs(toP) == P_wk)) p.ph ^= Zobrist::ZT[to][toId];
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

   // king capture : is that necessary ???
   if (toP == P_wk) p.king[Co_White] = INVALIDSQUARE;
   else if (toP == P_bk)
      p.king[Co_Black] = INVALIDSQUARE;

   STOP_AND_SUM_TIMER(MovePiece)
}

void applyNull(Searcher&, Position& pN) {
   START_TIMER
   pN.c = ~pN.c;
   pN.h ^= Zobrist::ZT[3][13];
   pN.h ^= Zobrist::ZT[4][13];
   if (pN.ep != INVALIDSQUARE) pN.h ^= Zobrist::ZT[pN.ep][13];
   pN.ep       = INVALIDSQUARE;
   pN.lastMove = NULLMOVE;
   if (pN.c == Co_White) ++pN.moves;
   ++pN.halfmoves;
   STOP_AND_SUM_TIMER(Apply)
}

bool applyMove(Position& p, const Move& m, bool noValidation) {
   START_TIMER
   assert(VALIDMOVE(m));
#ifdef DEBUG_MATERIAL
   Position previous = p;
#endif
   const Position::MoveInfo moveInfo(p,m);
   Piece promPiece = P_none;
#ifdef DEBUG_APPLY
   if (!isPseudoLegal(p, m)) {
      Logging::LogIt(Logging::logError) << ToString(m) << " " << moveInfo.type;
      Logging::LogIt(Logging::logFatal) << "Apply error, not legal " << ToString(p);
      assert(false);
   }
#endif

#ifdef WITH_NNUE
   // if king is not moving, update nnue evaluator
   // this is based on current position state
   if (DynamicConfig::useNNUE && std::abs(moveInfo.fromP) != P_wk) {
      if (p.c == Co_White) p.updateNNUEEvaluator<Co_White>(p.Evaluator(), m, moveInfo);
      else
         p.updateNNUEEvaluator<Co_Black>(p.Evaluator(), m, moveInfo);
   }
#endif

   switch (moveInfo.type) {
      case T_std:
      case T_capture:
      case T_reserved:
         // update material
         if (moveInfo.isCapNoEP) { p.mat[~p.c][std::abs(moveInfo.toP)]--; }
         // update hash, BB, board and castling rights
         movePiece(p, moveInfo.from, moveInfo.to, moveInfo.fromP, moveInfo.toP, moveInfo.type == T_capture);
         break;
      case T_ep:
         {
            assert(p.ep != INVALIDSQUARE);
            assert(SQRANK(p.ep) == EPRank[p.c]);
            const Square epCapSq = p.ep + (p.c == Co_White ? -8 : +8);
            assert(squareOK(epCapSq));
            BBTools::unSetBit(p, epCapSq, ~moveInfo.fromP); // BEFORE setting p.b new shape !!!
            BB::_unSetBit(p.allPieces[moveInfo.fromP > 0 ? Co_Black : Co_White], epCapSq);
            BBTools::unSetBit(p, moveInfo.from, moveInfo.fromP);
            BB::_unSetBit(p.allPieces[moveInfo.fromP > 0 ? Co_White : Co_Black], moveInfo.from);
            BBTools::setBit(p, moveInfo.to, moveInfo.fromP);
            BB::_setBit(p.allPieces[moveInfo.fromP > 0 ? Co_White : Co_Black], moveInfo.to);
            p.board(moveInfo.from)    = P_none;
            p.board(moveInfo.to)      = moveInfo.fromP;
            p.board(epCapSq) = P_none;

            p.h ^= Zobrist::ZT[moveInfo.from][moveInfo.fromId];                     // remove fromP at from
            p.h ^= Zobrist::ZT[epCapSq][PieceIdx(p.c == Co_White ? P_bp : P_wp)];   // remove captured pawn
            p.h ^= Zobrist::ZT[moveInfo.to][moveInfo.fromId];                       // add fromP at to

            p.ph ^= Zobrist::ZT[moveInfo.from][moveInfo.fromId];                    // remove fromP at from
            p.ph ^= Zobrist::ZT[epCapSq][PieceIdx(p.c == Co_White ? P_bp : P_wp)];  // remove captured pawn
            p.ph ^= Zobrist::ZT[moveInfo.to][moveInfo.fromId];                      // add fromP at to

            p.mat[~p.c][M_p]--;
         }
         break;
      case T_promq:
      case T_cappromq:
         MaterialHash::updateMaterialProm(p, moveInfo.to, moveInfo.type);
         promPiece = (p.c == Co_White ? P_wq : P_bq);
         movePiece(p, moveInfo.from, moveInfo.to, moveInfo.fromP, moveInfo.toP, moveInfo.type == T_cappromq, promPiece);
         break;
      case T_promr:
      case T_cappromr:
         MaterialHash::updateMaterialProm(p, moveInfo.to, moveInfo.type);
         promPiece = (p.c == Co_White ? P_wr : P_br);
         movePiece(p, moveInfo.from, moveInfo.to, moveInfo.fromP, moveInfo.toP, moveInfo.type == T_cappromr, promPiece);
         break;
      case T_promb:
      case T_cappromb:
         MaterialHash::updateMaterialProm(p, moveInfo.to, moveInfo.type);
         promPiece = (p.c == Co_White ? P_wb : P_bb);
         movePiece(p, moveInfo.from, moveInfo.to, moveInfo.fromP, moveInfo.toP, moveInfo.type == T_cappromb, promPiece);
         break;
      case T_promn:
      case T_cappromn:
         MaterialHash::updateMaterialProm(p, moveInfo.to, moveInfo.type);
         promPiece = (p.c == Co_White ? P_wn : P_bn);
         movePiece(p, moveInfo.from, moveInfo.to, moveInfo.fromP, moveInfo.toP, moveInfo.type == T_cappromn, promPiece);
         break;
      case T_wks: movePieceCastle<Co_White>(p, CT_OO, Sq_g1, Sq_f1); break;
      case T_wqs: movePieceCastle<Co_White>(p, CT_OOO, Sq_c1, Sq_d1); break;
      case T_bks: movePieceCastle<Co_Black>(p, CT_OO, Sq_g8, Sq_f8); break;
      case T_bqs: movePieceCastle<Co_Black>(p, CT_OOO, Sq_c8, Sq_d8); break;
   }

   if (!noValidation && isAttacked(p, kingSquare(p))) {
      STOP_AND_SUM_TIMER(Apply)
      return false; // this is the only legal move validation needed
   }

   const bool pawnMove = abs(moveInfo.fromP) == P_wp;
   // update EP
   if (p.ep != INVALIDSQUARE) p.h ^= Zobrist::ZT[p.ep][13];
   p.ep = INVALIDSQUARE;
   if (pawnMove && abs(moveInfo.to - moveInfo.from) == 16) {
      p.ep = (moveInfo.from + moveInfo.to) / 2;
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
   // if a king was moved (including castling), reset nnue evaluator
   // this is based on new position state !
   if (DynamicConfig::useNNUE && std::abs(moveInfo.fromP) == P_wk) { p.resetNNUEEvaluator(p.Evaluator()); }
#endif

#ifdef DEBUG_NNUE_UPDATE
   Position      p2 = p;
   NNUEEvaluator evaluator;
   p2.associateEvaluator(evaluator);
   p2.resetNNUEEvaluator(p2.Evaluator());
   if (p2.Evaluator() != p.Evaluator()) {
      Logging::LogIt(Logging::logWarn) << "Evaluator update error";
      Logging::LogIt(Logging::logWarn) << ToString(p);
      Logging::LogIt(Logging::logWarn) << ToString(m);
      Logging::LogIt(Logging::logWarn) << ToString(p.lastMove);
      Logging::LogIt(Logging::logWarn) << p2.Evaluator().white.active();
      Logging::LogIt(Logging::logWarn) << p2.Evaluator().black.active();
      Logging::LogIt(Logging::logWarn) << "--------------------";
      Logging::LogIt(Logging::logWarn) << p.Evaluator().white.active();
      Logging::LogIt(Logging::logWarn) << p.Evaluator().black.active();
      Logging::LogIt(Logging::logWarn) << backtrace();
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
      Logging::LogIt(Logging::logFatal) << "Last move " << ToString(p.lastMove) << " current move " << ToString(m);
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
      while (b) {
         ++count_bb2;
         const Square s = BB::popBit(b);
         if (p.board_const(s) != pp) {
            Logging::LogIt(Logging::logWarn) << SquareNames[s];
            Logging::LogIt(Logging::logWarn) << ToString(p);
            Logging::LogIt(Logging::logWarn) << ToString(bb);
            Logging::LogIt(Logging::logWarn) << (int)pp;
            Logging::LogIt(Logging::logWarn) << (int)p.board_const(s);
            Logging::LogIt(Logging::logWarn) << "last move " << ToString(p.lastMove);
            Logging::LogIt(Logging::logWarn) << " current move " << ToString(m);
            Logging::LogIt(Logging::logFatal) << "Wrong bitboard ";
         }
      }
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
   p.lastMove = m;
   STOP_AND_SUM_TIMER(Apply)
   return true;
}

ScoreType randomMover(const Position& p, PVList& pv, bool isInCheck) {
   MoveList moves;
   MoveGen::generate<MoveGen::GP_all>(p, moves, false);
   if (moves.empty()) return isInCheck ? -MATE : 0;
   static std::random_device rd;
   static std::mt19937       g(rd()); // here really random
   std::shuffle(moves.begin(), moves.end(), g);
   for (auto it = moves.begin(); it != moves.end(); ++it) {
      Position p2 = p;
#if defined(WITH_NNUE) && defined(DEBUG_NNUE_UPDATE)
      NNUEEvaluator evaluator = p.Evaluator();
      p2.associateEvaluator(evaluator);
#endif
      if (!applyMove(p2, *it)) continue;
      PVList childPV;
      updatePV(pv, *it, childPV);
      const Square to = Move2To(*it);
      if (p.c == Co_White && to == p.king[Co_Black]) return MATE + 1;
      if (p.c == Co_Black && to == p.king[Co_White]) return MATE + 1;
      return 0; // found one valid move
   }
   // no move is valid ...
   return isInCheck ? -MATE : 0;
}

#ifdef DEBUG_PSEUDO_LEGAL
bool isPseudoLegal2(const Position& p, Move m); // forward decl
bool isPseudoLegal(const Position& p, Move m) {
   const bool b = isPseudoLegal2(p, m);
   if (b) {
      MoveList moves;
      MoveGen::generate<MoveGen::GP_all>(p, moves);
      bool found = false;
      for (auto it = moves.begin(); it != moves.end() && !found; ++it)
         if (sameMove(*it, m)) found = true;
      if (!found) {
         std::cout << ToString(p) << "\n" << ToString(m) << "\t" << m << std::endl;
         std::cout << SquareNames[Move2From(m)] << std::endl;
         std::cout << SquareNames[Move2To(m)] << std::endl;
         std::cout << (int)Move2Type(m) << std::endl;
         std::cout << Move2Score(m) << std::endl;
         std::cout << int(m & 0x0000FFFF) << std::endl;
         for (auto it = moves.begin(); it != moves.end(); ++it) std::cout << ToString(*it) << "\t" << *it << "\t";
         std::cout << std::endl;
         std::cout << "Not a generated move !" << std::endl;
      }
   }
   return b;
}

bool isPseudoLegal2(const Position& p, Move m) { // validate TT move
#else
bool isPseudoLegal(const Position& p, Move m) { // validate TT move
#endif
#ifdef DEBUG_PSEUDO_LEGAL
#define PSEUDO_LEGAL_RETURN(b, r)                                \
   {                                                             \
      if (!b) std::cout << "isPseudoLegal2: " << r << std::endl; \
      STOP_AND_SUM_TIMER(PseudoLegal) return b;                  \
   }
#else
#define PSEUDO_LEGAL_RETURN(b, r) \
   { STOP_AND_SUM_TIMER(PseudoLegal) return b; }
#endif
   START_TIMER
   if (!VALIDMOVE(m)) PSEUDO_LEGAL_RETURN(false, -1)
   const Square from = Move2From(m);
   assert(squareOK(from));
   const Piece fromP = p.board_const(from);
   if (fromP == P_none || (fromP > 0 && p.c == Co_Black) || (fromP < 0 && p.c == Co_White)) PSEUDO_LEGAL_RETURN(false, 0)
   const Square to = Move2To(m);
   assert(squareOK(to));
   const Piece toP = p.board_const(to);
   if ((toP > 0 && p.c == Co_White) || (toP < 0 && p.c == Co_Black)) PSEUDO_LEGAL_RETURN(false, 1)
   if ((Piece)std::abs(toP) == P_wk) PSEUDO_LEGAL_RETURN(false, 2)
   const Piece fromPieceType = (Piece)std::abs(fromP);
   const MType t             = Move2Type(m);
   assert(moveTypeOK(t));
   if (t == T_reserved) PSEUDO_LEGAL_RETURN(false, 3)
   if (toP == P_none && (isCapture(t) && t != T_ep)) PSEUDO_LEGAL_RETURN(false, 4)
   if (toP != P_none && !isCapture(t)) PSEUDO_LEGAL_RETURN(false, 5)
   if (t == T_ep && (p.ep == INVALIDSQUARE || fromPieceType != P_wp)) PSEUDO_LEGAL_RETURN(false, 6)
   if (t == T_ep && p.board_const(p.ep + (p.c == Co_White ? -8 : +8)) != (p.c == Co_White ? P_bp : P_wp)) PSEUDO_LEGAL_RETURN(false, 7)
   if (isPromotion(m) && fromPieceType != P_wp) PSEUDO_LEGAL_RETURN(false, 8)
   const BitBoard occupancy = p.occupancy();
   if (isCastling(m)) {
      // Be carefull, here rooksInit are only accessed if the corresponding p.castling is set
      // This way rooksInit is not INVALIDSQUARE, and thus mask[] is not out of bound
      // Moreover, king[] also is assumed to be not INVALIDSQUARE which is verified in readFen
      if (p.c == Co_White) {
         if (t == T_wqs && (p.castling & C_wqs) && from == p.rootInfo().kingInit[Co_White] && fromP == P_wk && to == Sq_c1 && toP == P_none &&
             (((BBTools::mask[p.king[Co_White]].between[Sq_c1] | BB::BBSq_c1 | BBTools::mask[p.rootInfo().rooksInit[Co_White][CT_OOO]].between[Sq_d1] |
                BB::BBSq_d1) &
               ~BBTools::mask[p.rootInfo().rooksInit[Co_White][CT_OOO]].bbsquare & ~BBTools::mask[p.king[Co_White]].bbsquare) &
              occupancy) == emptyBitBoard &&
             !isAttacked(p, BBTools::mask[p.king[Co_White]].between[Sq_c1] | SquareToBitboard(p.king[Co_White]) | BB::BBSq_c1))
            PSEUDO_LEGAL_RETURN(true, 9)
         if (t == T_wks && (p.castling & C_wks) && from == p.rootInfo().kingInit[Co_White] && fromP == P_wk && to == Sq_g1 && toP == P_none &&
             (((BBTools::mask[p.king[Co_White]].between[Sq_g1] | BB::BBSq_g1 | BBTools::mask[p.rootInfo().rooksInit[Co_White][CT_OO]].between[Sq_f1] |
                BB::BBSq_f1) &
               ~BBTools::mask[p.rootInfo().rooksInit[Co_White][CT_OO]].bbsquare & ~BBTools::mask[p.king[Co_White]].bbsquare) &
              occupancy) == emptyBitBoard &&
             !isAttacked(p, BBTools::mask[p.king[Co_White]].between[Sq_g1] | SquareToBitboard(p.king[Co_White]) | BB::BBSq_g1))
            PSEUDO_LEGAL_RETURN(true, 10)
         PSEUDO_LEGAL_RETURN(false, 11)
      }
      else {
         if (t == T_bqs && (p.castling & C_bqs) && from == p.rootInfo().kingInit[Co_Black] && fromP == P_bk && to == Sq_c8 && toP == P_none &&
             (((BBTools::mask[p.king[Co_Black]].between[Sq_c8] | BB::BBSq_c8 | BBTools::mask[p.rootInfo().rooksInit[Co_Black][CT_OOO]].between[Sq_d8] |
                BB::BBSq_d8) &
               ~BBTools::mask[p.rootInfo().rooksInit[Co_Black][CT_OOO]].bbsquare & ~BBTools::mask[p.king[Co_Black]].bbsquare) &
              occupancy) == emptyBitBoard &&
             !isAttacked(p, BBTools::mask[p.king[Co_Black]].between[Sq_c8] | SquareToBitboard(p.king[Co_Black]) | BB::BBSq_c8))
            PSEUDO_LEGAL_RETURN(true, 12)
         if (t == T_bks && (p.castling & C_bks) && from == p.rootInfo().kingInit[Co_Black] && fromP == P_bk && to == Sq_g8 && toP == P_none &&
             (((BBTools::mask[p.king[Co_Black]].between[Sq_g8] | BB::BBSq_g8 | BBTools::mask[p.rootInfo().rooksInit[Co_Black][CT_OO]].between[Sq_f8] |
                BB::BBSq_f8) &
               ~BBTools::mask[p.rootInfo().rooksInit[Co_Black][CT_OO]].bbsquare & ~BBTools::mask[p.king[Co_Black]].bbsquare) &
              occupancy) == emptyBitBoard &&
             !isAttacked(p, BBTools::mask[p.king[Co_Black]].between[Sq_g8] | SquareToBitboard(p.king[Co_Black]) | BB::BBSq_g8))
            PSEUDO_LEGAL_RETURN(true, 13)
         PSEUDO_LEGAL_RETURN(false, 14)
      }
   }
   if (fromPieceType == P_wp) {
      if (t == T_ep && to != p.ep) PSEUDO_LEGAL_RETURN(false, 15)
      if (t != T_ep && p.ep != INVALIDSQUARE && to == p.ep) PSEUDO_LEGAL_RETURN(false, 16)
      if (!isPromotion(m) && SQRANK(to) == PromRank[p.c]) PSEUDO_LEGAL_RETURN(false, 17)
      if (isPromotion(m) && SQRANK(to) != PromRank[p.c]) PSEUDO_LEGAL_RETURN(false, 18)
      BitBoard validPush = BBTools::mask[from].push[p.c] & ~occupancy;
      if ((BBTools::mask[from].push[p.c] & occupancy) == emptyBitBoard) validPush |= BBTools::mask[from].dpush[p.c] & ~occupancy;
      if (validPush & SquareToBitboard(to)) PSEUDO_LEGAL_RETURN(true, 19)
      const BitBoard validCap = BBTools::mask[from].pawnAttack[p.c] & ~p.allPieces[p.c];
      if ((validCap & SquareToBitboard(to)) && ((t != T_ep && toP != P_none) || (t == T_ep && to == p.ep && toP == P_none)))
         PSEUDO_LEGAL_RETURN(true, 20)
      PSEUDO_LEGAL_RETURN(false, 21)
   }
   if (fromPieceType != P_wk) {
      if ((BBTools::pfCoverage[fromPieceType - 1](from, occupancy, p.c) & SquareToBitboard(to)) != emptyBitBoard) PSEUDO_LEGAL_RETURN(true, 22)
      PSEUDO_LEGAL_RETURN(false, 23)
   }
   if ((BBTools::mask[p.king[p.c]].kingZone & SquareToBitboard(to)) != emptyBitBoard) PSEUDO_LEGAL_RETURN(true, 24) // only king is not verified yet
   PSEUDO_LEGAL_RETURN(false, 25)
}
