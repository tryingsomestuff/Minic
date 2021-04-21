#include "material.hpp"

#include "attack.hpp"
#include "evalConfig.hpp"
#include "kpk.hpp"
#include "logging.hpp"

namespace MaterialHash { // idea from Gull

    // helper function function pointer table
    ScoreType (* helperTable[TotalMat])(const Position &, Color, ScoreType );

    // the material cache
    MaterialHashEntry materialHashTable[TotalMat];

    Hash getMaterialHash(const Position::Material & mat) {
        assert(mat[Co_White][M_q]  >= 0);
        assert(mat[Co_Black][M_q]  >= 0);
        assert(mat[Co_White][M_r]  >= 0);
        assert(mat[Co_Black][M_r]  >= 0);
        assert(mat[Co_White][M_bl] >= 0);
        assert(mat[Co_Black][M_bl] >= 0);
        assert(mat[Co_White][M_bd] >= 0);
        assert(mat[Co_Black][M_bd] >= 0);
        assert(mat[Co_White][M_n]  >= 0);
        assert(mat[Co_Black][M_n]  >= 0);
        assert(mat[Co_White][M_p]  >= 0);
        assert(mat[Co_Black][M_p]  >= 0);

        if (mat[Co_White][M_q] > 2  || mat[Co_Black][M_q] > 2 
         || mat[Co_White][M_r] > 2  || mat[Co_Black][M_r] > 2 
         || mat[Co_White][M_bl] > 1 || mat[Co_Black][M_bl] > 1 
         || mat[Co_White][M_bd] > 1 || mat[Co_Black][M_bd] > 1 
         || mat[Co_White][M_n] > 2  || mat[Co_Black][M_n] > 2 
         || mat[Co_White][M_p] > 8  || mat[Co_Black][M_p] > 8) return nullHash;

        return mat[Co_White][M_p]  * MatWP + mat[Co_Black][M_p]  * MatBP 
             + mat[Co_White][M_n]  * MatWN + mat[Co_Black][M_n]  * MatBN 
             + mat[Co_White][M_bl] * MatWL + mat[Co_Black][M_bl] * MatBL 
             + mat[Co_White][M_bd] * MatWD + mat[Co_Black][M_bd] * MatBD 
             + mat[Co_White][M_r]  * MatWR + mat[Co_Black][M_r]  * MatBR 
             + mat[Co_White][M_q]  * MatWQ + mat[Co_Black][M_q]  * MatBQ;
    }

    Position::Material indexToMat(int index){
        Position::Material m = {{{{0}}}};
        m[Co_White][M_q] = index % 3; index /= 3;
        m[Co_Black][M_q] = index % 3; index /= 3;
        m[Co_White][M_r] = index % 3; index /= 3;
        m[Co_Black][M_r] = index % 3; index /= 3;
        m[Co_White][M_bl]= index % 2; index /= 2;
        m[Co_Black][M_bl]= index % 2; index /= 2;
        m[Co_White][M_bd]= index % 2; index /= 2;
        m[Co_Black][M_bd]= index % 2; index /= 2;
        m[Co_White][M_n] = index % 3; index /= 3;
        m[Co_Black][M_n] = index % 3; index /= 3;
        m[Co_White][M_p] = index % 9; index /= 9;
        m[Co_Black][M_p] = index;
        m[Co_White][M_b] = m[Co_White][M_bl] + m[Co_White][M_bd];
        m[Co_Black][M_b] = m[Co_Black][M_bl] + m[Co_Black][M_bd];
        m[Co_White][M_M] = m[Co_White][M_q] + m[Co_White][M_r];  m[Co_Black][M_M] = m[Co_Black][M_q] + m[Co_Black][M_r];
        m[Co_White][M_m] = m[Co_White][M_b] + m[Co_White][M_n];  m[Co_Black][M_m] = m[Co_Black][M_b] + m[Co_Black][M_n];
        m[Co_White][M_t] = m[Co_White][M_M] + m[Co_White][M_m];  m[Co_Black][M_t] = m[Co_Black][M_M] + m[Co_Black][M_m];
        return m;
    }

