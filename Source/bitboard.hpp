#pragma once

#include "definition.hpp"

/*!
 * Main bitboard utilities
 * especially countBit (using POPCOUNT) and popBit (bsf)
 * are optimized for the platform being used
 */

#ifdef __MINGW32__
#define POPCOUNT(x) static_cast<int>(__builtin_popcountll(x))
inline int bitScanForward(BitBoard bb) {
   assert(isNotEmpty(bb));
   return __builtin_ctzll(bb);
}
#define bsf(x, i)     (i = bitScanForward(x))
#define swapbits(x)   (__builtin_bswap64(x))
#define swapbits32(x) (__builtin_bswap32(x))
#else // __MINGW32__
#ifdef _WIN32
#ifdef _WIN64
#define POPCOUNT(x)   __popcnt64(x)
#define bsf(x, i)     _BitScanForward64(&i, x)
#define swapbits(x)   (_byteswap_uint64(x))
#define swapbits32(x) (_byteswap_ulong(x))
#else // we are _WIN32 but not _WIN64
inline int popcount(uint64_t b) {
   b = (b & 0x5555555555555555LU) + (b >> 1 & 0x5555555555555555LU);
   b = (b & 0x3333333333333333LU) + (b >> 2 & 0x3333333333333333LU);
   b = b + (b >> 4) & 0x0F0F0F0F0F0F0F0FLU;
   b = b + (b >> 8);
   b = b + (b >> 16);
   b = b + (b >> 32) & 0x0000007F;
   return static_cast<int>(b);
}
inline const int index64[NbSquare] = {
   0,  1, 48,  2, 57, 49, 28,  3,
   61, 58, 50, 42, 38, 29, 17,  4,
   62, 55, 59, 36, 53, 51, 43, 22,
   45, 39, 33, 30, 24, 18, 12,  5,
   63, 47, 56, 27, 60, 41, 37, 16,
   54, 35, 52, 21, 44, 32, 23, 11,
   46, 26, 40, 15, 34, 20, 31, 10,
   25, 14, 19,  9, 13,  8,  7,  6
};
inline int bitScanForward(int64_t bb) {
   const uint64_t debruijn64 = 0x03f79d71b4cb0a89;
   assert(isNotEmpty(bb));
   return index64[((bb & -bb) * debruijn64) >> 58];
}
#define POPCOUNT(x)   popcount(x)
#define bsf(x, i)     (i = bitScanForward(x))
#define swapbits(x)   (_byteswap_uint64(x))
#define swapbits32(x) (_byteswap_ulong(x))
#endif // _WIN64
#else  // _WIN32 (thus linux)
#define POPCOUNT(x)   static_cast<int>(__builtin_popcountll(x))
inline int bitScanForward(BitBoard bb) {
   assert(isNotEmpty(bb));
   return __builtin_ctzll(bb);
}
#define bsf(x, i)     (i = bitScanForward(x))
#define swapbits(x)   (__builtin_bswap64(x))
#define swapbits32(x) (__builtin_bswap32(x))
#endif // linux
#endif // __MINGW32__

// Hard to say which one is better, bit shifting or precalculated access to mask data ...
#define SquareToBitboard(k)      static_cast<BitBoard>(1ull << (k))
#define SquareToBitboardTable(k) BBTools::mask[k].bbsquare

