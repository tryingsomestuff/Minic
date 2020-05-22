#pragma once

#include "attack.hpp"
#include "bitboardTools.hpp"
#include "dynamicConfig.hpp"
#include "evalConfig.hpp"
#include "hash.hpp"
#include "positionTools.hpp"
#include "searcher.hpp"
#include "score.hpp"
#include "timers.hpp"

namespace{ // some Color / Piece helpers
   template<Color C> inline bool isPasser(const Position &p, Square k)     { return (BBTools::mask[k].passerSpan[C] & p.pieces_const<P_wp>(~C)) == empty;}
   template<Color C> inline constexpr ScoreType ColorSignHelper()          { return C==Co_White?+1:-1;}
   template<Color C> inline Square PromotionSquare(const Square k)         { return C==Co_White? (SQFILE(k) + 56) : SQFILE(k);}
   template<Color C> inline Rank ColorRank(const Square k)                 { return Rank(C==Co_White? SQRANK(k) : (7-SQRANK(k)));}
   template<Color C> inline bool isBackward(const Position &p, Square k, const BitBoard pAtt[2], const BitBoard pAttSpan[2]){ return ((BBTools::shiftN<C>(SquareToBitboard(k))&~p.pieces_const<P_wp>(~C)) & pAtt[~C] & ~pAttSpan[C]) != empty; }
   template<Piece T> inline BitBoard alignedThreatPieceSlider(const Position & p, Color C);
   template<> inline BitBoard alignedThreatPieceSlider<P_wn>(const Position &  , Color  ){ return empty;}
   template<> inline BitBoard alignedThreatPieceSlider<P_wb>(const Position & p, Color C){ return p.pieces_const<P_wb>(C) | p.pieces_const<P_wq>(C) /*| p.pieces_const<P_wn>(C)*/;}
   template<> inline BitBoard alignedThreatPieceSlider<P_wr>(const Position & p, Color C){ return p.pieces_const<P_wr>(C) | p.pieces_const<P_wq>(C) /*| p.pieces_const<P_wn>(C)*/;}
   template<> inline BitBoard alignedThreatPieceSlider<P_wq>(const Position & p, Color C){ return p.pieces_const<P_wb>(C) | p.pieces_const<P_wr>(C) /*| p.pieces_const<P_wn>(C)*/;} ///@todo this is false ...
   template<> inline BitBoard alignedThreatPieceSlider<P_wk>(const Position &  , Color  ){ return empty;}
}

template < Piece T , Color C>
inline void evalPiece(const Position & p, BitBoard pieceBBiterator, const BitBoard (& kingZone)[2], EvalScore & score, BitBoard & attBy, BitBoard & att, BitBoard & att2, ScoreType (& kdanger)[2], BitBoard & checkers, float & forwardness){
    while (pieceBBiterator) {
        const Square k = popBit(pieceBBiterator);
        const Square kk = ColorSquarePstHelper<C>(k);
        score += EvalConfig::PST[T-1][kk] * ColorSignHelper<C>();
        forwardness += SQRANK(kk);
        const BitBoard target = BBTools::pfCoverage[T-1](k, p.occupancy(), C); // real targets
        if ( target ){
           attBy |= target;
           att2  |= att & target;
           att   |= target;
           if ( target & p.pieces_const<P_wk>(~C) ) checkers |= SquareToBitboard(k);
        }
        const BitBoard shadowTarget = BBTools::pfCoverage[T-1](k, p.occupancy() ^ /*p.pieces<T>(C)*/ alignedThreatPieceSlider<T>(p,C), C); // aligned threats of same piece type also taken into account and knight in front also removed ///@todo better?
        if ( shadowTarget ){
           kdanger[C]  -= countBit(shadowTarget & kingZone[C])  * EvalConfig::kingAttWeight[EvalConfig::katt_defence][T-1];
           kdanger[~C] += countBit(shadowTarget & kingZone[~C]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack][T-1];
        }
    }
}
///@todo special version of evalPiece for king ??

template < Piece T ,Color C>
inline void evalMob(const Position & p, BitBoard pieceBBiterator, EvalScore & score, const BitBoard safe, EvalData & data){
    while (pieceBBiterator){
        const unsigned short int mob = countBit(BBTools::pfCoverage[T-1](popBit(pieceBBiterator), p.occupancy(), C) & ~p.allPieces[C] & safe);
        data.mobility[C] += mob;
        score += EvalConfig::MOB[T-2][mob]*ColorSignHelper<C>();
    }
}

template < Color C >
inline void evalMobQ(const Position & p, BitBoard pieceBBiterator, EvalScore & score, const BitBoard safe, EvalData & data){
    while (pieceBBiterator){
        const Square s = popBit(pieceBBiterator);
        unsigned short int mob = countBit(BBTools::pfCoverage[P_wb-1](s, p.occupancy(), C) & ~p.allPieces[C] & safe);
        data.mobility[C] += mob;
        score += EvalConfig::MOB[3][mob]*ColorSignHelper<C>();
        mob = countBit(BBTools::pfCoverage[P_wr-1](s, p.occupancy(), C) & ~p.allPieces[C] & safe);
        data.mobility[C] += mob;
        score += EvalConfig::MOB[4][mob]*ColorSignHelper<C>();
    }
}

template < Color C>
inline void evalMobK(const Position & p, BitBoard pieceBBiterator, EvalScore & score, const BitBoard safe, EvalData & data){
    while (pieceBBiterator){
        const unsigned short int mob = countBit(BBTools::pfCoverage[P_wk-1](popBit(pieceBBiterator), p.occupancy(), C) & ~p.allPieces[C] & safe);
        data.mobility[C] += mob;
        score += EvalConfig::MOB[5][mob]*ColorSignHelper<C>();
    }
}

template< Color C>
inline void evalPawnPasser(const Position & p, BitBoard pieceBBiterator, EvalScore & score){
    while (pieceBBiterator) {
        const Square k = popBit(pieceBBiterator);
        const EvalScore kingNearBonus   = EvalConfig::kingNearPassedPawn * ScoreType( chebyshevDistance(p.king[~C], k) - chebyshevDistance(p.king[C], k) );
        const bool unstoppable          = (p.mat[~C][M_t] == 0)&&((chebyshevDistance(p.king[~C],PromotionSquare<C>(k))-int(p.c!=C)) > std::min(Square(5), chebyshevDistance(PromotionSquare<C>(k),k)));
        if (unstoppable) score += ColorSignHelper<C>()*(Values[P_wr+PieceShift] - Values[P_wp+PieceShift]); // yes rook not queen to force promotion asap
        else             score += (EvalConfig::passerBonus[ColorRank<C>(k)] + kingNearBonus)*ColorSignHelper<C>();
    }
}

