#pragma once

#include "definition.hpp"
#include "dynamicConfig.hpp"
#include "score.hpp"

// from Stockfish implementation
namespace Skill {
[[nodiscard]] FORCE_FINLINE unsigned int convertElo2Level() {
   const double a = 2.1586e-7;
   const double b = -0.001097;
   const double c = 2.79303;
   const double d = -978.848;
   const int intcorrectedElo = static_cast<int>(a * pow(DynamicConfig::strength,3) + b * pow(DynamicConfig::strength,2) + c * DynamicConfig::strength + d);
   return std::max(0, (intcorrectedElo - 500) / 29 - 10);
} ///@todo to be tuned
[[nodiscard]] FORCE_FINLINE unsigned int convertLevel2Elo() { return 29 * (DynamicConfig::level + 10) + 500; }                 ///@todo to be tuned
[[nodiscard]] FORCE_FINLINE bool         enabled() { return DynamicConfig::level > 0 && DynamicConfig::level < 100; }          // 0 is random mover !
[[nodiscard]] FORCE_FINLINE DepthType    limitedDepth() {
   return DepthType(1 + 2 * std::sqrt(std::max(0, static_cast<int>(DynamicConfig::level - 20))));
} ///@todo to be tuned
[[nodiscard]] FORCE_FINLINE uint64_t limitedNodes() { return static_cast<uint64_t>(std::exp((convertLevel2Elo() + 840.) / 240.)); } ///@todo to be tuned
[[nodiscard]] Move pick(std::vector<MultiPVScores>& multiPVMoves);
} // namespace Skill