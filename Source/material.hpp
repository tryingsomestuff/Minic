#pragma once

#include "bitboardTools.hpp"
#include "score.hpp"

/* A material table implementation
 * Mostly inspired by Gull by Vadim Demichev
 * after a discussion on talkchess (http://talkchess.com/forum3/viewtopic.php?f=7&t=67558&p=763426)
 */

namespace MaterialHash {
    const int MatWQ = 1;
    const int MatBQ = 3;
    const int MatWR = (3 * 3);
    const int MatBR = (3 * 3 * 3);
    const int MatWL = (3 * 3 * 3 * 3);
    const int MatBL = (3 * 3 * 3 * 3 * 2);
    const int MatWD = (3 * 3 * 3 * 3 * 2 * 2);
    const int MatBD = (3 * 3 * 3 * 3 * 2 * 2 * 2);
    const int MatWN = (3 * 3 * 3 * 3 * 2 * 2 * 2 * 2);
    const int MatBN = (3 * 3 * 3 * 3 * 2 * 2 * 2 * 2 * 3);
    const int MatWP = (3 * 3 * 3 * 3 * 2 * 2 * 2 * 2 * 3 * 3);
    const int MatBP = (3 * 3 * 3 * 3 * 2 * 2 * 2 * 2 * 3 * 3 * 9);
    const int TotalMat = ((2 * (MatWQ + MatBQ) + MatWL + MatBL + MatWD + MatBD + 2 * (MatWR + MatBR + MatWN + MatBN) + 8 * (MatWP + MatBP)) + 1);

    Hash getMaterialHash(const Position::Material & mat);

    enum Terminaison : unsigned char {
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

    extern ScoreType (* helperTable[TotalMat])(const Position &, Color, ScoreType );

#pragma pack(push, 1)
    struct MaterialHashEntry  {
      EvalScore score={0,0};
      float gp = 1.f;
      Terminaison t = Ter_Unknown;
    };
#pragma pack(pop)    

    extern MaterialHashEntry materialHashTable[TotalMat];

    EvalScore Imbalance(const Position::Material & mat, Color c);

    void InitMaterialScore(bool display = true);

    struct MaterialHashInitializer {
        MaterialHashInitializer(const Position::Material & mat, Terminaison t) { materialHashTable[getMaterialHash(mat)].t = t; }
        MaterialHashInitializer(const Position::Material & mat, Terminaison t, ScoreType (*helper)(const Position &, Color, ScoreType) ) { materialHashTable[getMaterialHash(mat)].t = t; helperTable[getMaterialHash(mat)] = helper; }
        static void init();
    };

    Terminaison probeMaterialHashTable(const Position::Material & mat);

    void updateMaterialOther(Position & p);

    void initMaterial(Position & p);

    void updateMaterialProm(Position &p, const Square toBeCaptured, MType mt);

} // MaterialHash
