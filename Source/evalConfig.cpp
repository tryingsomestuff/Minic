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
      {  98,  88},{ 133,  73},{  60,  78},{  95,  53},{  68,  91},{ 126,  65},{  34, 130},{ -11, 137},
      {  -4,  70},{   7,  45},{  23,  21},{  28, -12},{  65, -27},{  56,  15},{  25,  43},{ -20,  45},
      {   8,  62},{   6,  40},{  11,  22},{   5,   9},{  14,  18},{  24,  27},{  13,  36},{  -1,  47},
      {  -2,  33},{ -14,  26},{   9,   1},{  16,  -5},{  17,  -9},{  22,   3},{  -2,  11},{   6,   8},
      {  -2,  29},{ -16,  27},{   9,  11},{   5,   9},{  18,  13},{  16,  12},{  31,  11},{  13,  11},
      { -15,  29},{ -12,  24},{   0,   7},{  -7,   5},{   6,  24},{  16,  14},{  26,   5},{  -9,   7},
      {   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},
   },
   {
      {-167, -58},{ -89, -39},{ -34, -12},{ -49, -29},{  61, -32},{ -97, -27},{ -15, -63},{-107, -99},
      { -73, -74},{ -42, -39},{  71, -69},{  36, -32},{  23, -53},{  62, -72},{   7, -59},{ -17, -57},
      { -47, -32},{  60, -42},{  29, -10},{  50, -14},{  82, -35},{ 129, -42},{  73, -43},{  44, -50},
      {  -9, -14},{  27,   4},{  50, -16},{  60,  -3},{  42,  -4},{  74,  -9},{  34,   0},{  28, -19},
      {  -3, -15},{   4, -11},{  34,  -8},{  25,  -2},{  25,   8},{  31, -12},{  24,  -2},{   3, -22},
      { -24, -30},{  -4, -10},{  16, -27},{  10,  -5},{   8,   3},{  10, -20},{  10, -26},{ -11, -27},
      { -29, -42},{ -53, -22},{  -5, -16},{   0, -36},{  -8, -25},{  -1, -31},{ -14, -22},{ -16, -30},
      {-105, -29},{ -32, -54},{ -58, -20},{ -36, -11},{ -36, -29},{ -30, -27},{ -39, -32},{ -23, -64},
   },
   {
      { -29, -14},{   4, -19},{ -82, -12},{ -37,  -8},{ -25,  -6},{ -42,  -9},{   7, -16},{  -8, -24},
      { -26,  -6},{  14,  -7},{ -18,   6},{ -13, -13},{  30,  -3},{  56, -14},{  18,  -7},{ -48, -15},
      {  -9,   3},{  37,  -2},{  37,  -1},{  40, -11},{  35, -10},{  49,  -4},{  37,   3},{  40,   0},
      {  -4,   7},{   1,  15},{  18,  11},{  29,  11},{  31,   6},{  33,   2},{  23,   3},{   9,   5},
      {  -2,  -1},{   5,  15},{  21,  16},{  25,  16},{  25,   7},{  24,   5},{  17,  -2},{  21, -14},
      {  12, -12},{  40,  13},{  38,   2},{  33,  11},{  37,  10},{  41,   8},{  44, -12},{  27, -16},
      {  25, -14},{  26, -12},{  39, -19},{  25,  -6},{  28,   1},{  37,  -8},{  47, -16},{  33, -27},
      { -32, -20},{  16,  -9},{  10, -15},{  -5,   5},{  -4,  17},{ -10,  13},{ -39,  -4},{ -21, -21},
   },
   {
      {  32,  12},{  42,   9},{  32,  14},{  51,   8},{  63,   7},{   9,  17},{  31,  14},{  43,   7},
      {  28,   5},{  27,  13},{  56,   1},{  61,  -2},{  79, -16},{  67,  -2},{  26,  16},{  44,   4},
      {  -6,  12},{  19,   8},{  25,  -5},{  35, -16},{  16,  -6},{  45,  -9},{  61,  -3},{  16,  -5},
      { -25,  18},{ -12,  18},{   6,  16},{  25,  -7},{  25, -17},{  35,   3},{  -8,   6},{ -19,  10},
      { -34,  17},{ -25,  20},{ -14,   9},{  -3,   6},{   6,  -9},{  -6,   5},{   6,  -3},{ -24,  -3},
      { -31,   4},{ -19,  11},{ -16,  -1},{ -17,   5},{   4, -15},{   7, -11},{  10,   1},{ -19,  -8},
      { -26,  -2},{ -10,   2},{ -17,   2},{  -5, -11},{   8, -11},{  13, -24},{  -2,  -8},{ -50,   3},
      {   0,   6},{   1,  -5},{  -1,  -1},{   6, -11},{  18, -17},{  16,  -6},{  22, -12},{  13, -22},
   },
   {
      { -28, -10},{   0,  24},{  29,  18},{  11,  20},{  59,  27},{  44,  18},{  43,  10},{  45,  20},
      { -23, -11},{ -46,  33},{ -25,  29},{  -1,  41},{ -20,  55},{  37,  15},{  24,  26},{  53,  -1},
      {   1, -16},{ -16,   7},{  -7,  13},{ -15,  48},{  -7,  37},{  43,  26},{  37,  17},{  23,  -7},
      { -12,  16},{  -3,  23},{ -22,  25},{ -37,  43},{ -36,  52},{  -8,  32},{   1,  51},{  16,  19},
      {   6,  13},{ -21,  39},{  -6,  15},{ -15,  45},{  -6,  24},{   3,  15},{   9,  33},{  32,  21},
      {   5, -15},{  15,  -7},{   9,  12},{  10,   0},{  20,  11},{  28,  -1},{  40,  -1},{  32,   3},
      { -10, -14},{  20, -15},{  20, -12},{  30,  -6},{  28,   2},{  36, -19},{  37, -33},{   2, -31},
      {  12, -32},{  17, -30},{  21, -13},{  20,   7},{  27,   0},{   5, -18},{ -31, -19},{ -50, -41},
   },
   {
      { -65, -74},{  23, -34},{  16, -17},{ -15,  -4},{ -56, -12},{ -34,  20},{   2,   4},{  13, -17},
      {  29, -15},{  -1,  20},{ -20,  14},{  -7,  32},{  -8,  18},{  -4,  43},{ -38,  22},{ -29,  11},
      {  -9,   8},{  24,  31},{   2,  41},{ -16,  40},{ -20,  41},{   6,  52},{  22,  44},{ -22,  12},
      { -17, -10},{ -20,  20},{ -12,  57},{ -27,  55},{ -30,  47},{ -25,  38},{ -14,  44},{ -36,   3},
      { -49, -19},{  -1,  15},{ -27,  41},{ -39,  41},{ -47,  36},{ -43,  30},{ -34,  -1},{ -51,  -5},
      { -14, -28},{ -14,  15},{ -24,   9},{ -48,  33},{ -45,  27},{ -29,   7},{ -13,  -1},{ -29, -27},
      {   1, -32},{   7,  -1},{  -6,   4},{ -52,  17},{ -48,  16},{ -15,   2},{  26, -14},{  38, -53},
      { -15, -54},{  91, -82},{  52, -53},{ -66,  -7},{  15, -71},{ -33, -27},{  64, -78},{  67,-128},
   }
};