template < Color C>
inline void evalPawn(BitBoard pieceBBiterator, EvalScore & score){
    while (pieceBBiterator) { score += EvalConfig::PST[0][ColorSquarePstHelper<C>(popBit(pieceBBiterator))] * ColorSignHelper<C>(); }
}

template< Color C>
inline void evalPawnFreePasser(const Position & p, BitBoard pieceBBiterator, EvalScore & score){
    while (pieceBBiterator) {
        const Square k = popBit(pieceBBiterator);
        score += EvalConfig::freePasserBonus[ColorRank<C>(k)] * ScoreType( (BBTools::mask[k].frontSpan[C] & p.allPieces[~C]) == empty ) * ColorSignHelper<C>();
    }
}

template< Color C>
inline void evalPawnProtected(BitBoard pieceBBiterator, EvalScore & score){
    while (pieceBBiterator) { score += EvalConfig::protectedPasserBonus[ColorRank<C>(popBit(pieceBBiterator))] * ColorSignHelper<C>();}
}

template< Color C>
inline void evalPawnCandidate(BitBoard pieceBBiterator, EvalScore & score){
    while (pieceBBiterator) { score += EvalConfig::candidate[ColorRank<C>(popBit(pieceBBiterator))] * ColorSignHelper<C>();}
}

template< Color C>
BitBoard getPinned(const Position & p, const Square s){
    BitBoard pinned = empty;
    if ( s == INVALIDSQUARE ) return pinned;
    BitBoard pinner = BBTools::attack<P_wb>(s, p.pieces_const<P_wb>(~C) | p.pieces_const<P_wq>(~C), p.allPieces[~C]) | BBTools::attack<P_wr>(s, p.pieces_const<P_wr>(~C) | p.pieces_const<P_wq>(~C), p.allPieces[~C]);
    while ( pinner ) { pinned |= BBTools::mask[popBit(pinner)].between[p.king[C]] & p.allPieces[C]; }
    return pinned;
}

template <typename T> int sgn(T val) { return (T(0) < val) - (val < T(0)); }