    Position::Material getMatReverseColor(const Position::Material & mat) {
        Position::Material rev = {{{{0}}}};
        rev[Co_White][M_k]  = mat[Co_Black][M_k];   rev[Co_Black][M_k]  = mat[Co_White][M_k];
        rev[Co_White][M_q]  = mat[Co_Black][M_q];   rev[Co_Black][M_q]  = mat[Co_White][M_q];
        rev[Co_White][M_r]  = mat[Co_Black][M_r];   rev[Co_Black][M_r]  = mat[Co_White][M_r];
        rev[Co_White][M_b]  = mat[Co_Black][M_b];   rev[Co_Black][M_b]  = mat[Co_White][M_b];
        rev[Co_White][M_bl] = mat[Co_Black][M_bl];  rev[Co_Black][M_bl] = mat[Co_White][M_bl];
        rev[Co_White][M_bd] = mat[Co_Black][M_bd];  rev[Co_Black][M_bd] = mat[Co_White][M_bd];
        rev[Co_White][M_n]  = mat[Co_Black][M_n];   rev[Co_Black][M_n]  = mat[Co_White][M_n];
        rev[Co_White][M_p]  = mat[Co_Black][M_p];   rev[Co_Black][M_p]  = mat[Co_White][M_p];
        rev[Co_White][M_M]  = mat[Co_Black][M_M];   rev[Co_Black][M_M]  = mat[Co_White][M_M];
        rev[Co_White][M_m]  = mat[Co_Black][M_m];   rev[Co_Black][M_m]  = mat[Co_White][M_m];
        rev[Co_White][M_t]  = mat[Co_Black][M_t];   rev[Co_Black][M_t]  = mat[Co_White][M_t];
        return rev;
    }

    Position::Material materialFromString(const std::string & strMat) {
        Position::Material mat = {{{{0}}}};
        Color c = Co_Black;
        for (auto it = strMat.begin(); it != strMat.end(); ++it) {
            switch (*it) {
            case 'K': c = ~c;             mat[c][M_k] += 1;   break;
            case 'Q': mat[c][M_q] += 1;                       mat[c][M_M] += 1;  mat[c][M_t] += 1;  break;
            case 'R': mat[c][M_r] += 1;                       mat[c][M_M] += 1;  mat[c][M_t] += 1;  break;
            case 'L': mat[c][M_bl]+= 1;   mat[c][M_b] += 1;   mat[c][M_m] += 1;  mat[c][M_t] += 1;  break;
            case 'D': mat[c][M_bd]+= 1;   mat[c][M_b] += 1;   mat[c][M_m] += 1;  mat[c][M_t] += 1;  break;
            case 'N': mat[c][M_n] += 1;                       mat[c][M_m] += 1;  mat[c][M_t] += 1;  break;
            case 'P': mat[c][M_p] += 1;   break;
            default:  Logging::LogIt(Logging::logFatal) << "Bad char in material definition";
            }
        }
        return mat;
    }

    Terminaison reverseTerminaison(Terminaison t) {
        switch (t) {
        case Ter_Unknown: case Ter_Draw: case Ter_MaterialDraw: case Ter_LikelyDraw: case Ter_HardToWin: return t;
        case Ter_WhiteWin:             return Ter_BlackWin;
        case Ter_WhiteWinWithHelper:   return Ter_BlackWinWithHelper;
        case Ter_BlackWin:             return Ter_WhiteWin;
        case Ter_BlackWinWithHelper:   return Ter_WhiteWinWithHelper;
        default:                       return Ter_Unknown;
        }
    }

    const ScoreType pushToEdges[NbSquare] = {
      100, 90, 80, 70, 70, 80, 90, 100,
       90, 70, 60, 50, 50, 60, 70,  90,
       80, 60, 40, 30, 30, 40, 60,  80,
       70, 50, 30, 20, 20, 30, 50,  70,
       70, 50, 30, 20, 20, 30, 50,  70,
       80, 60, 40, 30, 30, 40, 60,  80,
       90, 70, 60, 50, 50, 60, 70,  90,
      100, 90, 80, 70, 70, 80, 90, 100
    };

    const ScoreType pushToCorners[NbSquare] = {
      200, 190, 180, 170, 160, 150, 140, 130,
      190, 180, 170, 160, 150, 140, 130, 140,
      180, 170, 155, 140, 140, 125, 140, 150,
      170, 160, 140, 120, 110, 140, 150, 160,
      160, 150, 140, 110, 120, 140, 160, 170,
      150, 140, 125, 140, 140, 155, 170, 180,
      140, 130, 140, 150, 160, 170, 180, 190,
      130, 140, 150, 160, 170, 180, 190, 200
    };

