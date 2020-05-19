#include "evalConfig.hpp"

#include "bitboard.hpp"
#include "position.hpp"

namespace EvalConfig {

CONST_TEXEL_TUNING EvalScore imbalance_mines[5][5] = {
    // pawn knight bishop rook queen
    { {   -5,  103}, },
    { {   54,  347}, { -127, -259}, },
    { {  227,  252}, { -224, -247}, { -202, -275}, },
    { {  243,  578}, { -215, -178}, { -239, -389}, { -195, -413}, },
    { {  517,  612}, { -492, -355}, { -721, -529}, {-1232,-1130}, { -417, -417}, }
}; 

CONST_TEXEL_TUNING EvalScore imbalance_theirs[5][5] = {
    // pawn knight bishop rook queen
    { { -156, -261}, },
    { {  210,  388}, {   13,  -47}, },
    { {  118,  531}, {   53,  -70}, {   27,  -45}, },
    { {  178,  675}, {   80,  -55}, {  -32, -221}, {  -38, -212}, },
    { {  732,  918}, {  445,  344}, {  494,  296}, {  360,  336}, {   35,    4}, }
};

CONST_TEXEL_TUNING EvalScore PST[6][64] = {
   {
      {   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},
      {  98,  88},{ 133,  73},{  60,  78},{  95,  53},{  68,  91},{ 126,  65},{  34, 129},{ -11, 137},
      {  -4,  73},{   7,  45},{  23,  22},{  28, -10},{  65, -30},{  56,  15},{  25,  42},{ -20,  45},
      {   7,  58},{   5,  44},{   9,  32},{   9,  12},{  18,  16},{  28,  19},{  16,  33},{  -1,  48},
      {  -4,  30},{ -16,  26},{   8,   7},{  22,  -5},{  22,  -3},{  22,   6},{   4,  11},{   5,  10},
      {  -6,  26},{ -23,  27},{   5,   9},{   5,   6},{  17,   9},{  16,   6},{  30,   3},{  12,   7},
      {  -9,  25},{ -23,  20},{   0,  10},{  -6,   7},{   9,  19},{  23,   8},{  22,  -2},{  -8,  10},
      {   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},
   },
   {
      {-167, -58},{ -89, -38},{ -34, -13},{ -49, -28},{  61, -31},{ -97, -27},{ -15, -63},{-107, -99},
      { -73, -75},{ -42, -45},{  71, -69},{  36, -34},{  23, -53},{  62, -76},{   7, -59},{ -17, -57},
      { -47, -32},{  60, -42},{  29,  -9},{  53, -21},{  82, -32},{ 129, -36},{  73, -42},{  44, -49},
      {  -9, -18},{  28,  -5},{  45,  -4},{  68,  -8},{  50,  -7},{  75,  -5},{  38, -10},{  26, -18},
      {  -5, -19},{   4,  -8},{  34,  -9},{  21,  10},{  28,  10},{  29, -13},{  24,  -3},{   2, -22},
      { -26, -26},{   2, -11},{  18, -28},{   6,   0},{  10,  -2},{  11, -20},{  12, -24},{ -12, -24},
      { -29, -42},{ -53, -20},{  -8, -15},{   1, -27},{  -3, -22},{   0, -32},{ -14, -20},{ -17, -34},
      {-105, -29},{ -35, -44},{ -58, -23},{ -36, -13},{ -37, -25},{ -29, -20},{ -41, -31},{ -23, -64},
   },
   {
      { -29, -14},{   4, -21},{ -82, -11},{ -37,  -9},{ -25,  -7},{ -42,  -9},{   7, -17},{  -8, -24},
      { -26,  -6},{  14,  -7},{ -18,   6},{ -13, -11},{  30,  -3},{  56, -15},{  18,  -5},{ -48, -14},
      { -10,   6},{  37,  -6},{  38,  -5},{  40,  -9},{  35,  -6},{  49,   2},{  37,   3},{  41,   3},
      {  -4,   5},{   3,  10},{  19,  18},{  37,   7},{  32,   4},{  33,   6},{  28,  -8},{   9,   4},
      {  -4,  -4},{   7,   9},{  26,  14},{  27,  11},{  28,   3},{  31,   4},{  17,   3},{  16, -14},
      {  14,  -6},{  42,   9},{  33,  11},{  36,   4},{  39,  18},{  39,   7},{  46, -11},{  28, -11},
      {  29, -16},{  25, -13},{  39, -15},{  20,  12},{  33,   0},{  34,  -4},{  47, -17},{  31, -26},
      { -32, -21},{  15,  -9},{  13, -11},{  -6,   9},{  -4,   7},{  -3,  14},{ -39,  -5},{ -21, -18},
   },
   {
      {  32,  12},{  42,   9},{  32,  12},{  51,   7},{  63,   8},{   9,  16},{  31,  14},{  43,   7},
      {  28,   2},{  27,  10},{  57,  -1},{  61,   0},{  79, -17},{  67,  -3},{  26,  13},{  44,   4},
      {  -6,  11},{  19,   7},{  25,  -6},{  35, -12},{  16,  -4},{  45, -10},{  61,  -5},{  16,  -6},
      { -25,  15},{ -12,  16},{   6,  15},{  25,  -1},{  25, -15},{  35,   2},{  -8,   8},{ -19,  10},
      { -34,  14},{ -25,  19},{ -14,  14},{  -3,   5},{   6,  -4},{  -6,   1},{   6,  -3},{ -24,  -4},
      { -31,   3},{ -19,  10},{ -16,   1},{ -17,   2},{   5, -14},{   6, -11},{   9,   2},{ -19,  -6},
      { -26,   0},{ -10,   4},{ -17,   4},{  -5,  -8},{   6, -13},{  14, -24},{  -2,  -9},{ -50,   4},
      {   5,  13},{   0,   4},{  -1,   4},{   9,  -7},{  20, -11},{  22,   0},{  18,  -7},{   7,  -3},
   },
   {
      { -28, -10},{   0,  22},{  29,  20},{  11,  21},{  59,  26},{  44,  19},{  43,  10},{  45,  20},
      { -26, -13},{ -49,  31},{ -25,  29},{   0,  42},{ -20,  58},{  38,  20},{  24,  30},{  53,   0},
      {   0, -18},{ -16,   5},{  -6,  10},{ -12,  48},{  -3,  38},{  43,  30},{  38,  20},{  23,  -1},
      { -14,  10},{  -6,  25},{ -23,  21},{ -33,  45},{ -30,  52},{  -5,  36},{   1,  54},{  10,  34},
      {   8,   6},{ -24,  33},{  -6,  15},{ -13,  40},{  -3,  28},{  -7,  33},{  10,  37},{  27,  26},
      {   4, -16},{  19, -19},{   4,  16},{   8,   9},{  15,  15},{  29,   3},{  42,   3},{  30,   8},
      { -11, -15},{  13, -15},{  20, -13},{  35, -15},{  34,  -9},{  31, -22},{  37, -33},{   2, -31},
      {  11, -33},{  17, -29},{  24, -22},{  28,   0},{  26,  -1},{   3, -23},{ -31, -20},{ -50, -41},
   },
   {
      { -65, -74},{  23, -35},{  16, -17},{ -15,  -4},{ -56, -12},{ -34,  20},{   2,   4},{  13, -17},
      {  29, -15},{  -1,  17},{ -20,  14},{  -7,  31},{  -8,  20},{  -4,  42},{ -38,  25},{ -29,  11},
      {  -9,   7},{  24,  29},{   2,  41},{ -16,  36},{ -20,  35},{   6,  46},{  22,  49},{ -22,  13},
      { -17,  -8},{ -20,  23},{ -12,  48},{ -27,  51},{ -30,  45},{ -25,  40},{ -14,  32},{ -36,  -3},
      { -49, -16},{  -1,   9},{ -27,  30},{ -39,  35},{ -47,  28},{ -43,  21},{ -34,   2},{ -51, -12},
      { -14, -24},{ -14,   7},{ -24,   6},{ -47,  23},{ -44,  19},{ -30,   9},{ -14,  -3},{ -28, -20},
      {   1, -31},{   7,  -3},{  -8,   1},{ -58,  21},{ -50,  15},{ -16,   4},{  27, -17},{  34, -50},
      { -15, -54},{  92, -74},{  58, -50},{ -67,  -8},{  14, -61},{ -32, -19},{  60, -63},{  66,-111},
   }
};

///@todo make more things depend on rank/file ???

CONST_TEXEL_TUNING EvalScore   pawnShieldBonus         = { 6,-4};
CONST_TEXEL_TUNING EvalScore   pawnFawnMalusKS         = {21,-5};
CONST_TEXEL_TUNING EvalScore   pawnFawnMalusQS         = {-6, 3};
// this depends on rank
CONST_TEXEL_TUNING EvalScore   passerBonus[8]          = { { 0, 0 }, {  4,-33}, { -9, -2}, { -5, 21}, {-20, 52}, { 10, 68}, { 26, 51}, {0, 0}};

CONST_TEXEL_TUNING EvalScore   rookBehindPassed        = { -6,39};
CONST_TEXEL_TUNING EvalScore   kingNearPassedPawn      = { -8,16};

// this depends on rank (shall try file also ??)
CONST_TEXEL_TUNING EvalScore   doublePawn[8][2]   = { {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}}, {{  6, 29},{ -2, 38}}, {{ 14, 22},{  3, 31}}, {{ 21, 22},{ 11, 16}}, {{-10,  3},{ 26, 26}}, {{ 23, 13},{ 45, 26}}, {{  0,  0},{  0,  0}} }; // close semiopenfile
CONST_TEXEL_TUNING EvalScore   isolatedPawn[8][2] = { {{  0,  0},{  0,  0}}, {{ 14,  2},{ 35, 10}}, {{  0,  7},{  2, 12}}, {{ -3,  1},{  0, 12}}, {{  8, -5},{-21, 37}}, {{ -1, 20},{-42, 12}}, {{  9,  7},{ 10,  7}}, {{  0,  0},{  0,  0}} }; // close semiopenfile
CONST_TEXEL_TUNING EvalScore   backwardPawn[8][2] = { {{  0,  0},{  0,  0}}, {{  3, -4},{ 29, 15}}, {{ 10,  6},{ 41, 17}}, {{ 15, 10},{ 39,  6}}, {{-11,  1},{ 47, -5}}, {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}} }; // close semiopenfile
CONST_TEXEL_TUNING EvalScore   detachedPawn[8][2] = { {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}}, {{ 24,  3},{ 40, 12}}, {{ 19, 10},{ 29,  7}}, {{ 11, 28},{ 14,  4}}, {{-14, -3},{  2, 21}}, {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}} }; // close semiopenfile

