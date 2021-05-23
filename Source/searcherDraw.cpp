#include "position.hpp"
#include "searcher.hpp"

bool Searcher::isRep(const Position& p, bool isPV) const {
   const int limit = isPV ? 3 : 1;
   if (p.fifty < (2 * limit - 1)) return false;
   int count = 0;
   const Hash h = computeHash(p);
   for (int k = p.halfmoves - 1; k >= 0; --k) {
      if (stack[k].h == nullHash) break;
      if (stack[k].h == h) ++count;
      if (count >= limit) return true;
   }
   return false;
}