    const ScoreType pushClose[8] = { 0, 0, 100, 80, 60, 40, 20, 10 };
    //const ScoreType pushAway [8] = { 0, 5, 20, 40, 60, 80, 90, 100 };

    ScoreType helperKXK(const Position &p, Color winningSide, ScoreType s){
        if (p.c != winningSide ){
           ///@todo stale mate detection for losing side
        }
        const Square winningK = p.king[winningSide];
        const Square losingK  = p.king[~winningSide];
        const ScoreType sc = pushToEdges[losingK] + pushClose[chebyshevDistance(winningK,losingK)];
        return clampScore(s + ((winningSide == Co_White)?(sc+WIN):(-sc-WIN)));
    }

    ScoreType helperKmmK(const Position &p, Color winningSide, ScoreType s){
        Square winningK = p.king[winningSide];
        Square losingK  = p.king[~winningSide];
        if ( (p.allBishop() & BB::whiteSquare) != 0 ){
            winningK = VFlip(winningK);
            losingK  = VFlip(losingK);
        }
        const ScoreType sc = pushToCorners[losingK] + pushClose[chebyshevDistance(winningK,losingK)];
        return clampScore( s + ((winningSide == Co_White)?(sc+WIN):(-sc-WIN)));
    }

    ScoreType helperDummy(const Position &, Color , ScoreType){ return 0; } ///@todo not 0 for debug purpose ??

    ScoreType helperKPK(const Position &p, Color winningSide, ScoreType ){
       const Square psq = KPK::normalizeSquare(p, winningSide, BBTools::SquareFromBitBoard(p.pieces_const<P_wp>(winningSide))); // we know there is at least one pawn
       if (!KPK::probe(KPK::normalizeSquare(p, winningSide, BBTools::SquareFromBitBoard(p.pieces_const<P_wk>(winningSide))), psq, KPK::normalizeSquare(p, winningSide, BBTools::SquareFromBitBoard(p.pieces_const<P_wk>(~winningSide))), winningSide == p.c ? Co_White:Co_Black)){
         if ( DynamicConfig::armageddon ){
           if ( p.c == Co_White ) return -MATE;
           else                   return  MATE;
         }
         return 0; // shall be drawScore but we don't care, this is not 3rep
       }
       return clampScore(((winningSide == Co_White)?+1:-1)*(WIN + ValuesEG[P_wp+PieceShift] + 10*SQRANK(psq)));
    }

    // Second-degree polynomial material imbalance, by Tord Romstad (old version, not the current Stockfish stuff)
    EvalScore Imbalance(const Position::Material & mat, Color c) {
        EvalScore bonus = 0;
        for (Mat m1 = M_p; m1 <= M_q; ++m1) {
            if (!mat[c][m1]) continue;
            for (Mat m2 = M_p; m2 <= m1; ++m2) {
                bonus += EvalConfig::imbalance_mines[m1-1][m2-1] * mat[c][m1] * mat[c][m2] + EvalConfig::imbalance_theirs[m1-1][m2-1] * mat[c][m1] * mat[~c][m2];
            }
        }
        return bonus/16;
    }

