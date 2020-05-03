#include "score.hpp"

#include "position.hpp"

ScoreType Score(EvalScore score, float scalingFactor, const Position &p, float gp) {
  return ScoreType(ScaleScore(score, gp)*scalingFactor*std::min(1.f, (110 - p.fifty) / 100.f));
}

