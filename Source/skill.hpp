#pragma once

#include "definition.hpp"
#include "dynamicConfig.hpp"
#include "score.hpp"

// from Stockfish implementation
namespace Skill {
    inline bool enabled() { return DynamicConfig::level > 0 && DynamicConfig::level < 100; }
    inline DepthType limitedDepth() { return 1 + std::max(0u,DynamicConfig::level/5); }
    Move pick(std::vector<RootScores> & multiPVMoves);
}