    void InitMaterialScore(bool display){
        if ( display) Logging::LogIt(Logging::logInfo) << "Material hash init";
        const float totalMatScore = 2.f * *absValues[P_wq] + 4.f * *absValues[P_wr] + 4.f * *absValues[P_wb] + 4.f * *absValues[P_wn] + 16.f * *absValues[P_wp];
        for (int k = 0 ; k < TotalMat ; ++k){
            const Position::Material mat = indexToMat(k);
            // MG
            const ScoreType matPieceScoreW = mat[Co_White][M_q] * *absValues[P_wq] + mat[Co_White][M_r] * *absValues[P_wr] + mat[Co_White][M_b] * *absValues[P_wb] + mat[Co_White][M_n] * *absValues[P_wn];
            const ScoreType matPieceScoreB = mat[Co_Black][M_q] * *absValues[P_wq] + mat[Co_Black][M_r] * *absValues[P_wr] + mat[Co_Black][M_b] * *absValues[P_wb] + mat[Co_Black][M_n] * *absValues[P_wn];
            const ScoreType matPawnScoreW  = mat[Co_White][M_p] * *absValues[P_wp];
            const ScoreType matPawnScoreB  = mat[Co_Black][M_p] * *absValues[P_wp];
            const ScoreType matScoreW = matPieceScoreW + matPawnScoreW;
            const ScoreType matScoreB = matPieceScoreB + matPawnScoreB;
            // EG
            const ScoreType matPieceScoreWEG = mat[Co_White][M_q] * *absValuesEG[P_wq] + mat[Co_White][M_r] * *absValuesEG[P_wr] + mat[Co_White][M_b] * *absValuesEG[P_wb] + mat[Co_White][M_n] * *absValuesEG[P_wn];
            const ScoreType matPieceScoreBEG = mat[Co_Black][M_q] * *absValuesEG[P_wq] + mat[Co_Black][M_r] * *absValuesEG[P_wr] + mat[Co_Black][M_b] * *absValuesEG[P_wb] + mat[Co_Black][M_n] * *absValuesEG[P_wn];
            const ScoreType matPawnScoreWEG  = mat[Co_White][M_p] * *absValuesEG[P_wp];
            const ScoreType matPawnScoreBEG  = mat[Co_Black][M_p] * *absValuesEG[P_wp];
            const ScoreType matScoreWEG = matPieceScoreWEG + matPawnScoreWEG;
            const ScoreType matScoreBEG = matPieceScoreBEG + matPawnScoreBEG;
#ifdef WITH_TEXEL_TUNING
            const EvalScore imbalanceW = {0,0};
            const EvalScore imbalanceB = {0,0};
#else
            const EvalScore imbalanceW = Imbalance(mat, Co_White);
            const EvalScore imbalanceB = Imbalance(mat, Co_Black);
#endif
            materialHashTable[k].gp = (matScoreW + matScoreB ) / totalMatScore;
            materialHashTable[k].score = EvalScore(imbalanceW[MG] + matScoreW   - (imbalanceB[MG] + matScoreB), imbalanceW[EG] + matScoreWEG - (imbalanceB[EG] + matScoreBEG));
        }
        if ( display) Logging::LogIt(Logging::logInfo) << "...Done";
    }

