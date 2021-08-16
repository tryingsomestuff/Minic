#pragma once

#include "bitboard.hpp"
#include "definition.hpp"
#include "pieceTools.hpp"

namespace BBTools {

/*!
 * Many little helper for evaluation (pawn structure mostly)
 * Most of them are taken from Topple chess engine by Vincent Tang (a.k.a Konsolas)
 */

[[nodiscard]] inline constexpr BitBoard _shiftSouth    (BitBoard b) { return b >> 8; }
[[nodiscard]] inline constexpr BitBoard _shiftNorth    (BitBoard b) { return b << 8; }
[[nodiscard]] inline constexpr BitBoard _shiftWest     (BitBoard b) { return b >> 1 & ~BB::fileH; }
[[nodiscard]] inline constexpr BitBoard _shiftEast     (BitBoard b) { return b << 1 & ~BB::fileA; }
[[nodiscard]] inline constexpr BitBoard _shiftNorthEast(BitBoard b) { return b << 9 & ~BB::fileA; }
[[nodiscard]] inline constexpr BitBoard _shiftNorthWest(BitBoard b) { return b << 7 & ~BB::fileH; }
[[nodiscard]] inline constexpr BitBoard _shiftSouthEast(BitBoard b) { return b >> 7 & ~BB::fileA; }
[[nodiscard]] inline constexpr BitBoard _shiftSouthWest(BitBoard b) { return b >> 9 & ~BB::fileH; }

[[nodiscard]] inline constexpr BitBoard adjacent(BitBoard b) { return _shiftWest(b) | _shiftEast(b); }

template<Color C> [[nodiscard]] inline constexpr BitBoard shiftN(const BitBoard b) {
   return C == Co_White ? BBTools::_shiftNorth(b) : BBTools::_shiftSouth(b);
}
template<Color C> [[nodiscard]] inline constexpr BitBoard shiftS(const BitBoard b) {
   return C != Co_White ? BBTools::_shiftNorth(b) : BBTools::_shiftSouth(b);
}
template<Color C> [[nodiscard]] inline constexpr BitBoard shiftSW(const BitBoard b) {
   return C == Co_White ? BBTools::_shiftSouthWest(b) : BBTools::_shiftNorthWest(b);
}
template<Color C> [[nodiscard]] inline constexpr BitBoard shiftSE(const BitBoard b) {
   return C == Co_White ? BBTools::_shiftSouthEast(b) : BBTools::_shiftNorthEast(b);
}
template<Color C> [[nodiscard]] inline constexpr BitBoard shiftNW(const BitBoard b) {
   return C != Co_White ? BBTools::_shiftSouthWest(b) : BBTools::_shiftNorthWest(b);
}
template<Color C> [[nodiscard]] inline constexpr BitBoard shiftNE(const BitBoard b) {
   return C != Co_White ? BBTools::_shiftSouthEast(b) : BBTools::_shiftNorthEast(b);
}

template<Color> [[nodiscard]] inline constexpr BitBoard fillForward(BitBoard b);
template<> [[nodiscard]] inline constexpr BitBoard      fillForward<Co_White>(BitBoard b) {
    b |= (b << 8u);    
    b |= (b << 16u);    
    b |= (b << 32u);    
    return b;
}
template<> [[nodiscard]] inline constexpr BitBoard fillForward<Co_Black>(BitBoard b) {
    b |= (b >> 8u);    
    b |= (b >> 16u);    
    b |= (b >> 32u);    
    return b;
}

template<Color> [[nodiscard]] inline constexpr BitBoard fillForwardOccluded(BitBoard b, BitBoard open);
template<> [[nodiscard]] inline constexpr BitBoard      fillForwardOccluded<Co_White>(BitBoard b, BitBoard open) {
    b |= open & (b << 8u);
    open &= (open << 8u);
    b |= open & (b << 16u);
    open &= (open << 16u);
    b |= open & (b << 32u);
    return b;
}
template<> [[nodiscard]] inline constexpr BitBoard fillForwardOccluded<Co_Black>(BitBoard b, BitBoard open) {
    b |= open & (b >> 8u);
    open &= (open >> 8u);
    b |= open & (b >> 16u);
    open &= (open >> 16u);
    b |= open & (b >> 32u);
    return b;
}

template<Color C> [[nodiscard]] inline constexpr BitBoard frontSpan(BitBoard b) { return fillForward<C>(shiftN<C>(b)); }
template<Color C> [[nodiscard]] inline constexpr BitBoard rearSpan(BitBoard b) { return frontSpan<~C>(b); }
template<Color C> [[nodiscard]] inline constexpr BitBoard pawnSemiOpen(BitBoard own, BitBoard opp) { return own & ~frontSpan<~C>(opp); }

[[nodiscard]] inline constexpr BitBoard fillFile(BitBoard b) { return fillForward<Co_White>(b) | fillForward<Co_Black>(b); }
[[nodiscard]] inline constexpr BitBoard openFiles(BitBoard w, BitBoard b) { return ~fillFile(w) & ~fillFile(b); }

template<Color C> [[nodiscard]] inline constexpr BitBoard pawnAttacks(BitBoard b) { return shiftNW<C>(b) | shiftNE<C>(b); }
template<Color C> [[nodiscard]] inline constexpr BitBoard pawnDoubleAttacks(BitBoard b) { return shiftNW<C>(b) & shiftNE<C>(b); }
template<Color C> [[nodiscard]] inline constexpr BitBoard pawnSingleAttacks(BitBoard b) { return shiftNW<C>(b) ^ shiftNE<C>(b); }

template<Color C> [[nodiscard]] inline constexpr BitBoard pawnHoles(BitBoard b) { return ~fillForward<C>(pawnAttacks<C>(b)); }
template<Color C> [[nodiscard]] inline constexpr BitBoard pawnDoubled(BitBoard b) { return frontSpan<C>(b) & b; }

[[nodiscard]] inline constexpr BitBoard pawnIsolated(BitBoard b) { return b & ~fillFile(_shiftEast(b)) & ~fillFile(_shiftWest(b)); }

template<Color C> [[nodiscard]] inline constexpr BitBoard pawnBackward(BitBoard own, BitBoard opp) {
   return shiftN<~C>((shiftN<C>(own) & ~opp) & ~fillForward<C>(pawnAttacks<C>(own)) & (pawnAttacks<~C>(opp)));
}

template<Color C> [[nodiscard]] inline constexpr BitBoard pawnForwardCoverage(BitBoard bb) { 
    const BitBoard spans = frontSpan<C>(bb); 
    return spans | _shiftEast(spans) | _shiftWest(spans);
}

template<Color C> [[nodiscard]] inline constexpr BitBoard pawnPassed(BitBoard own, BitBoard opp) { return own & ~pawnForwardCoverage<~C>(opp); }

template<Color C> [[nodiscard]] inline constexpr BitBoard pawnCandidates(BitBoard own, BitBoard opp) { 
   return pawnSemiOpen<C>(own, opp) &
          shiftN<~C>((pawnSingleAttacks<C>(own) & pawnSingleAttacks<~C>(opp)) | (pawnDoubleAttacks<C>(own) & pawnDoubleAttacks<~C>(opp)));
}

template<Color C> [[nodiscard]] inline constexpr BitBoard pawnUndefendable(BitBoard own, BitBoard other) {
    return own & ~pawnAttacks<C>(fillForwardOccluded<C>(own, ~other)) & BB::advancedRanks;
}

[[nodiscard]] inline constexpr BitBoard pawnAlone(BitBoard b) { return b & ~(adjacent(b) | pawnAttacks<Co_White>(b) | pawnAttacks<Co_Black>(b)); }

template<Color C> [[nodiscard]] inline constexpr BitBoard pawnDetached(BitBoard own, BitBoard other) {
    return pawnUndefendable<C>(own, other) & pawnAlone(own);
}

// return first square of the bitboard only
// beware emptyness of the bitboard is only checked in debug mode !
[[nodiscard]] Square SquareFromBitBoard(const BitBoard& b);

// Position relative helpers
inline void unSetBit(Position& p, Square k) {
   assert(isValidSquare(k));
   BB::_unSetBit(p._pieces(p.board_const(k)), k);
}
inline void unSetBit(Position& p, Square k, Piece pp) {
   assert(isValidSquare(k));
   BB::_unSetBit(p._pieces(pp), k);
}
inline void setBit(Position& p, Square k, Piece pp) {
   assert(isValidSquare(k));
   BB::_setBit(p._pieces(pp), k);
}
void clearBitBoards(Position& p);
void setBitBoards(Position& p);

} // namespace BBTools
