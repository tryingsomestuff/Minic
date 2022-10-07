#pragma once

#include "position.hpp"

namespace PieceTools {
[[nodiscard]] inline Piece getPieceIndex_(const Position &p, const Square k) {
   assert(isValidSquare(k));
   const Piece pp = p.board_const(k);
   assert(isValidPiece(pp));
   return (Piece)PieceIdx(pp);
}
[[nodiscard]] inline Piece getPieceType(const Position &p, const Square k) {
   assert(isValidSquare(k));
   const Piece pp = p.board_const(k);
   assert(isValidPiece(pp));
   return (Piece)std::abs(pp);
}
[[nodiscard]] inline std::string_view getName(const Position &p, const Square k) {
   assert(isValidSquare(k));
   return PieceNames[getPieceIndex_(p, k)];
}
/*
[[nodiscard]] inline ScoreType getValue(const Position &p, Square k) {
   assert(isValidSquare(k));
   return Values[getPieceIndex_(p, k)];
}
*/
[[nodiscard]] inline ScoreType getAbsValue(const Position &p, const Square k) {
   assert(isValidSquare(k));
   return std::abs(Values[getPieceIndex_(p, k)]);
}
} // namespace PieceTools
