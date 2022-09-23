#pragma once

#include "definition.hpp"

struct Position;

/*!
 * A simple Zobrist hash implementation
 */
namespace Zobrist {
extern array2d<Hash,NbSquare,14> ZT; // should be 13 but last ray is for castling[0 7 56 63][13] and ep [k][13] and Color [3 4][13]
extern array1d<Hash,16> ZTCastling; // castling
extern array1d<Hash,std::numeric_limits<uint16_t>::max()> ZTMove; // MiniMove

void initHash();
} // namespace Zobrist

// Position hash is computed only once and then updated on the fly
// But this encapsulating function is usefull for debugging
[[nodiscard]] Hash computeHash(const Position &p);

// Same holds from K+P hash (used for pawn hash table)
[[nodiscard]] Hash computePHash(const Position &p);
