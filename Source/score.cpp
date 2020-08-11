#include "score.hpp"

#include "position.hpp"

ScoreType Score(ScoreType score, float scalingFactor, const Position &p) {
  return ScoreType(score*scalingFactor*std::min(1.f, (110 - p.fifty) / 100.f));
}

