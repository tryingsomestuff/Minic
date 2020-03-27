#pragma once

#include "definition.hpp"

#include "bitboard.hpp"
#include "pieceTools.hpp"

namespace BBTools {

/* Many little helper for evaluation (pawn structure mostly)
 * Most of then are taken from Topple chess engine by Vincent Tang (a.k.a Konsolas)
 */

inline constexpr BitBoard _shiftSouth    (BitBoard b) { return b >> 8; }
inline constexpr BitBoard _shiftNorth    (BitBoard b) { return b << 8; }
inline constexpr BitBoard _shiftWest     (BitBoard b) { return b >> 1 & ~fileH; }
inline constexpr BitBoard _shiftEast     (BitBoard b) { return b << 1 & ~fileA; }
inline constexpr BitBoard _shiftNorthEast(BitBoard b) { return b << 9 & ~fileA; }
inline constexpr BitBoard _shiftNorthWest(BitBoard b) { return b << 7 & ~fileH; }
inline constexpr BitBoard _shiftSouthEast(BitBoard b) { return b >> 7 & ~fileA; }
inline constexpr BitBoard _shiftSouthWest(BitBoard b) { return b >> 9 & ~fileH; }

template<Color C> inline constexpr BitBoard shiftN  (const BitBoard b) { return C==Co_White? BBTools::_shiftNorth(b)     : BBTools::_shiftSouth(b);    }
template<Color C> inline constexpr BitBoard shiftS  (const BitBoard b) { return C!=Co_White? BBTools::_shiftNorth(b)     : BBTools::_shiftSouth(b);    }
template<Color C> inline constexpr BitBoard shiftSW (const BitBoard b) { return C==Co_White? BBTools::_shiftSouthWest(b) : BBTools::_shiftNorthWest(b);}
template<Color C> inline constexpr BitBoard shiftSE (const BitBoard b) { return C==Co_White? BBTools::_shiftSouthEast(b) : BBTools::_shiftNorthEast(b);}
template<Color C> inline constexpr BitBoard shiftNW (const BitBoard b) { return C!=Co_White? BBTools::_shiftSouthWest(b) : BBTools::_shiftNorthWest(b);}
template<Color C> inline constexpr BitBoard shiftNE (const BitBoard b) { return C!=Co_White? BBTools::_shiftSouthEast(b) : BBTools::_shiftNorthEast(b);}

template<Color  > inline constexpr BitBoard fillForward(BitBoard b);
template<>        inline constexpr BitBoard fillForward<Co_White>(BitBoard b) {  b |= (b << 8u);    b |= (b << 16u);    b |= (b << 32u);    return b;}
template<>        inline constexpr BitBoard fillForward<Co_Black>(BitBoard b) {  b |= (b >> 8u);    b |= (b >> 16u);    b |= (b >> 32u);    return b;}
template<Color C> inline constexpr BitBoard frontSpan(BitBoard b) { return fillForward<C>(shiftN<C>(b));}
template<Color C> inline constexpr BitBoard rearSpan (BitBoard b) { return frontSpan<~C>(b);}
template<Color C> inline constexpr BitBoard pawnSemiOpen(BitBoard own, BitBoard opp) { return own & ~frontSpan<~C>(opp);}

inline constexpr BitBoard fillFile(BitBoard b) { return fillForward<Co_White>(b) | fillForward<Co_Black>(b);}
inline constexpr BitBoard openFiles(BitBoard w, BitBoard b) { return ~fillFile(w) & ~fillFile(b);}

template<Color C> inline constexpr BitBoard pawnAttacks(BitBoard b)       { return shiftNW<C>(b) | shiftNE<C>(b);}
template<Color C> inline constexpr BitBoard pawnDoubleAttacks(BitBoard b) { return shiftNW<C>(b) & shiftNE<C>(b);}
template<Color C> inline constexpr BitBoard pawnSingleAttacks(BitBoard b) { return shiftNW<C>(b) ^ shiftNE<C>(b);}

template<Color C> inline constexpr BitBoard pawnHoles(BitBoard b)   { return ~fillForward<C>(pawnAttacks<C>(b));}
template<Color C> inline constexpr BitBoard pawnDoubled(BitBoard b) { return frontSpan<C>(b) & b;}

inline constexpr BitBoard pawnIsolated(BitBoard b) { return b & ~fillFile(_shiftEast(b)) & ~fillFile(_shiftWest(b));}

template<Color C> inline constexpr BitBoard pawnBackward       (BitBoard own, BitBoard opp) {return shiftN<~C>( (shiftN<C>(own)& ~opp) & ~fillForward<C>(pawnAttacks<C>(own)) & (pawnAttacks<~C>(opp)));}
template<Color C> inline constexpr BitBoard pawnForwardCoverage(BitBoard bb               ) { BitBoard spans = frontSpan<C>(bb); return spans | _shiftEast(spans) | _shiftWest(spans);}
template<Color C> inline constexpr BitBoard pawnPassed         (BitBoard own, BitBoard opp) { return own & ~pawnForwardCoverage<~C>(opp);}
template<Color C> inline constexpr BitBoard pawnCandidates     (BitBoard own, BitBoard opp) { return pawnSemiOpen<C>(own, opp) & shiftN<~C>((pawnSingleAttacks<C>(own) & pawnSingleAttacks<~C>(opp)) | (pawnDoubleAttacks<C>(own) & pawnDoubleAttacks<~C>(opp)));}
//template<Color C> inline constexpr BitBoard pawnStraggler      (BitBoard own, BitBoard opp, BitBoard own_backwards) { return own_backwards & pawnSemiOpen<C>(own, opp) & (C ? 0x00ffff0000000000ull : 0x0000000000ffff00ull);} ///@todo use this !

// return first square of the bitboard only
// beware emptyness of the bitboard is only checked in debug mode !
Square SquareFromBitBoard(const BitBoard & b);

// Position relative helpers
inline void unSetBit(Position & p, Square k)           { assert(k >= 0 && k < 64); ::_unSetBit(p.allB[PieceTools::getPieceIndex(p, k)], k);}
inline void unSetBit(Position & p, Square k, Piece pp) { assert(k >= 0 && k < 64); ::_unSetBit(p.allB[pp + PieceShift]                , k);}
inline void setBit  (Position & p, Square k, Piece pp) { assert(k >= 0 && k < 64); ::_setBit  (p.allB[pp + PieceShift]                , k);}
void initBitBoards(Position & p);
void setBitBoards (Position & p);

} // BBTools
