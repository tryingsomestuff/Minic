#include "com.hpp"
#include "position.hpp"
#include "searcher.hpp"

bool Searcher::isRep(const Position& p, bool isPV) const {
   // handles chess variants
   const int limit = isPV ? 2 : 1;
   if (p.fifty < (2 * limit - 1)) return false;
   int count = 0;
   const Hash h = computeHash(p);
   int k = p.halfmoves - 2;
   bool irreversible = false;
   //std::cout << "***************** " << h << " " << GetFEN(p) << std::endl;
   // look in stack first
   for ( ; k >= 0; k-=2) {
      //std::cout << "stack " << stack[k].h << " " << count << " " << ToString(stack[k].p.lastMove) << " " << GetFEN(stack[k].p) << std::endl;
      if (stack[k].h == nullHash) break; // no more "history" in stack (will look at game state later)
      if (stack[k].h == h){
         ++count;
#ifdef DEBUG_FIFTY_COLLISION
         if(stack[k].p != p){
            Logging::LogIt(Logging::logFatal) << "Collision in fifty hash comparation" << ToString(p) << ToString(stack[k].p);
         }
#endif
         if (count >= limit) return true;
      }
      // irreversible moves ?
      if (k > 0 && isValidMove(stack[k-1].p.lastMove) && 
          (isCapture(stack[k-1].p.lastMove) || PieceTools::getPieceType(stack[k-1].p, Move2To(stack[k-1].p.lastMove)) == P_wp)){
            irreversible=true;
            break;
      }       
      if (isValidMove(stack[k].p.lastMove) && 
          (isCapture(stack[k].p.lastMove) || PieceTools::getPieceType(stack[k].p, Move2To(stack[k].p.lastMove)) == P_wp)){
            irreversible=true;
            break;
      }
   }
   // then double check in game history
   while(!irreversible && k>=0){
      const std::optional<Hash> curh = COM::GetGameInfo().getHash(k);
      if (!curh.has_value()) break;
      //std::cout << "history " << curh.value() << " " << count << " " << ToString(COM::GetGameInfo().getMove(k).value()) << " " << GetFEN(COM::GetGameInfo().getPosition(k).value()) << std::endl;
      if (curh.value() == h){
         ++count;
         if (count >= limit) return true;
      }
      // irreversible moves ?
      if(k>0){
         const auto m = COM::GetGameInfo().getMove(k-1);
         const auto pp = COM::GetGameInfo().getPosition(k-1);
         if ( m.has_value() &&
            (isCapture(m.value()) || PieceTools::getPieceType(pp.value(), Move2To(m.value())) == P_wp)){
               irreversible=true;
               break;
         }
      }
      const auto m = COM::GetGameInfo().getMove(k);
      const auto pp = COM::GetGameInfo().getPosition(k);
      if ( m.has_value() &&
          (isCapture(m.value()) || PieceTools::getPieceType(pp.value(), Move2To(m.value())) == P_wp)){
            irreversible=true;
            break;
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