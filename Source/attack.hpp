#pragma once

#include "bitboardTools.hpp"
#include "definition.hpp"

#ifdef __BMI2__
#include <immintrin.h>
#endif

namespace BBTools {

/*!
 * Two bitboard attack generation tools can be used here (controlled by WITH_MAGIC define from config.hpp):
 * - hyperbola quintessence (https://www.chessprogramming.org/Hyperbola_Quintessence)
 * - or magic (https://www.chessprogramming.org/Magic_Bitboards) with BMI2 extension if available
 */

// mask variable is filled with many usefull pre-computed bitboard information
// many of then will be usefull during evaluation and move generation
// note that between to not include start and end square
struct Mask {
    BitBoard bbsquare {emptyBitBoard};
    BitBoard kingZone {emptyBitBoard};
    BitBoard knight {emptyBitBoard};
    BitBoard king {emptyBitBoard};
    BitBoard diagonal {emptyBitBoard};
    BitBoard antidiagonal {emptyBitBoard};
    BitBoard file {emptyBitBoard};
    colored<BitBoard> pawnAttack {{emptyBitBoard, emptyBitBoard}};
    colored<BitBoard> push {{emptyBitBoard, emptyBitBoard}};
    colored<BitBoard> dpush {{emptyBitBoard, emptyBitBoard}};
#if !defined(WITH_SMALL_MEMORY)
    BitBoard enpassant {emptyBitBoard};
    colored<BitBoard> frontSpan {{emptyBitBoard, emptyBitBoard}};
    colored<BitBoard> rearSpan {{emptyBitBoard, emptyBitBoard}};
    colored<BitBoard> passerSpan {{emptyBitBoard, emptyBitBoard}};
    colored<BitBoard> attackFrontSpan {{emptyBitBoard, emptyBitBoard}};
    array1d<BitBoard, NbSquare> between {emptyBitBoard};
#endif
};

extern array1d<Mask,NbSquare> mask;
void initMask();

constexpr auto SquareToBitboardTable(const Square k) { return BBTools::mask[k].bbsquare;}

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
template < Piece > [[nodiscard]] FORCE_FINLINE BitBoard coverage      (const Square x, const BitBoard occupancy = 0, const Color c = Co_White) { assert(false); return emptyBitBoard; }
template <       > [[nodiscard]] FORCE_FINLINE BitBoard coverage<P_wp>(const Square x, const BitBoard          , const Color c) { assert(isValidSquare(x)); return mask[x].pawnAttack[c]; }
template <       > [[nodiscard]] FORCE_FINLINE BitBoard coverage<P_wn>(const Square x, const BitBoard          , const Color  ) { assert(isValidSquare(x)); return mask[x].knight; }
template <       > [[nodiscard]] FORCE_FINLINE BitBoard coverage<P_wb>(const Square x, const BitBoard occupancy, const Color  ) { assert(isValidSquare(x)); return diagonalAttack(occupancy, x) | antidiagonalAttack(occupancy, x); }
template <       > [[nodiscard]] FORCE_FINLINE BitBoard coverage<P_wr>(const Square x, const BitBoard occupancy, const Color  ) { assert(isValidSquare(x)); return fileAttack    (occupancy, x) | rankAttack        (occupancy, x); }
template <       > [[nodiscard]] FORCE_FINLINE BitBoard coverage<P_wq>(const Square x, const BitBoard occupancy, const Color  ) { assert(isValidSquare(x)); return diagonalAttack(occupancy, x) | antidiagonalAttack(occupancy, x) | fileAttack(occupancy, x) | rankAttack(occupancy, x); }
template <       > [[nodiscard]] FORCE_FINLINE BitBoard coverage<P_wk>(const Square x, const BitBoard          , const Color  ) { assert(isValidSquare(x)); return mask[x].king; }

// Attack function is just coverage interseted with a target bitboard
template<Piece pp>
[[nodiscard]] FORCE_FINLINE BitBoard attack(const Square x, const BitBoard target, const BitBoard occupancy = 0, const Color c = Co_White) {
   return coverage<pp>(x, occupancy, c) & target;
}

#else // MAGIC

namespace MagicBB {

/*!
 * This compact magic BB implementation is very near the one used
 * in RubiChess and Stockfish
 * It is not really much faster than HQ BB, maybe around +10%
 */

inline constexpr size_t BISHOP_INDEX_BITS = 9;
inline constexpr size_t ROOK_INDEX_BITS = 12;

struct SMagic {
  BitBoard mask {emptyBitBoard};
  BitBoard magic {emptyBitBoard};
};

extern array1d<SMagic, NbSquare> bishopMagic;
extern array1d<SMagic, NbSquare> rookMagic;

extern array2d<BitBoard, NbSquare, 1 << BISHOP_INDEX_BITS> bishopAttacks;
extern array2d<BitBoard, NbSquare, 1 << ROOK_INDEX_BITS> rookAttacks;

#if defined(__BMI2__) && !defined(__znver1) && !defined(__znver2) && !defined(__bdver4) && defined(ENV64BIT)
inline auto MAGICBISHOPINDEX(const BitBoard m, const Square x) { return _pext_u64(m, MagicBB::bishopMagic[x].mask);}
inline auto MAGICROOKINDEX(const BitBoard m, const Square x)    { return _pext_u64(m, MagicBB::rookMagic[x].mask);}
#else
inline auto MAGICBISHOPINDEX(const BitBoard m, const Square x) { return static_cast<int>(((m & MagicBB::bishopMagic[x].mask) * MagicBB::bishopMagic[x].magic) >> (NbSquare - BISHOP_INDEX_BITS));}
inline auto MAGICROOKINDEX(const BitBoard m, const Square x)   { return static_cast<int>(((m & MagicBB::rookMagic[x].mask) * MagicBB::rookMagic[x].magic) >> (NbSquare - ROOK_INDEX_BITS));}
#endif

inline auto MAGICBISHOPATTACKS(const BitBoard m, const Square x) { return MagicBB::bishopAttacks[x][MAGICBISHOPINDEX(m, x)];}
inline auto MAGICROOKATTACKS(const BitBoard m, const Square x)   { return MagicBB::rookAttacks  [x][MAGICROOKINDEX(m, x)];}

void initMagic();

} // namespace MagicBB

// Next functions define the user API for piece move
template < Piece > [[nodiscard]] FORCE_FINLINE BitBoard coverage      (const Square  , const BitBoard          , const Color  ) { assert(false); return emptyBitBoard; }
template <       > [[nodiscard]] FORCE_FINLINE BitBoard coverage<P_wp>(const Square s, const BitBoard          , const Color c) { assert(isValidSquare(s)); return mask[s].pawnAttack[c]; }
template <       > [[nodiscard]] FORCE_FINLINE BitBoard coverage<P_wn>(const Square s, const BitBoard          , const Color  ) { assert(isValidSquare(s)); return mask[s].knight; }
template <       > [[nodiscard]] FORCE_FINLINE BitBoard coverage<P_wb>(const Square s, const BitBoard occupancy, const Color  ) { assert(isValidSquare(s)); return MagicBB::MAGICBISHOPATTACKS(occupancy, s); }
template <       > [[nodiscard]] FORCE_FINLINE BitBoard coverage<P_wr>(const Square s, const BitBoard occupancy, const Color  ) { assert(isValidSquare(s)); return MagicBB::MAGICROOKATTACKS  (occupancy, s); }
template <       > [[nodiscard]] FORCE_FINLINE BitBoard coverage<P_wq>(const Square s, const BitBoard occupancy, const Color  ) { assert(isValidSquare(s)); return MagicBB::MAGICBISHOPATTACKS(occupancy, s) | MagicBB::MAGICROOKATTACKS(occupancy, s); }
template <       > [[nodiscard]] FORCE_FINLINE BitBoard coverage<P_wk>(const Square s, const BitBoard          , const Color  ) { assert(isValidSquare(s)); return mask[s].king; }

// Attack function is just coverage interseted with a target bitboard
template<Piece pp>
[[nodiscard]] FORCE_FINLINE BitBoard attack(const Square s,
                                            const BitBoard target,
                                            const BitBoard occupancy = 0,
                                            const Color c = Co_White) { // color is only important/needed for pawns
   return target ? target & coverage<pp>(s, occupancy, c) : emptyBitBoard;
}

#endif // MAGIC

BitBoard between(const Square sq1, const Square sq2);

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
[[nodiscard]] bool isPosInCheck(const Position& p);
