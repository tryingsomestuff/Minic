#pragma once

#include "definition.hpp"

/*!
 * A simple, and partial, UCI implementation
 * Some help here : https://www.shredderchess.com/download/div/uci.zip
 */
namespace UCI {
void        init();
void        uci();
[[nodiscard]] std::string uciScore(ScoreType score, unsigned int ply);
} // namespace UCI