template < bool display, bool safeMatEvaluator>
inline ScoreType eval(const Position & p, EvalData & data, Searcher &context){
    START_TIMER

    // king captured
    const bool white2Play = p.c == Co_White;
    if ( p.king[Co_White] == INVALIDSQUARE ){
      STOP_AND_SUM_TIMER(Eval)
      ++context.stats.counters[Stats::sid_evalNoKing];
      return data.gp=0,(white2Play?-1:+1)* MATE;
    }
    if ( p.king[Co_Black] == INVALIDSQUARE ) {
      STOP_AND_SUM_TIMER(Eval)
      ++context.stats.counters[Stats::sid_evalNoKing];
      return data.gp=0,(white2Play?+1:-1)* MATE;
    }

    ///@todo activate features based on skill level
    
    context.prefetchPawn(computeHash(p));

    // main features
    EvalFeatures features;

    // Material evaluation
    const Hash matHash = MaterialHash::getMaterialHash(p.mat);
    if ( matHash ){
       ++context.stats.counters[Stats::sid_materialTableHits];
       // Hash data
       const MaterialHash::MaterialHashEntry & MEntry = MaterialHash::materialHashTable[matHash];
       data.gp = MEntry.gp;
       features.materialScore += MEntry.score;
       // end game knowledge (helper or scaling)
       if ( safeMatEvaluator && (p.mat[Co_White][M_t]+p.mat[Co_Black][M_t]<6) ){
          const Color winningSideEG = features.materialScore[EG]>0?Co_White:Co_Black;
          if      ( MEntry.t == MaterialHash::Ter_WhiteWinWithHelper || MEntry.t == MaterialHash::Ter_BlackWinWithHelper ){
            STOP_AND_SUM_TIMER(Eval)
            return (white2Play?+1:-1)*(MaterialHash::helperTable[matHash](p,winningSideEG,features.materialScore[EG]));
          }
          else if ( MEntry.t == MaterialHash::Ter_Draw){ 
              if (!isAttacked(p, kingSquare(p))) {
                 STOP_AND_SUM_TIMER(Eval)
                 return context.drawScore();
              }
          }
          else if ( MEntry.t == MaterialHash::Ter_MaterialDraw) {
              STOP_AND_SUM_TIMER(Eval)
              if (!isAttacked(p, kingSquare(p))) return context.drawScore();
          }
          else if ( MEntry.t == MaterialHash::Ter_WhiteWin || MEntry.t == MaterialHash::Ter_BlackWin) features.scalingFactor = 5 - 5*p.fifty/100.f;
          else if ( MEntry.t == MaterialHash::Ter_HardToWin)  features.scalingFactor = 0.5f - 0.5f*(p.fifty/100.f);
          else if ( MEntry.t == MaterialHash::Ter_LikelyDraw) features.scalingFactor = 0.3f - 0.3f*(p.fifty/100.f);
       }
    }
    else{ // game phase and material scores out of table
       ScoreType matScoreW = 0;
       ScoreType matScoreB = 0;
       data.gp = gamePhase(p,matScoreW, matScoreB);
       features.materialScore[EG] += (p.mat[Co_White][M_q] - p.mat[Co_Black][M_q]) * *absValuesEG[P_wq] + (p.mat[Co_White][M_r] - p.mat[Co_Black][M_r]) * *absValuesEG[P_wr] + (p.mat[Co_White][M_b] - p.mat[Co_Black][M_b]) * *absValuesEG[P_wb] + (p.mat[Co_White][M_n] - p.mat[Co_Black][M_n]) * *absValuesEG[P_wn] + (p.mat[Co_White][M_p] - p.mat[Co_Black][M_p]) * *absValuesEG[P_wp];
       features.materialScore[MG] += matScoreW - matScoreB;
       ++context.stats.counters[Stats::sid_materialTableMiss];
    }

#ifdef WITH_TEXEL_TUNING
    features.materialScore += MaterialHash::Imbalance(p.mat, Co_White) - MaterialHash::Imbalance(p.mat, Co_Black);
#endif

    STOP_AND_SUM_TIMER(Eval1)

    // usefull bitboards accumulator
    const BitBoard pawns[2]          = {p.whitePawn(), p.blackPawn()};
    const BitBoard allPawns          = pawns[Co_White] | pawns[Co_Black];
    ScoreType kdanger[2]             = {0, 0};
    BitBoard att[2]                  = {empty, empty};
    BitBoard att2[2]                 = {empty, empty};
    BitBoard attFromPiece[2][6]      = {{empty}}; ///@todo use this more!
    BitBoard checkers[2][6]          = {{empty}};

    const BitBoard kingZone[2]   = { BBTools::mask[p.king[Co_White]].kingZone, BBTools::mask[p.king[Co_Black]].kingZone};
    const BitBoard kingShield[2] = { kingZone[Co_White] & ~BBTools::shiftS<Co_White>(ranks[SQRANK(p.king[Co_White])]) , kingZone[Co_Black] & ~BBTools::shiftS<Co_Black>(ranks[SQRANK(p.king[Co_Black])]) };

    // PST, attack, danger
    evalPiece<P_wn,Co_White>(p,p.pieces_const<P_wn>(Co_White),kingZone,features.positionalScore,attFromPiece[Co_White][P_wn-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wn-1],data.shashinForwardness[Co_White]);
    evalPiece<P_wb,Co_White>(p,p.pieces_const<P_wb>(Co_White),kingZone,features.positionalScore,attFromPiece[Co_White][P_wb-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wb-1],data.shashinForwardness[Co_White]);
    evalPiece<P_wr,Co_White>(p,p.pieces_const<P_wr>(Co_White),kingZone,features.positionalScore,attFromPiece[Co_White][P_wr-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wr-1],data.shashinForwardness[Co_White]);
    evalPiece<P_wq,Co_White>(p,p.pieces_const<P_wq>(Co_White),kingZone,features.positionalScore,attFromPiece[Co_White][P_wq-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wq-1],data.shashinForwardness[Co_White]);
    evalPiece<P_wk,Co_White>(p,p.pieces_const<P_wk>(Co_White),kingZone,features.positionalScore,attFromPiece[Co_White][P_wk-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wk-1],data.shashinForwardness[Co_White]);
    evalPiece<P_wn,Co_Black>(p,p.pieces_const<P_wn>(Co_Black),kingZone,features.positionalScore,attFromPiece[Co_Black][P_wn-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wn-1],data.shashinForwardness[Co_Black]);
    evalPiece<P_wb,Co_Black>(p,p.pieces_const<P_wb>(Co_Black),kingZone,features.positionalScore,attFromPiece[Co_Black][P_wb-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wb-1],data.shashinForwardness[Co_Black]);
    evalPiece<P_wr,Co_Black>(p,p.pieces_const<P_wr>(Co_Black),kingZone,features.positionalScore,attFromPiece[Co_Black][P_wr-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wr-1],data.shashinForwardness[Co_Black]);
    evalPiece<P_wq,Co_Black>(p,p.pieces_const<P_wq>(Co_Black),kingZone,features.positionalScore,attFromPiece[Co_Black][P_wq-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wq-1],data.shashinForwardness[Co_Black]);
    evalPiece<P_wk,Co_Black>(p,p.pieces_const<P_wk>(Co_Black),kingZone,features.positionalScore,attFromPiece[Co_Black][P_wk-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wk-1],data.shashinForwardness[Co_Black]);

    /*
#ifndef WITH_TEXEL_TUNING
    // lazy evaluation
    ScoreType lazyScore = score.Score(p,data.gp); // scale both phase and 50 moves rule
    if ( std::abs(lazyScore) > ScaleScore({600,1000}, data.gp)){ // winning / losing position
        ScoreType ret = (white2Play?+1:-1)*lazyScore;
        STOP_AND_SUM_TIMER(Eval)
        return ret;
    }
#endif
    */

    STOP_AND_SUM_TIMER(Eval2)

    Searcher::PawnEntry * pePtr = nullptr;
#ifdef WITH_TEXEL_TUNING
    Searcher::PawnEntry dummy; // used for texel tuning
    pePtr = &dummy;
    {
#else
    if ( !context.getPawnEntry(computePHash(p), pePtr) ){
#endif
       assert(pePtr);
       Searcher::PawnEntry & pe = *pePtr;
       pe.reset();
       const BitBoard backward      [2] = {BBTools::pawnBackward  <Co_White>(pawns[Co_White],pawns[Co_Black]) , BBTools::pawnBackward  <Co_Black>(pawns[Co_Black],pawns[Co_White])};
       const BitBoard isolated      [2] = {BBTools::pawnIsolated            (pawns[Co_White])                 , BBTools::pawnIsolated            (pawns[Co_Black])};
       const BitBoard doubled       [2] = {BBTools::pawnDoubled   <Co_White>(pawns[Co_White])                 , BBTools::pawnDoubled   <Co_Black>(pawns[Co_Black])};
       const BitBoard candidates    [2] = {BBTools::pawnCandidates<Co_White>(pawns[Co_White],pawns[Co_Black]) , BBTools::pawnCandidates<Co_Black>(pawns[Co_Black],pawns[Co_White])};
       const BitBoard semiOpenPawn  [2] = {BBTools::pawnSemiOpen  <Co_White>(pawns[Co_White],pawns[Co_Black]) , BBTools::pawnSemiOpen  <Co_Black>(pawns[Co_Black],pawns[Co_White])};
       const BitBoard detached      [2] = {BBTools::pawnDetached  <Co_White>(pawns[Co_White],pawns[Co_Black]) , BBTools::pawnDetached  <Co_Black>(pawns[Co_Black],pawns[Co_White])};
       pe.pawnTargets   [Co_White] = BBTools::pawnAttacks   <Co_White>(pawns[Co_White])                       ; pe.pawnTargets   [Co_Black] = BBTools::pawnAttacks   <Co_Black>(pawns[Co_Black]);
       pe.semiOpenFiles [Co_White] = BBTools::fillFile(pawns[Co_White]) & ~BBTools::fillFile(pawns[Co_Black]) ; pe.semiOpenFiles [Co_Black] = BBTools::fillFile(pawns[Co_Black]) & ~BBTools::fillFile(pawns[Co_White]); // semiOpen white means with white pawn, and without black pawn
       pe.passed        [Co_White] = BBTools::pawnPassed    <Co_White>(pawns[Co_White],pawns[Co_Black])       ; pe.passed        [Co_Black] = BBTools::pawnPassed    <Co_Black>(pawns[Co_Black],pawns[Co_White]);
       pe.holes         [Co_White] = BBTools::pawnHoles     <Co_White>(pawns[Co_White]) & holesZone[Co_White] ; pe.holes         [Co_Black] = BBTools::pawnHoles     <Co_Black>(pawns[Co_Black]) & holesZone[Co_White];
       pe.openFiles =  BBTools::openFiles(pawns[Co_White], pawns[Co_Black]);

       // PST, attack
       evalPawn<Co_White>(p.pieces_const<P_wp>(Co_White),pe.score);
       evalPawn<Co_Black>(p.pieces_const<P_wp>(Co_Black),pe.score);
       
       // danger in king zone
       pe.danger[Co_White] -= countBit(pe.pawnTargets[Co_White] & kingZone[Co_White]) * EvalConfig::kingAttWeight[EvalConfig::katt_defence][0];
       pe.danger[Co_White] += countBit(pe.pawnTargets[Co_Black] & kingZone[Co_White]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack] [0];
       pe.danger[Co_Black] -= countBit(pe.pawnTargets[Co_Black] & kingZone[Co_Black]) * EvalConfig::kingAttWeight[EvalConfig::katt_defence][0];
       pe.danger[Co_Black] += countBit(pe.pawnTargets[Co_White] & kingZone[Co_Black]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack] [0];
       
       // pawn passer
       evalPawnPasser<Co_White>(p,pe.passed[Co_White],pe.score);
       evalPawnPasser<Co_Black>(p,pe.passed[Co_Black],pe.score);
       // pawn protected
       evalPawnProtected<Co_White>(pe.passed[Co_White] & pe.pawnTargets[Co_White],pe.score);
       evalPawnProtected<Co_Black>(pe.passed[Co_Black] & pe.pawnTargets[Co_Black],pe.score);
       // pawn candidate
       evalPawnCandidate<Co_White>(candidates[Co_White],pe.score);
       evalPawnCandidate<Co_Black>(candidates[Co_Black],pe.score);
       ///@todo hidden passed

       // bad pawns
       const BitBoard backwardOpenW  = backward[Co_White] &  semiOpenPawn[Co_White];
       const BitBoard backwardCloseW = backward[Co_White] & ~semiOpenPawn[Co_White];
       const BitBoard backwardOpenB  = backward[Co_Black] &  semiOpenPawn[Co_Black];
       const BitBoard backwardCloseB = backward[Co_Black] & ~semiOpenPawn[Co_Black];
       const BitBoard doubledOpenW   = doubled[Co_White]  &  semiOpenPawn[Co_White];
       const BitBoard doubledCloseW  = doubled[Co_White]  & ~semiOpenPawn[Co_White];
       const BitBoard doubledOpenB   = doubled[Co_Black]  &  semiOpenPawn[Co_Black];
       const BitBoard doubledCloseB  = doubled[Co_Black]  & ~semiOpenPawn[Co_Black];       
       const BitBoard isolatedOpenW  = isolated[Co_White] &  semiOpenPawn[Co_White];
       const BitBoard isolatedCloseW = isolated[Co_White] & ~semiOpenPawn[Co_White];
       const BitBoard isolatedOpenB  = isolated[Co_Black] &  semiOpenPawn[Co_Black];
       const BitBoard isolatedCloseB = isolated[Co_Black] & ~semiOpenPawn[Co_Black];   
       const BitBoard detachedOpenW  = detached[Co_White] &  semiOpenPawn[Co_White] & ~backward[Co_White];
       const BitBoard detachedCloseW = detached[Co_White] & ~semiOpenPawn[Co_White] & ~backward[Co_White];   
       const BitBoard detachedOpenB  = detached[Co_Black] &  semiOpenPawn[Co_Black] & ~backward[Co_Black];
       const BitBoard detachedCloseB = detached[Co_Black] & ~semiOpenPawn[Co_Black] & ~backward[Co_Black];   
       for (Rank r = Rank_1 ; r <= Rank_8 ; ++r){ // simply r2..r7 for classic chess will work
         // pawn backward  
         pe.score -= EvalConfig::backwardPawn[r][EvalConfig::Close]      * countBit(backwardCloseW & ranks[r]);
         pe.score -= EvalConfig::backwardPawn[r][EvalConfig::SemiOpen]   * countBit(backwardOpenW  & ranks[r]);
         // double pawn malus
         pe.score -= EvalConfig::doublePawn[r][EvalConfig::Close]        * countBit(doubledCloseW  & ranks[r]);
         pe.score -= EvalConfig::doublePawn[r][EvalConfig::SemiOpen]     * countBit(doubledOpenW   & ranks[r]);
         // isolated pawn malus
         pe.score -= EvalConfig::isolatedPawn[r][EvalConfig::Close]      * countBit(isolatedCloseW & ranks[r]);
         pe.score -= EvalConfig::isolatedPawn[r][EvalConfig::SemiOpen]   * countBit(isolatedOpenW  & ranks[r]);
         // detached pawn (not backward)
         pe.score -= EvalConfig::detachedPawn[r][EvalConfig::Close]      * countBit(detachedCloseW & ranks[r]);
         pe.score -= EvalConfig::detachedPawn[r][EvalConfig::SemiOpen]   * countBit(detachedOpenW  & ranks[r]);

         // pawn backward  
         pe.score += EvalConfig::backwardPawn[7-r][EvalConfig::Close]    * countBit(backwardCloseB & ranks[r]);
         pe.score += EvalConfig::backwardPawn[7-r][EvalConfig::SemiOpen] * countBit(backwardOpenB  & ranks[r]);
         // double pawn malus
         pe.score += EvalConfig::doublePawn[7-r][EvalConfig::Close]      * countBit(doubledCloseB  & ranks[r]);
         pe.score += EvalConfig::doublePawn[7-r][EvalConfig::SemiOpen]   * countBit(doubledOpenB   & ranks[r]);
         // isolated pawn malus
         pe.score += EvalConfig::isolatedPawn[7-r][EvalConfig::Close]    * countBit(isolatedCloseB & ranks[r]);
         pe.score += EvalConfig::isolatedPawn[7-r][EvalConfig::SemiOpen] * countBit(isolatedOpenB  & ranks[r]);       
         // detached pawn (not backward)
         pe.score += EvalConfig::detachedPawn[7-r][EvalConfig::Close]    * countBit(detachedCloseB & ranks[r]);
         pe.score += EvalConfig::detachedPawn[7-r][EvalConfig::SemiOpen] * countBit(detachedOpenB  & ranks[r]);
       }

       // pawn shield (PST and king troppism alone is not enough)
       const int pawnShieldW = countBit(kingShield[Co_White] & pawns[Co_White]);
       const int pawnShieldB = countBit(kingShield[Co_Black] & pawns[Co_Black]);
       pe.score += EvalConfig::pawnShieldBonus * std::min(pawnShieldW*pawnShieldW,9);
       pe.score -= EvalConfig::pawnShieldBonus * std::min(pawnShieldB*pawnShieldB,9);
       // malus for king on a pawnless flank
       const File wkf = (File)SQFILE(p.king[Co_White]);
       const File bkf = (File)SQFILE(p.king[Co_Black]);
       if (!(pawns[Co_White] & kingFlank[wkf])) pe.score += EvalConfig::pawnlessFlank;
       if (!(pawns[Co_Black] & kingFlank[bkf])) pe.score -= EvalConfig::pawnlessFlank;
       // pawn storm
       pe.score -= EvalConfig::pawnStormMalus * countBit(kingFlank[wkf] & (rank3|rank4) & pawns[Co_Black]);
       pe.score += EvalConfig::pawnStormMalus * countBit(kingFlank[bkf] & (rank5|rank6) & pawns[Co_White]);
       // open file near king
       pe.danger[Co_White] += EvalConfig::kingAttOpenfile        * countBit(kingFlank[wkf] & pe.openFiles              )/8;
       pe.danger[Co_White] += EvalConfig::kingAttSemiOpenfileOpp * countBit(kingFlank[wkf] & pe.semiOpenFiles[Co_White])/8;
       pe.danger[Co_White] += EvalConfig::kingAttSemiOpenfileOur * countBit(kingFlank[wkf] & pe.semiOpenFiles[Co_Black])/8;
       pe.danger[Co_Black] += EvalConfig::kingAttOpenfile        * countBit(kingFlank[bkf] & pe.openFiles              )/8;
       pe.danger[Co_Black] += EvalConfig::kingAttSemiOpenfileOpp * countBit(kingFlank[bkf] & pe.semiOpenFiles[Co_Black])/8;
       pe.danger[Co_Black] += EvalConfig::kingAttSemiOpenfileOur * countBit(kingFlank[bkf] & pe.semiOpenFiles[Co_White])/8;
       // Fawn
       pe.score -= EvalConfig::pawnFawnMalusKS * countBit((pawns[Co_White] & (BBSq_h2 | BBSq_g3)) | (pawns[Co_Black] & BBSq_h3) | (p.king[Co_White] & kingSide))/4;
       pe.score += EvalConfig::pawnFawnMalusKS * countBit((pawns[Co_Black] & (BBSq_h7 | BBSq_g6)) | (pawns[Co_White] & BBSq_h6) | (p.king[Co_Black] & kingSide))/4;
       pe.score -= EvalConfig::pawnFawnMalusQS * countBit((pawns[Co_White] & (BBSq_a2 | BBSq_b3)) | (pawns[Co_Black] & BBSq_a3) | (p.king[Co_White] & queenSide))/4;
       pe.score += EvalConfig::pawnFawnMalusQS * countBit((pawns[Co_Black] & (BBSq_a7 | BBSq_b6)) | (pawns[Co_White] & BBSq_a6) | (p.king[Co_Black] & queenSide))/4;

       ++context.stats.counters[Stats::sid_ttPawnInsert];
       pe.h = Hash64to32(computePHash(p)); // set the pawn entry
    }
    assert(pePtr);
    const Searcher::PawnEntry & pe = *pePtr;
    features.pawnStructScore += pe.score;

    // update global things with pawn entry stuff
    kdanger[Co_White] += pe.danger[Co_White];
    kdanger[Co_Black] += pe.danger[Co_Black];
    checkers[Co_White][0] = BBTools::pawnAttacks<Co_Black>(p.pieces_const<P_wk>(Co_Black)) & pawns[Co_White];
    checkers[Co_Black][0] = BBTools::pawnAttacks<Co_White>(p.pieces_const<P_wk>(Co_White)) & pawns[Co_Black];
    att2[Co_White] |= att[Co_White] & pe.pawnTargets[Co_White];
    att2[Co_Black] |= att[Co_Black] & pe.pawnTargets[Co_Black];
    att[Co_White]  |= pe.pawnTargets[Co_White];
    att[Co_Black]  |= pe.pawnTargets[Co_Black];

    STOP_AND_SUM_TIMER(Eval3)

    /*
    // lazy evaluation 2
    lazyScore = score.Score(p,data.gp); // scale both phase and 50 moves rule
    if ( std::abs(lazyScore) > 700){ // winning / losing position
        ScoreType ret = (white2Play?+1:-1)*lazyScore;
        STOP_AND_SUM_TIMER(Eval)
        return ret;
    }
    */

    const BitBoard nonPawnMat[2]               = {p.allPieces[Co_White] & ~pawns[Co_White] , p.allPieces[Co_Black] & ~pawns[Co_Black]};
    //const BitBoard attackedOrNotDefended[2]    = {att[Co_White]  | ~att[Co_Black]  , att[Co_Black]  | ~att[Co_White] };
    const BitBoard attackedAndNotDefended[2]   = {att[Co_White]  & ~att[Co_Black]  , att[Co_Black]  & ~att[Co_White] };
    const BitBoard attacked2AndNotDefended2[2] = {att2[Co_White] & ~att2[Co_Black] , att2[Co_Black] & ~att2[Co_White]};

    const BitBoard weakSquare[2]       = {att[Co_Black] & ~att2[Co_White] & (~att[Co_White] | attFromPiece[Co_White][P_wk-1] | attFromPiece[Co_White][P_wq-1]) , att[Co_White] & ~att2[Co_Black] & (~att[Co_Black] | attFromPiece[Co_Black][P_wk-1] | attFromPiece[Co_Black][P_wq-1])};
    const BitBoard safeSquare[2]       = {~att[Co_Black] | ( weakSquare[Co_Black] & att2[Co_White] ) , ~att[Co_White] | ( weakSquare[Co_White] & att2[Co_Black] ) };
    const BitBoard protectedSquare[2]  = {pe.pawnTargets[Co_White] | attackedAndNotDefended[Co_White] | attacked2AndNotDefended2[Co_White] , pe.pawnTargets[Co_Black] | attackedAndNotDefended[Co_Black] | attacked2AndNotDefended2[Co_Black] };

    // own piece in front of pawn
    features.developmentScore += EvalConfig::pieceFrontPawn * countBit( BBTools::shiftN<Co_White>(pawns[Co_White]) & nonPawnMat[Co_White] );
    features.developmentScore -= EvalConfig::pieceFrontPawn * countBit( BBTools::shiftN<Co_Black>(pawns[Co_Black]) & nonPawnMat[Co_Black] );

    // center control
    features.developmentScore += EvalConfig::centerControl * countBit(protectedSquare[Co_White] & extendedCenter);
    features.developmentScore -= EvalConfig::centerControl * countBit(protectedSquare[Co_Black] & extendedCenter);

    // pawn hole, unprotected
    features.positionalScore += EvalConfig::holesMalus * countBit(pe.holes[Co_White] & ~protectedSquare[Co_White]);
    features.positionalScore -= EvalConfig::holesMalus * countBit(pe.holes[Co_Black] & ~protectedSquare[Co_Black]);

    // free passer bonus
    evalPawnFreePasser<Co_White>(p,pe.passed[Co_White], features.pawnStructScore);
    evalPawnFreePasser<Co_Black>(p,pe.passed[Co_Black], features.pawnStructScore);

    // rook behind passed
    features.pawnStructScore += EvalConfig::rookBehindPassed * (countBit(p.pieces_const<P_wr>(Co_White) & BBTools::rearSpan<Co_White>(pe.passed[Co_White])) - countBit(p.pieces_const<P_wr>(Co_Black) & BBTools::rearSpan<Co_White>(pe.passed[Co_White])));
    features.pawnStructScore -= EvalConfig::rookBehindPassed * (countBit(p.pieces_const<P_wr>(Co_Black) & BBTools::rearSpan<Co_Black>(pe.passed[Co_Black])) - countBit(p.pieces_const<P_wr>(Co_White) & BBTools::rearSpan<Co_Black>(pe.passed[Co_Black])));

    // protected minor blocking openfile
    features.positionalScore += EvalConfig::minorOnOpenFile * countBit(pe.openFiles & (p.whiteBishop()|p.whiteKnight()) & pe.pawnTargets[Co_White]);
    features.positionalScore -= EvalConfig::minorOnOpenFile * countBit(pe.openFiles & (p.blackBishop()|p.blackKnight()) & pe.pawnTargets[Co_Black]);

    // knight on opponent hole, protected
    features.positionalScore += EvalConfig::outpost * countBit(pe.holes[Co_Black] & p.whiteKnight() & pe.pawnTargets[Co_White]);
    features.positionalScore -= EvalConfig::outpost * countBit(pe.holes[Co_White] & p.blackKnight() & pe.pawnTargets[Co_Black]);

    // knight far from both kings gets a penalty
    BitBoard knights = p.whiteKnight();
    while(knights){
        const Square knighSq = popBit(knights);
        features.positionalScore += EvalConfig::knightTooFar[std::min(chebyshevDistance(p.king[Co_White],knighSq),chebyshevDistance(p.king[Co_Black],knighSq))];
    }
    knights = p.blackKnight();
    while(knights){
        const Square knighSq = popBit(knights);
        features.positionalScore -= EvalConfig::knightTooFar[std::min(chebyshevDistance(p.king[Co_White],knighSq),chebyshevDistance(p.king[Co_Black],knighSq))];
    }

    // reward safe checks
    for (Piece pp = P_wp ; pp < P_wk ; ++pp) {
        kdanger[Co_White] += EvalConfig::kingAttSafeCheck[pp-1] * countBit( checkers[Co_Black][pp-1] & safeSquare[Co_White] );
        kdanger[Co_Black] += EvalConfig::kingAttSafeCheck[pp-1] * countBit( checkers[Co_White][pp-1] & safeSquare[Co_Black] );
    }

    // danger : use king danger score. **DO NOT** apply this in end-game
    features.attackScore[MG] -=  EvalConfig::kingAttTable[std::min(std::max(ScoreType(kdanger[Co_White]/32),ScoreType(0)),ScoreType(63))];
    features.attackScore[MG] +=  EvalConfig::kingAttTable[std::min(std::max(ScoreType(kdanger[Co_Black]/32),ScoreType(0)),ScoreType(63))];
    data.danger[Co_White] = kdanger[Co_White];
    data.danger[Co_Black] = kdanger[Co_Black];

    // number of hanging pieces (complexity ...)
    const BitBoard hanging[2] = {nonPawnMat[Co_White] & weakSquare[Co_White] , nonPawnMat[Co_Black] & weakSquare[Co_Black] };
    features.attackScore += EvalConfig::hangingPieceMalus * (countBit(hanging[Co_White]) - countBit(hanging[Co_Black]));

    // threats by minor
    BitBoard targetThreat = (nonPawnMat[Co_White] | (pawns[Co_White] & weakSquare[Co_White]) ) & (attFromPiece[Co_Black][P_wn-1] | attFromPiece[Co_Black][P_wb-1]);
    while (targetThreat) features.attackScore += EvalConfig::threatByMinor[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    targetThreat = (nonPawnMat[Co_Black] | (pawns[Co_Black] & weakSquare[Co_Black]) ) & (attFromPiece[Co_White][P_wn-1] | attFromPiece[Co_White][P_wb-1]);
    while (targetThreat) features.attackScore -= EvalConfig::threatByMinor[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    // threats by rook
    targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wr-1];
    while (targetThreat) features.attackScore += EvalConfig::threatByRook[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wr-1];
    while (targetThreat) features.attackScore -= EvalConfig::threatByRook[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    // threats by queen
    targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wq-1];
    while (targetThreat) features.attackScore += EvalConfig::threatByQueen[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wq-1];
    while (targetThreat) features.attackScore -= EvalConfig::threatByQueen[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    // threats by king
    targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wk-1];
    while (targetThreat) features.attackScore += EvalConfig::threatByKing[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wk-1];
    while (targetThreat) features.attackScore -= EvalConfig::threatByKing[PieceTools::getPieceType(p, popBit(targetThreat))-1];

    // threat by safe pawn
    const BitBoard safePawnAtt[2]  = {nonPawnMat[Co_Black] & BBTools::pawnAttacks<Co_White>(pawns[Co_White] & safeSquare[Co_White]), nonPawnMat[Co_White] & BBTools::pawnAttacks<Co_Black>(pawns[Co_Black] & safeSquare[Co_Black])};
    features.attackScore += EvalConfig::pawnSafeAtt * (countBit(safePawnAtt[Co_White]) - countBit(safePawnAtt[Co_Black]));

    // safe pawn push (protected once or not attacked)
    const BitBoard safePawnPush[2]  = {BBTools::shiftN<Co_White>(pawns[Co_White]) & ~p.occupancy() & safeSquare[Co_White], BBTools::shiftN<Co_Black>(pawns[Co_Black]) & ~p.occupancy() & safeSquare[Co_Black]};
    features.mobilityScore += EvalConfig::pawnMobility * (countBit(safePawnPush[Co_White]) - countBit(safePawnPush[Co_Black]));

    // threat by safe pawn push
    features.attackScore += EvalConfig::pawnSafePushAtt * (countBit(nonPawnMat[Co_Black] & BBTools::pawnAttacks<Co_White>(safePawnPush[Co_White])) - countBit(nonPawnMat[Co_White] & BBTools::pawnAttacks<Co_Black>(safePawnPush[Co_Black])));

    STOP_AND_SUM_TIMER(Eval4)

    // pieces mobility
    evalMob <P_wn,Co_White>(p,p.pieces_const<P_wn>(Co_White),features.mobilityScore,safeSquare[Co_White],data);
    evalMob <P_wb,Co_White>(p,p.pieces_const<P_wb>(Co_White),features.mobilityScore,safeSquare[Co_White],data);
    evalMob <P_wr,Co_White>(p,p.pieces_const<P_wr>(Co_White),features.mobilityScore,safeSquare[Co_White],data);
    evalMobQ<     Co_White>(p,p.pieces_const<P_wq>(Co_White),features.mobilityScore,safeSquare[Co_White],data);
    evalMobK<     Co_White>(p,p.pieces_const<P_wk>(Co_White),features.mobilityScore,~att[Co_Black]      ,data);
    evalMob <P_wn,Co_Black>(p,p.pieces_const<P_wn>(Co_Black),features.mobilityScore,safeSquare[Co_Black],data);
    evalMob <P_wb,Co_Black>(p,p.pieces_const<P_wb>(Co_Black),features.mobilityScore,safeSquare[Co_Black],data);
    evalMob <P_wr,Co_Black>(p,p.pieces_const<P_wr>(Co_Black),features.mobilityScore,safeSquare[Co_Black],data);
    evalMobQ<     Co_Black>(p,p.pieces_const<P_wq>(Co_Black),features.mobilityScore,safeSquare[Co_Black],data);
    evalMobK<     Co_Black>(p,p.pieces_const<P_wk>(Co_Black),features.mobilityScore,~att[Co_White]      ,data);

    STOP_AND_SUM_TIMER(Eval5)

    // rook on open file
    features.positionalScore += EvalConfig::rookOnOpenFile         * countBit(p.whiteRook() & pe.openFiles);
    features.positionalScore += EvalConfig::rookOnOpenSemiFileOur  * countBit(p.whiteRook() & pe.semiOpenFiles[Co_White]);
    features.positionalScore += EvalConfig::rookOnOpenSemiFileOpp  * countBit(p.whiteRook() & pe.semiOpenFiles[Co_Black]);
    features.positionalScore -= EvalConfig::rookOnOpenFile         * countBit(p.blackRook() & pe.openFiles);
    features.positionalScore -= EvalConfig::rookOnOpenSemiFileOur  * countBit(p.blackRook() & pe.semiOpenFiles[Co_Black]);
    features.positionalScore -= EvalConfig::rookOnOpenSemiFileOpp  * countBit(p.blackRook() & pe.semiOpenFiles[Co_White]);

    // enemy rook facing king
    features.positionalScore += EvalConfig::rookFrontKingMalus * countBit(BBTools::frontSpan<Co_White>(p.whiteKing()) & p.blackRook());
    features.positionalScore -= EvalConfig::rookFrontKingMalus * countBit(BBTools::frontSpan<Co_Black>(p.blackKing()) & p.whiteRook());

    // enemy rook facing queen
    features.positionalScore += EvalConfig::rookFrontQueenMalus * countBit(BBTools::frontSpan<Co_White>(p.whiteQueen()) & p.blackRook());
    features.positionalScore -= EvalConfig::rookFrontQueenMalus * countBit(BBTools::frontSpan<Co_Black>(p.blackQueen()) & p.whiteRook());

    // queen aligned with own rook
    features.positionalScore += EvalConfig::rookQueenSameFile * countBit(BBTools::fillFile(p.whiteQueen()) & p.whiteRook());
    features.positionalScore -= EvalConfig::rookQueenSameFile * countBit(BBTools::fillFile(p.blackQueen()) & p.blackRook());

    const Square whiteQueenSquare = p.whiteQueen() ? BBTools::SquareFromBitBoard(p.whiteQueen()) : INVALIDSQUARE;
    const Square blackQueenSquare = p.blackQueen() ? BBTools::SquareFromBitBoard(p.blackQueen()) : INVALIDSQUARE;

    // pins on king and queen
    const BitBoard pinnedK [2] = { getPinned<Co_White>(p,p.king[Co_White]), getPinned<Co_Black>(p,p.king[Co_Black]) };
    const BitBoard pinnedQ [2] = { getPinned<Co_White>(p,whiteQueenSquare), getPinned<Co_Black>(p,blackQueenSquare) };
    for (Piece pp = P_wp ; pp < P_wk ; ++pp) {
        const BitBoard bw = p.pieces_const(Co_White, pp);
        if (bw) {
            if (pinnedK[Co_White] & bw) features.attackScore -= EvalConfig::pinnedKing [pp - 1] * countBit(pinnedK[Co_White] & bw);
            if (pinnedQ[Co_White] & bw) features.attackScore -= EvalConfig::pinnedQueen[pp - 1] * countBit(pinnedQ[Co_White] & bw);
        }
        const BitBoard bb = p.pieces_const(Co_Black, pp);
        if (bb) {
            if (pinnedK[Co_Black] & bb) features.attackScore += EvalConfig::pinnedKing [pp - 1] * countBit(pinnedK[Co_Black] & bb);
            if (pinnedQ[Co_Black] & bb) features.attackScore += EvalConfig::pinnedQueen[pp - 1] * countBit(pinnedQ[Co_Black] & bb);
        }
    }

    // attack : queen distance to opponent king (wrong if multiple queens ...)
    if ( blackQueenSquare != INVALIDSQUARE ) features.positionalScore -= EvalConfig::queenNearKing * (7 - chebyshevDistance(p.king[Co_White], blackQueenSquare) );
    if ( whiteQueenSquare != INVALIDSQUARE ) features.positionalScore += EvalConfig::queenNearKing * (7 - chebyshevDistance(p.king[Co_Black], whiteQueenSquare) );

    // number of pawn and piece type value  ///@todo closedness instead ?
    features.materialScore += EvalConfig::adjRook  [p.mat[Co_White][M_p]] * ScoreType(p.mat[Co_White][M_r]);
    features.materialScore -= EvalConfig::adjRook  [p.mat[Co_Black][M_p]] * ScoreType(p.mat[Co_Black][M_r]);
    features.materialScore += EvalConfig::adjKnight[p.mat[Co_White][M_p]] * ScoreType(p.mat[Co_White][M_n]);
    features.materialScore -= EvalConfig::adjKnight[p.mat[Co_Black][M_p]] * ScoreType(p.mat[Co_Black][M_n]);

    // bad bishop
    if (p.whiteBishop() & whiteSquare) features.materialScore -= EvalConfig::badBishop[countBit(pawns[Co_White] & whiteSquare)];
    if (p.whiteBishop() & blackSquare) features.materialScore -= EvalConfig::badBishop[countBit(pawns[Co_White] & blackSquare)];
    if (p.blackBishop() & whiteSquare) features.materialScore += EvalConfig::badBishop[countBit(pawns[Co_Black] & whiteSquare)];
    if (p.blackBishop() & blackSquare) features.materialScore += EvalConfig::badBishop[countBit(pawns[Co_Black] & blackSquare)];

    // adjust piece pair score
    features.materialScore += ( (p.mat[Co_White][M_b] > 1 ? EvalConfig::bishopPairBonus[p.mat[Co_White][M_p]] : 0)-(p.mat[Co_Black][M_b] > 1 ? EvalConfig::bishopPairBonus[p.mat[Co_Black][M_p]] : 0) );
    features.materialScore += ( (p.mat[Co_White][M_n] > 1 ? EvalConfig::knightPairMalus : 0)-(p.mat[Co_Black][M_n] > 1 ? EvalConfig::knightPairMalus : 0) );
    features.materialScore += ( (p.mat[Co_White][M_r] > 1 ? EvalConfig::rookPairMalus   : 0)-(p.mat[Co_Black][M_r] > 1 ? EvalConfig::rookPairMalus   : 0) );

    if ( display ) displayEval(data,features);

    // Compute various Shashin ratio of current position (may be used later in search)
    data.shashinMaterialFactor = std::max(0.f, 1.f- SQR(float(features.materialScore[MG])/EvalConfig::shashinThreshold));
    data.shashinMobRatio       = std::max(0.5f,std::min(2.f,float(data.mobility[p.c]) / std::max(0.5f,float(data.mobility[~p.c]))));
    // data.shashinForwardness inside [ 8..120 ]
    ///@todo compacity ??    
    // Apply Shashin corrections
    EvalConfig::applyShashinCorrection(p,data,features);

    // Sum everything
    EvalScore score = features.materialScore + features.positionalScore + features.developmentScore + features.mobilityScore + features.pawnStructScore + features.attackScore;

    if ( display ){
        Logging::LogIt(Logging::logInfo) << "Post correction ... ";
        displayEval(data,features);
        Logging::LogIt(Logging::logInfo) << "> All (before scaling ) " << score;
    }

    // initiative
    EvalScore initiativeBonus = EvalConfig::initiative[0] * countBit(allPawns) + EvalConfig::initiative[1] * ((allPawns & queenSide) && (allPawns & kingSide)) + EvalConfig::initiative[2] * (countBit(p.occupancy() & ~allPawns) == 2) - EvalConfig::initiative[3];
    initiativeBonus[MG] = sgn(score[MG]) * std::max(initiativeBonus[MG], ScoreType(-std::abs(score[MG])));
    initiativeBonus[EG] = sgn(score[EG]) * std::max(initiativeBonus[EG], ScoreType(-std::abs(score[EG])));
    score += initiativeBonus;
    if ( display ){
        Logging::LogIt(Logging::logInfo) << "Initiative    " << initiativeBonus;
        Logging::LogIt(Logging::logInfo) << "> All (before initiative) " << score;
    }

    ///@todo complexity

    // tempo
    score += EvalConfig::tempo*(white2Play?+1:-1);
    if ( display ){
        Logging::LogIt(Logging::logInfo) << "> All (with tempo) " << score;
    }

    // contempt
    score += context.contempt;
    if ( display ){
        Logging::LogIt(Logging::logInfo) << "> All (with contempt) " << score;
    }

    // scale score
    ScoreType ret = (white2Play?+1:-1)*Score(score,features.scalingFactor,p,data.gp); // scale both phase and 50 moves rule
    if ( display ){
        Logging::LogIt(Logging::logInfo) << "==> All (fully scaled) " << score;
    }

    STOP_AND_SUM_TIMER(Eval)
    return ret;
}