namespace BB {

enum BBSq : BitBoard { BBSq_a1 = SquareToBitboard(Sq_a1),BBSq_b1 = SquareToBitboard(Sq_b1),BBSq_c1 = SquareToBitboard(Sq_c1),BBSq_d1 = SquareToBitboard(Sq_d1),BBSq_e1 = SquareToBitboard(Sq_e1),BBSq_f1 = SquareToBitboard(Sq_f1),BBSq_g1 = SquareToBitboard(Sq_g1),BBSq_h1 = SquareToBitboard(Sq_h1),
                       BBSq_a2 = SquareToBitboard(Sq_a2),BBSq_b2 = SquareToBitboard(Sq_b2),BBSq_c2 = SquareToBitboard(Sq_c2),BBSq_d2 = SquareToBitboard(Sq_d2),BBSq_e2 = SquareToBitboard(Sq_e2),BBSq_f2 = SquareToBitboard(Sq_f2),BBSq_g2 = SquareToBitboard(Sq_g2),BBSq_h2 = SquareToBitboard(Sq_h2),
                       BBSq_a3 = SquareToBitboard(Sq_a3),BBSq_b3 = SquareToBitboard(Sq_b3),BBSq_c3 = SquareToBitboard(Sq_c3),BBSq_d3 = SquareToBitboard(Sq_d3),BBSq_e3 = SquareToBitboard(Sq_e3),BBSq_f3 = SquareToBitboard(Sq_f3),BBSq_g3 = SquareToBitboard(Sq_g3),BBSq_h3 = SquareToBitboard(Sq_h3),
                       BBSq_a4 = SquareToBitboard(Sq_a4),BBSq_b4 = SquareToBitboard(Sq_b4),BBSq_c4 = SquareToBitboard(Sq_c4),BBSq_d4 = SquareToBitboard(Sq_d4),BBSq_e4 = SquareToBitboard(Sq_e4),BBSq_f4 = SquareToBitboard(Sq_f4),BBSq_g4 = SquareToBitboard(Sq_g4),BBSq_h4 = SquareToBitboard(Sq_h4),
                       BBSq_a5 = SquareToBitboard(Sq_a5),BBSq_b5 = SquareToBitboard(Sq_b5),BBSq_c5 = SquareToBitboard(Sq_c5),BBSq_d5 = SquareToBitboard(Sq_d5),BBSq_e5 = SquareToBitboard(Sq_e5),BBSq_f5 = SquareToBitboard(Sq_f5),BBSq_g5 = SquareToBitboard(Sq_g5),BBSq_h5 = SquareToBitboard(Sq_h5),
                       BBSq_a6 = SquareToBitboard(Sq_a6),BBSq_b6 = SquareToBitboard(Sq_b6),BBSq_c6 = SquareToBitboard(Sq_c6),BBSq_d6 = SquareToBitboard(Sq_d6),BBSq_e6 = SquareToBitboard(Sq_e6),BBSq_f6 = SquareToBitboard(Sq_f6),BBSq_g6 = SquareToBitboard(Sq_g6),BBSq_h6 = SquareToBitboard(Sq_h6),
                       BBSq_a7 = SquareToBitboard(Sq_a7),BBSq_b7 = SquareToBitboard(Sq_b7),BBSq_c7 = SquareToBitboard(Sq_c7),BBSq_d7 = SquareToBitboard(Sq_d7),BBSq_e7 = SquareToBitboard(Sq_e7),BBSq_f7 = SquareToBitboard(Sq_f7),BBSq_g7 = SquareToBitboard(Sq_g7),BBSq_h7 = SquareToBitboard(Sq_h7),
                       BBSq_a8 = SquareToBitboard(Sq_a8),BBSq_b8 = SquareToBitboard(Sq_b8),BBSq_c8 = SquareToBitboard(Sq_c8),BBSq_d8 = SquareToBitboard(Sq_d8),BBSq_e8 = SquareToBitboard(Sq_e8),BBSq_f8 = SquareToBitboard(Sq_f8),BBSq_g8 = SquareToBitboard(Sq_g8),BBSq_h8 = SquareToBitboard(Sq_h8)};

inline constexpr BitBoard whiteSquare = 0x55AA55AA55AA55AA;
inline constexpr BitBoard blackSquare = 0xAA55AA55AA55AA55;
//inline constexpr BitBoard whiteSideSquare = 0x00000000FFFFFFFF;
//inline constexpr BitBoard blackSideSquare = 0xFFFFFFFF00000000;
inline constexpr BitBoard fileA    = 0x0101010101010101;
inline constexpr BitBoard fileB    = 0x0202020202020202;
inline constexpr BitBoard fileC    = 0x0404040404040404;
inline constexpr BitBoard fileD    = 0x0808080808080808;
inline constexpr BitBoard fileE    = 0x1010101010101010;
inline constexpr BitBoard fileF    = 0x2020202020202020;
inline constexpr BitBoard fileG    = 0x4040404040404040;
inline constexpr BitBoard fileH    = 0x8080808080808080;
inline constexpr BitBoard files[8] = {fileA, fileB, fileC, fileD, fileE, fileF, fileG, fileH};
inline constexpr BitBoard rank1    = 0x00000000000000ff;
inline constexpr BitBoard rank2    = 0x000000000000ff00;
inline constexpr BitBoard rank3    = 0x0000000000ff0000;
inline constexpr BitBoard rank4    = 0x00000000ff000000;
inline constexpr BitBoard rank5    = 0x000000ff00000000;
inline constexpr BitBoard rank6    = 0x0000ff0000000000;
inline constexpr BitBoard rank7    = 0x00ff000000000000;
inline constexpr BitBoard rank8    = 0xff00000000000000;
inline constexpr BitBoard ranks[8] = {rank1, rank2, rank3, rank4, rank5, rank6, rank7, rank8};
//inline constexpr BitBoard diagA1H8 = 0x8040201008040201;
//inline constexpr BitBoard diagA8H1 = 0x0102040810204080;
//inline constexpr BitBoard center = BBSq_d4 | BBSq_d5 | BBSq_e4 | BBSq_e5;
inline constexpr BitBoard advancedRanks = 0x0000ffffffff0000;
inline constexpr BitBoard rank1_or_rank8 = BB::rank1 | BB::rank8;
inline constexpr BitBoard extendedCenter = BBSq_c3 | BBSq_c4 | BBSq_c5 | BBSq_c6 |
                                           BBSq_d3 | BBSq_d4 | BBSq_d5 | BBSq_d6 |
                                           BBSq_e3 | BBSq_e4 | BBSq_e5 | BBSq_e6 |
                                           BBSq_f3 | BBSq_f4 | BBSq_f5 | BBSq_f6;

inline constexpr BitBoard seventhRank[2] = {rank7, rank2};

inline constexpr BitBoard holesZone[2] = {rank2 | rank3 | rank4 | rank5, rank4 | rank5 | rank6 | rank7};
inline constexpr BitBoard queenSide    = fileA | fileB | fileC | fileD;
inline constexpr BitBoard centerFiles  = fileC | fileD | fileE | fileF;
inline constexpr BitBoard kingSide     = fileE | fileF | fileG | fileH;
inline constexpr BitBoard kingFlank[8] = {queenSide ^ fileD, queenSide, queenSide, centerFiles, centerFiles, kingSide, kingSide, kingSide ^ fileE};

inline void _setBit  (BitBoard& b, const Square k) { b |= SquareToBitboard(k); }
inline void _unSetBit(BitBoard& b, const Square k) { b &= ~SquareToBitboard(k); }

[[nodiscard]] inline ScoreType countBit(const BitBoard& b) { return ScoreType(POPCOUNT(b)); }

[[nodiscard]] inline Square popBit(BitBoard& b) {
   assert(isNotEmpty(b));
#ifdef _WIN64
    unsigned long i = 0ull;
#else
    int i = 0;
#endif
    bsf(b, i);
    b &= b - 1;
    return static_cast<Square>(i);
}

/*
[[nodiscard]] constexpr bool moreThanOne(const BitBoard b) {
  return b & (b - 1);
}
*/

} // namespace BB