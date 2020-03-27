#pragma once

#include "definition.hpp"

struct Position;

/* A simple Zobrist hash implementation
 */

namespace Zobrist {
    template < typename T = Hash>
    T randomInt(T m, T M) {
        static std::mt19937 mt(42); // fixed seed for ZHash !!!
        static std::uniform_int_distribution<T> dist(m,M);
        return dist(mt);
    }
    extern Hash ZT[64][14]; // should be 13 but last ray is for castling[0 7 56 63][13] and ep [k][13] and color [3 4][13]
    void initHash();
}

// Position hash is computed only once and then updated on the fly
// But this encapsulating function is usefull for debugging
Hash computeHash(const Position &p);

// Same holds from K+P hash (used for pawn hash table)
Hash computePHash(const Position &p);

