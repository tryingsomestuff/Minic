#include "evalConfig.hpp"

#include "bitboard.hpp"
#include "position.hpp"

namespace EvalConfig {

CONST_EVAL_TUNING EvalScore imbalance_mines[5][5] = {
    // pawn knight bishop rook queen
    { {    9,  108} },
    { {  -19,  343}, { -121, -257} },
    { {  244,  236}, { -200, -240}, { -209, -281} },
    { {  178,  627}, { -186, -134}, { -204, -342}, { -166, -367} },
    { {  478,  641}, { -531, -360}, { -792, -537}, {-1268,-1125}, { -447, -416} }
};

CONST_EVAL_TUNING EvalScore imbalance_theirs[5][5] = {
    // pawn knight bishop rook queen
    { { -179, -309} },
    { {  196,  419}, {    5,  -56} },
    { {  118,  525}, {   16,  -86}, {   16,  -37} },
    { {  104,  712}, {   46,  -78}, {  -37, -224}, {    0, -194} },
    { {  725,  980}, {  436,  338}, {  474,  300}, {  316,  347}, {   12,   10} }
};

CONST_EVAL_TUNING EvalScore PST[PieceShift][NbSquare] = {
   {
      {   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},
      {  98,  88},{ 133,  73},{  60,  78},{  95,  53},{  68,  91},{ 126,  65},{  34, 131},{ -11, 137},
      {  -4,  67},{   7,  44},{  23,  24},{  28, -14},{  65, -21},{  56,  16},{  25,  46},{ -20,  51},
      {   6,  64},{   0,  44},{   7,  24},{  -5,   6},{   9,  14},{  18,  32},{  10,  36},{  -1,  44},
      {  -5,  32},{ -18,  22},{   4,   0},{  12,  -6},{  13,  -7},{  16,  -1},{  -2,  13},{   5,   1},
      {  -7,  30},{ -10,  24},{   8,   6},{   0,   1},{  15,   9},{  13,   5},{  35,   2},{  12,   5},
      {  -9,  21},{ -14,  18},{   0,   3},{ -11,   4},{   5,  22},{   8,   9},{  32,  -1},{ -24,   4},
      {   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},
   },
   {
      {-167, -56},{ -89, -40},{ -34,  -8},{ -49, -30},{  61, -32},{ -97, -28},{ -15, -63},{-107, -99},
      { -73, -72},{ -42, -37},{  71, -70},{  36, -35},{  23, -53},{  62, -76},{   7, -59},{ -17, -57},
      { -47, -32},{  60, -42},{  27,  -8},{  49,  -7},{  81, -39},{ 128, -47},{  72, -45},{  44, -50},
      {  -9, -10},{  30,  -2},{  57, -15},{  63,  -8},{  50,  -5},{  81, -14},{  45,  -9},{  31, -19},
      {   0, -14},{   4, -11},{  34,  -8},{  22,   3},{  27,  10},{  31, -14},{  28,  -4},{   1, -17},
      { -20, -32},{   3, -14},{  26, -35},{  15,  -6},{  20,  -7},{  19, -28},{  21, -28},{  -9, -23},
      { -29, -41},{ -53, -19},{  -9, -12},{   7, -31},{   2, -36},{  -4, -28},{ -14, -23},{ -18, -19},
      {-105, -28},{ -33, -50},{ -58, -21},{ -36, -11},{ -33, -29},{ -24, -31},{ -32, -50},{ -23, -63},
   },
   {
      { -29, -14},{   4, -17},{ -82, -12},{ -37,  -8},{ -25,  -6},{ -42,  -9},{   7, -17},{  -8, -25},
      { -26,  -6},{  14,  -7},{ -18,   7},{ -13, -13},{  30,  -5},{  56, -15},{  18, -10},{ -48, -15},
      {  -8,  10},{  37,   0},{  37,  -4},{  40, -13},{  34, -13},{  48,  -8},{  37,   2},{  40,  -2},
      {  -4,   7},{   3,  20},{  18,  18},{  29,  11},{  36,   2},{  32,  -1},{  25,  -6},{  10,   2},
      {  -3,  -3},{   2,   7},{  28,  13},{  31,   4},{  29,  -3},{  30,   5},{  16,   7},{  20, -16},
      {  14,  -6},{  41,  14},{  38,  10},{  34,   5},{  40,  21},{  47,  -4},{  49,  -4},{  32, -10},
      {  31, -22},{  32, -19},{  44, -27},{  29,  -2},{  36,  -5},{  42,  -9},{  52, -14},{  34, -26},
      { -30, -20},{  22, -15},{  19, -20},{  -3,   3},{  -4,  15},{  -1,  12},{ -39,  -4},{ -21, -22},
   },
   {
      {  32,  12},{  42,   8},{  32,  15},{  51,   6},{  63,   6},{   9,  17},{  31,  14},{  43,   7},
      {  28,   7},{  25,  15},{  55,   3},{  61,   1},{  79, -16},{  67,  -1},{  26,  16},{  44,   6},
      {  -6,  11},{  19,   6},{  25,  -1},{  35, -14},{  16,  -6},{  45,  -9},{  61,  -3},{  16,  -4},
      { -25,  23},{ -12,  18},{   5,  13},{  25,  -9},{  25, -15},{  35,  -2},{  -8,   6},{ -19,  12},
      { -34,  17},{ -25,  20},{ -14,  10},{  -3,   7},{   5,  -9},{  -6,   1},{   6,  -1},{ -24,  -3},
      { -31,   2},{ -19,   9},{ -17,  -4},{ -17,  -4},{   3, -18},{   6, -19},{  13,   0},{ -19,  -9},
      { -25,  -1},{ -11,   1},{ -15,  -3},{  -7, -12},{   6, -14},{  10, -25},{  -1,  -7},{ -49,   3},
      {   6,  -3},{   2,  -8},{  -3,  -5},{   8, -14},{  23, -21},{  23, -14},{  21, -10},{  15, -18},
   },
   {
      { -28,  -9},{   0,  22},{  29,  18},{  11,  16},{  59,  26},{  44,  18},{  43,  10},{  45,  20},
      { -20,  -3},{ -42,  30},{ -30,  29},{  -1,  37},{ -20,  55},{  31,  15},{  20,  26},{  53,  -1},
      {   2, -16},{ -16,   7},{  -7,  16},{ -18,  45},{ -12,  40},{  43,  22},{  38,  16},{  31,  -7},
      {  -9,  20},{  -1,  24},{ -22,  26},{ -39,  39},{ -43,  50},{  -6,  28},{   3,  48},{  17,   6},
      {  13,  10},{ -19,  37},{  -9,  13},{ -13,  39},{   5,  12},{   0,  10},{   9,  29},{  28,  21},
      {   7, -13},{  19,  -5},{   7,  15},{  12, -10},{  17,  10},{  28,   3},{  43,   7},{  35,   3},
      {  -7, -13},{  20, -13},{  26,  -3},{  31,   3},{  38,   1},{  33,  -9},{  43, -32},{   2, -31},
      {  13, -31},{  18, -29},{  26, -10},{  34,  15},{  36,   5},{  12, -16},{ -31, -19},{ -50, -41},
   },
   {
      { -65, -73},{  23, -34},{  16, -17},{ -15,  -4},{ -56, -10},{ -34,  20},{   2,   4},{  13, -17},
      {  29, -16},{  -1,  20},{ -20,  16},{  -7,  33},{  -8,  23},{  -4,  44},{ -38,  23},{ -29,  11},
      {  -9,   6},{  24,  50},{   2,  43},{ -16,  41},{ -20,  48},{   6,  55},{  22,  48},{ -22,  10},
      { -17,  -5},{ -20,  23},{ -12,  70},{ -27,  61},{ -30,  55},{ -25,  43},{ -14,  51},{ -36,   6},
      { -49, -26},{  -1,  31},{ -27,  30},{ -39,  45},{ -47,  40},{ -43,  30},{ -34,  14},{ -51,   5},
      { -14, -33},{ -14,  15},{ -24,  16},{ -48,  31},{ -45,  34},{ -30,  12},{ -12,   3},{ -29, -25},
      {   1, -33},{   9,   9},{  -5,   5},{ -41,  18},{ -39,  23},{  -9,   4},{  36, -20},{  41, -64},
      { -15, -54},{  89, -86},{  50, -53},{ -64, -10},{  13, -57},{ -37, -20},{  64, -77},{  69,-125},
   }
};

///@todo make more things depend on rank/file ???

CONST_EVAL_TUNING EvalScore   pawnShieldBonus         = {  7, -4};
CONST_EVAL_TUNING EvalScore   pawnFawnMalusKS         = { 14, 68};
CONST_EVAL_TUNING EvalScore   pawnFawnMalusQS         = {-19, 43};
// this depends on rank
CONST_EVAL_TUNING EvalScore   passerBonus[8]          = { { 0, 0 }, {  1,-35}, { -4, -5}, { -3, 13}, {-22, 64}, {-15, 84}, { 19, 43}, {0, 0}};

CONST_EVAL_TUNING EvalScore   rookBehindPassed        = { -19, 54};

// distance to pawn / rank of pawn
CONST_EVAL_TUNING EvalScore   kingNearPassedPawnSupport[8][8] = { { {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0} },
                                                                  { {    0,    0}, {    0,   26}, {  -14,   54}, {    2,   50}, {   -9,   90}, {   -7,  109}, {   -8,   35}, {    0,    0} },
                                                                  { {    0,    0}, {   -7,   15}, {  -21,   39}, {  -44,   49}, {  -14,   62}, {   -9,   66}, {   -8,   17}, {    0,    0} },
                                                                  { {    0,    0}, {  -17,   22}, {  -32,   48}, {  -31,   23}, {  -19,   32}, {   -9,   22}, {   -8,   11}, {    0,    0} },
                                                                  { {    0,    0}, {  -28,   32}, {   -9,   28}, {  -15,    7}, {   -6,   -2}, {   -5,    7}, {   -8,   17}, {    0,    0} },
                                                                  { {    0,    0}, {   -4,   14}, {  -10,   20}, {    7,   -1}, {   14,  -16}, {    3,  -13}, {   -8,   17}, {    0,    0} },
                                                                  { {    0,    0}, {   19,    9}, {   17,    9}, {   21,    1}, {   -8,   29}, {   -8,   18}, {    4,   -7}, {    0,    0} },
                                                                  { {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0} } };

CONST_EVAL_TUNING EvalScore   kingNearPassedPawnDefend[8][8]  = { { {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0} },
                                                                  { {    0,    0}, {  -22, -133}, {  -17,  -93}, {  -11,  -69}, {    0,  -32}, {   -8,    3}, {   -8,   15}, {    0,    0} },
                                                                  { {    0,    0}, {   -9,   -6}, {   39,  -81}, {   20,  -64}, {   -4,  -26}, {   -9,  -15}, {   -8,   13}, {    0,    0} },
                                                                  { {    0,    0}, {   -7,   37}, {   -8,   17}, {   27,  -47}, {   24,  -33}, {   -4,  -16}, {   -9,   30}, {    0,    0} },
                                                                  { {    0,    0}, {   -7,   39}, {  -10,   65}, {    8,   14}, {   24,  -23}, {   -5,  -11}, {  -10,   16}, {    0,    0} },
                                                                  { {    0,    0}, {   -7,   23}, {    4,   44}, {   11,   35}, {    0,   20}, {    5,  -13}, {  -12,   15}, {    0,    0} },
                                                                  { {    0,    0}, {   -8,   18}, {   -4,   25}, {   -2,   27}, {    3,   25}, {    9,   -5}, {  -13,   22}, {    0,    0} },
                                                                  { {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0} } };

// this depends on rank (shall try file also ??)
CONST_EVAL_TUNING EvalScore   doublePawn[8][2]   = { {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}}, {{ 18, 25},{ -6, 53}}, {{ 18, 23},{ -2, 33}}, {{ 23, 15},{  8, 17}}, {{ -1,  4},{ 27, 29}}, {{ 23, 13},{ 50, 37}}, {{  0,  0},{  0,  0}} }; // close semiopenfile
CONST_EVAL_TUNING EvalScore   isolatedPawn[8][2] = { {{  0,  0},{  0,  0}}, {{  8,  3},{ 30,  2}}, {{  5,  0},{ 15,  3}}, {{ 17,  2},{  9,  8}}, {{ 14,-13},{-10, 19}}, {{ -2, 39},{-43,  4}}, {{  9,  7},{ 10,  0}}, {{  0,  0},{  0,  0}} }; // close semiopenfile
CONST_EVAL_TUNING EvalScore   backwardPawn[8][2] = { {{  0,  0},{  0,  0}}, {{  1,  2},{ 27, 14}}, {{ 11,  5},{ 41, 16}}, {{  8, -1},{ 35,  7}}, {{-13, 21},{ 43, 10}}, {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}} }; // close semiopenfile
CONST_EVAL_TUNING EvalScore   detachedPawn[8][2] = { {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}}, {{ 18, 15},{ 40, 15}}, {{ -2,  7},{ 27,  9}}, {{  5, 33},{  4, 28}}, {{-28, -2},{ -9, 46}}, {{  0,  0},{  0,  0}}, {{  0,  0},{  0,  0}} }; // close semiopenfile

