#include "bitboardTools.hpp"

#include "logging.hpp"

namespace BBTools {

Square SquareFromBitBoard(const BitBoard & b) {
    assert(b != empty);
    unsigned long i = 0;
    bsf(b, i);
    return Square(i);
}

void initBitBoards(Position & p) {
    p._allB.fill(empty);
    p.allPieces[Co_White] = p.allPieces[Co_Black] = empty;
}

void setBitBoards(Position & p) {
    initBitBoards(p);
    for (Square k = 0; k < 64; ++k) { 
        const Piece pp = p.board_const(k);
        if ( pp != P_none ){
           setBit(p,k,pp); 
           p.allPieces[pp>0?Co_White:Co_Black] |= SquareToBitboard(k);
        }
    }
}

} // BBTools
