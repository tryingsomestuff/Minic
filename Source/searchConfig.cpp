#include "searchConfig.hpp"

namespace SearchConfig {

///@todo try parity pruning, prune less when ply is odd
///@todo tune everything when evalScore is from TT score

CONST_SEARCH_TUNING Coeff<2,2> staticNullMoveCoeff        = { {0, 0}, {0, 0}, {80, 80}, {0, 0}, {0, 0}, {6, 6}, "staticNullMove" };
CONST_SEARCH_TUNING Coeff<2,2> razoringCoeff              = { {200, 200}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {3, 3}, "razoring" };
CONST_SEARCH_TUNING Coeff<2,2> threatCoeff                = { {0, 0}, {30, 0}, {0, 0}, {0, 0}, {0, 0}, {2, 2}, "threat" };
CONST_SEARCH_TUNING Coeff<2,2> historyPruningCoeff        = { {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {2, 1}, "historyPruning" };
CONST_SEARCH_TUNING Coeff<2,2> captureHistoryPruningCoeff = { {0, 0}, {0, 0}, {-128, -128}, {0, 0}, {0, 0}, {4, 3}, "captureHistoryPruning" };
CONST_SEARCH_TUNING Coeff<2,2> futilityPruningCoeff       = { {0, 0}, {0, 0}, {160, 160}, {0, 0}, {0, 0}, {10, 10}, "futilityPruning" };
CONST_SEARCH_TUNING Coeff<2,2> failHighReductionCoeff     = { {130, 130}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {MAX_DEPTH, MAX_DEPTH}, "failHighReduction" };

// first value if eval score is used, second if hash score is used
CONST_SEARCH_TUNING ScoreType qfutilityMargin[2]              = {132, 132};
CONST_SEARCH_TUNING DepthType nullMoveMinDepth                = 2;
CONST_SEARCH_TUNING DepthType nullMoveVerifDepth              = 12;
CONST_SEARCH_TUNING ScoreType nullMoveMargin                  = 0;
CONST_SEARCH_TUNING ScoreType nullMoveMargin2                 = 0;
CONST_SEARCH_TUNING ScoreType nullMoveDynamicDivisor          = 180;
CONST_SEARCH_TUNING ScoreType historyExtensionThreshold       = 512;
CONST_SEARCH_TUNING DepthType CMHMaxDepth[2]                  = {4,3};
CONST_SEARCH_TUNING DepthType iidMinDepth                     = 7;
CONST_SEARCH_TUNING DepthType iidMinDepth2                    = 10;
CONST_SEARCH_TUNING DepthType iidMinDepth3                    = 3;
CONST_SEARCH_TUNING DepthType probCutMinDepth                 = 5;
CONST_SEARCH_TUNING int       probCutMaxMoves                 = 5;
CONST_SEARCH_TUNING ScoreType probCutMargin                   = 80;
CONST_SEARCH_TUNING ScoreType seeCaptureFactor                = 100;
CONST_SEARCH_TUNING ScoreType seeCapDangerDivisor             = 8;
CONST_SEARCH_TUNING ScoreType seeQuietFactor                  = 15;
CONST_SEARCH_TUNING ScoreType seeQuietDangerDivisor           = 8;
CONST_SEARCH_TUNING ScoreType seeQThreshold                   = 0;
CONST_SEARCH_TUNING ScoreType betaMarginDynamicHistory        = 180;
//CONST_SEARCH_TUNING ScoreType probCutThreshold              = 450;
CONST_SEARCH_TUNING DepthType lmrMinDepth            = 2;
CONST_SEARCH_TUNING DepthType singularExtensionDepth = 8;
CONST_SEARCH_TUNING int       lmrCapHistoryFactor    = 8;
///@todo on move / opponent
CONST_SEARCH_TUNING ScoreType dangerLimitPruning        = 64; //off
CONST_SEARCH_TUNING ScoreType dangerLimitForwardPruning = 64; //off
CONST_SEARCH_TUNING ScoreType dangerLimitReduction      = 64; //off
CONST_SEARCH_TUNING ScoreType dangerDivisor             = 180;
CONST_SEARCH_TUNING ScoreType failLowRootMargin         = 100;

CONST_SEARCH_TUNING ScoreType deltaBadMargin            = 150;
CONST_SEARCH_TUNING ScoreType deltaBadSEEThreshold      = 25;
CONST_SEARCH_TUNING ScoreType deltaGoodMargin           = 135;
CONST_SEARCH_TUNING ScoreType deltaGoodSEEThreshold     = 160;

///@todo probably should be tuned with/without a net
CONST_SEARCH_TUNING DepthType aspirationMinDepth  = 4;
CONST_SEARCH_TUNING ScoreType aspirationInit      = 6;
CONST_SEARCH_TUNING ScoreType aspirationDepthCoef = 0;
CONST_SEARCH_TUNING ScoreType aspirationDepthInit = 0;

CONST_SEARCH_TUNING DepthType ttMaxFiftyValideDepth = 92;

//CONST_SEARCH_TUNING ScoreType randomAggressiveReductionFactor = 2;

} // namespace SearchConfig
