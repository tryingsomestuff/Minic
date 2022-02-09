#include "score.hpp"

#include "dynamicConfig.hpp"
#include "evalConfig.hpp"
#include "position.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"

EvalScore EvalFeatures::SumUp() const {
   EvalScore score = scores[F_material] + scores[F_positional] + scores[F_development] + scores[F_mobility] + scores[F_pawnStruct] + scores[F_attack];
   score += scores[F_complexity] * sgn(score[MG]);
   return score;
}

#pragma GCC diagnostic pop

void EvalFeatures::callBack() {
   DynamicConfig::stylized = false;
   DynamicConfig::stylized |= DynamicConfig::styleComplexity  != 50;
   DynamicConfig::stylized |= DynamicConfig::styleMaterial    != 50;
   DynamicConfig::stylized |= DynamicConfig::stylePositional  != 50;
   DynamicConfig::stylized |= DynamicConfig::styleDevelopment != 50;
   DynamicConfig::stylized |= DynamicConfig::styleMobility    != 50;
   DynamicConfig::stylized |= DynamicConfig::styleAttack      != 50;
   DynamicConfig::stylized |= DynamicConfig::stylePawnStruct  != 50;
   DynamicConfig::stylized |= DynamicConfig::styleForwardness != 50;
}
