#pragma once

#include "definition.hpp"

#include "bitboardTools.hpp"

#ifdef __BMI2__
#include <immintrin.h>
#endif

namespace BBTools {

/*!
 * Two bitboard attack generation tools can be used here (controlled by WITH_MAGIC define from definition.hpp):
 * - hyperbola quintessence (https://www.chessprogramming.org/Hyperbola_Quintessence)
 * - or magic (https://www.chessprogramming.org/Magic_Bitboards) with BMI2 extension if available
 */

// mask variable is filled with many usefull pre-computed bitboard information
// many of then will be usefull during evaluation and move generation
// note that between to not include start and end square
struct Mask {
    static int ranks[512];
    BitBoard bbsquare, diagonal, antidiagonal, file, kingZone, pawnAttack[2], push[2], dpush[2], enpassant, knight, king, frontSpan[2], rearSpan[2], passerSpan[2], attackFrontSpan[2], between[NbSquare];
    Mask():bbsquare(emptyBitBoard), diagonal(emptyBitBoard), antidiagonal(emptyBitBoard), file(emptyBitBoard), kingZone(emptyBitBoard), pawnAttack{ emptyBitBoard,emptyBitBoard }, push{ emptyBitBoard,emptyBitBoard }, dpush{ emptyBitBoard,emptyBitBoard }, enpassant(emptyBitBoard), knight(emptyBitBoard), king(emptyBitBoard), frontSpan{emptyBitBoard}, rearSpan{emptyBitBoard}, passerSpan{emptyBitBoard}, attackFrontSpan{emptyBitBoard}, between{emptyBitBoard}{}
};
extern Mask mask[NbSquare];
void initMask();

#ifndef WITH_MAGIC

/*!
 * This HQ BB implementation is comming from Dumb/Amoeba by Richard Delorme (a.k.a Abulmo)
 * after a discussion on talkchess about a small BB implementation
 * http://talkchess.com/forum3/viewtopic.php?f=7&t=68741&p=777898#p777682
 */

[[nodiscard]] BitBoard attack            (const BitBoard occupancy, const Square x, const BitBoard m);
[[nodiscard]] BitBoard rankAttack        (const BitBoard occupancy, const Square x);
[[nodiscard]] BitBoard fileAttack        (const BitBoard occupancy, const Square x);
[[nodiscard]] BitBoard diagonalAttack    (const BitBoard occupancy, const Square x);
[[nodiscard]] BitBoard antidiagonalAttack(const BitBoard occupancy, const Square x);

// Next functions define the user API for piece move
template < Piece > [[nodiscard]] inline BitBoard coverage      (const Square x, const BitBoard occupancy = 0, const Color c = Co_White) { assert(false); return empty; }
template <       > [[nodiscard]] inline BitBoard coverage<P_wp>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return mask[x].pawnAttack[c]; }
template <       > [[nodiscard]] inline BitBoard coverage<P_wn>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return mask[x].knight; }
template <       > [[nodiscard]] inline BitBoard coverage<P_wb>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return diagonalAttack(occupancy, x) | antidiagonalAttack(occupancy, x); }
template <       > [[nodiscard]] inline BitBoard coverage<P_wr>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return fileAttack    (occupancy, x) | rankAttack        (occupancy, x); }
template <       > [[nodiscard]] inline BitBoard coverage<P_wq>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return diagonalAttack(occupancy, x) | antidiagonalAttack(occupancy, x) | fileAttack(occupancy, x) | rankAttack(occupancy, x); }
template <       > [[nodiscard]] inline BitBoard coverage<P_wk>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return mask[x].king; }

// Attack function is just coverage interseted with a target bitboard
template < Piece pp > [[nodiscard]] inline BitBoard attack(const Square x, const BitBoard target, const BitBoard occupancy = 0, const Color c = Co_White) { return coverage<pp>(x, occupancy, c) & target; }

#else // MAGIC

