#pragma once

#include "definition.hpp"

/*!
 * A simple, and partial, UCI implementation
 * Some help here : https://www.shredderchess.com/download/div/uci.zip
 */
namespace UCI {
void        init();
void        uci();
[[nodiscard]] const std::string uciScore(const ScoreType score, const unsigned int ply);
} // namespace UCI
