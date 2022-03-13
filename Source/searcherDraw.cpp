#include "position.hpp"
#include "searcher.hpp"

bool Searcher::isRep(const Position& p, bool isPV) const {
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
