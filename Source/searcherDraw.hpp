#pragma once

#include "material.hpp"

template<bool isPV>
[[nodiscard]] FORCE_FINLINE std::optional<ScoreType> Searcher::interiorNodeRecognizer(const Position& p, DepthType height) const{
   // handles chess variants
   ///@todo other chess variants
   if (isRep(p,isPV))     return std::optional<ScoreType>(drawScore(p, height));
   if (isMaterialDraw(p)) return std::optional<ScoreType>(drawScore(p, height));
   if (is50moves(p,true)) return std::optional<ScoreType>(drawScore(p, height)); // pre move loop version at 101 not 100
   return std::nullopt;
}
