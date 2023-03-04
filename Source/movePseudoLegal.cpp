#include "movePseudoLegal.hpp"

#include "moveGen.hpp"
#include "position.hpp"

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
         for (const auto & it : moves) std::cout << ToString(it) << "\t" << *it << "\t";
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

   const Piece fromPieceType = Abs(fromP);

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
