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

    EvalScore scale(float s_mg,float s_eg)const{ 
        EvalScore e(*this); 
        e[MG]= ScoreType(s_mg*e[MG]); 
        e[EG]= ScoreType(s_eg*e[EG]); 
        return e;
    }
    /*
    EvalScore scale(int mult_mg,int mult_eg, int div_mg,int div_eg)const{ 
        EvalScore e(*this); 
        e[MG]= ScoreType((mult_mg*e[MG])/div_mg); 
        e[EG]= ScoreType((mult_eg*e[EG])/div_eg); 
        return e;
    }
    */
    //EvalScore scale(float s)const{ return scale(s,s);}
};

inline std::ostream & operator<<(std::ostream & of, const EvalScore & s){
    of << "MG : " << s[MG] << ", EG : " << s[EG];
    return of;
}

enum Feature : unsigned char { F_material = 0, F_positional, F_development, F_mobility, F_attack, F_pawnStruct, F_complexity, F_max };
inline Feature operator++(Feature & f){f=Feature(f+1); return f;}
struct EvalFeatures{
    EvalScore scores[F_max] = { {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0} };
    float scalingFactor        = 1.f;
    inline EvalScore SumUp()const{
        return scores[F_material] 
             + scores[F_positional] 
             + scores[F_development] 
             + scores[F_mobility] 
             + scores[F_pawnStruct] 
             + scores[F_attack] 
             + scores[F_complexity];
    }
};

inline ScoreType ScaleScore(EvalScore s, float gp){ return ScoreType(gp*s[MG] + (1.f-gp)*s[EG]);}
ScoreType Score(EvalScore score, float scalingFactor, const Position &p, float gp);

/* Evaluation is returning the score of course, but also fill this little structure to provide
 * additionnal usefull information, such as game phase and current danger. Things that are
 * possibly used in search later.
 */
struct EvalData{
    float gp = 0;
    ScoreType danger[2] = {0,0};
    unsigned short int mobility[2] = {0,0};
};

// used for easy move detection
struct RootScores { 
    Move m; 
    ScoreType s; 
};

inline void displayEval(const EvalData & data, const EvalFeatures & features ){
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