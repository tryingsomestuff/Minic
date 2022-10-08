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

void displayEval(const EvalData& data, const EvalFeatures& features) {
   Logging::LogIt(Logging::logInfo) << "Game phase    " << data.gp;
   Logging::LogIt(Logging::logInfo) << "ScalingFactor " << features.scalingFactor;
   Logging::LogIt(Logging::logInfo) << "Material      " << features.scores[F_material];
   Logging::LogIt(Logging::logInfo) << "Positional    " << features.scores[F_positional];
   Logging::LogIt(Logging::logInfo) << "Development   " << features.scores[F_development];
   Logging::LogIt(Logging::logInfo) << "Mobility      " << features.scores[F_mobility];
   Logging::LogIt(Logging::logInfo) << "Pawn          " << features.scores[F_pawnStruct];
   Logging::LogIt(Logging::logInfo) << "Attack        " << features.scores[F_attack];
   Logging::LogIt(Logging::logInfo) << "Complexity    " << features.scores[F_complexity];
}
