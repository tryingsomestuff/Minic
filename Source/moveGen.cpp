#include "moveGen.hpp"

#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "material.hpp"
#include "positionTools.hpp"
#include "searcher.hpp"
#include "tools.hpp"

void movePiece(Position& p, const Square from, const Square to, const Piece fromP, const Piece toP, const bool isCapture, const Piece prom) {
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

bool applyMove(Position& p, const Position::MoveInfo & moveInfo, const bool noNNUEUpdate) {
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
         if (moveInfo.isCapNoEP) { --p.mat[~p.c][std::abs(moveInfo.toP)]; }
         // update hash, BB, board and castling rights
         movePiece(p, moveInfo.from, moveInfo.to, moveInfo.fromP, moveInfo.toP, moveInfo.type == T_capture);
         break;
      case T_ep:
         {
            assert(p.ep != INVALIDSQUARE);
            assert(SQRANK(p.ep) == EPRank[p.c]);
            const Square epCapSq = static_cast<Square>(p.ep + (p.c == Co_White ? -8 : +8));
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

void applyMoveNNUEUpdate(Position & p, const Position::MoveInfo & moveInfo){
#ifdef WITH_NNUE
   if (!DynamicConfig::useNNUE) return;
   // if king is not moving, update nnue evaluator
   // this is based on initial position state (most notably ep square), 
   // all is available in the previously built moveInfo)
   if (std::abs(moveInfo.fromP) != P_wk) {
      // ***be carefull here***, p.c has already been updated !!!
      if (p.c == Co_Black) updateNNUEEvaluator<Co_White>(p.evaluator(), moveInfo);
      else updateNNUEEvaluator<Co_Black>(p.evaluator(), moveInfo);
   }
   // if a king was moved (including castling), reset nnue evaluator
   // this is based on new position state !
   else { 
      p.resetNNUEEvaluator(p.evaluator()); 
   }
#endif

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
#endif   
}

ScoreType randomMover(const Position& p, PVList& pv, const bool isInCheck) {
   MoveList moves;
   MoveGen::generate<MoveGen::GP_all>(p, moves, false);
   if (moves.empty()) return isInCheck ? matedScore(0) : 0;
   ///@todo this is slow because the static here implies a guard variable !!
   static std::random_device rd;
   static std::mt19937       g(rd()); // here really random
   std::shuffle(moves.begin(), moves.end(), g);
   for (auto it = moves.begin(); it != moves.end(); ++it) {
      Position p2 = p;
#if defined(WITH_NNUE) && defined(DEBUG_NNUE_UPDATE)
      NNUEEvaluator evaluator = p.evaluator();
      p2.associateEvaluator(evaluator);
#endif
      const Position::MoveInfo moveInfo(p2, *it);
      if (!applyMove(p2, moveInfo)) continue;
      pv.emplace_back(*it); // updatePV
      const Square to = Move2To(*it);
      // king capture (works only for most standard chess variants)
      if (p.c == Co_White && to == p.king[Co_Black]) return matingScore(0);
      if (p.c == Co_Black && to == p.king[Co_White]) return matingScore(0);
      return 0; // found one valid move
   }
   // no move is valid ...
   return isInCheck ? matedScore(0) : 0;
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
         std::cout << static_cast<int>(Move2Type(m)) << std::endl;
         std::cout << Move2Score(m) << std::endl;
         std::cout << static_cast<int>(m & 0x0000FFFF) << std::endl;
         for (auto it = moves.begin(); it != moves.end(); ++it) std::cout << ToString(*it) << "\t" << *it << "\t";
         std::cout << std::endl;
         std::cout << "Not a generated move !" << std::endl;
      }
   }
   return b;
}

bool isPseudoLegal2(const Position& p, const Move m) { // validate TT move
#else
bool isPseudoLegal(const Position& p, const Move m) { // validate TT move
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

   if (!isValidMove(m)) PSEUDO_LEGAL_RETURN(false, -1)
   const MType t = Move2Type(m);
   
   if (!isValidMoveType(t)) PSEUDO_LEGAL_RETURN(false, -4)
   if (t == T_reserved) PSEUDO_LEGAL_RETURN(false, 3)
   
   const Square from = Move2From(m);
   if (!isValidSquare(from)) PSEUDO_LEGAL_RETURN(false, -2)
   
   const Piece fromP = p.board_const(from);
   if (fromP == P_none || (fromP > 0 && p.c == Co_Black) || (fromP < 0 && p.c == Co_White)) PSEUDO_LEGAL_RETURN(false, 0)
   
   const Square to = Move2To(m);
   if (!isValidSquare(to)) PSEUDO_LEGAL_RETURN(false, -3)
   
   // cannot capture own piece, except for castling where it is checked later
   const Piece toP = p.board_const(to);
   if ((toP > 0 && p.c == Co_White) || (toP < 0 && p.c == Co_Black)){
      if(!DynamicConfig::FRC){
         if (!isCastling(t)) PSEUDO_LEGAL_RETURN(false, 1)
         else{
            ;// see castling
         }
      }
      else{
         if (!isCastling(t)) PSEUDO_LEGAL_RETURN(false, 1)
         else{
            ;// see castling
         }
      }
   }

   // opponent king cannot be captured
   ///@todo variant were king can be captured   
   if (toP == (p.c == Co_White ? P_bk : P_wk)) PSEUDO_LEGAL_RETURN(false, 2)
   
   // empty destination square cannot be a capture (except EP)
   if (toP == P_none && isCaptureNoEP(t)) PSEUDO_LEGAL_RETURN(false, 4)

   // none empty destination square must be capture, except for castling where it is checked later
   ///@todo variant ?
   if (toP != P_none && !isCapture(t)){
      if (!DynamicConfig::FRC){
         if (!isCastling(t)) PSEUDO_LEGAL_RETURN(false, 5)
         else{
            ;// see castling
         }
      }
      else{
         if (!isCastling(t)) PSEUDO_LEGAL_RETURN(false, 5)
         else{
            ;// see castling
         }
      }
   }

   const Piece fromPieceType = static_cast<Piece>(std::abs(fromP));

   // EP comes from a pawn
   if (t == T_ep && (p.ep == INVALIDSQUARE || fromPieceType != P_wp)) PSEUDO_LEGAL_RETURN(false, 6)
   // EP geometry checks
   if (t == T_ep && p.board_const(static_cast<Square>(p.ep + (p.c == Co_White ? -8 : +8))) != (p.c == Co_White ? P_bp : P_wp)) PSEUDO_LEGAL_RETURN(false, 7)

   // Promotion comes from a pawn
   if (isPromotion(m) && fromPieceType != P_wp) PSEUDO_LEGAL_RETURN(false, 8)

   const BitBoard occupancy = p.occupancy();

   auto isCastlingOk = [&]<MType mt>(){
      // Be carefull, here rooksInit are only accessed if the corresponding p.castling is set
      // This way rooksInit is not INVALIDSQUARE, and thus mask[] is not out of bound
      // Moreover, king[] also is assumed to be not INVALIDSQUARE which is verified in readFen
      constexpr Color c = CastlingTraits<mt>::c;
      return t == mt && 
             (p.castling & CastlingTraits<mt>::cr) && 
             from == p.rootInfo().kingInit[c] && 
             to == p.rootInfo().rooksInit[c][CastlingTraits<mt>::ct] &&
             fromP == CastlingTraits<mt>::fromP && 
             (((BBTools::between(p.king[c], CastlingTraits<mt>::kingLanding) | CastlingTraits<mt>::kingLandingBB | BBTools::between(to, CastlingTraits<mt>::rookLanding) | CastlingTraits<mt>::rookLandingBB) &
               ~BBTools::mask[to].bbsquare & ~BBTools::mask[p.king[c]].bbsquare) &
              occupancy) == emptyBitBoard &&
             !isAttacked(p, BBTools::between(p.king[c], CastlingTraits<mt>::kingLanding) | SquareToBitboard(p.king[c]) | CastlingTraits<mt>::kingLandingBB);
   };

   if (isCastling(t)) {
      if (p.c == Co_White) {
         if (isCastlingOk.operator()<T_wqs>()) PSEUDO_LEGAL_RETURN(true, 9)
         if (isCastlingOk.operator()<T_wks>()) PSEUDO_LEGAL_RETURN(true, 10)
         PSEUDO_LEGAL_RETURN(false, 11)
      }
      else {
         if (isCastlingOk.operator()<T_bqs>()) PSEUDO_LEGAL_RETURN(true, 12)
         if (isCastlingOk.operator()<T_bks>()) PSEUDO_LEGAL_RETURN(true, 13)
         PSEUDO_LEGAL_RETURN(false, 14)
      }
   }

   if (fromPieceType == P_wp) {
      // ep must land on good square
      if (t == T_ep && to != p.ep) PSEUDO_LEGAL_RETURN(false, 15)

      if (t != T_ep && p.ep != INVALIDSQUARE && to == p.ep) PSEUDO_LEGAL_RETURN(false, 16)
      // non promotion pawn move cannot reach 8th rank
      if (!isPromotion(m) && SQRANK(to) == PromRank[p.c]) PSEUDO_LEGAL_RETURN(false, 17)
      // promotion must be 8th rank
      if (isPromotion(m) && SQRANK(to) != PromRank[p.c]) PSEUDO_LEGAL_RETURN(false, 18)

      // (double)pawn push must be legal
      BitBoard validPush = BBTools::mask[from].push[p.c] & ~occupancy;
      if ((BBTools::mask[from].push[p.c] & occupancy) == emptyBitBoard) validPush |= BBTools::mask[from].dpush[p.c] & ~occupancy;
      if (validPush & SquareToBitboard(to)) PSEUDO_LEGAL_RETURN(true, 19)

      // pawn capture (including ep)
      const BitBoard validCap = BBTools::mask[from].pawnAttack[p.c] & ~p.allPieces[p.c];
      if ((validCap & SquareToBitboard(to)) && ((t != T_ep && toP != P_none) || (t == T_ep && to == p.ep && toP == P_none)))
         PSEUDO_LEGAL_RETURN(true, 20)

      PSEUDO_LEGAL_RETURN(false, 21)
   }

   // possible piece move (excluding king)
   if (fromPieceType != P_wk) {
      if ((BBTools::pfCoverage[fromPieceType - 1](from, occupancy, p.c) & SquareToBitboard(to)) != emptyBitBoard) PSEUDO_LEGAL_RETURN(true, 22)

      PSEUDO_LEGAL_RETURN(false, 23)
   }

   // only king moving is not verified yet
   if ((BBTools::mask[p.king[p.c]].kingZone & SquareToBitboard(to)) != emptyBitBoard) PSEUDO_LEGAL_RETURN(true, 24)

   PSEUDO_LEGAL_RETURN(false, 25)
}
