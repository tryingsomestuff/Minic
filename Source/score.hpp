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

inline ScoreType ScaleScore(EvalScore s, float gp){ return ScoreType(gp*s[MG] + (1.f-gp)*s[EG]);}

enum eScores : unsigned char { sc_Mat = 0, sc_PST, sc_Rand, sc_MOB, sc_ATT, sc_PieceBlockPawn, sc_Center, sc_Holes, sc_Outpost, sc_knightFar, sc_FreePasser, sc_PwnPush, sc_PwnSafeAtt, sc_PwnPushAtt, sc_Adjust, sc_OpenFile, sc_RookFrontKing, sc_RookFrontQueen, sc_RookQueenSameFile, sc_AttQueenMalus, sc_MinorOnOpenFile, sc_RookBehindPassed, sc_QueenNearKing, sc_Hanging, sc_Threat, sc_PinsK, sc_PinsQ, sc_PawnTT, sc_Tempo, sc_Contempt, sc_initiative, sc_NN, sc_max };
static const std::string scNames[sc_max] = { "Mat", "PST", "RAND", "MOB", "Att", "PieceBlockPawn", "Center", "Holes", "Outpost", "knightTooFar", "FreePasser", "PwnPush", "PwnSafeAtt", "PwnPushAtt" , "Adjust", "OpenFile", "RookFrontKing", "RookFrontQueen", "RookQueenSameFile", "AttQueenMalus", "MinorOnOpenFile", "RookBehindPassed", "QueenNearKing", "Hanging", "Threats", "PinsK", "PinsQ", "PawnTT", "Tempo", "Contempt", "initiative", "NN" };

/* Score accumulator for evaluation
 * In release mode, this is just an EvalScore
 * In debug mode, one can see each part of the evaluation as each contribution is store in a table
 */
#ifdef DEBUG_ACC

struct ScoreAcc{
    float scalingFactor = 1;
    std::array<EvalScore, sc_max> scores = { 0 };
    EvalScore & operator[](eScores e) { return scores[e]; }
    ScoreType Score(const Position &p, float gp);
    void Display(const Position &p, float gp);
};

#else

struct ScoreAcc {
    float scalingFactor = 1;
    EvalScore score = { 0 };
    inline EvalScore & operator[](eScores ) { return score; }
    ScoreType Score(const Position &p, float gp);
    void Display(const Position &p, float gp);
};

#endif

/* Evaluation is returning the score of course, but also fill this little structure to provide
 * additionnal usefull information, such as game phase and current danger. Things that are
 * possibly used in search later
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