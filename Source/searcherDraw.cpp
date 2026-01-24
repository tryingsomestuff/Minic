#include "com.hpp"
#include "position.hpp"
#include "searcher.hpp"

bool Searcher::isRep(const Position& p, bool isPV) const {
   // handles chess variants
   const int limit = isPV ? 2 : 1;
   if (p.fifty < (2 * limit - 1)) return false;
   int count = 0;
   const Hash h = computeHash(p);
   int k = p.halfmoves - 4;
   bool irreversible = false;
   //std::cout << "***************** " << k << " " << h << " " << GetFEN(p) << std::endl;
   // look in stack first
   for ( ; k >= 0; k-=2) {
      //std::cout << "stack " << k << " " << stack[k].h << " " << count << " " << ToString(stack[k].p.lastMove) << " " << GetFEN(stack[k].p) << std::endl;
      if (stack[k].h == nullHash) break; // no more "history" in stack (will look at game state later)
      if (stack[k].h == h){
#ifdef DEBUG_FIFTY_COLLISION
         if (stack[k].p != p){
            Logging::LogIt(Logging::logFatal) << "Collision in fifty hash comparation" << ToString(p) << ToString(stack[k].p);
         }
#endif
         ++count;
         if (count >= limit){
            //std::cout << "draw" << std::endl;
            return true;
         }
      }
      // irreversible moves - check both k-1 and k positions
      if (k > 0) {
         const Move prevMove = stack[k-1].p.lastMove;
         if (isValidMove(prevMove) && 
             (isCapture(prevMove) || PieceTools::getPieceType(stack[k-1].p, Move2To(prevMove)) == P_wp)){
               irreversible=true;
               break;
         }
      }
      const Move curMove = stack[k].p.lastMove;
      if (isValidMove(curMove) && 
          (isCapture(curMove) || PieceTools::getPieceType(stack[k].p, Move2To(curMove)) == P_wp)){
            irreversible=true;
            break;
      }
   }
   // then double check in game history
   while(!irreversible && k>=0){
      const std::optional<Hash> curh = COM::GetGameInfo().getHash(k);
      if (!curh.has_value()) break;
      //std::cout << "history " << k << " " << curh.value() << " " << count << " " << ToString(COM::GetGameInfo().getMove(k).value()) << " " << GetFEN(COM::GetGameInfo().getPosition(k).value()) << std::endl;
      if (curh.value() == h){
         ++count;
         if (count >= limit){
            //std::cout << "draw" << std::endl;
            return true;
         }
      }
      // irreversible moves - check both k-1 and k
      if (k>0){
         const auto m = COM::GetGameInfo().getMove(k-1);
         if (m.has_value()){
            const Move move = m.value();
            if (isCapture(move)){
               irreversible=true;
               break;
            }
            const auto pp = COM::GetGameInfo().getPosition(k-1);
            if (pp.has_value() && PieceTools::getPieceType(pp.value(), Move2To(move)) == P_wp){
               irreversible=true;
               break;
            }
         }
      }
      const auto m = COM::GetGameInfo().getMove(k);
      if (m.has_value()){
         const Move move = m.value();
         if (isCapture(move)){
            irreversible=true;
            break;
         }
         const auto pp = COM::GetGameInfo().getPosition(k);
         if (pp.has_value() && PieceTools::getPieceType(pp.value(), Move2To(move)) == P_wp){
            irreversible=true;
            break;
         }
      }
      k-=2;
   }
   return false;
}

bool Searcher::isMaterialDraw(const Position& p) const{
   // handles chess variants
   if ( (p.occupancy() & ~p.allKing()) == emptyBitBoard) return true;
   const auto ter = ( (p.mat[Co_White][M_p] + p.mat[Co_Black][M_p]) < 2) ? MaterialHash::probeMaterialHashTable(p.mat) : MaterialHash::Terminaison::Ter_Unknown;
   return ter == MaterialHash::Terminaison::Ter_Draw || ter == MaterialHash::Terminaison::Ter_MaterialDraw;
}

bool Searcher::is50moves(const Position& p, bool afterMoveLoop) const{
   // handles chess variants
   // checking 50MR at 100 or 101 depending on being before or after move loop because it might be reset by checkmate
   return p.fifty >= 100 + afterMoveLoop;
}