#pragma once

#include "bitboardTools.hpp"
#include "score.hpp"

/*!
 * A material table implementation
 * Mostly inspired by Gull by Vadim Demichev
 * after a discussion on talkchess (http://talkchess.com/forum3/viewtopic.php?f=7&t=67558&p=763426)
 * but we allow 2 bishops of the same color
 */

namespace MaterialHash {

inline const int MatWQ    = 1;
inline const int MatBQ    = 3;
inline const int MatWR    = (3 * 3);
inline const int MatBR    = (3 * 3 * 3);
inline const int MatWL    = (3 * 3 * 3 * 3);
inline const int MatBL    = (3 * 3 * 3 * 3 * 3);
inline const int MatWD    = (3 * 3 * 3 * 3 * 3 * 3);
inline const int MatBD    = (3 * 3 * 3 * 3 * 3 * 3 * 3);
inline const int MatWN    = (3 * 3 * 3 * 3 * 3 * 3 * 3 * 3);
inline const int MatBN    = (3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3);
inline const int MatWP    = (3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3);
inline const int MatBP    = (3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 3 * 9);
#ifdef WITH_MATERIAL_TABLE
inline const int TotalMat = ((2 * (MatWQ + MatBQ) + 2 * (MatWL + MatBL + MatWD + MatBD) + 2 * (MatWR + MatBR + MatWN + MatBN) + 8 * (MatWP + MatBP)) + 1);
#else
inline const int TotalMat = 1;
#endif

[[nodiscard]] Hash getMaterialHash(const Position::Material &mat);
#ifndef WITH_MATERIAL_TABLE
[[nodiscard]] Hash getMaterialHash2(const Position::Material &mat);
#endif

[[nodiscard]] Position::Material materialFromString(const std::string &strMat);

ScoreType helperKXK(const Position &p, Color winningSide, ScoreType s, DepthType height);
ScoreType helperKmmK(const Position &p, Color winningSide, ScoreType s, DepthType height);
ScoreType helperKPK(const Position &p, Color winningSide, ScoreType, DepthType height);
ScoreType helperKBPK(const Position &p, Color winningSide, ScoreType s, DepthType height);

enum Terminaison : uint8_t {
   Ter_Unknown = 0,
   Ter_WhiteWinWithHelper,
   Ter_WhiteWin,
   Ter_BlackWinWithHelper,
   Ter_BlackWin,
   Ter_Draw,
   Ter_MaterialDraw,
   Ter_LikelyDraw,
   Ter_HardToWin
};

extern ScoreType (*helperTable[TotalMat])(const Position &, Color, ScoreType, DepthType);

#pragma pack(push, 1)
struct MaterialHashEntry {
   EvalScore    score = {0, 0};
   uint8_t      gp    = 255;
   Terminaison  t     = Ter_Unknown;
   inline float gamePhase() const { return gp / 255.f; }
   inline void  setGamePhase(float gpf) { gp = (uint8_t)(255 * gpf); }
};
#pragma pack(pop)

extern MaterialHashEntry materialHashTable[TotalMat];

[[nodiscard]] EvalScore Imbalance(const Position::Material &mat, Color c);

void InitMaterialScore(bool display = true);

struct MaterialHashInitializer {
   MaterialHashInitializer(const Position::Material &mat, Terminaison t) { materialHashTable[getMaterialHash(mat)].t = t; }
   MaterialHashInitializer(const Position::Material &mat, Terminaison t, ScoreType (*helper)(const Position &, Color, ScoreType, DepthType)) {
      materialHashTable[getMaterialHash(mat)].t = t;
      helperTable[getMaterialHash(mat)]         = helper;
   }
   static void init();
};

[[nodiscard]] Terminaison probeMaterialHashTable(const Position::Material &mat);

void updateMaterialOther(Position &p);

void initMaterial(Position &p);

void updateMaterialProm(Position &p, const Square toBeCaptured, const MType mt);

} // namespace MaterialHash