CONST_TEXEL_TUNING EvalScore   holesMalus              = { -3, 1};
CONST_TEXEL_TUNING EvalScore   outpost                 = { 17,18};
CONST_TEXEL_TUNING EvalScore   pieceFrontPawn          = {-15, 5};
CONST_TEXEL_TUNING EvalScore   centerControl           = {  7, 3};
CONST_TEXEL_TUNING EvalScore   knightTooFar[8]         = { {  0,  0}, { 19,  7}, { 14,  8}, {  5, 11}, {-16, 20}, { -6,  7}, {-10,  1}, {  0,  0} };

// this depends on rank
CONST_TEXEL_TUNING EvalScore   candidate[8]            = { {0, 0}, {-16,  4}, {-13, 11}, {  5, 29}, { 58, 57}, { 34, 64}, { 34, 64}, { 0, 0} };
CONST_TEXEL_TUNING EvalScore   protectedPasserBonus[8] = { {0, 0}, { 24, -8}, { 10,-15}, {  3, -4}, { 53, -5}, {111, 30}, {  7, 30}, { 0, 0} };
CONST_TEXEL_TUNING EvalScore   freePasserBonus[8]      = { {0, 0}, {  9, 14}, {  1,  2}, { -6, 24}, {-11, 52}, {-10,143}, { 52,148}, { 0, 0} };

