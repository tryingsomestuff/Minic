#pragma once

#include "definition.hpp"

struct Position;

/*!
 * A simple Zobrist hash implementation
 */
namespace Zobrist {
extern Hash ZT[NbSquare][14]; // should be 13 but last ray is for castling[0 7 56 63][13] and ep [k][13] and Color [3 4][13]
extern Hash ZTCastling[16];   // castling
void initHash();
} // namespace Zobrist

// Position hash is computed only once and then updated on the fly
// But this encapsulating function is usefull for debugging
[[nodiscard]] Hash computeHash(const Position &p);

// Same holds from K+P hash (used for pawn hash table)
[[nodiscard]] Hash computePHash(const Position &p);
