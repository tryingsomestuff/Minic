#include "evalConfig.hpp"

#include "bitboard.hpp"
#include "position.hpp"

namespace EvalConfig {

CONST_EVAL_TUNING EvalScore imbalance_mines[5][5] = {
   // pawn knight bishop rook queen
   { {   11,  138} },
   { {   -3,  342}, { -114, -261} },
   { {  273,  239}, { -183, -240}, { -210, -284} },
   { {  158,  629}, { -184, -131}, { -193, -338}, { -162, -383} },
   { {  467,  629}, { -528, -360}, { -793, -538}, {-1271,-1127}, { -444, -416} }
};

CONST_EVAL_TUNING EvalScore imbalance_theirs[5][5] = {
   // pawn knight bishop rook queen
   { { -184, -330} },
   { {  190,  430}, {    6,  -54} },
   { {  135,  523}, {   21,  -83}, {   33,  -44} },
   { {  107,  717}, {   45,  -86}, {  -23, -229}, {    6, -204} },
   { {  712,  975}, {  442,  338}, {  483,  303}, {  306,  339}, {   15,   11} }
};

CONST_EVAL_TUNING EvalScore PST[PieceShift][NbSquare] = {
   {
      {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, 
      {  98,  88}, { 133,  73}, {  60,  78}, {  95,  53}, {  68,  91}, { 126,  65}, {  34, 131}, { -11, 137}, 
      {  -4,  71}, {   7,  46}, {  23,  21}, {  28, -11}, {  65, -21}, {  56,  22}, {  25,  41}, { -20,  50}, 
      {   4,  62}, {   0,  43}, {   9,  23}, {  -2,  13}, {  10,  14}, {  19,  23}, {  15,  25}, {   0,  30}, 
      {  -7,  26}, { -25,  26}, {  -1,  -1}, {   8,   4}, {  18,  -6}, {  15,   3}, {  -6,  20}, {   7,   7}, 
      {  -9,  24}, { -25,  24}, {   1,   2}, {   1,   6}, {  10,  10}, {   7,   6}, {  22,   6}, {  14,   5}, 
      { -24,  25}, { -28,  17}, {  -6,   3}, { -13,   5}, {   3,  17}, {  11,  16}, {  24,   3}, { -17,  10}, 
      {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0}, {   0,   0} 
   },
   {
      {-167, -56}, { -89, -41}, { -34, -10}, { -49, -30}, {  61, -31}, { -97, -30}, { -15, -63}, {-107, -98}, 
      { -73, -71}, { -42, -28}, {  71, -74}, {  36, -35}, {  23, -53}, {  62, -74}, {   6, -58}, { -17, -58}, 
      { -47, -35}, {  60, -45}, {  27,  -9}, {  48,  -8}, {  81, -50}, { 128, -46}, {  71, -52}, {  44, -51}, 
      {  -9,  -3}, {  35,  -5}, {  58, -18}, {  60,  -7}, {  45,  -2}, {  73, -13}, {  36, -16}, {  32, -18}, 
      {  -1,  -7}, {   4,  -9}, {  40, -15}, {  29,  16}, {  29,  15}, {  32,  -6}, {  31,  -2}, {   5, -20}, 
      { -16, -32}, {  12, -11}, {  24, -32}, {  15,   5}, {  19,  -9}, {  21, -37}, {  30, -26}, { -10, -22}, 
      { -29, -39}, { -52, -20}, { -14, -19}, {   9, -17}, {  11, -32}, {  -3, -32}, { -14, -19}, { -21, -22}, 
      {-105, -24}, { -35, -39}, { -58, -22}, { -36,  -8}, { -35, -32}, { -21, -24}, { -30, -54}, { -23, -61} 
   },
   {
      { -29, -14}, {   4, -16}, { -82, -10}, { -37,  -8}, { -25,  -4}, { -42,  -9}, {   7, -17}, {  -8, -26}, 
      { -26,  -5}, {  14,  -6}, { -18,   8}, { -13, -15}, {  30,  -8}, {  56, -14}, {  17, -12}, { -48, -16}, 
      {  -7,  11}, {  37,  -5}, {  37,  -3}, {  39, -17}, {  34, -14}, {  46,  -6}, {  36,   6}, {  38,  -5}, 
      {  -4,  10}, {  -2,  28}, {  17,  11}, {  19,  15}, {  30,   5}, {  33,  -3}, {  34, -16}, {  13,   6}, 
      {  -3,  -2}, {   3,   6}, {  32,  12}, {  32,  10}, {  32, -10}, {  32,   4}, {  19,   7}, {  23, -10}, 
      {  18, -18}, {  44,  15}, {  43,  11}, {  33,   2}, {  47,   9}, {  50,  -2}, {  52, -12}, {  31,  -9}, 
      {  35, -27}, {  29, -26}, {  46, -35}, {  32,   2}, {  39,   4}, {  41,  -7}, {  49, -22}, {  36, -30}, 
      { -30, -16}, {  24, -22}, {  21, -25}, {  -1,   6}, {  -3,  25}, {  -3,   2}, { -39,  -1}, { -21, -24} 
   },
   {
      {  32,  14}, {  42,  11}, {  32,  21}, {  51,   6}, {  63,  11}, {   9,  15}, {  31,  16}, {  43,   5}, 
      {  27,  10}, {  24,  13}, {  54,   6}, {  61,   2}, {  79, -15}, {  67,  -1}, {  26,  16}, {  44,   8}, 
      {  -6,  12}, {  19,   9}, {  25,  -3}, {  35, -23}, {  16, -10}, {  45, -12}, {  61,  -5}, {  16,  -6}, 
      { -25,  25}, { -12,  15}, {   5,  10}, {  25, -11}, {  25, -17}, {  32,  -6}, {  -8,   8}, { -19,  11}, 
      { -34,  18}, { -25,  19}, { -14,   9}, {  -3,   4}, {   5,  -7}, {  -6,  -2}, {   6,   2}, { -24,  -4}, 
      { -31,   3}, { -19,   9}, { -17,  -8}, { -16,   0}, {   3, -17}, {   6, -25}, {  13,  -4}, { -19, -11}, 
      { -25,  -1}, { -12,  -7}, { -13, -11}, {  -8,  -8}, {   9, -19}, {  11, -13}, {  -1,  -6}, { -49,   8}, 
      {   1,   2}, {   4,  -6}, {  -6,  14}, {   5,  -9}, {  19, -13}, {  16, -21}, {  17, -11}, {  11, -27} 
   },
   {
      { -28,  -9}, {   0,  22}, {  29,  14}, {  11,  13}, {  59,  25}, {  44,  18}, {  43,   9}, {  45,  19}, 
      { -15,  -6}, { -40,  29}, { -33,  32}, {  -1,  36}, { -21,  60}, {  29,  11}, {  20,  26}, {  53,   0}, 
      {   3, -12}, { -15,   9}, {  -5,  16}, { -16,  44}, { -14,  39}, {  40,  17}, {  39,  16}, {  30, -13}, 
      {  -8,  19}, {  -1,  29}, { -19,  27}, { -31,  42}, { -41,  48}, { -11,  26}, {   3,  38}, {  20,   2}, 
      {  13,  18}, { -20,  35}, { -11,  20}, { -16,  32}, {   3,  15}, {   0,   4}, {   9,  34}, {  27,  23}, 
      {   7, -13}, {   7,   7}, {  11,  19}, {  11, -15}, {  15,  16}, {  21,   8}, {  33,   7}, {  35,   2}, 
      {  -7, -10}, {  17, -11}, {  16,  -9}, {  22,  12}, {  36, -17}, {  27, -13}, {  42, -35}, {   3, -29}, 
      {  13, -29}, {  20, -27}, {  20, -11}, {  29, -28}, {  26,  11}, {  15, -16}, { -31, -21}, { -50, -42} 
   },
   {
      { -65, -73}, {  23, -34}, {  16, -16}, { -15,  -4}, { -56, -10}, { -34,  19}, {   2,   3}, {  13, -17}, 
      {  29, -17}, {  -1,  21}, { -20,  19}, {  -7,  36}, {  -8,  22}, {  -4,  45}, { -38,  24}, { -29,  12}, 
      {  -9,   4}, {  24,  51}, {   2,  51}, { -16,  44}, { -20,  52}, {   6,  59}, {  22,  51}, { -22,   7}, 
      { -17,  -7}, { -21,  29}, { -12,  76}, { -26,  60}, { -30,  51}, { -25,  48}, { -14,  48}, { -36,   9}, 
      { -49, -29}, {  -1,  33}, { -28,  39}, { -40,  46}, { -44,  49}, { -45,  29}, { -36,  18}, { -51,   0}, 
      { -14, -33}, { -14,  18}, { -23,  19}, { -48,  17}, { -46,  36}, { -31,  14}, { -13,   4}, { -29, -19}, 
      {   1, -27}, {  10,   8}, {  -1,  -3}, { -41,  23}, { -42,  17}, {  -5,  -2}, {  33, -12}, {  45, -58}, 
      { -14, -55}, { 100, -93}, {  41, -52}, { -61, -11}, {   6, -65}, { -32, -26}, {  61, -79}, {  61,-131} 
   }
};

///@todo make more things depend on rank/file ???

CONST_EVAL_TUNING EvalScore   pawnShieldBonus         = {  6, -8};
CONST_EVAL_TUNING EvalScore   pawnFawnMalusKS         = { 21, 50};
CONST_EVAL_TUNING EvalScore   pawnFawnMalusQS         = {-24, 17};
// this depends on rank
CONST_EVAL_TUNING EvalScore   passerBonus[8]          = { {-21,-34}, {-10,-17}, {-27, 28}, {-27,109}, { -2,157}, { 23, 77} };

CONST_EVAL_TUNING EvalScore   rookBehindPassed        = { -19, 38};

// distance to pawn / rank of pawn
CONST_EVAL_TUNING EvalScore   kingNearPassedPawnSupport[8][8] = { { {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0} },
                                                                  { {    0,    0}, {   22,   19}, {  -16,   57}, {    7,   63}, {  -13,   86}, {   -3,  139}, {   -9,   37}, {    0,    0} },
                                                                  { {    0,    0}, {    0,   16}, {  -26,   45}, {  -44,   60}, {  -12,   53}, {   -9,   69}, {   -8,   22}, {    0,    0} },
                                                                  { {    0,    0}, {   -8,   27}, {  -41,   52}, {  -35,   41}, {  -20,   29}, {  -14,   35}, {   -8,   15}, {    0,    0} },
                                                                  { {    0,    0}, {  -18,   40}, {   -8,   41}, {  -20,   26}, {  -18,   -1}, {   -6,   -3}, {   -8,   18}, {    0,    0} },
                                                                  { {    0,    0}, {    2,   26}, {  -12,   30}, {   13,   12}, {   19,   -1}, {   -6,  -14}, {   -8,    8}, {    0,    0} },
                                                                  { {    0,    0}, {   35,    8}, {   30,    9}, {   21,   18}, {   -3,   31}, {   -7,   20}, {    6,  -11}, {    0,    0} },
                                                                  { {    0,    0}, {    1,    6}, {    0,    3}, {    0,    7}, {    0,   -2}, {    0,    1}, {    0,   -1}, {    0,    0} } };

CONST_EVAL_TUNING EvalScore   kingNearPassedPawnDefend[8][8]  = { { {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0}, {    0,    0} },
                                                                  { {    0,    0}, {  -28, -150}, {  -19, -150}, {   -8,  -94}, {    3,  -40}, {  -10,    4}, {   -8,   16}, {    0,    0} },
                                                                  { {    0,    0}, {   -9,   -6}, {   31, -106}, {   26,  -86}, {   -4,  -24}, {   -9,   -7}, {   -9,   15}, {    0,    0} },
                                                                  { {    0,    0}, {   -7,   47}, {   -5,   12}, {   30,  -59}, {   33,  -25}, {   -7,   -6}, {   -9,   29}, {    0,    0} },
                                                                  { {    0,    0}, {   -8,   51}, {   -4,   72}, {   15,   13}, {   29,  -22}, {   14,  -11}, {   -3,   12}, {    0,    0} },
                                                                  { {    0,    0}, {   -7,   28}, {    3,   48}, {   10,   38}, {    5,   34}, {   13,   -8}, {    4,   11}, {    0,    0} },
                                                                  { {    0,    0}, {   -8,   24}, {   -4,   29}, {    0,   34}, {    5,   43}, {   11,   10}, {    2,   20}, {    0,    0} },
                                                                  { {    0,    0}, {    0,    0}, {    0,    0}, {    0,   -2}, {    1,   -1}, {    0,    3}, {    1,    0}, {    0,    0} } };

// this depends on rank (shall try file also ??)
CONST_EVAL_TUNING EvalScore   doublePawn[8][2]   = { {{  0,  0}, {  0,  0}}, {{  0,  0}, {  0,  0}}, {{ 17, 26}, { -7, 54}}, {{ 10, 37}, {  2, 34}}, {{ 23, 14}, { 15, 16}}, {{ -3, -2}, { 20, 37}}, {{ 23, 13}, { 51, 33}}, {{  0,  0}, {  0,  0}}  }; // close semiopenfile
CONST_EVAL_TUNING EvalScore   isolatedPawn[8][2] = { {{  0,  0}, {  0,  0}}, {{  2,  2}, { 22, 10}}, {{ -7,  9}, {  4,  5}}, {{ 12, -3}, {  0, 29}}, {{ 18,-16}, { -2, 18}}, {{ -2, 28}, {-44,  3}}, {{  9,  7}, { 10, -3}}, {{  0,  0}, {  0,  0}}  }; // close semiopenfile
CONST_EVAL_TUNING EvalScore   backwardPawn[8][2] = { {{  0,  0}, {  0,  0}}, {{  5, -5}, { 30, 16}}, {{  8,  8}, { 41, 12}}, {{  9, -2}, { 36, -6}}, {{-14, 16}, { 28,  8}}, {{  0,  0}, {  0,  0}}, {{  0,  0}, {  0,  0}}, {{  0,  0}, {  0,  0}}  }; // close semiopenfile
CONST_EVAL_TUNING EvalScore   detachedPawn[8][2] = { {{  0,  0}, {  0,  0}}, {{  0,  0}, {  0,  0}}, {{ 37,-11}, { 42, 20}}, {{ -2,  5}, { 27, -2}}, {{  1, 27}, { -4, 32}}, {{-25,  0}, { -6, 29}}, {{  0,  0}, {  0,  0}}, {{  0,  0}, {  0,  0}}  }; // close semiopenfile

CONST_EVAL_TUNING EvalScore   holesMalus              = {-23, -1};
CONST_EVAL_TUNING EvalScore   outpostN                = { 33, 14};
CONST_EVAL_TUNING EvalScore   outpostB                = { 48, -2};
CONST_EVAL_TUNING EvalScore   pieceFrontPawn          = {-17, 10};
CONST_EVAL_TUNING EvalScore   pawnFrontMinor[8]       = { {  3,  7}, { 17,  6}, { 19, -1}, { 11, 13}, { 25, 15}, { 36, 25}, {  0,  0}, {  0,  0} };
CONST_EVAL_TUNING EvalScore   centerControl           = {  8, -1};
CONST_EVAL_TUNING EvalScore   knightTooFar[8]         = { {  0,  0}, { 22, 27}, { 19, 14}, { 14, 13}, { -3, 21}, {  1, 10}, { -4,  0}, {  0,  0} };

// this depends on rank
CONST_EVAL_TUNING EvalScore   candidate[8]            = { {0, 0}, {-24, 17}, {-17,  9}, { 15, 33}, { 62, 64}, { 34, 64}, { 34, 64}, { 0, 0} };
CONST_EVAL_TUNING EvalScore   protectedPasserBonus[8] = { {0, 0}, { 24, -8}, {  5,-13}, { 19,-25}, { 56,  2}, {129, 26}, { 11, 48}, { 0, 0} };
CONST_EVAL_TUNING EvalScore   freePasserBonus[8]      = { {0, 0}, {  1, -3}, {  0,-11}, {  4, -3}, {  1, -3}, {-31, 22}, { 48, 93}, { 0, 0} };

CONST_EVAL_TUNING EvalScore   pawnMobility            = {-12, 13};
CONST_EVAL_TUNING EvalScore   pawnSafeAtt             = {103, 36};
CONST_EVAL_TUNING EvalScore   pawnSafePushAtt         = { 26, 20};
CONST_EVAL_TUNING EvalScore   pawnlessFlank           = { 24,-39};
CONST_EVAL_TUNING EvalScore   pawnStormMalus          = { 24,-27};
CONST_EVAL_TUNING EvalScore   rookOnOpenFile          = { 41, 24};
CONST_EVAL_TUNING EvalScore   rookOnOpenSemiFileOur   = { 24, -2};
CONST_EVAL_TUNING EvalScore   rookOnOpenSemiFileOpp   = { 28, 17};

CONST_EVAL_TUNING EvalScore   rookQueenSameFile       = { -1, 35};
CONST_EVAL_TUNING EvalScore   rookFrontQueenMalus     = {-19, -1};
CONST_EVAL_TUNING EvalScore   rookFrontKingMalus      = {-23, 22};
CONST_EVAL_TUNING EvalScore   minorOnOpenFile         = {  4,  0};

CONST_EVAL_TUNING EvalScore   pinnedKing [5]          = { {  1,-10}, { 31, 55}, {  2, 53}, { 14, 53}, { -3, 41} };
CONST_EVAL_TUNING EvalScore   pinnedQueen[5]          = { {  7,-29}, {-10, 10}, { -4,  9}, { -5,  6}, { 29, 51} };

CONST_EVAL_TUNING EvalScore   hangingPieceMalus       = {-29, -18};

CONST_EVAL_TUNING EvalScore   threatByMinor[6]        = { { -6,-28}, {-33,-36}, {-39,-47}, {-79,-47}, {-71,-79}, {-57, -5} };
CONST_EVAL_TUNING EvalScore   threatByRook[6]         = { { -7,-29}, {-48,-23}, {-35,-21}, {-13,-10}, {-76,-96}, {-18, -7} };
CONST_EVAL_TUNING EvalScore   threatByQueen[6]        = { { -1,-16}, { -1,-18}, { 11,-37}, { 17,-13}, { 27, 31}, {-46,-63} };
CONST_EVAL_TUNING EvalScore   threatByKing[6]         = { {-34,-43}, {-17,-47}, {-39,-52}, {  9,-55}, { -1,  0}, {  0,  0} };

// this depends on number of pawn
CONST_EVAL_TUNING EvalScore   adjKnight[9]            = { {-26,-20}, {-10, -4}, { -1, -8}, {-16,  8}, { -2, 14}, { 10, 26}, { 11, 43}, { 13, 67}, { 41, 72} };
CONST_EVAL_TUNING EvalScore   adjRook[9]              = { { 24, -8}, { 15, 26}, {-34, 52}, {-49, 65}, {-65, 67}, {-66, 60}, {-65, 57}, {-47, 46}, {-46, 36} };
CONST_EVAL_TUNING EvalScore   bishopPairBonus[9]      = { { 29, 56}, { 28, 58}, { 31, 71}, { 31, 82}, { 48, 78}, { 31,102}, { 41,101}, { 31, 95}, { 40,119} };
CONST_EVAL_TUNING EvalScore   badBishop[9]            = { { 13,-40}, { 23,-34}, { 23,-17}, { 35,-16}, { 40, -7}, { 42, -4}, { 37, 14}, { 32, 53}, { 39, 66} };

CONST_EVAL_TUNING EvalScore   knightPairMalus         = { 14, -7};
CONST_EVAL_TUNING EvalScore   rookPairMalus           = { 58,-30};

CONST_EVAL_TUNING EvalScore   queenNearKing           = { 10,  6};

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
CONST_EVAL_TUNING EvalScore MOB[6][15] = { { {  0,-15}, {  9, 23}, { 15, 34}, { 19, 37}, { 29, 38}, { 32, 49}, { 40, 35}, { 35, 48}, { 29, 47}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0} },
                                           { {-33, -2}, {-26, 31}, {-16, 30}, {-12, 49}, { -5, 41}, { -8, 48}, {  3, 45}, { 21, 32}, { 41, 29}, { 45, 36}, { 55, 37}, { 71, 65}, { 71, 56}, {120, 95}, {  0,  0} },
                                           { { 30, 21}, { 29, 57}, { 32, 57}, { 30, 59}, { 27, 69}, { 33, 69}, { 30, 73}, { 26, 80}, { 19, 92}, { 45, 74}, { 57, 71}, { 56, 73}, { 48, 76}, { 57, 69}, { 72, 56} },
                                           { {-22,-19}, {-19, 12}, {-10, 12}, {-12, 21}, { -5, 18}, { -1, 27}, {  5, 20}, { 16, 40}, { 13, 14}, { 23, 47}, { 28, 50}, { 34, 24}, { 16, 32}, { 24, 87}, {  0,  0} },
                                           { { -2,-54}, { -5, -7}, {  0, -9}, { -5,  9}, { -8, 16}, { -7, 17}, { -3, 18}, {  7, 14}, { 13, 14}, { 21, 26}, {  8, 37}, { 21, 43}, { 25, 48}, { 19, 45}, { 24, 54} },
                                           { {  2,  0}, {  1, 36}, {-10, 42}, {-24, 46}, {-30, 47}, {-26, 28}, {-29, 31}, {-31, 22}, {-40,  9}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0} } };

