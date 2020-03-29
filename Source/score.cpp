#include "score.hpp"

#include "position.hpp"

#ifdef DEBUG_ACC

ScoreType ScoreAcc::Score(const Position &p, float gp){
    EvalScore sc;
    for(int k = 0 ; k < sc_max ; ++k){ sc += scores[k]; }
    return ScoreType(ScaleScore(sc,gp)*scalingFactor*std::min(1.f,(110-p.fifty)/100.f));
}
void ScoreAcc::Display(const Position &p, float gp){
    EvalScore sc;
    for(int k = 0 ; k < sc_max ; ++k){
        Logging::LogIt(Logging::logInfo) << scNames[k] << "       " << scores[k][MG];
        Logging::LogIt(Logging::logInfo) << scNames[k] << "EG     " << scores[k][EG];
        sc += scores[k];
    }
    Logging::LogIt(Logging::logInfo) << "Score  " << sc[MG];
    Logging::LogIt(Logging::logInfo) << "EG     " << sc[EG];
    Logging::LogIt(Logging::logInfo) << "Scaling factor " << scalingFactor;
    Logging::LogIt(Logging::logInfo) << "Game phase " << gp;
    Logging::LogIt(Logging::logInfo) << "Fifty  " << std::min(1.f,(110-p.fifty)/100.f);
    Logging::LogIt(Logging::logInfo) << "Total  " << ScoreType(ScaleScore(sc,gp)*scalingFactor*std::min(1.f,(110-p.fifty)/100.f));
}

#else

ScoreType ScoreAcc::Score(const Position &p, float gp) {
  return ScoreType(ScaleScore(score, gp)*scalingFactor*std::min(1.f, (110 - p.fifty) / 100.f));
}

void ScoreAcc::Display(const Position &p, float gp) {
    Logging::LogIt(Logging::logInfo) << "Score  " << score[MG];
    Logging::LogIt(Logging::logInfo) << "EG     " << score[EG];
    Logging::LogIt(Logging::logInfo) << "Scaling factor " << scalingFactor;
    Logging::LogIt(Logging::logInfo) << "Game phase " << gp;
    Logging::LogIt(Logging::logInfo) << "Fifty  " << std::min(1.f, (110 - p.fifty) / 100.f);
    Logging::LogIt(Logging::logInfo) << "Total  " << ScoreType(ScaleScore(score, gp)*scalingFactor*std::min(1.f, (110 - p.fifty) / 100.f));
}

#endif
