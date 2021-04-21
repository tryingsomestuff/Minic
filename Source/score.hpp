#pragma once

#include "definition.hpp"
#include "logging.hpp"

struct Position;

// Stockfish trick (two short in one int) is not compatible with Texel tuning !
#ifndef WITH_EVALSCORE_AS_INT
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

    [[nodiscard]] EvalScore scale(float s_mg,float s_eg)const{ 
        EvalScore e(*this); 
        e[MG]= ScoreType(s_mg*e[MG]); 
        e[EG]= ScoreType(s_eg*e[EG]); 
        return e;
    }
};

#else

#define MakeScore(mg, eg) ((int)((unsigned int)(eg) << 16) + (mg))
#define MGScore(s) ((int16_t)((uint16_t)((unsigned)((s)))))
#define EGScore(s) ((int16_t)((uint16_t)((unsigned)((s) + 0x8000) >> 16)))

struct EvalScore{
    int sc = 0;
    EvalScore(ScoreType mg,ScoreType eg):sc{MakeScore(mg,eg)}{}
    EvalScore(ScoreType s):sc{MakeScore(s,s)}{}
    EvalScore():sc{0}{}
    EvalScore(const EvalScore & s):sc{s.sc}{}

    inline ScoreType operator[](GamePhase g)const{ return g == MG ? MGScore(sc) : EGScore(sc);}

    EvalScore& operator*=(const EvalScore& s){sc = MakeScore(MGScore(sc)*MGScore(s.sc),EGScore(sc)*EGScore(s.sc)); return *this;}
    EvalScore& operator/=(const EvalScore& s){sc = MakeScore(MGScore(sc)/MGScore(s.sc),EGScore(sc)/EGScore(s.sc)); return *this;}
    EvalScore& operator+=(const EvalScore& s){sc = MakeScore(MGScore(sc)+MGScore(s.sc),EGScore(sc)+EGScore(s.sc)); return *this;}
    EvalScore& operator-=(const EvalScore& s){sc = MakeScore(MGScore(sc)-MGScore(s.sc),EGScore(sc)-EGScore(s.sc)); return *this;}
    EvalScore  operator *(const EvalScore& s)const{return MakeScore(MGScore(sc)*MGScore(s.sc),EGScore(sc)*EGScore(s.sc));}
    EvalScore  operator /(const EvalScore& s)const{return MakeScore(MGScore(sc)/MGScore(s.sc),EGScore(sc)/EGScore(s.sc));}
    EvalScore  operator +(const EvalScore& s)const{return MakeScore(MGScore(sc)+MGScore(s.sc),EGScore(sc)+EGScore(s.sc));}
    EvalScore  operator -(const EvalScore& s)const{return MakeScore(MGScore(sc)-MGScore(s.sc),EGScore(sc)-EGScore(s.sc));}
    void       operator =(const EvalScore& s){sc=s.sc;}

    EvalScore& operator*=(const ScoreType& s){sc = MakeScore(MGScore(sc)*s,EGScore(sc)*s); return *this;}
    EvalScore& operator/=(const ScoreType& s){sc = MakeScore(MGScore(sc)/s,EGScore(sc)/s); return *this;}
    EvalScore& operator+=(const ScoreType& s){sc = MakeScore(MGScore(sc)+s,EGScore(sc)+s); return *this;}
    EvalScore& operator-=(const ScoreType& s){sc = MakeScore(MGScore(sc)-s,EGScore(sc)-s); return *this;}
    EvalScore  operator *(const ScoreType& s)const{return MakeScore(MGScore(sc)*s,EGScore(sc)*s);}
    EvalScore  operator /(const ScoreType& s)const{return MakeScore(MGScore(sc)/s,EGScore(sc)/s);}
    EvalScore  operator +(const ScoreType& s)const{return MakeScore(MGScore(sc)+s,EGScore(sc)+s);}
    EvalScore  operator -(const ScoreType& s)const{return MakeScore(MGScore(sc)-s,EGScore(sc)-s);}
    void       operator =(const ScoreType& s){sc=MakeScore(s,s);}

    [[nodiscard]] EvalScore scale(float s_mg,float s_eg)const{ return EvalScore(ScoreType(MGScore(sc)*s_mg),ScoreType(EGScore(sc)*s_eg));}
};
#endif

inline std::ostream & operator<<(std::ostream & of, const EvalScore & s){
    of << s[MG] << " " << s[EG];
    return of;
}

enum Feature : unsigned char { F_material = 0, F_positional, F_development, F_mobility, F_attack, F_pawnStruct, F_complexity, F_max };
inline Feature operator++(Feature & f){f=Feature(f+1); return f;}
struct EvalFeatures{
    EvalScore scores[F_max] = { {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0} };
    float scalingFactor = 1.f; // only used in end-game !
    EvalScore SumUp()const;
    static void callBack();
};

/*
inline std::ostream & operator<<(std::ostream & of, const EvalFeatures & features){
    for (size_t k = 0 ; k < F_max; ++k) of << features.scores[k] << " ";
    of << features.scalingFactor << " ";
    return of;
}
*/

[[nodiscard]] inline ScoreType ScaleScore(EvalScore s, float gp, float scalingFactorEG = 1.f){ return ScoreType(gp*s[MG] + (1.f-gp)*scalingFactorEG*s[EG]);}

[[nodiscard]] ScoreType Score(ScoreType score, const Position &p);

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

namespace WDL{
    // Coefficients of a 3rd order polynomial fit based on Minic test data (from CCRL, CEGT, FGRL)
    const float as[] = {-21.17073707f, 145.19861843f, -196.25853651f, 122.33049978f};
    const float bs[] = {-17.74159650f, 122.49082242f, -214.56679719f, 149.45430947f};
}

// formula from Stockfish
inline int toWDLModel(ScoreType v, DepthType ply) {
    // limit input ply and rescale (and rescale)
    const float m = std::min(DepthType(256), ply) / 64.0f;
    const float a = (((WDL::as[0] * m + WDL::as[1]) * m + WDL::as[2]) * m) + WDL::as[3];
    const float b = (((WDL::bs[0] * m + WDL::bs[1]) * m + WDL::bs[2]) * m) + WDL::bs[3];    
    // clamp score
    const float cp = std::clamp(float(v), -1000.0f, 1000.0f);
    // win probability from 0 to 1000
    return int(0.5 + 1000.f / (1 + std::exp((a - cp) / b)));
}

inline ScoreType fromWDLModel(int w, DepthType ply) {
    // limit input ply and rescale (and rescale)
    const float m = std::min(DepthType(256), ply) / 64.0f;
    const float a = (((WDL::as[0] * m + WDL::as[1]) * m + WDL::as[2]) * m) + WDL::as[3];
    const float b = (((WDL::bs[0] * m + WDL::bs[1]) * m + WDL::bs[2]) * m) + WDL::bs[3];    
    const float s = a - b * std::log(1000.f/(w-0.5) - 1.f);
    return ScoreType(std::clamp(s, -1000.f, 1000.f));
}