CONST_TEXEL_TUNING EvalScore   pawnMobility            = {-11, 13};
CONST_TEXEL_TUNING EvalScore   pawnSafeAtt             = { 77, 23};
CONST_TEXEL_TUNING EvalScore   pawnSafePushAtt         = { 23, 24};
CONST_TEXEL_TUNING EvalScore   pawnlessFlank           = {-21,-30};
CONST_TEXEL_TUNING EvalScore   pawnStormMalus          = { 21,-28};
CONST_TEXEL_TUNING EvalScore   rookOnOpenFile          = { 45, 19};
CONST_TEXEL_TUNING EvalScore   rookOnOpenSemiFileOur   = { 20, -2};
CONST_TEXEL_TUNING EvalScore   rookOnOpenSemiFileOpp   = { 32,  5};

CONST_TEXEL_TUNING EvalScore   rookQueenSameFile       = {  2, 17};
CONST_TEXEL_TUNING EvalScore   rookFrontQueenMalus     = {-14,-12};
CONST_TEXEL_TUNING EvalScore   rookFrontKingMalus      = {-29, 26};
CONST_TEXEL_TUNING EvalScore   minorOnOpenFile         = {  9,  6};

CONST_TEXEL_TUNING EvalScore   pinnedKing [5]          = { { -4,  0}, { 23, 50}, {  2, 60}, {  3, 61}, { -2, 25} };
CONST_TEXEL_TUNING EvalScore   pinnedQueen[5]          = { {  1,-23}, {-20,  7}, { -8, 12}, { -3,  6}, { 43, 36} };

