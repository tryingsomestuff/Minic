#pragma once

#include "definition.hpp"

/*!
 * A simple, and partial, UCI implementation
 */
namespace UCI {
void        init();
void        uci();
[[nodiscard]] std::string uciScore(ScoreType score, unsigned int ply);
} // namespace UCI