CONST_EVAL_TUNING EvalScore   holesMalus              = { -2, 1};
CONST_EVAL_TUNING EvalScore   outpostN                = { 31,15};
CONST_EVAL_TUNING EvalScore   outpostB                = { 43, 0};
CONST_EVAL_TUNING EvalScore   pieceFrontPawn          = {-13, 4};
CONST_EVAL_TUNING EvalScore   pawnFrontMinor[8]       = { { 10, -1}, {  6, 10}, { 14,  6}, { 15, 12}, { 25, 15}, { 36, 25}, {  0,  0}, {  0,  0} };
CONST_EVAL_TUNING EvalScore   centerControl           = {  7, 2};
CONST_EVAL_TUNING EvalScore   knightTooFar[8]         = { {  0,  0}, { 26, 22}, { 21, 19}, { 15, 16}, { -4, 24}, {  1, 11}, {  2, -4}, {  0,  0} };

// this depends on rank
CONST_EVAL_TUNING EvalScore   candidate[8]            = { {0, 0}, {-18, -1}, {-19,  8}, { 12, 31}, { 54, 66}, { 34, 64}, { 34, 64}, { 0, 0} };
CONST_EVAL_TUNING EvalScore   protectedPasserBonus[8] = { {0, 0}, { 24, -8}, { 14,-16}, {  6, -5}, { 58,-12}, {131, 21}, {  9, 45}, { 0, 0} };
CONST_EVAL_TUNING EvalScore   freePasserBonus[8]      = { {0, 0}, { 11, 12}, {  0,  8}, { -8, 41}, { -4, 51}, { -2,150}, { 64,150}, { 0, 0} };

