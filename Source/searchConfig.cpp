#include "searchConfig.hpp"

namespace SearchConfig {

DepthType lmrReduction[MAX_DEPTH][MAX_MOVE];
ScoreType MvvLvaScores[6][6];

///@todo try parity pruning, prune less when ply is odd
///@todo tune everything when evalScore is from TT score

// first value if eval score is used, second if hash score is used
CONST_CLOP_TUNING ScoreType qfutilityMargin[2]           = {132, 132};
CONST_CLOP_TUNING DepthType staticNullMoveMaxDepth[2]    = {6, 6};
CONST_CLOP_TUNING ScoreType staticNullMoveDepthCoeff[2]  = {80, 80};
CONST_CLOP_TUNING ScoreType staticNullMoveDepthInit[2]   = {0, 0};
CONST_CLOP_TUNING DepthType razoringMaxDepth[2]          = {3, 3};
CONST_CLOP_TUNING ScoreType razoringMarginDepthCoeff[2]  = {0, 0};
CONST_CLOP_TUNING ScoreType razoringMarginDepthInit[2]   = {200, 200};
CONST_CLOP_TUNING DepthType nullMoveMinDepth             = 2;
CONST_CLOP_TUNING DepthType nullMoveVerifDepth           = 16;
CONST_CLOP_TUNING ScoreType nullMoveMargin               = 0;
CONST_CLOP_TUNING ScoreType nullMoveMargin2              = 0;
CONST_CLOP_TUNING ScoreType nullMoveDynamicDivisor       = 180;
CONST_CLOP_TUNING DepthType historyPruningMaxDepth       = 3;
CONST_CLOP_TUNING ScoreType historyPruningThresholdInit  = 0;
CONST_CLOP_TUNING ScoreType historyPruningThresholdDepth = 0;
CONST_CLOP_TUNING ScoreType historyExtensionThreshold    = 512;
CONST_CLOP_TUNING DepthType CMHMaxDepth                  = 4;
CONST_CLOP_TUNING DepthType futilityMaxDepth[2]          = {10, 10};
CONST_CLOP_TUNING ScoreType futilityDepthCoeff[2]        = {160, 160};
CONST_CLOP_TUNING ScoreType futilityDepthInit[2]         = {0, 0};
CONST_CLOP_TUNING DepthType iidMinDepth                  = 7;
CONST_CLOP_TUNING DepthType iidMinDepth2                 = 10;
CONST_CLOP_TUNING DepthType iidMinDepth3                 = 3;
CONST_CLOP_TUNING DepthType probCutMinDepth              = 5;
CONST_CLOP_TUNING int       probCutMaxMoves              = 5;
CONST_CLOP_TUNING ScoreType probCutMargin                = 80;
CONST_CLOP_TUNING ScoreType seeCaptureFactor             = 100;
CONST_CLOP_TUNING ScoreType seeQuietFactor               = 15;
CONST_CLOP_TUNING ScoreType betaMarginDynamicHistory     = 80;
//CONST_CLOP_TUNING ScoreType probCutThreshold              = 450;
CONST_CLOP_TUNING DepthType lmrMinDepth            = 2;
CONST_CLOP_TUNING DepthType singularExtensionDepth = 8;
///@todo on move / opponent
CONST_CLOP_TUNING ScoreType dangerLimitPruning   = 9;
CONST_CLOP_TUNING ScoreType dangerLimitReduction = 7;
CONST_CLOP_TUNING ScoreType failLowRootMargin    = 100;

CONST_CLOP_TUNING DepthType aspirationMinDepth  = 4;
CONST_CLOP_TUNING DepthType aspirationInit      = 6;
CONST_CLOP_TUNING DepthType aspirationDepthCoef = -2;
CONST_CLOP_TUNING DepthType aspirationDepthInit = 40;

CONST_CLOP_TUNING ScoreType failHighReductionThresholdInit[2]  = {130, 130};
CONST_CLOP_TUNING ScoreType failHighReductionThresholdDepth[2] = {0, 0};

} // namespace SearchConfig
