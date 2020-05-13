#pragma once

#include "definition.hpp"
#include "dynamicConfig.hpp"
#include "score.hpp"

// from Stockfish implementation
namespace Skill {
    inline bool enabled() { return DynamicConfig::level > 0 && DynamicConfig::level < 100; } // 0 is random mover !
    inline DepthType limitedDepth() { return DepthType(1 + 2 * std::sqrt(std::max(0,int(DynamicConfig::level-20)))); }
    Move pick(std::vector<RootScores> & multiPVMoves);
    static inline unsigned int Elo2Level(){ return std::max(0,(DynamicConfig::strength - 500))/29;}
}