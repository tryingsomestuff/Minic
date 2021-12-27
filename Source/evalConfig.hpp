#pragma once

#include "definition.hpp"
#include "score.hpp"

/*!
 * All parameters for evaluation are defined here
 * There are const when no esvaluation tuning is use and of course need to be mutable when begin tuned
 */
namespace EvalConfig {

// This idea was taken from the second-degree polynomial material imbalance by Tord Romstad
extern CONST_EVAL_TUNING EvalScore imbalance_mines[5][5];
extern CONST_EVAL_TUNING EvalScore imbalance_theirs[5][5];

extern CONST_EVAL_TUNING EvalScore PST[PieceShift][NbSquare];

extern CONST_EVAL_TUNING EvalScore pawnShieldBonus;
extern CONST_EVAL_TUNING EvalScore pawnFawnMalusKS;
extern CONST_EVAL_TUNING EvalScore pawnFawnMalusQS;
extern CONST_EVAL_TUNING EvalScore passerBonus[8];
extern CONST_EVAL_TUNING EvalScore rookBehindPassed;
extern CONST_EVAL_TUNING EvalScore kingNearPassedPawnSupport[8][8];
extern CONST_EVAL_TUNING EvalScore kingNearPassedPawnDefend[8][8];
enum PawnEvalSemiOpen { Close = 0, SemiOpen = 1 };

extern CONST_EVAL_TUNING EvalScore doublePawn[8][2];
extern CONST_EVAL_TUNING EvalScore isolatedPawn[8][2];
extern CONST_EVAL_TUNING EvalScore backwardPawn[8][2];
extern CONST_EVAL_TUNING EvalScore detachedPawn[8][2];

extern CONST_EVAL_TUNING EvalScore holesMalus;
extern CONST_EVAL_TUNING EvalScore pieceFrontPawn;
extern CONST_EVAL_TUNING EvalScore pawnFrontMinor[8];
extern CONST_EVAL_TUNING EvalScore outpostN;
extern CONST_EVAL_TUNING EvalScore outpostB;
extern CONST_EVAL_TUNING EvalScore centerControl;
extern CONST_EVAL_TUNING EvalScore knightTooFar[8];
extern CONST_EVAL_TUNING EvalScore candidate[8];
extern CONST_EVAL_TUNING EvalScore protectedPasserBonus[8];
extern CONST_EVAL_TUNING EvalScore freePasserBonus[8];
extern CONST_EVAL_TUNING EvalScore pawnMobility;
extern CONST_EVAL_TUNING EvalScore pawnSafeAtt;
extern CONST_EVAL_TUNING EvalScore pawnSafePushAtt;
extern CONST_EVAL_TUNING EvalScore pawnlessFlank;
extern CONST_EVAL_TUNING EvalScore pawnStormMalus;
extern CONST_EVAL_TUNING EvalScore rookOnOpenFile;
extern CONST_EVAL_TUNING EvalScore rookOnOpenSemiFileOur;
extern CONST_EVAL_TUNING EvalScore rookOnOpenSemiFileOpp;

extern CONST_EVAL_TUNING EvalScore rookQueenSameFile;
extern CONST_EVAL_TUNING EvalScore rookFrontQueenMalus;
extern CONST_EVAL_TUNING EvalScore rookFrontKingMalus;
extern CONST_EVAL_TUNING EvalScore minorOnOpenFile;

extern CONST_EVAL_TUNING EvalScore pinnedKing[5];
extern CONST_EVAL_TUNING EvalScore pinnedQueen[5];

extern CONST_EVAL_TUNING EvalScore hangingPieceMalus;

extern CONST_EVAL_TUNING EvalScore threatByMinor[6];
extern CONST_EVAL_TUNING EvalScore threatByRook[6];
extern CONST_EVAL_TUNING EvalScore threatByQueen[6];
extern CONST_EVAL_TUNING EvalScore threatByKing[6];

extern CONST_EVAL_TUNING EvalScore adjKnight[9];
extern CONST_EVAL_TUNING EvalScore adjRook[9];
extern CONST_EVAL_TUNING EvalScore badBishop[9];
extern CONST_EVAL_TUNING EvalScore bishopPairBonus[9];
extern CONST_EVAL_TUNING EvalScore knightPairMalus;
extern CONST_EVAL_TUNING EvalScore rookPairMalus;
extern CONST_EVAL_TUNING EvalScore queenNearKing;

//N B R QB QR K
extern CONST_EVAL_TUNING EvalScore MOB[6][15];

// This is of course Stockfish inspired
extern CONST_EVAL_TUNING EvalScore initiative[4];

// scaling (/128.f inside eval)
extern CONST_EVAL_TUNING ScoreType scalingFactorWin;
extern CONST_EVAL_TUNING ScoreType scalingFactorHardWin;
extern CONST_EVAL_TUNING ScoreType scalingFactorLikelyDraw;

extern CONST_EVAL_TUNING ScoreType scalingFactorOppBishopAlone;
extern CONST_EVAL_TUNING ScoreType scalingFactorOppBishopAloneSlope;
extern CONST_EVAL_TUNING ScoreType scalingFactorOppBishop;
extern CONST_EVAL_TUNING ScoreType scalingFactorOppBishopSlope;
extern CONST_EVAL_TUNING ScoreType scalingFactorQueenNoQueen;
extern CONST_EVAL_TUNING ScoreType scalingFactorQueenNoQueenSlope;
extern CONST_EVAL_TUNING ScoreType scalingFactorPawns;
extern CONST_EVAL_TUNING ScoreType scalingFactorPawnsSlope;
extern CONST_EVAL_TUNING ScoreType scalingFactorPawnsOneSide;

// This idea was taken from Topple by Vincent Tang a.k.a Konsolas
enum katt_att_def : uint8_t { katt_attack = 0, katt_defence = 1 };
extern CONST_EVAL_TUNING ScoreType kingAttMax;
extern CONST_EVAL_TUNING ScoreType kingAttTrans;
extern CONST_EVAL_TUNING ScoreType kingAttScale;
extern CONST_EVAL_TUNING ScoreType kingAttOffset;
extern CONST_EVAL_TUNING ScoreType kingAttWeight[2][6];
extern CONST_EVAL_TUNING ScoreType kingAttSafeCheck[6];
extern CONST_EVAL_TUNING ScoreType kingAttOpenfile;
extern CONST_EVAL_TUNING ScoreType kingAttSemiOpenfileOpp;
extern CONST_EVAL_TUNING ScoreType kingAttSemiOpenfileOur;
extern CONST_EVAL_TUNING ScoreType kingAttNoQueen;
extern ScoreType                    kingAttTable[64];

extern CONST_EVAL_TUNING ScoreType tempo;

// from 0 to m with offset, translation and scale
// exp is not (yet?) constexpr
[[nodiscard]] inline /*constexpr*/ double sigmoid(double x, double m = 1.f, double trans = 0.f, double scale = 1.f, double offset = 0.f) {
   return m / (1 + exp((trans - x) / scale)) - offset;
}

inline void initEval() {
   for (Square i = 0; i < NbSquare; i++) {
      EvalConfig::kingAttTable[i] =
          (int)sigmoid(i, EvalConfig::kingAttMax, EvalConfig::kingAttTrans, EvalConfig::kingAttScale, EvalConfig::kingAttOffset);
   }
} // idea taken from Topple

} // namespace EvalConfig
