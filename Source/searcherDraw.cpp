#include "position.hpp"
#include "searcher.hpp"

bool Searcher::isRep(const Position& p, bool isPV) const {
   // handles chess variants
   const int limit = isPV ? 3 : 1;
   if (p.fifty < (2 * limit - 1)) return false;
   int count = 0;
   const Hash h = computeHash(p);
   for (int k = p.halfmoves - 1; k >= 0; --k) {
      if (stack[k].h == nullHash) break;
      if (stack[k].h == h){
         ++count;
#ifdef DEBUG_FIFTY_COLLISION
         if(stack[k].p != p){
            Logging::LogIt(Logging::logFatal) << "Collision in fifty hash comparation" << ToString(p) << ToString(stack[k].p);
         }
#endif
      }
      if (count >= limit) return true;
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