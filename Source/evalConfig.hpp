#pragma once

#include "definition.hpp"
#include "score.hpp"

/* All parameters for evaluation are defined here
 * There are const when no Texel tuning is use and of course need to be mutable when begin tuned
 */

namespace EvalConfig {

// This idea was taken from the second-degree polynomial material imbalance by Tord Romstad
extern CONST_TEXEL_TUNING EvalScore imbalance_mines[5][5];
extern CONST_TEXEL_TUNING EvalScore imbalance_theirs[5][5];

extern CONST_TEXEL_TUNING EvalScore PST[6][64];

extern CONST_TEXEL_TUNING EvalScore   pawnShieldBonus        ;
extern CONST_TEXEL_TUNING EvalScore   pawnFawnMalusKS        ;
extern CONST_TEXEL_TUNING EvalScore   pawnFawnMalusQS        ;
extern CONST_TEXEL_TUNING EvalScore   passerBonus[8]         ;
extern CONST_TEXEL_TUNING EvalScore   rookBehindPassed       ;
extern CONST_TEXEL_TUNING EvalScore   kingNearPassedPawn     ;
enum PawnEvalSemiOpen{ Close=0, SemiOpen=1};
///@todo make this depends on file! (/rank?)
extern CONST_TEXEL_TUNING EvalScore   doublePawnMalus[8][2]  ;
extern CONST_TEXEL_TUNING EvalScore   isolatedPawnMalus[8][2];
extern CONST_TEXEL_TUNING EvalScore   backwardPawnMalus[8][2];
extern CONST_TEXEL_TUNING EvalScore   detachedPawnMalus[2]   ;
extern CONST_TEXEL_TUNING EvalScore   holesMalus             ;
extern CONST_TEXEL_TUNING EvalScore   pieceFrontPawn         ;
extern CONST_TEXEL_TUNING EvalScore   outpost                ;
extern CONST_TEXEL_TUNING EvalScore   centerControl          ;
extern CONST_TEXEL_TUNING EvalScore   knightTooFar[8]        ;
extern CONST_TEXEL_TUNING EvalScore   candidate[8]           ;
extern CONST_TEXEL_TUNING EvalScore   protectedPasserBonus[8];
extern CONST_TEXEL_TUNING EvalScore   freePasserBonus[8]     ;
extern CONST_TEXEL_TUNING EvalScore   pawnMobility           ;
extern CONST_TEXEL_TUNING EvalScore   pawnSafeAtt            ;
extern CONST_TEXEL_TUNING EvalScore   pawnSafePushAtt        ;
extern CONST_TEXEL_TUNING EvalScore   pawnlessFlank          ;
extern CONST_TEXEL_TUNING EvalScore   pawnStormMalus         ;
extern CONST_TEXEL_TUNING EvalScore   rookOnOpenFile         ;
extern CONST_TEXEL_TUNING EvalScore   rookOnOpenSemiFileOur  ;
extern CONST_TEXEL_TUNING EvalScore   rookOnOpenSemiFileOpp  ;

extern CONST_TEXEL_TUNING EvalScore   rookQueenSameFile      ;
extern CONST_TEXEL_TUNING EvalScore   rookFrontQueenMalus    ;
extern CONST_TEXEL_TUNING EvalScore   rookFrontKingMalus     ;
extern CONST_TEXEL_TUNING EvalScore   minorOnOpenFile        ;

extern CONST_TEXEL_TUNING EvalScore   pinnedKing [5]         ;
extern CONST_TEXEL_TUNING EvalScore   pinnedQueen[5]         ;

extern CONST_TEXEL_TUNING EvalScore   hangingPieceMalus      ;

extern CONST_TEXEL_TUNING EvalScore   threatByMinor[6]       ;
extern CONST_TEXEL_TUNING EvalScore   threatByRook[6]        ;
extern CONST_TEXEL_TUNING EvalScore   threatByQueen[6]       ;
extern CONST_TEXEL_TUNING EvalScore   threatByKing[6]        ;

extern CONST_TEXEL_TUNING EvalScore   adjKnight[9]           ;
extern CONST_TEXEL_TUNING EvalScore   adjRook[9]             ;
extern CONST_TEXEL_TUNING EvalScore   badBishop[9]           ;
extern CONST_TEXEL_TUNING EvalScore   bishopPairBonus[9]     ;
extern CONST_TEXEL_TUNING EvalScore   knightPairMalus        ;
extern CONST_TEXEL_TUNING EvalScore   rookPairMalus          ;
extern CONST_TEXEL_TUNING EvalScore   queenNearKing          ;

//N B R QB QR K
extern CONST_TEXEL_TUNING EvalScore MOB[6][15];

// This is of course Stockfish inspired
extern CONST_TEXEL_TUNING EvalScore initiative[4];

// This idea was taken from Topple by Vincent Tang a.k.a Konsolas
enum katt_att_def : unsigned char { katt_attack = 0, katt_defence = 1 };
extern CONST_TEXEL_TUNING ScoreType kingAttMax             ;
extern CONST_TEXEL_TUNING ScoreType kingAttTrans           ;
extern CONST_TEXEL_TUNING ScoreType kingAttScale           ;
extern CONST_TEXEL_TUNING ScoreType kingAttOffset          ;
extern CONST_TEXEL_TUNING ScoreType kingAttWeight[2][6]    ;
extern CONST_TEXEL_TUNING ScoreType kingAttSafeCheck[6]    ;
extern CONST_TEXEL_TUNING ScoreType kingAttOpenfile        ;
extern CONST_TEXEL_TUNING ScoreType kingAttSemiOpenfileOpp ;
extern CONST_TEXEL_TUNING ScoreType kingAttSemiOpenfileOur ;
extern ScoreType kingAttTable[64];

extern CONST_TEXEL_TUNING EvalScore tempo;

inline double sigmoid(double x, double m = 1.f, double trans = 0.f, double scale = 1.f, double offset = 0.f){ return m / (1 + exp((trans - x) / scale)) - offset;}
inline void initEval(){ for(Square i = 0; i < 64; i++){ EvalConfig::kingAttTable[i] = (int) sigmoid(i,EvalConfig::kingAttMax,EvalConfig::kingAttTrans,EvalConfig::kingAttScale,EvalConfig::kingAttOffset); } }// idea taken from Topple

///@todo
const ScoreType shashinThreshold = 180;
const EvalScore forwardnessMalus = {20,10};
enum ShashinType : unsigned char { Shashin_None = 0, Shashin_Tal, Shashin_Tal_Capablanca, Shashin_Capablanca, Shashin_Capablanca_Petrosian, Shashin_Petrosian, Shashin_Forced };
void applyShashinCorrection(const Position & p, const EvalData & data, EvalScore & materialScore, EvalScore & developmentScore, EvalScore & positionalScore, EvalScore & mobilityScore, EvalScore & pawnStructScore, EvalScore & attackScore);

} // EvalConfig

