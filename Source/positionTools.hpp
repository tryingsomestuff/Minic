#pragma once

#include "definition.hpp"
#include "position.hpp"

[[nodiscard]] std::string GetFENShort(const Position &p);

[[nodiscard]] std::string GetFENShort2(const Position &p);

[[nodiscard]] std::string GetFEN(const Position &p);

[[nodiscard]] std::string SanitizeCastling(const Position &p, const std::string &str);

[[nodiscard]] Move SanitizeCastling(const Position &p, const Move &m);

[[nodiscard]] Square kingSquare(const Position &p);

bool readMove(const Position &p, const std::string &ss, Square &from, Square &to, MType &moveType);

[[nodiscard]] float gamePhase(const Position &p, ScoreType &matScoreW, ScoreType &matScoreB);

bool readEPDFile(const std::string &fileName, std::vector<std::string> &positions);

namespace chess960 {
extern const std::string positions[960];
}