#pragma once

#include "definition.hpp"
#include "dynamicConfig.hpp"
#include "score.hpp"

// from Stockfish implementation
namespace Skill {
    [[nodiscard]] inline bool enabled() { return DynamicConfig::level > 0 && DynamicConfig::level < 100; } // 0 is random mover !
    [[nodiscard]] inline DepthType limitedDepth() { return DepthType(1 + 2 * std::sqrt(std::max(0,int(DynamicConfig::level-20)))); }
    [[nodiscard]] Move pick(std::vector<RootScores> & multiPVMoves);
    [[nodiscard]] static inline unsigned int Elo2Level(){ return std::max(0,(DynamicConfig::strength - 500))/29;}
}