#include "searchConfig.hpp"

namespace SearchConfig {

///@todo try parity pruning, prune less when ply is odd
///@todo tune everything when evalScore is from TT score

CONST_SEARCH_TUNING Coeff<2,2> staticNullMoveCoeff        = { {0, 0}, {381, 137}, {80, 80}, {158, 20}, {0, 0}, {6, 6}, "staticNullMove" };
CONST_SEARCH_TUNING Coeff<2,2> razoringCoeff              = { {120, 120}, {256, 631}, {80, 80}, {0, 0}, {0, 0}, {2, 2}, "razoring" };
CONST_SEARCH_TUNING Coeff<2,2> threatCoeff                = { {0, 0}, {256, 256}, {0, 0}, {0, 0}, {0, 0}, {2, 2}, "threat" };
//CONST_SEARCH_TUNING Coeff<2,2> ownThreatCoeff             = { {0, 0}, {256, 256}, {64, 64}, {0, 0}, {0, 0}, {2, 2}, "ownThreat" };
CONST_SEARCH_TUNING Coeff<2,2> historyPruningCoeff        = { {0, 0}, {256, 256}, {0, 0}, {0, 0}, {0, 0}, {2, 1}, "historyPruning" };
CONST_SEARCH_TUNING Coeff<2,2> captureHistoryPruningCoeff = { {0, 0}, {256, 256}, {-128, -128}, {0, 0}, {0, 0}, {4, 3}, "captureHistoryPruning" };
CONST_SEARCH_TUNING Coeff<2,2> futilityPruningCoeff       = { {0, 0}, {172, 546}, {160, 160}, {0, 0}, {0, 0}, {10, 10}, "futilityPruning" };
CONST_SEARCH_TUNING Coeff<2,2> failHighReductionCoeff     = { {130, 130}, {256, 256}, {0, 0}, {0, 0}, {0, 0}, {MAX_DEPTH, MAX_DEPTH}, "failHighReduction" };

// first value if eval score is used, second if hash score is used
CONST_SEARCH_TUNING colored<ScoreType> qfutilityMargin        = {1024, 1024};
CONST_SEARCH_TUNING DepthType nullMoveMinDepth                = 2;
CONST_SEARCH_TUNING DepthType nullMoveVerifDepth              = 6;
CONST_SEARCH_TUNING ScoreType nullMoveMargin                  = 19;
CONST_SEARCH_TUNING ScoreType nullMoveMargin2                 = 0;
CONST_SEARCH_TUNING ScoreType nullMoveReductionDepthDivisor   = 6;
CONST_SEARCH_TUNING ScoreType nullMoveReductionInit           = 5;
CONST_SEARCH_TUNING ScoreType nullMoveDynamicDivisor          = 141;
CONST_SEARCH_TUNING ScoreType historyExtensionThreshold       = 512;
CONST_SEARCH_TUNING colored<DepthType> CMHMaxDepth            = {3,4};
CONST_SEARCH_TUNING DepthType iidMinDepth                     = 7;
CONST_SEARCH_TUNING DepthType iidMinDepth2                    = 10;
CONST_SEARCH_TUNING DepthType iidMinDepth3                    = 3;
CONST_SEARCH_TUNING DepthType probCutMinDepth                 = 10;
CONST_SEARCH_TUNING DepthType probCutSearchDepthFactor        = 3;
CONST_SEARCH_TUNING int       probCutMaxMoves                 = 6;
CONST_SEARCH_TUNING ScoreType probCutMargin                   = 130;
CONST_SEARCH_TUNING ScoreType probCutMarginSlope              = -56;
CONST_SEARCH_TUNING ScoreType seeCaptureFactor                = 161;
CONST_SEARCH_TUNING ScoreType seeCaptureInit                  = 61;
CONST_SEARCH_TUNING ScoreType seeCapDangerDivisor             = 8;
CONST_SEARCH_TUNING ScoreType seeQuietFactor                  = 64;
CONST_SEARCH_TUNING ScoreType seeQuietInit                    = 32;
CONST_SEARCH_TUNING ScoreType seeQuietDangerDivisor           = 11;
CONST_SEARCH_TUNING ScoreType seeQThreshold                   = -50;
CONST_SEARCH_TUNING ScoreType betaMarginDynamicHistory        = 175;
//CONST_SEARCH_TUNING ScoreType probCutThreshold              = 450;
CONST_SEARCH_TUNING DepthType lmrMinDepth                 = 2;
CONST_SEARCH_TUNING int       lmrCapHistoryFactor         = 9;
CONST_SEARCH_TUNING ScoreType lmrLateExtensionMargin      = 64;
CONST_SEARCH_TUNING DepthType singularExtensionDepth      = 8;
CONST_SEARCH_TUNING DepthType singularExtensionDepthMinus = 5;
///@todo on move / opponent
CONST_SEARCH_TUNING ScoreType dangerLimitPruning        = 16;
CONST_SEARCH_TUNING ScoreType dangerLimitForwardPruning = 16;
CONST_SEARCH_TUNING ScoreType dangerLimitReduction      = 16;
CONST_SEARCH_TUNING ScoreType dangerDivisor             = 180;

CONST_SEARCH_TUNING ScoreType failLowRootMargin         = 106;

CONST_SEARCH_TUNING ScoreType deltaBadMargin            = 167;
CONST_SEARCH_TUNING ScoreType deltaBadSEEThreshold      = 21;
CONST_SEARCH_TUNING ScoreType deltaGoodMargin           = 50;
CONST_SEARCH_TUNING ScoreType deltaGoodSEEThreshold     = 183;

///@todo probably should be tuned with/without a net
CONST_SEARCH_TUNING DepthType aspirationMinDepth  = 4;
CONST_SEARCH_TUNING ScoreType aspirationInit      = 6;
CONST_SEARCH_TUNING ScoreType aspirationDepthCoef = 3;
CONST_SEARCH_TUNING ScoreType aspirationDepthInit = 40;

CONST_SEARCH_TUNING DepthType ttMaxFiftyValideDepth = 92;

CONST_SEARCH_TUNING DepthType iirMinDepth           = 2;
CONST_SEARCH_TUNING DepthType iirReduction          = 1;

CONST_SEARCH_TUNING DepthType ttAlphaCutDepth       = 1;
CONST_SEARCH_TUNING ScoreType ttAlphaCutMargin      = 63;
CONST_SEARCH_TUNING DepthType ttBetaCutDepth        = 1;
CONST_SEARCH_TUNING ScoreType ttBetaCutMargin       = 65;

CONST_SEARCH_TUNING ScoreType lazySortThreshold     = -512;
CONST_SEARCH_TUNING ScoreType lazySortThresholdQS   = -512;

CONST_SEARCH_TUNING int       distributedTTBufSize  = 16;

//CONST_SEARCH_TUNING ScoreType randomAggressiveReductionFactor = 2;

} // namespace SearchConfig
