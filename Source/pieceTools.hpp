#pragma once

#include "position.hpp"

namespace PieceTools {
[[nodiscard]] inline Piece getPieceIndex_(const Position &p, Square k) {
   assert(k >= 0 && k < NbSquare);
   return (Piece)PieceIdx(p.board_const(k));
}
[[nodiscard]] inline Piece getPieceType(const Position &p, Square k) {
   assert(k >= 0 && k < NbSquare);
   return (Piece)std::abs(p.board_const(k));
}
[[nodiscard]] inline std::string getName(const Position &p, Square k) {
   assert(k >= 0 && k < NbSquare);
   return PieceNames[getPieceIndex_(p, k)];
}
/*
[[nodiscard]] inline ScoreType getValue(const Position &p, Square k) {
   assert(k >= 0 && k < NbSquare);
   return Values[getPieceIndex_(p, k)];
}
*/
[[nodiscard]] inline ScoreType getAbsValue(const Position &p, Square k) {
   assert(k >= 0 && k < NbSquare);
   return std::abs(Values[getPieceIndex_(p, k)]);
}
} // namespace PieceTools