CONST_EVAL_TUNING EvalScore   pawnMobility            = { -9, 11};
CONST_EVAL_TUNING EvalScore   pawnSafeAtt             = { 68, 31};
CONST_EVAL_TUNING EvalScore   pawnSafePushAtt         = { 29, 18};
CONST_EVAL_TUNING EvalScore   pawnlessFlank           = {-21,-31};
CONST_EVAL_TUNING EvalScore   pawnStormMalus          = { 20,-30};
CONST_EVAL_TUNING EvalScore   rookOnOpenFile          = { 48, 19};
CONST_EVAL_TUNING EvalScore   rookOnOpenSemiFileOur   = { 20, -3};
CONST_EVAL_TUNING EvalScore   rookOnOpenSemiFileOpp   = { 34,  2};

CONST_EVAL_TUNING EvalScore   rookQueenSameFile       = {  6, 23};
CONST_EVAL_TUNING EvalScore   rookFrontQueenMalus     = {-19,  9};
CONST_EVAL_TUNING EvalScore   rookFrontKingMalus      = {-24, 18};
CONST_EVAL_TUNING EvalScore   minorOnOpenFile         = {  9, -1};

CONST_EVAL_TUNING EvalScore   pinnedKing [5]          = { { -2, -5}, { 15, 58}, {  5, 48}, { 10, 52}, { -1, 40} };
CONST_EVAL_TUNING EvalScore   pinnedQueen[5]          = { {  4,-33}, { -9, 10}, {-14,  8}, { -5,  5}, { 35, 49} };

