#pragma once

#include "definition.hpp"

struct Position;

/*!
 * A simple Zobrist hash implementation
 */
namespace Zobrist {
extern array2d<Hash,NbSquare,14> ZT; // should be 13 but last ray is for castling[0 7 56 63][13] and ep [k][13] and Color [3 4][13]
extern array1d<Hash,16> ZTCastling; // castling

void initHash();

// Compute hash for a move using from/to squares
// Uses unused ZT slot [5][NbPiece] to add variation
[[nodiscard]] constexpr Hash hashMove(MiniMove m) {
   const Square from = Move2From(m);
   const Square to = Move2To(m);
   return ZT[from][NbPiece] ^ ZT[to][NbPiece] ^ ZT[5][NbPiece];
}
} // namespace Zobrist

// Position hash is computed only once and then updated on the fly
// But this encapsulating function is usefull for debugging
[[nodiscard]] Hash computeHash(const Position &p);

// Same holds from K+P hash (used for pawn hash table)
[[nodiscard]] Hash computePHash(const Position &p);