    void MaterialHashInitializer::init() {
        Logging::LogIt(Logging::logInfo) << "Material hash total : " << TotalMat;
        Logging::LogIt(Logging::logInfo) << "Material hash size : " << TotalMat*sizeof(MaterialHashEntry)/1024/1024 << "Mb";
        InitMaterialScore();
        for(size_t k = 0 ; k < TotalMat ; ++k) helperTable[k] = &helperDummy;

#define DEF_MAT(x,t)     const Position::Material MAT##x = materialFromString(TO_STR(x)); MaterialHashInitializer LINE_NAME(dummyMaterialInitializer,MAT##x)( MAT##x ,t   );
#define DEF_MAT_H(x,t,h) const Position::Material MAT##x = materialFromString(TO_STR(x)); MaterialHashInitializer LINE_NAME(dummyMaterialInitializer,MAT##x)( MAT##x ,t, h);
#define DEF_MAT_REV(rev,x)     const Position::Material MAT##rev = MaterialHash::getMatReverseColor(MAT##x); MaterialHashInitializer LINE_NAME(dummyMaterialInitializerRev,MAT##x)( MAT##rev,reverseTerminaison(materialHashTable[getMaterialHash(MAT##x)].t)   );
#define DEF_MAT_REV_H(rev,x,h) const Position::Material MAT##rev = MaterialHash::getMatReverseColor(MAT##x); MaterialHashInitializer LINE_NAME(dummyMaterialInitializerRev,MAT##x)( MAT##rev,reverseTerminaison(materialHashTable[getMaterialHash(MAT##x)].t), h);

        // WARNING : we assume STM has no capture

        ///@todo some Ter_MaterialDraw are Ter_Draw (FIDE)

        // other FIDE draw
        DEF_MAT(KLKL,   Ter_MaterialDraw)        
        DEF_MAT(KDKD,   Ter_MaterialDraw)

        // sym (and pseudo sym) : all should be draw (or very nearly)
        DEF_MAT(KK,     Ter_MaterialDraw)        
        //DEF_MAT(KQQKQQ, Ter_LikelyDraw)    // useless
        //DEF_MAT(KQKQ,   Ter_LikelyDraw)    // useless 
        //DEF_MAT(KRRKRR, Ter_LikelyDraw)    // useless
        //DEF_MAT(KRKR,   Ter_LikelyDraw)    // useless
        //DEF_MAT(KLDKLD, Ter_MaterialDraw)  // useless
        //DEF_MAT(KLLKLL, Ter_MaterialDraw)  // not a valid Hash !       
        //DEF_MAT(KDDKDD, Ter_MaterialDraw)  // not a valid Hash !
        //DEF_MAT(KNNKNN, Ter_MaterialDraw)  // useless
        DEF_MAT(KNKN  , Ter_MaterialDraw)  

        //DEF_MAT(KLDKLL, Ter_MaterialDraw)        DEF_MAT_REV(KLLKLD, KLDKLL)  // not a valid Hash !         
        //DEF_MAT(KLDKDD, Ter_MaterialDraw)        DEF_MAT_REV(KDDKLD, KLDKDD)  // not a valid Hash !  
        DEF_MAT(KLKD,   Ter_MaterialDraw)        DEF_MAT_REV(KDKL,   KLKD)

        // 2M M
        DEF_MAT(KQQKQ, Ter_WhiteWin)             DEF_MAT_REV(KQKQQ, KQQKQ)            
        DEF_MAT(KQQKR, Ter_WhiteWin)             DEF_MAT_REV(KRKQQ, KQQKR)
        DEF_MAT(KRRKQ, Ter_LikelyDraw)           DEF_MAT_REV(KQKRR, KRRKQ)            
        DEF_MAT(KRRKR, Ter_WhiteWin)             DEF_MAT_REV(KRKRR, KRRKR)
        DEF_MAT(KQRKQ, Ter_WhiteWin)             DEF_MAT_REV(KQKQR, KQRKQ)            
        DEF_MAT(KQRKR, Ter_WhiteWin)             DEF_MAT_REV(KRKQR, KQRKR)

        // 2M m (all easy wins)
        DEF_MAT(KQQKL, Ter_WhiteWin)             DEF_MAT_REV(KLKQQ, KQQKL)            
        DEF_MAT(KRRKL, Ter_WhiteWin)             DEF_MAT_REV(KLKRR, KRRKL)
        DEF_MAT(KQRKL, Ter_WhiteWin)             DEF_MAT_REV(KLKQR, KQRKL)            
        DEF_MAT(KQQKD, Ter_WhiteWin)             DEF_MAT_REV(KDKQQ, KQQKD)
        DEF_MAT(KRRKD, Ter_WhiteWin)             DEF_MAT_REV(KDKRR, KRRKD)            
        DEF_MAT(KQRKD, Ter_WhiteWin)             DEF_MAT_REV(KDKQR, KQRKD)
        DEF_MAT(KQQKN, Ter_WhiteWin)             DEF_MAT_REV(KNKQQ, KQQKN)            
        DEF_MAT(KRRKN, Ter_WhiteWin)             DEF_MAT_REV(KNKRR, KRRKN)
        DEF_MAT(KQRKN, Ter_WhiteWin)             DEF_MAT_REV(KNKQR, KQRKN)

        // 2m M
        DEF_MAT(KLDKQ, Ter_HardToWin)            DEF_MAT_REV(KQKLD,KLDKQ)            
        DEF_MAT(KLDKR, Ter_LikelyDraw)         DEF_MAT_REV(KRKLD,KLDKR)
        //DEF_MAT(KLLKQ, Ter_HardToWin)            DEF_MAT_REV(KQKLL,KLLKQ) // not a valid Hash !           
        //DEF_MAT(KLLKR, Ter_LikelyDraw)         DEF_MAT_REV(KRKLL,KLLKR) // not a valid Hash !
        //DEF_MAT(KDDKQ, Ter_HardToWin)            DEF_MAT_REV(KQKDD,KDDKQ) // not a valid Hash !           
        //DEF_MAT(KDDKR, Ter_LikelyDraw)         DEF_MAT_REV(KRKDD,KDDKR) // not a valid Hash !
        DEF_MAT(KNNKQ, Ter_HardToWin)            DEF_MAT_REV(KQKNN,KNNKQ)            
        DEF_MAT(KNNKR, Ter_LikelyDraw)         DEF_MAT_REV(KRKNN,KNNKR)
        DEF_MAT(KLNKQ, Ter_HardToWin)            DEF_MAT_REV(KQKLN,KLNKQ)            
        DEF_MAT(KLNKR, Ter_LikelyDraw)         DEF_MAT_REV(KRKLN,KLNKR)
        DEF_MAT(KDNKQ, Ter_HardToWin)            DEF_MAT_REV(KQKDN,KDNKQ)            
        DEF_MAT(KDNKR, Ter_LikelyDraw)         DEF_MAT_REV(KRKDN,KDNKR)

        // 2m m : all draw
        DEF_MAT(KLDKL, Ter_LikelyDraw)         DEF_MAT_REV(KLKLD,KLDKL)            
        DEF_MAT(KLDKD, Ter_LikelyDraw)         DEF_MAT_REV(KDKLD,KLDKD)
        DEF_MAT(KLDKN, Ter_LikelyDraw)         DEF_MAT_REV(KNKLD,KLDKN)            
        //DEF_MAT(KLLKL, Ter_LikelyDraw)         DEF_MAT_REV(KLKLL,KLLKL) // not a valid Hash !
        //DEF_MAT(KLLKD, Ter_LikelyDraw)         DEF_MAT_REV(KDKLL,KLLKD) // not a valid Hash !           
        //DEF_MAT(KLLKN, Ter_LikelyDraw)         DEF_MAT_REV(KNKLL,KLLKN) // not a valid Hash !
        //DEF_MAT(KDDKL, Ter_LikelyDraw)         DEF_MAT_REV(KLKDD,KDDKL) // not a valid Hash !           
        //DEF_MAT(KDDKD, Ter_LikelyDraw)         DEF_MAT_REV(KDKDD,KDDKD) // not a valid Hash !
        //DEF_MAT(KDDKN, Ter_LikelyDraw)         DEF_MAT_REV(KNKDD,KDDKN) // not a valid Hash !            
        DEF_MAT(KNNKL, Ter_LikelyDraw)         DEF_MAT_REV(KLKNN,KNNKL)
        DEF_MAT(KNNKD, Ter_LikelyDraw)         DEF_MAT_REV(KDKNN,KNNKD)            
        DEF_MAT(KNNKN, Ter_LikelyDraw)         DEF_MAT_REV(KNKNN,KNNKN)
        DEF_MAT(KLNKL, Ter_LikelyDraw)         DEF_MAT_REV(KLKLN,KLNKL)            
        DEF_MAT(KLNKD, Ter_LikelyDraw)         DEF_MAT_REV(KDKLN,KLNKD)
        DEF_MAT(KLNKN, Ter_LikelyDraw)         DEF_MAT_REV(KNKLN,KLNKN)            
        DEF_MAT(KDNKL, Ter_LikelyDraw)         DEF_MAT_REV(KLKDN,KDNKL)
        DEF_MAT(KDNKD, Ter_LikelyDraw)         DEF_MAT_REV(KDKDN,KDNKD)            
        DEF_MAT(KDNKN, Ter_LikelyDraw)         DEF_MAT_REV(KNKDN,KDNKN)

        // Q x : all should be win (against rook with helper)
        DEF_MAT_H(KQKR, Ter_WhiteWinWithHelper,&helperKXK)      DEF_MAT_REV_H(KRKQ,KQKR,&helperKXK)    
        DEF_MAT_H(KQKL, Ter_WhiteWinWithHelper,&helperKXK)      DEF_MAT_REV_H(KLKQ,KQKL,&helperKXK)
        DEF_MAT_H(KQKD, Ter_WhiteWinWithHelper,&helperKXK)      DEF_MAT_REV_H(KDKQ,KQKD,&helperKXK)              
        DEF_MAT_H(KQKN, Ter_WhiteWinWithHelper,&helperKXK)      DEF_MAT_REV_H(KNKQ,KQKN,&helperKXK)

        // R x : all should be draw
        DEF_MAT(KRKL, Ter_LikelyDraw)            DEF_MAT_REV(KLKR,KRKL)              
        DEF_MAT(KRKD, Ter_LikelyDraw)            DEF_MAT_REV(KDKR,KRKD)
        DEF_MAT(KRKN, Ter_LikelyDraw)            DEF_MAT_REV(KNKR,KRKN)

        // B x : all are draw
        DEF_MAT(KLKN, Ter_MaterialDraw)          DEF_MAT_REV(KNKL,KLKN)              
        DEF_MAT(KDKN, Ter_MaterialDraw)          DEF_MAT_REV(KNKD,KDKN)

        // X 0 : QR win, BN draw
        DEF_MAT_H(KQK, Ter_WhiteWinWithHelper,&helperKXK)   DEF_MAT_REV_H(KKQ,KQK,&helperKXK)
        DEF_MAT_H(KRK, Ter_WhiteWinWithHelper,&helperKXK)   DEF_MAT_REV_H(KKR,KRK,&helperKXK)
        DEF_MAT(KLK, Ter_MaterialDraw)                      DEF_MAT_REV(KKL,KLK)
        DEF_MAT(KDK, Ter_MaterialDraw)                      DEF_MAT_REV(KKD,KDK)
        DEF_MAT(KNK, Ter_MaterialDraw)                      DEF_MAT_REV(KKN,KNK)

        // 2X 0 : all win except LL, DD, NN
        DEF_MAT(KQQK, Ter_WhiteWin)                         DEF_MAT_REV(KKQQ,KQQK)
        DEF_MAT(KRRK, Ter_WhiteWin)                         DEF_MAT_REV(KKRR,KRRK)
        DEF_MAT_H(KLDK, Ter_WhiteWinWithHelper,&helperKmmK) DEF_MAT_REV_H(KKLD,KLDK,&helperKmmK)
        //DEF_MAT(KLLK, Ter_MaterialDraw)                     DEF_MAT_REV(KKLL,KLLK)
        //DEF_MAT(KDDK, Ter_MaterialDraw)                     DEF_MAT_REV(KKDD,KDDK)
        DEF_MAT(KNNK, Ter_MaterialDraw)                     DEF_MAT_REV(KKNN,KNNK)
        DEF_MAT_H(KLNK, Ter_WhiteWinWithHelper,&helperKmmK) DEF_MAT_REV_H(KKLN,KLNK,&helperKmmK)
        DEF_MAT_H(KDNK, Ter_WhiteWinWithHelper,&helperKmmK) DEF_MAT_REV_H(KKDN,KDNK,&helperKmmK)

        // Rm R : draws
        DEF_MAT(KRNKR, Ter_LikelyDraw)            DEF_MAT_REV(KRKRN,KRNKR)
        DEF_MAT(KRLKR, Ter_LikelyDraw)            DEF_MAT_REV(KRKRL,KRLKR)
        DEF_MAT(KRDKR, Ter_LikelyDraw)            DEF_MAT_REV(KRKRD,KRDKR)

        // Rm m : draws
        DEF_MAT(KRNKN, Ter_HardToWin)             DEF_MAT_REV(KNKRN,KRNKN)            
        DEF_MAT(KRNKL, Ter_HardToWin)             DEF_MAT_REV(KLKRN,KRNKL)
        DEF_MAT(KRNKD, Ter_HardToWin)             DEF_MAT_REV(KDKRN,KRNKD)            
        DEF_MAT(KRLKN, Ter_HardToWin)             DEF_MAT_REV(KNKRL,KRLKN)
        DEF_MAT(KRLKL, Ter_HardToWin)             DEF_MAT_REV(KLKRL,KRLKL)            
        DEF_MAT(KRLKD, Ter_HardToWin)             DEF_MAT_REV(KDKRL,KRLKD)
        DEF_MAT(KRDKN, Ter_HardToWin)             DEF_MAT_REV(KNKRD,KRDKN)            
        DEF_MAT(KRDKL, Ter_HardToWin)             DEF_MAT_REV(KLKRD,KRDKL)
        DEF_MAT(KRDKD, Ter_HardToWin)             DEF_MAT_REV(KDKRD,KRDKD)

        // Qm Q : draws
        DEF_MAT(KQNKQ, Ter_LikelyDraw)             DEF_MAT_REV(KQKQN,KQNKQ)
        DEF_MAT(KQLKQ, Ter_LikelyDraw)             DEF_MAT_REV(KQKQL,KQLKQ)
        DEF_MAT(KQDKQ, Ter_LikelyDraw)             DEF_MAT_REV(KQKQD,KQDKQ)

        // Qm R : wins
        DEF_MAT_H(KQNKR, Ter_WhiteWin,&helperKXK)              DEF_MAT_REV_H(KRKQN,KQNKR,&helperKXK)
        DEF_MAT_H(KQLKR, Ter_WhiteWin,&helperKXK)              DEF_MAT_REV_H(KRKQL,KQLKR,&helperKXK)
        DEF_MAT_H(KQDKR, Ter_WhiteWin,&helperKXK)              DEF_MAT_REV_H(KRKQD,KQRKR,&helperKXK)

        // Q Rm : draws
        DEF_MAT(KQKRN, Ter_LikelyDraw)             DEF_MAT_REV(KRNKQ,KQKRN)
        DEF_MAT(KQKRL, Ter_LikelyDraw)             DEF_MAT_REV(KRLKQ,KQKRL)
        DEF_MAT(KQKRD, Ter_LikelyDraw)             DEF_MAT_REV(KRDKQ,KQKRD)

        // Opposite bishop with Ps
        DEF_MAT(KLPKD, Ter_LikelyDraw)            DEF_MAT_REV(KDKLP,KLPKD)            
        DEF_MAT(KDPKL, Ter_LikelyDraw)            DEF_MAT_REV(KLKDP,KDPKL)
        DEF_MAT(KLPPKD, Ter_LikelyDraw)           DEF_MAT_REV(KDKLPP,KLPPKD)          
        DEF_MAT(KDPPKL, Ter_LikelyDraw)           DEF_MAT_REV(KLKDPP,KDPPKL)

        // KPK
        DEF_MAT_H(KPK, Ter_WhiteWinWithHelper,&helperKPK)    DEF_MAT_REV_H(KKP,KPK,&helperKPK)

        ///@todo other (with more pawn ...), especially KPsKB with wrong Color bishop

    }

