#pragma once

#include "bitboard.hpp"
#include "definition.hpp"
#include "pieceTools.hpp"

namespace BBTools {

/*!
 * Many little helper for evaluation (pawn structure mostly)
 * Most of them are taken from Topple chess engine by Vincent Tang (a.k.a Konsolas)
 */

[[nodiscard]] constexpr BitBoard _shiftSouth    (const BitBoard b) { return b >> 8; }
[[nodiscard]] constexpr BitBoard _shiftNorth    (const BitBoard b) { return b << 8; }
[[nodiscard]] constexpr BitBoard _shiftWest     (const BitBoard b) { return b >> 1 & ~BB::fileH; }
[[nodiscard]] constexpr BitBoard _shiftEast     (const BitBoard b) { return b << 1 & ~BB::fileA; }
[[nodiscard]] constexpr BitBoard _shiftNorthEast(const BitBoard b) { return b << 9 & ~BB::fileA; }
[[nodiscard]] constexpr BitBoard _shiftNorthWest(const BitBoard b) { return b << 7 & ~BB::fileH; }
[[nodiscard]] constexpr BitBoard _shiftSouthEast(const BitBoard b) { return b >> 7 & ~BB::fileA; }
[[nodiscard]] constexpr BitBoard _shiftSouthWest(const BitBoard b) { return b >> 9 & ~BB::fileH; }

[[nodiscard]] constexpr BitBoard adjacent(const BitBoard b) { return _shiftWest(b) | _shiftEast(b); }

template<Color C> [[nodiscard]] constexpr BitBoard shiftN(const BitBoard b) {
   return C == Co_White ? BBTools::_shiftNorth(b) : BBTools::_shiftSouth(b);
}
template<Color C> [[nodiscard]] constexpr BitBoard shiftS(const BitBoard b) {
   return C != Co_White ? BBTools::_shiftNorth(b) : BBTools::_shiftSouth(b);
}
template<Color C> [[nodiscard]] constexpr BitBoard shiftSW(const BitBoard b) {
   return C == Co_White ? BBTools::_shiftSouthWest(b) : BBTools::_shiftNorthWest(b);
}
template<Color C> [[nodiscard]] constexpr BitBoard shiftSE(const BitBoard b) {
   return C == Co_White ? BBTools::_shiftSouthEast(b) : BBTools::_shiftNorthEast(b);
}
template<Color C> [[nodiscard]] constexpr BitBoard shiftNW(const BitBoard b) {
   return C != Co_White ? BBTools::_shiftSouthWest(b) : BBTools::_shiftNorthWest(b);
}
template<Color C> [[nodiscard]] constexpr BitBoard shiftNE(const BitBoard b) {
   return C != Co_White ? BBTools::_shiftSouthEast(b) : BBTools::_shiftNorthEast(b);
}

template<Color> [[nodiscard]] constexpr BitBoard fillForward(BitBoard b);
template<> [[nodiscard]] constexpr BitBoard      fillForward<Co_White>(BitBoard b) {
    b |= (b << 8u);
    b |= (b << 16u);
    b |= (b << 32u);
    return b;
}
template<> [[nodiscard]] constexpr BitBoard fillForward<Co_Black>(BitBoard b) {
    b |= (b >> 8u);
    b |= (b >> 16u);
    b |= (b >> 32u);
    return b;
}

template<Color> [[nodiscard]] constexpr BitBoard fillForwardOccluded(BitBoard b, BitBoard open);
template<> [[nodiscard]] constexpr BitBoard      fillForwardOccluded<Co_White>(BitBoard b, BitBoard open) {
    b |= open & (b << 8u);
    open &= (open << 8u);
    b |= open & (b << 16u);
    open &= (open << 16u);
    b |= open & (b << 32u);
    return b;
}
template<> [[nodiscard]] constexpr BitBoard fillForwardOccluded<Co_Black>(BitBoard b, BitBoard open) {
    b |= open & (b >> 8u);
    open &= (open >> 8u);
    b |= open & (b >> 16u);
    open &= (open >> 16u);
    b |= open & (b >> 32u);
    return b;
}

template<Color C> [[nodiscard]] constexpr BitBoard frontSpan(const BitBoard b) { return fillForward<C>(shiftN<C>(b)); }
template<Color C> [[nodiscard]] constexpr BitBoard rearSpan(const BitBoard b) { return frontSpan<~C>(b); }
template<Color C> [[nodiscard]] constexpr BitBoard pawnSemiOpen(const BitBoard own, const BitBoard opp) { return own & ~frontSpan<~C>(opp); }

[[nodiscard]] constexpr BitBoard fillFile(const BitBoard b) { return fillForward<Co_White>(b) | fillForward<Co_Black>(b); }
[[nodiscard]] constexpr BitBoard openFiles(const BitBoard w, const BitBoard b) { return ~fillFile(w) & ~fillFile(b); }

template<Color C> [[nodiscard]] constexpr BitBoard pawnAttacks(const BitBoard b) { return shiftNW<C>(b) | shiftNE<C>(b); }
template<Color C> [[nodiscard]] constexpr BitBoard pawnDoubleAttacks(const BitBoard b) { return shiftNW<C>(b) & shiftNE<C>(b); }
template<Color C> [[nodiscard]] constexpr BitBoard pawnSingleAttacks(const BitBoard b) { return shiftNW<C>(b) ^ shiftNE<C>(b); }

template<Color C> [[nodiscard]] constexpr BitBoard pawnHoles(const BitBoard b) { return ~fillForward<C>(pawnAttacks<C>(b)); }
template<Color C> [[nodiscard]] constexpr BitBoard pawnDoubled(const BitBoard b) { return frontSpan<C>(b) & b; }

[[nodiscard]] constexpr BitBoard pawnIsolated(const BitBoard b) { return b & ~fillFile(_shiftEast(b)) & ~fillFile(_shiftWest(b)); }

template<Color C> [[nodiscard]] constexpr BitBoard pawnBackward(const BitBoard own, const BitBoard opp) {
   return shiftN<~C>((shiftN<C>(own) & ~opp) & ~fillForward<C>(pawnAttacks<C>(own)) & (pawnAttacks<~C>(opp)));
}

template<Color C> [[nodiscard]] constexpr BitBoard pawnForwardCoverage(const BitBoard bb) {
    const BitBoard spans = frontSpan<C>(bb);
    return spans | _shiftEast(spans) | _shiftWest(spans);
}

template<Color C> [[nodiscard]] constexpr BitBoard pawnPassed(const BitBoard own, const BitBoard opp) { return own & ~pawnForwardCoverage<~C>(opp); }

template<Color C> [[nodiscard]] constexpr BitBoard pawnCandidates(const BitBoard own, const BitBoard opp) {
   return pawnSemiOpen<C>(own, opp) &
          shiftN<~C>((pawnSingleAttacks<C>(own) & pawnSingleAttacks<~C>(opp)) | (pawnDoubleAttacks<C>(own) & pawnDoubleAttacks<~C>(opp)));
}

template<Color C> [[nodiscard]] constexpr BitBoard pawnUndefendable(const BitBoard own, const BitBoard other) {
    return own & ~pawnAttacks<C>(fillForwardOccluded<C>(own, ~other)) & BB::advancedRanks;
}

[[nodiscard]] constexpr BitBoard pawnAlone(const BitBoard b) { return b & ~(adjacent(b) | pawnAttacks<Co_White>(b) | pawnAttacks<Co_Black>(b)); }

template<Color C> [[nodiscard]] constexpr BitBoard pawnDetached(const BitBoard own, const BitBoard other) {
    return pawnUndefendable<C>(own, other) & pawnAlone(own);
}

// return first square of the bitboard only
// beware emptyness of the bitboard is only checked in debug mode !
[[nodiscard]] Square SquareFromBitBoard(const BitBoard& b);

// Position relative helpers
FORCE_FINLINE void unSetBit(Position& p, const Square k) {
   assert(isValidSquare(k));
   BB::_unSetBit(p._pieces(p.board_const(k)), k);
}
FORCE_FINLINE void unSetBit(Position& p, const Square k, const Piece pp) {
   assert(isValidSquare(k));
   BB::_unSetBit(p._pieces(pp), k);
}
FORCE_FINLINE void setBit(Position& p, const Square k, const Piece pp) {
   assert(isValidSquare(k));
   BB::_setBit(p._pieces(pp), k);
}
void clearBitBoards(Position& p);
void setBitBoards(Position& p);

} // namespace BBTools
