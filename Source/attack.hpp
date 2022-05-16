#pragma once

#include "bitboardTools.hpp"
#include "definition.hpp"

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
    BitBoard bbsquare, kingZone, pawnAttack[2], push[2], dpush[2], knight, king, frontSpan[2], between[NbSquare], diagonal, antidiagonal, file;
#if !defined(ARDUINO) && !defined(ESP32)
    BitBoard enpassant, rearSpan[2], passerSpan[2], attackFrontSpan[2];
#endif
   Mask():
       bbsquare(emptyBitBoard),
       kingZone(emptyBitBoard),
       pawnAttack {emptyBitBoard, emptyBitBoard},
       push {emptyBitBoard, emptyBitBoard},
       dpush {emptyBitBoard, emptyBitBoard},
       knight(emptyBitBoard),
       king(emptyBitBoard),
       frontSpan {emptyBitBoard},
       between {emptyBitBoard},
       diagonal(emptyBitBoard),
       antidiagonal(emptyBitBoard),
       file(emptyBitBoard)
#if !defined(ARDUINO) && !defined(ESP32)       
       ,enpassant(emptyBitBoard),
       rearSpan {emptyBitBoard},
       passerSpan {emptyBitBoard},
       attackFrontSpan {emptyBitBoard} 
#endif
       {}
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
template < Piece > [[nodiscard]] inline BitBoard coverage      (const Square x, const BitBoard occupancy = 0, const Color c = Co_White) { assert(false); return emptyBitBoard; }
template <       > [[nodiscard]] inline BitBoard coverage<P_wp>(const Square x, const BitBoard          , const Color c) { assert(isValidSquare(x)); return mask[x].pawnAttack[c]; }
template <       > [[nodiscard]] inline BitBoard coverage<P_wn>(const Square x, const BitBoard          , const Color  ) { assert(isValidSquare(x)); return mask[x].knight; }
template <       > [[nodiscard]] inline BitBoard coverage<P_wb>(const Square x, const BitBoard occupancy, const Color  ) { assert(isValidSquare(x)); return diagonalAttack(occupancy, x) | antidiagonalAttack(occupancy, x); }
template <       > [[nodiscard]] inline BitBoard coverage<P_wr>(const Square x, const BitBoard occupancy, const Color  ) { assert(isValidSquare(x)); return fileAttack    (occupancy, x) | rankAttack        (occupancy, x); }
template <       > [[nodiscard]] inline BitBoard coverage<P_wq>(const Square x, const BitBoard occupancy, const Color  ) { assert(isValidSquare(x)); return diagonalAttack(occupancy, x) | antidiagonalAttack(occupancy, x) | fileAttack(occupancy, x) | rankAttack(occupancy, x); }
template <       > [[nodiscard]] inline BitBoard coverage<P_wk>(const Square x, const BitBoard          , const Color  ) { assert(isValidSquare(x)); return mask[x].king; }

// Attack function is just coverage interseted with a target bitboard
template<Piece pp>
[[nodiscard]] inline BitBoard attack(const Square x, const BitBoard target, const BitBoard occupancy = 0, const Color c = Co_White) {
   return coverage<pp>(x, occupancy, c) & target;
}

#else // MAGIC

namespace MagicBB {

/*!
 * This compact magic BB implementation is very near the one used
 * in RubiChess and Stockfish
 * It is not really much faster than HQ BB, maybe around +10%
 */

#define BISHOP_INDEX_BITS 9
#define ROOK_INDEX_BITS   12

struct SMagic {
  BitBoard mask, magic;
};

extern SMagic bishopMagic[NbSquare];
extern SMagic rookMagic[NbSquare];

extern BitBoard bishopAttacks[NbSquare][1 << BISHOP_INDEX_BITS];
extern BitBoard rookAttacks[NbSquare][1 << ROOK_INDEX_BITS];

#if defined(__BMI2__) && !defined(__znver1) && !defined(__znver2) && !defined(__bdver4) && defined(ENV64BIT)
#define MAGICBISHOPINDEX(m, x) (_pext_u64(m, MagicBB::bishopMagic[x].mask))
#define MAGICROOKINDEX(m, x)   (_pext_u64(m, MagicBB::rookMagic[x].mask))
#else
#define MAGICBISHOPINDEX(m, x) static_cast<int>((((m)&MagicBB::bishopMagic[x].mask) * MagicBB::bishopMagic[x].magic) >> (NbSquare - BISHOP_INDEX_BITS))
#define MAGICROOKINDEX(m, x)   static_cast<int>((((m)&MagicBB::rookMagic[x].mask) * MagicBB::rookMagic[x].magic) >> (NbSquare - ROOK_INDEX_BITS))
#endif

#define MAGICBISHOPATTACKS(m, x) (MagicBB::bishopAttacks[x][MAGICBISHOPINDEX(m, x)])
#define MAGICROOKATTACKS(m, x)   (MagicBB::rookAttacks  [x][MAGICROOKINDEX(m, x)])

void initMagic();

} // namespace MagicBB

