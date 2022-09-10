#pragma once

#include "definition.hpp"

/*!
 * A simple, and partial, UCI implementation
 * Some help here : https://www.shredderchess.com/download/div/uci.zip
 */
namespace UCI {
void init();
void uci();
void processCommand(const std::string & command);
[[nodiscard]] std::string uciScore(const ScoreType score, const unsigned int ply);
void handleVariant();
} // namespace UCI
