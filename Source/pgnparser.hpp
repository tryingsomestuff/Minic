#pragma once

#include "definition.hpp"

#ifdef WITH_PGN_PARSER

#include "position.hpp"

/*!
 * This thing was used to parse pgn file
 * in order to generate learning file for the Texel tuning process
 */

struct PGNGame {
   std::vector<Position>  p;
   int                    result;
   int                    whiteElo = 0;
   int                    blackElo = 0;
   std::string            resultStr;
   std::vector<Move>      moves;
   int                    n           = 0;
   static const int       minElo      = 1600;
   static const ScoreType equalMargin = 400;
   static const ScoreType quietMargin = 600;
};

[[nodiscard]] int PGNParse(const std::string& file);

#endif
