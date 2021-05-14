#include "bitboardTools.hpp"

#include "logging.hpp"

namespace BBTools {

Square SquareFromBitBoard(const BitBoard & b) {
    assert(b != emptyBitBoard);
    int i = 0;
    bsf(b, i);
    return Square(i);
}

void initBitBoards(Position & p) {
    p._allB.fill(emptyBitBoard);
    p.allPieces[Co_White] = p.allPieces[Co_Black] = emptyBitBoard;
}

void setBitBoards(Position & p) {
    initBitBoards(p);
    for (Square k = 0; k < NbSquare; ++k) { 
        const Piece pp = p.board_const(k);
        if ( pp != P_none ){
           setBit(p,k,pp); 
           p.allPieces[pp>0?Co_White:Co_Black] |= SquareToBitboard(k);
        }
    }
}

} // BBTools