CONST_TEXEL_TUNING EvalScore   hangingPieceMalus       = {-31, -7};
   
CONST_TEXEL_TUNING EvalScore   threatByMinor[6]        = { {-10,-16}, {-29,-28}, {-33,-31}, {-58,-23}, {-47,-19}, {-60,-22} };
CONST_TEXEL_TUNING EvalScore   threatByRook[6]         = { {-10,-26}, {-35,-24}, {-42,-22}, { -8,  8}, {-73,-12}, {-19,-13} };
CONST_TEXEL_TUNING EvalScore   threatByQueen[6]        = { {  0,-14}, {  1,-14}, {  7,-26}, { 13, -8}, { 47, 20}, {-32,-51} };
CONST_TEXEL_TUNING EvalScore   threatByKing[6]         = { {-42,-40}, {  3,-36}, {-10,-58}, { 13,-51}, {  0,  0}, {  0,  0} };
   
// this depends on number of pawn
CONST_TEXEL_TUNING EvalScore   adjKnight[9]            = { {-26,-20}, {-10, -4}, {  1,  0}, {  1,  6}, { -2,  9}, {  6, 17}, { 13, 35}, { 24, 59}, { 43, 45} };
CONST_TEXEL_TUNING EvalScore   adjRook[9]              = { { 24, -8}, { 15,  6}, {-29, 28}, {-52, 34}, {-57, 26}, {-62, 19}, {-58, 12}, {-51,  9}, {-51, 29} };
CONST_TEXEL_TUNING EvalScore   bishopPairBonus[9]      = { { 29, 56}, { 28, 58}, { 30, 72}, { 15, 85}, { 22, 79}, { 20, 90}, { 19,101}, { 24,115}, { 45, 68} };
CONST_TEXEL_TUNING EvalScore   badBishop[9]            = { { -6, -5}, { -5,  1}, {  2,  6}, {  6, 17}, { 13, 23}, { 12, 40}, { 18, 46}, { 23, 52}, { 39, 65} };

CONST_TEXEL_TUNING EvalScore   knightPairMalus         = { 5,  9};
CONST_TEXEL_TUNING EvalScore   rookPairMalus           = { 4, -4};

CONST_TEXEL_TUNING EvalScore   queenNearKing           = { 4, 11};

//N B R QB QR K
CONST_TEXEL_TUNING EvalScore MOB[6][15] = { { { -5,-22},{  2, 19},{  8, 32},{ 14, 36},{ 20, 34},{ 24, 43},{ 41, 30},{ 35, 46},{ 29, 47},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0} },
                                            { {-22, -6},{-14, 21},{ -4, 28},{ -1, 32},{  3, 36},{  0, 43},{ 10, 37},{ 21, 33},{ 41, 25},{ 45, 36},{ 55, 37},{ 71, 65},{ 71, 56},{120, 95},{  0,  0} },
                                            { { 18, 11},{ 18, 47},{ 20, 60},{ 21, 62},{ 13, 69},{ 18, 70},{ 18, 73},{ 20, 76},{ 27, 75},{ 45, 70},{ 57, 70},{ 56, 73},{ 48, 76},{ 57, 69},{ 72, 56} },
                                            { {-15,-29},{-15, 15},{ -8, 15},{ -6, 23},{ -7, 30},{  1, 25},{  9, 22},{ 16, 41},{ 13, 13},{ 23, 47},{ 28, 50},{ 34, 24},{ 16, 32},{ 24, 87},{  0,  0} },
                                            { {  3,-58},{  1, -7},{  5, -6},{  1,  8},{ -4, 22},{  2, 15},{  5, 14},{  7, 15},{ 13, 15},{ 21, 26},{  8, 37},{ 21, 43},{ 25, 48},{ 19, 45},{ 24, 54} },
                                            { {  8, -1},{ -6, 27},{-12, 29},{-19, 30},{-27, 29},{-28, 21},{-29, 17},{-30, 12},{-40,  1},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0} } };

