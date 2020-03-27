#pragma once

#include "definition.hpp"
#include "logging.hpp"

enum eScores : unsigned char { sc_Mat = 0, sc_PST, sc_Rand, sc_MOB, sc_ATT, sc_PieceBlockPawn, sc_Center, sc_Holes, sc_Outpost, sc_FreePasser, sc_PwnPush, sc_PwnSafeAtt, sc_PwnPushAtt, sc_Adjust, sc_OpenFile, sc_RookFrontKing, sc_RookFrontQueen, sc_RookQueenSameFile, sc_AttQueenMalus, sc_MinorOnOpenFile, sc_RookBehindPassed, sc_QueenNearKing, sc_Hanging, sc_Threat, sc_PinsK, sc_PinsQ, sc_PawnTT, sc_Tempo, sc_initiative, sc_NN, sc_max };
static const std::string scNames[sc_max] = { "Mat", "PST", "RAND", "MOB", "Att", "PieceBlockPawn", "Center", "Holes", "Outpost", "FreePasser", "PwnPush", "PwnSafeAtt", "PwnPushAtt" , "Adjust", "OpenFile", "RookFrontKing", "RookFrontQueen", "RookQueenSameFile", "AttQueenMalus", "MinorOnOpenFile", "RookBehindPassed", "QueenNearKing", "Hanging", "Threats", "PinsK", "PinsQ", "PawnTT", "Tempo", "initiative", "NN" };

/* Score accumulator for evaluation
 * In release mode, this is just an EvalScore
 * In debug mode, one can see each part of the evaluation as each contribution is store in a table
 */
#ifdef DEBUG_ACC

struct ScoreAcc{
    float scalingFactor = 1;
    std::array<EvalScore, sc_max> scores = { 0 };
    EvalScore & operator[](eScores e) { return scores[e]; }
    ScoreType Score(const Position &p, float gp){
        EvalScore sc;
        for(int k = 0 ; k < sc_max ; ++k){ sc += scores[k]; }
        return ScoreType(ScaleScore(sc,gp)*scalingFactor*std::min(1.f,(110-p.fifty)/100.f));
    }
    void Display(const Position &p, float gp){
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
};

#else

struct ScoreAcc {
    float scalingFactor = 1;
    EvalScore score = { 0 };
    inline EvalScore & operator[](eScores ) { return score; }
    ScoreType Score(const Position &p, float gp) { return ScoreType(ScaleScore(score, gp)*scalingFactor*std::min(1.f, (110 - p.fifty) / 100.f)); }
    void Display(const Position &p, float gp) {
        Logging::LogIt(Logging::logInfo) << "Score  " << score[MG];
        Logging::LogIt(Logging::logInfo) << "EG     " << score[EG];
        Logging::LogIt(Logging::logInfo) << "Scaling factor " << scalingFactor;
        Logging::LogIt(Logging::logInfo) << "Game phase " << gp;
        Logging::LogIt(Logging::logInfo) << "Fifty  " << std::min(1.f, (110 - p.fifty) / 100.f);
        Logging::LogIt(Logging::logInfo) << "Total  " << ScoreType(ScaleScore(score, gp)*scalingFactor*std::min(1.f, (110 - p.fifty) / 100.f));
    }
};

#endif

/* Evaluation is returning the score of course, but also fill this little structure to provide
 * additionnal usefull information, such as game phase and current danger. Things that are
 * possibly used in search later
 */
struct EvalData{
    float gp = 0;
    ScoreType danger[2] = {0,0};
};