CONST_EVAL_TUNING EvalScore   hangingPieceMalus       = {-26, 2};

CONST_EVAL_TUNING EvalScore   threatByMinor[6]        = { { -8,-19}, {-32,-32}, {-34,-40}, {-59,-20}, {-34,-72}, {-57, -6} };
CONST_EVAL_TUNING EvalScore   threatByRook[6]         = { {-15,-19}, {-44,-15}, {-44,-23}, {  2, -9}, {-41,-96}, {-17, -1} };
CONST_EVAL_TUNING EvalScore   threatByQueen[6]        = { {  4,-14}, {  1,-28}, {  3,-41}, { 13,-19}, { 33, 27}, {-46,-65} };
CONST_EVAL_TUNING EvalScore   threatByKing[6]         = { {-63,-29}, {-12,-39}, {-29,-47}, { 34,-48}, { -1,  0}, {  0,  0} };

// this depends on number of pawn
CONST_EVAL_TUNING EvalScore   adjKnight[9]            = { {-26,-20}, {-10, -4}, { -1, -8}, {-16,  1}, { -9,  6}, {  3, 16}, { 12, 39}, { 22, 64}, { 33, 71} };
CONST_EVAL_TUNING EvalScore   adjRook[9]              = { { 24, -8}, { 15, 25}, {-35, 52}, {-51, 59}, {-63, 54}, {-66, 54}, {-67, 52}, {-64, 51}, {-50, 36} };
CONST_EVAL_TUNING EvalScore   bishopPairBonus[9]      = { { 29, 56}, { 28, 58}, { 31, 72}, { 29, 82}, { 47, 77}, { 28, 98}, { 29,103}, { 37,105}, { 25,118} };
CONST_EVAL_TUNING EvalScore   badBishop[9]            = { { 11,-33}, { 13,-26}, { 20,-21}, { 23, -8}, { 31, -5}, { 28, 11}, { 33, 25}, { 30, 54}, { 39, 65} };

