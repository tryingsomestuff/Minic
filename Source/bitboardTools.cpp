#include "bitboardTools.hpp"

#include "logging.hpp"

namespace BBTools {

Square SquareFromBitBoard(const BitBoard& b) {
   assert(isNotEmpty(b));
#ifdef _WIN64
   unsigned long i = 0ull;
#else
   int i = 0;
#endif
   bsf(b, i);
   return Square(i);
}

void clearBitBoards(Position& p) {
   p._allB.fill(emptyBitBoard);
   p.allPieces[Co_White] = p.allPieces[Co_Black] = emptyBitBoard;
}

void setBitBoards(Position& p) {
   clearBitBoards(p);
   for (Square k = 0; k < NbSquare; ++k) {
      const Piece pp = p.board_const(k);
      if (pp != P_none) {
         setBit(p, k, pp);
         p.allPieces[pp > 0 ? Co_White : Co_Black] |= SquareToBitboard(k);
      }
   }
}

} // namespace BBTools
