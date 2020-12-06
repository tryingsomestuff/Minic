#pragma once

#include "definition.hpp"

/*!
 * A simple, and partial, UCI implementation
 */
namespace UCI {
    void init();
    void uci();
    std::string uciScore(ScoreType score);
}

