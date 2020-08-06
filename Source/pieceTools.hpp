#pragma once

#include "position.hpp"

namespace PieceTools{
inline Piece getPieceIndex  (const Position &p, Square k){ assert(k >= 0 && k < NbSquare); return Piece(p.board_const(k) + PieceShift);}
inline Piece getPieceType   (const Position &p, Square k){ assert(k >= 0 && k < NbSquare); return (Piece)std::abs(p.board_const(k));}
inline std::string getName  (const Position &p, Square k){ assert(k >= 0 && k < NbSquare); return PieceNames[getPieceIndex(p,k)];}
inline ScoreType getValue   (const Position &p, Square k){ assert(k >= 0 && k < NbSquare); return Values[getPieceIndex(p,k)];}
inline ScoreType getAbsValue(const Position &p, Square k){ assert(k >= 0 && k < NbSquare); return std::abs(Values[getPieceIndex(p,k)]); }
}
