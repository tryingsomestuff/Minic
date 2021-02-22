#include "score.hpp"

#include "dynamicConfig.hpp"
#include "evalConfig.hpp"
#include "position.hpp"

ScoreType Score(ScoreType score, const Position &p) {
  return ScoreType(clampScore(int(score*std::min(1.f, (110 - p.fifty) / 100.f))));
}

EvalScore EvalFeatures::SumUp()const{
    EvalScore score = scores[F_material] 
                    + scores[F_positional] 
                    + scores[F_development] 
                    + scores[F_mobility] 
                    + scores[F_pawnStruct] 
                    + scores[F_attack];
    score += scores[F_complexity] * sgn(score[MG]);

/*
    if ( !DynamicConfig::stylized){
       // try some kind of "second" order correction on features (if no style is activated)
       EvalScore score2 = 0;
       for (auto f = F_material ; f <= F_pawnStruct; ++f){
          for (auto g = F_material ; g < f; ++g){
             score2 += ((scores[f]-scores[g])*EvalConfig::secondOrderFeature[f][g]);
          }
       }
       score += score2/1024;
    }
*/
    return score;
}

void EvalFeatures::callBack(){
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