CONST_EVAL_TUNING EvalScore   knightPairMalus         = { 4, 11};
CONST_EVAL_TUNING EvalScore   rookPairMalus           = {44,-18};

CONST_EVAL_TUNING EvalScore   queenNearKing           = { 7, 15};

// scaling (/128 inside eval)
CONST_EVAL_TUNING ScoreType   scalingFactorWin                 = 384;
CONST_EVAL_TUNING ScoreType   scalingFactorHardWin             = 64;
CONST_EVAL_TUNING ScoreType   scalingFactorLikelyDraw          = 39;
CONST_EVAL_TUNING ScoreType   scalingFactorOppBishopAlone      = 34;
CONST_EVAL_TUNING ScoreType   scalingFactorOppBishopAloneSlope = 20;
CONST_EVAL_TUNING ScoreType   scalingFactorOppBishop           = 80;
CONST_EVAL_TUNING ScoreType   scalingFactorOppBishopSlope      = 1;
CONST_EVAL_TUNING ScoreType   scalingFactorQueenNoQueen        = 79;
CONST_EVAL_TUNING ScoreType   scalingFactorQueenNoQueenSlope   = 0;
CONST_EVAL_TUNING ScoreType   scalingFactorPawns               = 75;
CONST_EVAL_TUNING ScoreType   scalingFactorPawnsSlope          = 25;
CONST_EVAL_TUNING ScoreType   scalingFactorPawnsOneSide        = -20; // a curious tuned value ...

