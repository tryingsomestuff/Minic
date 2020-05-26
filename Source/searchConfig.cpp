#include "searchConfig.hpp"

namespace SearchConfig{

DepthType lmrReduction[MAX_DEPTH][MAX_MOVE];
ScoreType MvvLvaScores[6][6];

///@todo try parity pruning, prune less when ply is odd
///@todo tune everything when evalScore is from TT score

// first value if eval score is used, second if hash score is used
CONST_CLOP_TUNING ScoreType qfutilityMargin           [2] = {90 , 90};
CONST_CLOP_TUNING DepthType staticNullMoveMaxDepth    [2] = {6  , 6};
CONST_CLOP_TUNING ScoreType staticNullMoveDepthCoeff  [2] = {80 , 80};
CONST_CLOP_TUNING ScoreType staticNullMoveDepthInit   [2] = {0  , 0};
CONST_CLOP_TUNING DepthType razoringMaxDepth          [2] = {3  , 3};
CONST_CLOP_TUNING ScoreType razoringMarginDepthCoeff  [2] = {0  , 0};
CONST_CLOP_TUNING ScoreType razoringMarginDepthInit   [2] = {200, 200};
CONST_CLOP_TUNING DepthType nullMoveMinDepth              = 2;
CONST_CLOP_TUNING DepthType nullMoveVerifDepth            = 64;
CONST_CLOP_TUNING DepthType historyPruningMaxDepth        = 3;
CONST_CLOP_TUNING ScoreType historyPruningThresholdInit   = 0;
CONST_CLOP_TUNING ScoreType historyPruningThresholdDepth  = 0;
CONST_CLOP_TUNING DepthType CMHMaxDepth                   = 4;
CONST_CLOP_TUNING DepthType futilityMaxDepth          [2] = {10 , 10};
CONST_CLOP_TUNING ScoreType futilityDepthCoeff        [2] = {160, 160};
CONST_CLOP_TUNING ScoreType futilityDepthInit         [2] = {0  , 0};
//CONST_CLOP_TUNING ScoreType failHighReductionThreshold[2] = {130, 130};
CONST_CLOP_TUNING DepthType iidMinDepth                   = 5;
CONST_CLOP_TUNING DepthType iidMinDepth2                  = 8;
CONST_CLOP_TUNING DepthType probCutMinDepth               = 5;
CONST_CLOP_TUNING int       probCutMaxMoves               = 5;
CONST_CLOP_TUNING ScoreType probCutMargin                 = 80;
//CONST_CLOP_TUNING ScoreType probCutThreshold              = 450;
CONST_CLOP_TUNING DepthType lmrMinDepth                   = 2;
CONST_CLOP_TUNING DepthType singularExtensionDepth        = 8;
// on move / opponent
CONST_CLOP_TUNING ScoreType dangerLimitPruning[2]         = {900,900};
CONST_CLOP_TUNING ScoreType dangerLimitReduction[2]       = {700,700};
CONST_CLOP_TUNING ScoreType failLowRootMargin             = 100;

} // SearchConfig