CONST_TEXEL_TUNING EvalScore initiative[4] = { {  1,  7}, { 59, 40}, {115, 65}, { 71, 88} };

CONST_TEXEL_TUNING ScoreType kingAttMax    = 428;
CONST_TEXEL_TUNING ScoreType kingAttTrans  = 44;
CONST_TEXEL_TUNING ScoreType kingAttScale  = 12;
CONST_TEXEL_TUNING ScoreType kingAttOffset =  9;
CONST_TEXEL_TUNING ScoreType kingAttWeight[2][6]    = { { 140, 241, 316, 238, 290, -34, },{ 198, 128, 112,  31, -20,  0 } };
CONST_TEXEL_TUNING ScoreType kingAttSafeCheck[6]    = {   128, 1184, 1152, 1056, 1024, 0};
CONST_TEXEL_TUNING ScoreType kingAttOpenfile        = 162;
CONST_TEXEL_TUNING ScoreType kingAttSemiOpenfileOpp = 93;
CONST_TEXEL_TUNING ScoreType kingAttSemiOpenfileOur = 111;
ScoreType kingAttTable[64] = {0};

CONST_TEXEL_TUNING EvalScore tempo = {28, 28};

// slow application of factor depending on materialFactor around 1 (meaning equal material)
inline void scaleShashin(EvalScore & score, const float materialFactor, const float factor){
   return score = EvalScore{ScoreType(materialFactor*score[MG]*factor+(1-materialFactor)*score[MG]),
                            ScoreType(materialFactor*score[EG]*factor+(1-materialFactor)*score[EG])};
}

///@todo use shashinMobRatio in draw score !

//-------------------------------------------
// if more or less even in material
// if mobility is already high
//    * attack ++
//    * positional ++
//    * try sac ?
// if mobility is even
//    * positional ++
//    * exchange ++
//    * push pawn ++
// if mobility is too small
//    * go back
//    * mobility ++
//-------------------------------------------
void applyShashinCorrection(const Position & /* p */, const EvalData & /* data */, EvalFeatures & /* features */ ){
    
    return;
/*
    const ShashinType stype = data.shashinMobRatio  < 0.8 ? Shashin_Petrosian :
                              data.shashinMobRatio == 0.8 ? Shashin_Capablanca_Petrosian :
                              data.shashinMobRatio  < 1.2 ? Shashin_Capablanca :
                              data.shashinMobRatio == 1.2 ? Shashin_Tal_Capablanca :
                              Shashin_Tal; // data.shashinMobRatio > 1.2

    // material
    features.materialScore = features.materialScore;

    // take forwardness into account if Petrosian
    if ( stype >= Shashin_Capablanca_Petrosian ) features.materialScore += EvalConfig::forwardnessMalus * (p.c==Co_White?-1:+1) * (data.shashinForwardness[p.c] - data.shashinForwardness[~p.c]);

    // positional
    //features.positionalScore = features.positionalScore;
    scaleShashin(features.positionalScore,data.shashinMaterialFactor,data.shashinMobRatio);

    // development
    //features.developmentScore = features.developmentScore;
    scaleShashin(features.developmentScore,data.shashinMaterialFactor,1.f/data.shashinMobRatio);

    // mobility
    //features.mobilityScore = features.mobilityScore;
    scaleShashin(features.mobilityScore,data.shashinMaterialFactor,1.f/data.shashinMobRatio);

    // pawn structure
    features.pawnStructScore = features.pawnStructScore;

    // attack
    //features.attackScore = features.attackScore;
    scaleShashin(features.attackScore,data.shashinMaterialFactor,data.shashinMobRatio);
*/
}

} // EvalConfig
