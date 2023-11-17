#pragma once

#include "position.hpp"

namespace PieceTools {
[[nodiscard]] FORCE_FINLINE Piece getPieceIndex_(const Position &p, const Square k) {
   assert(isValidSquare(k));
   const Piece pp = p.board_const(k);
   assert(isValidPiece(pp));
   return (Piece)PieceIdx(pp);
}
[[nodiscard]] FORCE_FINLINE Piece getPieceType(const Position &p, const Square k) {
   assert(isValidSquare(k));
   const Piece pp = p.board_const(k);
   assert(isValidPiece(pp));
   return Abs(pp);
}
[[nodiscard]] FORCE_FINLINE std::string_view getName(const Position &p, const Square k) {
   assert(isValidSquare(k));
   return PieceNames[getPieceIndex_(p, k)];
}
[[nodiscard]] FORCE_FINLINE std::string_view getNameUTF(const Position &p, const Square k) {
   assert(isValidSquare(k));
   return PieceNamesUTF[getPieceIndex_(p, k)];
}
/*
[[nodiscard]] FORCE_FINLINE ScoreType getValue(const Position &p, Square k) {
   assert(isValidSquare(k));
   return Values[getPieceIndex_(p, k)];
}
*/
[[nodiscard]] FORCE_FINLINE ScoreType getAbsValue(const Position &p, const Square k) {
   assert(isValidSquare(k));
   return Abs(Values[getPieceIndex_(p, k)]);
}
} // namespace PieceTools
