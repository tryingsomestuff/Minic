#pragma once

#include "definition.hpp"
#include "logging.hpp"

struct Position;

///@todo use Stockfish trick (two short in one int) but it's hard to make it compatible with Texel tuning !
struct EvalScore{
    std::array<ScoreType,GP_MAX> sc = {0};
    EvalScore(ScoreType mg,ScoreType eg):sc{mg,eg}{}
    EvalScore(ScoreType s):sc{s,s}{}
    EvalScore():sc{0,0}{}
    EvalScore(const EvalScore & e):sc{e.sc[MG],e.sc[EG]}{}

    inline ScoreType & operator[](GamePhase g){ return sc[g];}
    inline const ScoreType & operator[](GamePhase g)const{ return sc[g];}

    EvalScore& operator*=(const EvalScore& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]*=s[g]; return *this;}
    EvalScore& operator/=(const EvalScore& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]/=s[g]; return *this;}
    EvalScore& operator+=(const EvalScore& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]+=s[g]; return *this;}
    EvalScore& operator-=(const EvalScore& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]-=s[g]; return *this;}
    EvalScore  operator *(const EvalScore& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]*=s[g]; return e;}
    EvalScore  operator /(const EvalScore& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]/=s[g]; return e;}
    EvalScore  operator +(const EvalScore& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]+=s[g]; return e;}
    EvalScore  operator -(const EvalScore& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]-=s[g]; return e;}
    void       operator =(const EvalScore& s){for(GamePhase g=MG; g<GP_MAX; ++g){sc[g]=s[g];}}

    EvalScore& operator*=(const ScoreType& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]*=s; return *this;}
    EvalScore& operator/=(const ScoreType& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]/=s; return *this;}
    EvalScore& operator+=(const ScoreType& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]+=s; return *this;}
    EvalScore& operator-=(const ScoreType& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]-=s; return *this;}
    EvalScore  operator *(const ScoreType& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]*=s; return e;}
    EvalScore  operator /(const ScoreType& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]/=s; return e;}
    EvalScore  operator +(const ScoreType& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]+=s; return e;}
    EvalScore  operator -(const ScoreType& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]-=s; return e;}
    void       operator =(const ScoreType& s){for(GamePhase g=MG; g<GP_MAX; ++g){sc[g]=s;}}

    EvalScore scale(float s_mg,float s_eg)const{ EvalScore e(*this); e[MG]= ScoreType(s_mg*e[MG]); e[EG]= ScoreType(s_eg*e[EG]); return e;}
};

inline std::ostream & operator<<(std::ostream & of, const EvalScore & s){
    of << "MG : " << s[MG] << ", EG : " << s[EG];
    return of;
}

struct EvalFeatures{
    EvalScore materialScore    = {0,0}; // material, imbalance
    EvalScore positionalScore  = {0,0}; // PST, output, minor on open
    EvalScore developmentScore = {0,0};
    EvalScore mobilityScore    = {0,0}; 
    EvalScore attackScore      = {0,0}; // attack, king safety
    EvalScore pawnStructScore  = {0,0};
    float scalingFactor        = 1.f;
};

inline ScoreType ScaleScore(EvalScore s, float gp){ return ScoreType(gp*s[MG] + (1.f-gp)*s[EG]);}
ScoreType Score(EvalScore score, float scalingFactor, const Position &p, float gp);

/* Evaluation is returning the score of course, but also fill this little structure to provide
 * additionnal usefull information, such as game phase and current danger. Things that are
 * possibly used in search later
 */
struct EvalData{
    float gp = 0;
    ScoreType danger[2] = {0,0};
    unsigned short int mobility[2] = {0,0};
    float shashinMaterialFactor = 1; // max(0,1-SQR(materialScore/150)), so 0 if too much imbalance
    float shashinForwardness[2] = {0,0};
    float shashinPacking[2] = {0,0};
    float shashinMobRatio = 1;
};

// used for easy move detection
struct RootScores { 
    Move m; 
    ScoreType s; 
};

inline void displayEval(const EvalData & data, const EvalFeatures & features ){
    Logging::LogIt(Logging::logInfo) << "Game phase    " << data.gp;
    Logging::LogIt(Logging::logInfo) << "ScalingFactor " << features.scalingFactor;
    Logging::LogIt(Logging::logInfo) << "Material      " << features.materialScore;
    Logging::LogIt(Logging::logInfo) << "Positional    " << features.positionalScore;
    Logging::LogIt(Logging::logInfo) << "Development   " << features.developmentScore;
    Logging::LogIt(Logging::logInfo) << "Mobility      " << features.mobilityScore;
    Logging::LogIt(Logging::logInfo) << "Pawn          " << features.pawnStructScore;
    Logging::LogIt(Logging::logInfo) << "Attack        " << features.attackScore;
}