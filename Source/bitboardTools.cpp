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
    p.allB.fill(empty);
    p.allPieces[Co_White] = p.allPieces[Co_Black] = p.occupancy = empty;
}

void setBitBoards(Position & p) {
    initBitBoards(p);
    for (Square k = 0; k < 64; ++k) { setBit(p,k,p.b[k]); }
    p.allPieces[Co_White] = p.whitePawn() | p.whiteKnight() | p.whiteBishop() | p.whiteRook() | p.whiteQueen() | p.whiteKing();
    p.allPieces[Co_Black] = p.blackPawn() | p.blackKnight() | p.blackBishop() | p.blackRook() | p.blackQueen() | p.blackKing();
    p.occupancy  = p.allPieces[Co_White] | p.allPieces[Co_Black];
}

} // BBTools