///@todo make more things depend on rank/file ???

CONST_TEXEL_TUNING EvalScore   pawnShieldBonus         = {  6,-5};
CONST_TEXEL_TUNING EvalScore   pawnFawnMalusKS         = { 19,-6};
CONST_TEXEL_TUNING EvalScore   pawnFawnMalusQS         = {-15, 7};
// this depends on rank
CONST_TEXEL_TUNING EvalScore   passerBonus[8]          = { { 0, 0 }, {  0,-33}, {-12,  2}, {-10, 24}, {-20, 59}, {  6, 74}, { 23, 50}, {0, 0}};

CONST_TEXEL_TUNING EvalScore   rookBehindPassed        = { -6,39};
CONST_TEXEL_TUNING EvalScore   kingNearPassedPawn      = { -8,16};

// this depends on rank (shall try file also ??)
CONST_TEXEL_TUNING EvalScore   doublePawn[8][2]   = { {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}}, {{ 11, 27},{  1, 48}}, {{ 15, 27},{  3, 27}}, {{ 21, 21},{  6, 31}}, {{ -4,  6},{ 27, 31}}, {{ 23, 13},{ 45, 27}}, {{  0,  0},{  0,  0}} }; // close semiopenfile
CONST_TEXEL_TUNING EvalScore   isolatedPawn[8][2] = { {{  0,  0},{  0,  0}}, {{  9,  3},{ 29,  5}}, {{  1, 10},{  3,  5}}, {{  5,  0},{  1, 18}}, {{ 11,-16},{-17, 38}}, {{ -1, 26},{-42, 11}}, {{  9,  7},{ 10,  4}}, {{  0,  0},{  0,  0}} }; // close semiopenfile
CONST_TEXEL_TUNING EvalScore   backwardPawn[8][2] = { {{  0,  0},{  0,  0}}, {{ -2,  0},{ 26, 17}}, {{ 12,  6},{ 44, 19}}, {{  8, 10},{ 36,  0}}, {{-11,  7},{ 45,-10}}, {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}} }; // close semiopenfile
CONST_TEXEL_TUNING EvalScore   detachedPawn[8][2] = { {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}}, {{ 26,  1},{ 39, 17}}, {{  8,  9},{ 28, -2}}, {{  6, 37},{  7, 11}}, {{-17,  0},{ -1, 29}}, {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}} }; // close semiopenfile

