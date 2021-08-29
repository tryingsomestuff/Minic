#include "searchConfig.hpp"

namespace SearchConfig {

DepthType lmrReduction[MAX_DEPTH][MAX_MOVE];
ScoreType MvvLvaScores[6][6];

///@todo try parity pruning, prune less when ply is odd
///@todo tune everything when evalScore is from TT score

// first value if eval score is used, second if hash score is used
CONST_SEARCH_TUNING ScoreType qfutilityMargin[2]              = {132, 132};
CONST_SEARCH_TUNING DepthType staticNullMoveMaxDepth[2]       = {6, 6};
CONST_SEARCH_TUNING ScoreType staticNullMoveDepthCoeff[2]     = {80, 80};
CONST_SEARCH_TUNING ScoreType staticNullMoveDepthInit[2]      = {0, 0};
CONST_SEARCH_TUNING DepthType razoringMaxDepth[2]             = {3, 3};
CONST_SEARCH_TUNING ScoreType razoringMarginDepthCoeff[2]     = {0, 0};
CONST_SEARCH_TUNING ScoreType razoringMarginDepthInit[2]      = {200, 200};
CONST_SEARCH_TUNING DepthType nullMoveMinDepth                = 2;
CONST_SEARCH_TUNING DepthType nullMoveVerifDepth              = 12;
CONST_SEARCH_TUNING ScoreType nullMoveMargin                  = 0;
CONST_SEARCH_TUNING ScoreType nullMoveMargin2                 = 0;
CONST_SEARCH_TUNING ScoreType nullMoveDynamicDivisor          = 180;
CONST_SEARCH_TUNING DepthType historyPruningMaxDepth          = 3;
CONST_SEARCH_TUNING ScoreType historyPruningThresholdInit     = 0;
CONST_SEARCH_TUNING ScoreType historyPruningThresholdDepth    = 0;
CONST_SEARCH_TUNING DepthType capHistoryPruningMaxDepth       = 5;
CONST_SEARCH_TUNING ScoreType capHistoryPruningThresholdInit  = 0;
CONST_SEARCH_TUNING ScoreType capHistoryPruningThresholdDepth = -128;
CONST_SEARCH_TUNING ScoreType historyExtensionThreshold       = 512;
CONST_SEARCH_TUNING DepthType CMHMaxDepth                     = 4;
CONST_SEARCH_TUNING DepthType futilityMaxDepth[2]             = {10, 10};
CONST_SEARCH_TUNING ScoreType futilityDepthCoeff[2]           = {160, 160};
CONST_SEARCH_TUNING ScoreType futilityDepthInit[2]            = {0, 0};
CONST_SEARCH_TUNING DepthType iidMinDepth                     = 7;
CONST_SEARCH_TUNING DepthType iidMinDepth2                    = 10;
CONST_SEARCH_TUNING DepthType iidMinDepth3                    = 3;
CONST_SEARCH_TUNING DepthType probCutMinDepth                 = 5;
CONST_SEARCH_TUNING int       probCutMaxMoves                 = 5;
CONST_SEARCH_TUNING ScoreType probCutMargin                   = 80;
CONST_SEARCH_TUNING ScoreType seeCaptureFactor                = 100;
CONST_SEARCH_TUNING ScoreType seeQuietFactor                  = 15;
CONST_SEARCH_TUNING ScoreType betaMarginDynamicHistory        = 180;
//CONST_SEARCH_TUNING ScoreType probCutThreshold              = 450;
CONST_SEARCH_TUNING DepthType lmrMinDepth            = 2;
CONST_SEARCH_TUNING DepthType singularExtensionDepth = 8;
///@todo on move / opponent
CONST_SEARCH_TUNING ScoreType dangerLimitPruning        = 64; //off
CONST_SEARCH_TUNING ScoreType dangerLimitForwardPruning = 64; //off
CONST_SEARCH_TUNING ScoreType dangerLimitReduction      = 64; //off
CONST_SEARCH_TUNING ScoreType dangerDivisor             = 180;
CONST_SEARCH_TUNING ScoreType failLowRootMargin         = 100;

///@todo probably should be tuned with/without a net
CONST_SEARCH_TUNING DepthType aspirationMinDepth  = 4;
CONST_SEARCH_TUNING ScoreType aspirationInit      = 6;
CONST_SEARCH_TUNING ScoreType aspirationDepthCoef = 0;
CONST_SEARCH_TUNING ScoreType aspirationDepthInit = 0;

CONST_SEARCH_TUNING DepthType ttMaxFiftyValideDepth = 92;

CONST_SEARCH_TUNING ScoreType failHighReductionThresholdInit[2]  = {130, 130};
CONST_SEARCH_TUNING ScoreType failHighReductionThresholdDepth[2] = {0, 0};

CONST_SEARCH_TUNING ScoreType randomAggressiveReductionFactor = 2;

} // namespace SearchConfig
