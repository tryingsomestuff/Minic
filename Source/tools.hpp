#pragma once

#include "definition.hpp"

#include "position.hpp"

std::string trim(const std::string& str, const std::string& whitespace = " \t");

void tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters = " " );

void debug_king_cap(const Position & p);

std::string ToString(const PVList & moves);

std::string ToString(const Move & m    , bool withScore = false);

std::string ToString(const Position & p, bool noEval = false);

std::string ToString(const Position::Material & mat);