namespace MagicBB{

/*!
 * This compact magic BB implemtation is very near the one used
 * in RubiChess and Stockfish
 * It is not really much faster than HQ BB, maybe around +10%
 */

#define BISHOP_INDEX_BITS  9
#define ROOK_INDEX_BITS   12

struct SMagic {
  BitBoard mask, magic;
};

extern SMagic bishop[NbSquare];
extern SMagic rook[NbSquare];

extern BitBoard bishopAttacks[NbSquare][1 << BISHOP_INDEX_BITS];
extern BitBoard rookAttacks  [NbSquare][1 << ROOK_INDEX_BITS  ];

#ifdef __BMI2__
   #define MAGICBISHOPINDEX(m,x) (_pext_u64(m, MagicBB::bishop[x].mask))
   #define MAGICROOKINDEX(m,x)   (_pext_u64(m, MagicBB::rook  [x].mask))
#else
   #define MAGICBISHOPINDEX(m,x) (int)((((m) & MagicBB::bishop[x].mask) * MagicBB::bishop[x].magic) >> (NbSquare - BISHOP_INDEX_BITS))
   #define MAGICROOKINDEX(m,x)   (int)((((m) & MagicBB::rook  [x].mask) * MagicBB::rook  [x].magic) >> (NbSquare - ROOK_INDEX_BITS))
#endif

#define MAGICBISHOPATTACKS(m,x) (MagicBB::bishopAttacks[x][MAGICBISHOPINDEX(m,x)])
#define MAGICROOKATTACKS(m,x)   (MagicBB::rookAttacks  [x][MAGICROOKINDEX(m,x)])

void initMagic();

} // MagicBB

// Next functions define the user API for piece move
template < Piece > [[nodiscard]] inline BitBoard coverage      (const Square  , const BitBoard          , const Color  ) { assert(false); return emptyBitBoard; }
template <       > [[nodiscard]] inline BitBoard coverage<P_wp>(const Square x, const BitBoard          , const Color c) { assert( x >= 0 && x < 64); return mask[x].pawnAttack[c]; }
template <       > [[nodiscard]] inline BitBoard coverage<P_wn>(const Square x, const BitBoard          , const Color  ) { assert( x >= 0 && x < 64); return mask[x].knight; }
template <       > [[nodiscard]] inline BitBoard coverage<P_wb>(const Square x, const BitBoard occupancy, const Color  ) { assert( x >= 0 && x < 64); return MAGICBISHOPATTACKS(occupancy, x); }
template <       > [[nodiscard]] inline BitBoard coverage<P_wr>(const Square x, const BitBoard occupancy, const Color  ) { assert( x >= 0 && x < 64); return MAGICROOKATTACKS  (occupancy, x); }
template <       > [[nodiscard]] inline BitBoard coverage<P_wq>(const Square x, const BitBoard occupancy, const Color  ) { assert( x >= 0 && x < 64); return MAGICBISHOPATTACKS(occupancy, x) | MAGICROOKATTACKS(occupancy, x); }
template <       > [[nodiscard]] inline BitBoard coverage<P_wk>(const Square x, const BitBoard          , const Color  ) { assert( x >= 0 && x < 64); return mask[x].king; }

// Attack function is just coverage interseted with a target bitboard
template < Piece pp > [[nodiscard]] inline BitBoard attack(const Square x, const BitBoard target, const BitBoard occupancy = 0, const Color c = Co_White) { 
   return coverage<pp>(x, occupancy, c) & target; 
}

#endif // MAGIC

// Those are convenient function pointers for coverage and attack
constexpr BitBoard(*const pfCoverage[])(const Square, const BitBoard, const Color) = { &BBTools::coverage<P_wp>, &BBTools::coverage<P_wn>, &BBTools::coverage<P_wb>, &BBTools::coverage<P_wr>, &BBTools::coverage<P_wq>, &BBTools::coverage<P_wk> };
//constexpr BitBoard(*const pfAttack[])  (const Square, const BitBoard, const BitBoard, const Color) = { &BBTools::attack<P_wp>,   &BBTools::attack<P_wn>,   &BBTools::attack<P_wb>,   &BBTools::attack<P_wr>,   &BBTools::attack<P_wq>,   &BBTools::attack<P_wk>   };

// Convenient function to check is a square is under attack or not
[[nodiscard]] bool isAttackedBB (const Position &p, const Square x, Color c);

// Convenient function to return the bitboard of all attacker of a specific square
[[nodiscard]] BitBoard allAttackedBB(const Position &p, const Square x, Color c);
[[nodiscard]] BitBoard allAttackedBB(const Position &p, const Square x);

} // BBTools

// Those are wrapper functions around isAttackedBB
[[nodiscard]] bool isAttacked(const Position & p, const Square k);
[[nodiscard]] bool isAttacked(const Position & p, BitBoard bb);
