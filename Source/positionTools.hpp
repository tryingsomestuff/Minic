#pragma once

#include "definition.hpp"
#include "position.hpp"

std::string GetFENShort(const Position &p );

std::string GetFENShort2(const Position &p);

std::string GetFEN(const Position &p);

std::string SanitizeCastling(const Position & p, const std::string & str);

Move SanitizeCastling(const Position & p, const Move & m);

Square kingSquare(const Position & p);

bool readMove(const Position & p, const std::string & ss, Square & from, Square & to, MType & moveType );

float gamePhase(const Position & p, ScoreType & matScoreW, ScoreType & matScoreB);

bool readEPDFile(const std::string & fileName, std::vector<std::string> & positions);