//N B R QB QR K
CONST_EVAL_TUNING EvalScore MOB[6][15] = { { { -1,-15},{  7, 20},{ 12, 40},{ 19, 43},{ 26, 40},{ 30, 44},{ 40, 33},{ 35, 46},{ 29, 47},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0} },
                                           { {-28, -7},{-22, 30},{ -9, 27},{ -4, 37},{  0, 41},{ -2, 46},{  7, 41},{ 21, 31},{ 41, 26},{ 45, 36},{ 55, 37},{ 71, 65},{ 71, 56},{120, 95},{  0,  0} },
                                           { { 20, 19},{ 23, 44},{ 24, 55},{ 28, 54},{ 20, 68},{ 19, 72},{ 22, 69},{ 20, 76},{ 25, 77},{ 45, 70},{ 57, 69},{ 56, 73},{ 48, 76},{ 57, 69},{ 72, 56} },
                                           { {-20,-22},{-21, 16},{-12, 19},{ -6, 18},{ -6, 24},{ -2, 29},{  5, 22},{ 16, 41},{ 13, 13},{ 23, 47},{ 28, 50},{ 34, 24},{ 16, 32},{ 24, 87},{  0,  0} },
                                           { { -3,-56},{ -5,  1},{ -1, -6},{ -4, 13},{ -5, 17},{ -3, 17},{  2, 18},{  7, 15},{ 13, 15},{ 21, 26},{  8, 37},{ 21, 43},{ 25, 48},{ 19, 45},{ 24, 54} },
                                           { {  0, -3},{ -2, 24},{-11, 35},{-17, 36},{-24, 32},{-26, 23},{-29, 21},{-30, 20},{-40, -3},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0} }
 };

CONST_EVAL_TUNING EvalScore initiative[4] = { {  0,  5}, { 68, 42}, {115, 65}, { 61, 84} };

CONST_EVAL_TUNING ScoreType kingAttMax    = 487;
CONST_EVAL_TUNING ScoreType kingAttTrans  = 43;
CONST_EVAL_TUNING ScoreType kingAttScale  = 12;
CONST_EVAL_TUNING ScoreType kingAttOffset =  5;
CONST_EVAL_TUNING ScoreType kingAttWeight[2][6]    = { { 129, 178, 272, 181, 189, -39, },{  38,  94,  88,  39,   5,  0 } };
CONST_EVAL_TUNING ScoreType kingAttSafeCheck[6]    = { 122, 785, 569, 934, 776, 0 };
CONST_EVAL_TUNING ScoreType kingAttOpenfile        = 299;
CONST_EVAL_TUNING ScoreType kingAttSemiOpenfileOur = 182;
CONST_EVAL_TUNING ScoreType kingAttSemiOpenfileOpp = 151;
CONST_EVAL_TUNING ScoreType kingAttNoQueen = 503;
ScoreType kingAttTable[64] = {0};

CONST_EVAL_TUNING ScoreType tempo = {15};

void initEval() {
   for (Square i = 0; i < NbSquare; i++) {
      EvalConfig::kingAttTable[i] =
          static_cast<ScoreType>(sigmoid(i, EvalConfig::kingAttMax, EvalConfig::kingAttTrans, EvalConfig::kingAttScale, EvalConfig::kingAttOffset));
   }
} // idea taken from Topple

} // EvalConfig
