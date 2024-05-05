#include "searchConfig.hpp"

namespace SearchConfig {

///@todo try parity pruning, prune less when ply is odd
///@todo tune everything when evalScore is from TT score
// "Init", "Bonus", "SlopeDepth", "SlopeGP", "MinDepth", "MaxDepth"
CONST_SEARCH_TUNING Coeff<2,2> staticNullMoveCoeff        = { {  -15,  -32}, {  404,  112}, {  82,  75}, {  149,  21}, {0, 0}, {6, 6}, "staticNullMove" };
CONST_SEARCH_TUNING Coeff<2,2> razoringCoeff              = { {  123,  119}, {  279,  651}, {  69,  66}, {  -22,  17}, {0, 0}, {2, 2}, "razoring" };
CONST_SEARCH_TUNING Coeff<2,2> threatCoeff                = { {  33,  10}, {  259,  261}, {  24,  18}, {  -15,  40}, {0, 0}, {2, 2}, "threat" };
//CONST_SEARCH_TUNING Coeff<2,2> ownThreatCoeff             = { {0, 0}, {256, 256}, {64, 64}, {0, 0}, {0, 0}, {2, 2}, "ownThreat" };
CONST_SEARCH_TUNING Coeff<2,2> historyPruningCoeff        = { {  8,  -12}, {  247,  306}, {  -13,  9}, {  16,  -37}, {0, 0}, {2, 1}, "historyPruning" };
CONST_SEARCH_TUNING Coeff<2,2> captureHistoryPruningCoeff = { {  5,  9}, {  265,  304}, {  -121,  -126}, {  -8,  22}, {0, 0}, {4, 3}, "captureHistoryPruning" };
CONST_SEARCH_TUNING Coeff<2,2> futilityPruningCoeff       = { {  7,  6}, {  155,  605}, {  161,  170}, {  -3,  -6}, {0, 0}, {10, 10}, "futilityPruning" };
CONST_SEARCH_TUNING Coeff<2,2> failHighReductionCoeff     = { {  119,  139}, {  303,  233}, {  -54,  7}, {  -25,  13}, {0, 0}, {MAX_DEPTH, MAX_DEPTH}, "failHighReduction" };

// first value if eval score is used, second if hash score is used
CONST_SEARCH_TUNING colored<ScoreType> qfutilityMargin        = {1024, 1024};
CONST_SEARCH_TUNING DepthType nullMoveMinDepth                = 2;
CONST_SEARCH_TUNING DepthType nullMoveVerifDepth              = 6;
CONST_SEARCH_TUNING ScoreType nullMoveMargin                  = 18;
CONST_SEARCH_TUNING ScoreType nullMoveMargin2                 = 0;
CONST_SEARCH_TUNING ScoreType nullMoveReductionDepthDivisor   = 6;
CONST_SEARCH_TUNING ScoreType nullMoveReductionInit           = 5;
CONST_SEARCH_TUNING ScoreType nullMoveDynamicDivisor          = 147;
CONST_SEARCH_TUNING ScoreType historyExtensionThreshold       = 512;
CONST_SEARCH_TUNING colored<DepthType> CMHMaxDepth            = {4,4};
CONST_SEARCH_TUNING DepthType iidMinDepth                     = 7;
CONST_SEARCH_TUNING DepthType iidMinDepth2                    = 10;
CONST_SEARCH_TUNING DepthType iidMinDepth3                    = 3;
CONST_SEARCH_TUNING DepthType probCutMinDepth                 = 10;
CONST_SEARCH_TUNING DepthType probCutSearchDepthFactor        = 3;
CONST_SEARCH_TUNING int       probCutMaxMoves                 = 5;
CONST_SEARCH_TUNING ScoreType probCutMargin                   = 124;
CONST_SEARCH_TUNING ScoreType probCutMarginSlope              = -53;
CONST_SEARCH_TUNING ScoreType seeCaptureFactor                = 180;
CONST_SEARCH_TUNING ScoreType seeCaptureInit                  = 65;
CONST_SEARCH_TUNING ScoreType seeCapDangerDivisor             = 8;
CONST_SEARCH_TUNING ScoreType seeQuietFactor                  = 61;
CONST_SEARCH_TUNING ScoreType seeQuietInit                    = 32;
CONST_SEARCH_TUNING ScoreType seeQuietDangerDivisor           = 11;
CONST_SEARCH_TUNING ScoreType seeQThreshold                   = -50;
CONST_SEARCH_TUNING ScoreType betaMarginDynamicHistory        = 170;
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
CONST_SEARCH_TUNING ScoreType dangerDivisor             = 183;

CONST_SEARCH_TUNING ScoreType failLowRootMargin         = 118;
CONST_SEARCH_TUNING ScoreType CMHMargin                 = -256;

CONST_SEARCH_TUNING ScoreType deltaBadMargin            = 169;
CONST_SEARCH_TUNING ScoreType deltaBadSEEThreshold      = 20;
CONST_SEARCH_TUNING ScoreType deltaGoodMargin           = 47;
CONST_SEARCH_TUNING ScoreType deltaGoodSEEThreshold     = 162;

///@todo probably should be tuned with/without a net
CONST_SEARCH_TUNING DepthType aspirationMinDepth  = 4;
CONST_SEARCH_TUNING ScoreType aspirationInit      = 5;
CONST_SEARCH_TUNING ScoreType aspirationDepthCoef = 3;
CONST_SEARCH_TUNING ScoreType aspirationDepthInit = 40;

CONST_SEARCH_TUNING DepthType ttMaxFiftyValideDepth = 92;

CONST_SEARCH_TUNING DepthType iirMinDepth           = 3;
CONST_SEARCH_TUNING DepthType iirReduction          = 1;

CONST_SEARCH_TUNING DepthType ttAlphaCutDepth       = 1;
CONST_SEARCH_TUNING ScoreType ttAlphaCutMargin      = 60;
CONST_SEARCH_TUNING DepthType ttBetaCutDepth        = 1;
CONST_SEARCH_TUNING ScoreType ttBetaCutMargin       = 64;

CONST_SEARCH_TUNING ScoreType lazySortThreshold     = -491;
CONST_SEARCH_TUNING ScoreType lazySortThresholdQS   = -512;

CONST_SEARCH_TUNING int       distributedTTBufSize  = 16;

CONST_SEARCH_TUNING int       capPSTScoreDivisor    = 2;
CONST_SEARCH_TUNING int       capMMLVAMultiplicator = 2;
CONST_SEARCH_TUNING int       capHistoryDivisor     = 4;


//CONST_SEARCH_TUNING ScoreType randomAggressiveReductionFactor = 2;

} // namespace SearchConfig