    Terminaison probeMaterialHashTable(const Position::Material & mat) {
      Hash h = getMaterialHash(mat);
      if ( h == nullHash ) return Ter_Unknown;
      return materialHashTable[h].t;
    }

    void updateMaterialOther(Position & p){
        p.mat[Co_White][M_M] = p.mat[Co_White][M_q] + p.mat[Co_White][M_r];  
        p.mat[Co_White][M_m] = p.mat[Co_White][M_b] + p.mat[Co_White][M_n];  
        p.mat[Co_White][M_t] = p.mat[Co_White][M_M] + p.mat[Co_White][M_m];  
        const BitBoard wb = p.whiteBishop();
        p.mat[Co_White][M_bl] = (unsigned char)BB::countBit(wb&BB::whiteSquare);   
        p.mat[Co_White][M_bd] = (unsigned char)BB::countBit(wb&BB::blackSquare);

        p.mat[Co_Black][M_M] = p.mat[Co_Black][M_q] + p.mat[Co_Black][M_r];
        p.mat[Co_Black][M_m] = p.mat[Co_Black][M_b] + p.mat[Co_Black][M_n];
        p.mat[Co_Black][M_t] = p.mat[Co_Black][M_M] + p.mat[Co_Black][M_m];
        const BitBoard bb = p.blackBishop();
        p.mat[Co_Black][M_bl] = (unsigned char)BB::countBit(bb&BB::whiteSquare);   
        p.mat[Co_Black][M_bd] = (unsigned char)BB::countBit(bb&BB::blackSquare);
    }

    void initMaterial(Position & p){ // M_p .. M_k is the same as P_wp .. P_wk
        for( Color c = Co_White ; c < Co_End ; ++c) for( Piece pp = P_wp ; pp <= P_wk ; ++pp) p.mat[c][pp] = (unsigned char)BB::countBit(p.pieces_const(c,pp));
        updateMaterialOther(p);
    }

    void updateMaterialProm(Position &p, const Square toBeCaptured, MType mt){
        p.mat[~p.c][PieceTools::getPieceType(p,toBeCaptured)]--; // capture if to square is not empty
        p.mat[p.c][M_p]--; // pawn
        p.mat[p.c][promShift(mt)]++;   // prom piece
    }

} // MaterialHash