// Next functions define the user API for piece move
template < Piece > [[nodiscard]] inline BitBoard coverage      (const Square  , const BitBoard          , const Color  ) { assert(false); return emptyBitBoard; }
template <       > [[nodiscard]] inline BitBoard coverage<P_wp>(const Square s, const BitBoard          , const Color c) { assert(isValidSquare(s)); return mask[s].pawnAttack[c]; }
template <       > [[nodiscard]] inline BitBoard coverage<P_wn>(const Square s, const BitBoard          , const Color  ) { assert(isValidSquare(s)); return mask[s].knight; }
template <       > [[nodiscard]] inline BitBoard coverage<P_wb>(const Square s, const BitBoard occupancy, const Color  ) { assert(isValidSquare(s)); return MAGICBISHOPATTACKS(occupancy, s); }
template <       > [[nodiscard]] inline BitBoard coverage<P_wr>(const Square s, const BitBoard occupancy, const Color  ) { assert(isValidSquare(s)); return MAGICROOKATTACKS  (occupancy, s); }
template <       > [[nodiscard]] inline BitBoard coverage<P_wq>(const Square s, const BitBoard occupancy, const Color  ) { assert(isValidSquare(s)); return MAGICBISHOPATTACKS(occupancy, s) | MAGICROOKATTACKS(occupancy, s); }
template <       > [[nodiscard]] inline BitBoard coverage<P_wk>(const Square s, const BitBoard          , const Color  ) { assert(isValidSquare(s)); return mask[s].king; }

// Attack function is just coverage interseted with a target bitboard
template<Piece pp>
[[nodiscard]] inline BitBoard attack(const Square s,
                                     const BitBoard target,
                                     const BitBoard occupancy = 0,
                                     const Color c = Co_White) { // color is only important/needed for pawns
   return coverage<pp>(s, occupancy, c) & target;
}

#endif // MAGIC

// Those are convenient function pointers for coverage and attack
constexpr BitBoard (*const pfCoverage[])(const Square, const BitBoard, const Color) = {&BBTools::coverage<P_wp>, &BBTools::coverage<P_wn>,
                                                                                       &BBTools::coverage<P_wb>, &BBTools::coverage<P_wr>,
                                                                                       &BBTools::coverage<P_wq>, &BBTools::coverage<P_wk>};
//constexpr BitBoard(*const pfAttack[])  (const Square, const BitBoard, const BitBoard, const Color) = { &BBTools::attack<P_wp>,   &BBTools::attack<P_wn>,   &BBTools::attack<P_wb>,   &BBTools::attack<P_wr>,   &BBTools::attack<P_wq>,   &BBTools::attack<P_wk>   };

// Convenient function to check is a square is under attack or not
[[nodiscard]] bool isAttackedBB(const Position &p, const Square s, const Color c);

// Convenient function to return the bitboard of all attacker of a specific square
[[nodiscard]] BitBoard allAttackedBB(const Position &p, const Square s, const Color c);
[[nodiscard]] BitBoard allAttackedBB(const Position &p, const Square s);

} // namespace BBTools

// Those are wrapper functions around isAttackedBB
[[nodiscard]] bool isAttacked(const Position &p, const Square s);
[[nodiscard]] bool isAttacked(const Position &p, BitBoard bb);
