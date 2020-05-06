#pragma once

#include "definition.hpp"

struct Position;

/* A simple Zobrist hash implementation
 */

namespace Zobrist {
    extern Hash ZT[64][14]; // should be 13 but last ray is for castling[0 7 56 63][13] and ep [k][13] and color [3 4][13]
    extern Hash ZTCastling[16]; // castling
    void initHash();
}

// Position hash is computed only once and then updated on the fly
// But this encapsulating function is usefull for debugging
Hash computeHash(const Position &p);

// Same holds from K+P hash (used for pawn hash table)
Hash computePHash(const Position &p);