CONST_EVAL_TUNING EvalScore initiative[4] = { { -1,  6}, { 70, 43}, {115, 65}, { 59, 83}  };

CONST_EVAL_TUNING ScoreType kingAttMax    = 492;
CONST_EVAL_TUNING ScoreType kingAttTrans  = 35;
CONST_EVAL_TUNING ScoreType kingAttScale  = 11;
CONST_EVAL_TUNING ScoreType kingAttOffset =  5;
CONST_EVAL_TUNING ScoreType kingAttWeight[2][6]    = { { 125, 189, 251, 153, 153, -39 }, {  41, 100,  96,  48,  26,  0 } };
CONST_EVAL_TUNING ScoreType kingAttSafeCheck[6]    = { 122, 785, 569, 934, 776, 0 };
CONST_EVAL_TUNING ScoreType kingAttOpenfile        = 283;
CONST_EVAL_TUNING ScoreType kingAttSemiOpenfileOur = 184;
CONST_EVAL_TUNING ScoreType kingAttSemiOpenfileOpp = 152;
CONST_EVAL_TUNING ScoreType kingAttNoQueen = 585;
ScoreType kingAttTable[64] = {0};

CONST_EVAL_TUNING ScoreType tempo = {15};

void initEval() {
   for (Square i = 0; i < NbSquare; i++) {
      EvalConfig::kingAttTable[i] =
          static_cast<ScoreType>(sigmoid(i, EvalConfig::kingAttMax, EvalConfig::kingAttTrans, EvalConfig::kingAttScale, EvalConfig::kingAttOffset));
   }
} // idea taken from Topple

} // EvalConfig
