#pragma once

#include "definition.hpp"
#include "position.hpp"

[[nodiscard]] std::string GetFENShort(const Position &p);

[[nodiscard]] std::string GetFENShort2(const Position &p);

[[nodiscard]] std::string GetFEN(const Position &p);

[[nodiscard]] std::string SanitizeCastling(const Position &p, const std::string &str);

[[nodiscard]] Square kingSquare(const Position &p);

///@todo std::optional to avoid output parameter
bool readMove(const Position &p, const std::string &ss, Square &from, Square &to, MType &moveType, bool forbidCastling = false);

[[nodiscard]] float gamePhase(const Position::Material &mat, ScoreType &matScoreW, ScoreType &matScoreB);

[[nodiscard]] bool readEPDFile(const std::string &fileName, std::vector<std::string> &positions);

#if !defined(ARDUINO) && !defined(ESP32)
namespace chess960 {
extern const std::string positions[960];
std::string getDFRCXFEN();
}
#endif