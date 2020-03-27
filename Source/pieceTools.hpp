#pragma once

#include "position.hpp"

namespace PieceTools{
inline Piece getPieceIndex  (const Position &p, Square k){ assert(k >= 0 && k < 64); return Piece(p.b[k] + PieceShift);}
inline Piece getPieceType   (const Position &p, Square k){ assert(k >= 0 && k < 64); return (Piece)std::abs(p.b[k]);}
inline std::string getName  (const Position &p, Square k){ assert(k >= 0 && k < 64); return PieceNames[getPieceIndex(p,k)];}
inline ScoreType getValue   (const Position &p, Square k){ assert(k >= 0 && k < 64); return Values[getPieceIndex(p,k)];}
inline ScoreType getAbsValue(const Position &p, Square k){ assert(k >= 0 && k < 64); return std::abs(Values[getPieceIndex(p,k)]); }
}
