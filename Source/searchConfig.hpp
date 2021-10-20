#pragma once

#include "definition.hpp"
#include "logging.hpp"

/*!
 All parameters for search are defined here
 * There are const when no CLOP tuning is use and of course need to be mutable when begin tuned
 */
namespace SearchConfig {
const bool doWindow            = true;
const bool doPVS               = true;
const bool doNullMove          = true;
const bool doFutility          = true;
const bool doLMR               = true;
const bool doLMP               = true;
const bool doStaticNullMove    = true;
const bool doRazoring          = true;
const bool doQFutility         = false;
const bool doQDeltaPruning     = false;
const bool doProbcut           = true;
const bool doHistoryPruning    = true;
const bool doCapHistoryPruning = true;
const bool doCMHPruning        = true;

// first value if eval score is used, second if hash score is used
extern CONST_SEARCH_TUNING ScoreType qfutilityMargin[2];
extern CONST_SEARCH_TUNING DepthType staticNullMoveMaxDepth[2];
extern CONST_SEARCH_TUNING ScoreType staticNullMoveDepthCoeff[2];
extern CONST_SEARCH_TUNING ScoreType staticNullMoveDepthInit[2];
extern CONST_SEARCH_TUNING DepthType razoringMaxDepth[2];
extern CONST_SEARCH_TUNING ScoreType razoringMarginDepthCoeff[2];
extern CONST_SEARCH_TUNING ScoreType razoringMarginDepthInit[2];
extern CONST_SEARCH_TUNING DepthType nullMoveMinDepth;
extern CONST_SEARCH_TUNING DepthType nullMoveVerifDepth;
extern CONST_SEARCH_TUNING ScoreType nullMoveMargin;
extern CONST_SEARCH_TUNING ScoreType nullMoveMargin2;
extern CONST_SEARCH_TUNING ScoreType nullMoveDynamicDivisor;
extern CONST_SEARCH_TUNING DepthType historyPruningMaxDepth[2];
extern CONST_SEARCH_TUNING ScoreType historyPruningThresholdInit;
extern CONST_SEARCH_TUNING ScoreType historyPruningThresholdDepth;
extern CONST_SEARCH_TUNING DepthType capHistoryPruningMaxDepth[2];
extern CONST_SEARCH_TUNING ScoreType capHistoryPruningThresholdInit;
extern CONST_SEARCH_TUNING ScoreType capHistoryPruningThresholdDepth;
extern CONST_SEARCH_TUNING ScoreType historyExtensionThreshold;
extern CONST_SEARCH_TUNING DepthType CMHMaxDepth[2];
extern CONST_SEARCH_TUNING DepthType futilityMaxDepth[2];
extern CONST_SEARCH_TUNING ScoreType futilityDepthCoeff[2];
extern CONST_SEARCH_TUNING ScoreType futilityDepthInit[2];
extern CONST_SEARCH_TUNING ScoreType failHighReductionThresholdInit[2];
extern CONST_SEARCH_TUNING ScoreType failHighReductionThresholdDepth[2];
//extern CONST_SEARCH_TUNING ScoreType randomAggressiveReductionFactor;
extern CONST_SEARCH_TUNING DepthType iidMinDepth;
extern CONST_SEARCH_TUNING DepthType iidMinDepth2;
extern CONST_SEARCH_TUNING DepthType iidMinDepth3;
extern CONST_SEARCH_TUNING DepthType probCutMinDepth;
extern CONST_SEARCH_TUNING int       probCutMaxMoves;
extern CONST_SEARCH_TUNING ScoreType probCutMargin;
extern CONST_SEARCH_TUNING ScoreType seeCaptureFactor;
extern CONST_SEARCH_TUNING ScoreType seeQuietFactor;
extern CONST_SEARCH_TUNING ScoreType betaMarginDynamicHistory;
//extern CONST_SEARCH_TUNING ScoreType probCutThreshold;
extern CONST_SEARCH_TUNING DepthType lmrMinDepth;
extern CONST_SEARCH_TUNING DepthType singularExtensionDepth;
///@todo on move / opponent
extern CONST_SEARCH_TUNING ScoreType dangerLimitPruning;
extern CONST_SEARCH_TUNING ScoreType dangerLimitForwardPruning;
extern CONST_SEARCH_TUNING ScoreType dangerLimitReduction;
extern CONST_SEARCH_TUNING ScoreType dangerDivisor;
extern CONST_SEARCH_TUNING ScoreType failLowRootMargin;

extern CONST_SEARCH_TUNING DepthType aspirationMinDepth;
extern CONST_SEARCH_TUNING ScoreType aspirationInit;
extern CONST_SEARCH_TUNING ScoreType aspirationDepthCoef;
extern CONST_SEARCH_TUNING ScoreType aspirationDepthInit;

extern CONST_SEARCH_TUNING DepthType ttMaxFiftyValideDepth;

const DepthType lmpMaxDepth = 10;
const int       lmpLimit[][SearchConfig::lmpMaxDepth + 1] = {{0, 2, 3, 5, 9, 13, 18, 25, 34, 45, 55}, {0, 5, 6, 9, 14, 21, 30, 41, 55, 69, 84}};

extern DepthType lmrReduction[MAX_DEPTH][MAX_MOVE];
inline void initLMR() {
   Logging::LogIt(Logging::logInfo) << "Init lmr";
   for (int d = 0; d < MAX_DEPTH; d++)
      for (int m = 0; m < MAX_MOVE; m++) lmrReduction[d][m] = DepthType(log(d * 1.6) * log(m) * 0.4);
}

extern ScoreType MvvLvaScores[6][6];
inline void initMvvLva() {
   Logging::LogIt(Logging::logInfo) << "Init mvv-lva";
   static const ScoreType IValues[6] = {1, 2, 3, 5, 9, 20}; ///@todo try N=B=3 ??
   for (int v = 0; v < 6; ++v)
      for (int a = 0; a < 6; ++a) MvvLvaScores[v][a] = IValues[v] * 20 - IValues[a];
}

} // namespace SearchConfig