CONST_TEXEL_TUNING EvalScore   holesMalus              = { -2, 1};
CONST_TEXEL_TUNING EvalScore   outpost                 = { 18,18};
CONST_TEXEL_TUNING EvalScore   pieceFrontPawn          = {-15, 5};
CONST_TEXEL_TUNING EvalScore   centerControl           = {  7, 1};
CONST_TEXEL_TUNING EvalScore   knightTooFar[8]         = { {  0,  0}, { 19,  7}, { 14,  8}, {  5, 11}, {-16, 20}, { -6,  7}, {-10,  1}, {  0,  0} };

// this depends on rank
CONST_TEXEL_TUNING EvalScore   candidate[8]            = { {0, 0}, {-21, 16}, {-19, 14}, {  3, 37}, { 53, 61}, { 34, 64}, { 34, 64}, { 0, 0} };
CONST_TEXEL_TUNING EvalScore   protectedPasserBonus[8] = { {0, 0}, { 24, -8}, { 10,-21}, {  8, -4}, { 54, -4}, {117, 30}, {  8, 31}, { 0, 0} };
CONST_TEXEL_TUNING EvalScore   freePasserBonus[8]      = { {0, 0}, {  6, 13}, { -1,  8}, { -1, 21}, {-17, 55}, {-11,150}, { 50,149}, { 0, 0} };

CONST_TEXEL_TUNING EvalScore   pawnMobility            = {-12, 15};
CONST_TEXEL_TUNING EvalScore   pawnSafeAtt             = { 75, 24};
CONST_TEXEL_TUNING EvalScore   pawnSafePushAtt         = { 29, 16};
CONST_TEXEL_TUNING EvalScore   pawnlessFlank           = {-18,-33};
CONST_TEXEL_TUNING EvalScore   pawnStormMalus          = { 23,-28};
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
CONST_TEXEL_TUNING EvalScore MOB[6][15] = { { { -8,-19},{ -1, 19},{  9, 33},{ 13, 37},{ 19, 40},{ 23, 45},{ 41, 31},{ 35, 46},{ 29, 47},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0} },
                                            { {-22,-10},{-14, 23},{ -8, 30},{ -2, 35},{  1, 35},{ -2, 45},{  9, 37},{ 21, 32},{ 41, 26},{ 45, 36},{ 55, 37},{ 71, 65},{ 71, 56},{120, 95},{  0,  0} },
                                            { { 19, 13},{ 22, 41},{ 19, 61},{ 26, 59},{ 17, 69},{ 20, 73},{ 17, 76},{ 20, 80},{ 25, 79},{ 45, 73},{ 57, 70},{ 56, 73},{ 48, 76},{ 57, 69},{ 72, 56} },
                                            { {-15,-28},{-16, 21},{ -4,  5},{ -5, 20},{ -4, 25},{  0, 27},{  9, 23},{ 16, 41},{ 13, 13},{ 23, 47},{ 28, 50},{ 34, 24},{ 16, 32},{ 24, 87},{  0,  0} },
                                            { {  1,-57},{ -4, -6},{  2, -7},{  1,  5},{ -1,  8},{  2, 18},{  4, 19},{  7, 15},{ 13, 15},{ 21, 26},{  8, 37},{ 21, 43},{ 25, 48},{ 19, 45},{ 24, 54} },
                                            { { 10,  0},{ -5, 31},{-10, 36},{-19, 31},{-25, 26},{-28, 15},{-29, 19},{-30, 14},{-40, -6},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0} } };

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
void applyShashinCorrection(const Position &  /*p*/, const EvalData &  /*data*/ , EvalFeatures & /*features*/ ){
    
    return;
    /*
    const ShashinType stype = data.shashinMobRatio  < 0.8 ? Shashin_Petrosian :
                              data.shashinMobRatio == 0.8 ? Shashin_Capablanca_Petrosian :
                              data.shashinMobRatio  < 1.2 ? Shashin_Capablanca :
                              data.shashinMobRatio == 1.2 ? Shashin_Tal_Capablanca :
                              Shashin_Tal; // data.shashinMobRatio > 1.2
    */

    // material
    //features.materialScore = features.materialScore;

    // take forwardness into account if Petrosian
    //if ( stype >= Shashin_Capablanca_Petrosian ) features.materialScore += EvalConfig::forwardnessMalus * (p.c==Co_White?-1:+1) * (data.shashinForwardness[p.c] - data.shashinForwardness[~p.c]);

    // positional
    //features.positionalScore = features.positionalScore;
    //scaleShashin(features.positionalScore,data.shashinMaterialFactor,data.shashinMobRatio);

    // development
    //features.developmentScore = features.developmentScore;
    //scaleShashin(features.developmentScore,data.shashinMaterialFactor,1.f/data.shashinMobRatio);

    // mobility
    //features.mobilityScore = features.mobilityScore;
    //scaleShashin(features.mobilityScore,data.shashinMaterialFactor,1.f/data.shashinMobRatio);

    // pawn structure
    //features.pawnStructScore = features.pawnStructScore;

    // attack
    //features.attackScore = features.attackScore;
    //scaleShashin(features.attackScore,data.shashinMaterialFactor,data.shashinMobRatio);

}

} // EvalConfig
