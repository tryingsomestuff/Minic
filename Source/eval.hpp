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
   template<Color C> inline Square ColorSquarePstHelper(Square k)          { return C==Co_White?(k^56):k;}
   template<Color C> inline constexpr ScoreType ColorSignHelper()          { return C==Co_White?+1:-1;}
   template<Color C> inline const Square PromotionSquare(const Square k)   { return C==Co_White? (SQFILE(k) + 56) : SQFILE(k);}
   template<Color C> inline const Rank ColorRank(const Square k)           { return Rank(C==Co_White? SQRANK(k) : (7-SQRANK(k)));}
   template<Color C> inline bool isBackward(const Position &p, Square k, const BitBoard pAtt[2], const BitBoard pAttSpan[2]){ return ((BBTools::shiftN<C>(SquareToBitboard(k))&~p.pieces_const<P_wp>(~C)) & pAtt[~C] & ~pAttSpan[C]) != empty; }
   template<Piece T> inline BitBoard alignedThreatPieceSlider(const Position & p, Color C);
   template<> inline BitBoard alignedThreatPieceSlider<P_wn>(const Position & p, Color C){ return empty;}
   template<> inline BitBoard alignedThreatPieceSlider<P_wb>(const Position & p, Color C){ return p.pieces_const<P_wb>(C) | p.pieces_const<P_wq>(C) /*| p.pieces_const<P_wn>(C)*/;}
   template<> inline BitBoard alignedThreatPieceSlider<P_wr>(const Position & p, Color C){ return p.pieces_const<P_wr>(C) | p.pieces_const<P_wq>(C) /*| p.pieces_const<P_wn>(C)*/;}
   template<> inline BitBoard alignedThreatPieceSlider<P_wq>(const Position & p, Color C){ return p.pieces_const<P_wb>(C) | p.pieces_const<P_wr>(C) /*| p.pieces_const<P_wn>(C)*/;} ///@todo this is false ...
   template<> inline BitBoard alignedThreatPieceSlider<P_wk>(const Position & p, Color C){ return empty;}
}

template < Piece T , Color C>
inline void evalPiece(const Position & p, BitBoard pieceBBiterator, const BitBoard (& kingZone)[2], EvalScore & score, BitBoard & attBy, BitBoard & att, BitBoard & att2, ScoreType (& kdanger)[2], BitBoard & checkers){
    while (pieceBBiterator) {
        const Square k = popBit(pieceBBiterator);
        const Square kk = ColorSquarePstHelper<C>(k);
        score += EvalConfig::PST[T-1][kk] * ColorSignHelper<C>();
        const BitBoard target = BBTools::pfCoverage[T-1](k, p.occupancy, C); // real targets
        if ( target ){
           attBy |= target;
           att2  |= att & target;
           att   |= target;
           if ( target & p.pieces_const<P_wk>(~C) ) checkers |= SquareToBitboard(k);
        }
        const BitBoard shadowTarget = BBTools::pfCoverage[T-1](k, p.occupancy ^ /*p.pieces<T>(C)*/ alignedThreatPieceSlider<T>(p,C), C); // aligned threats of same piece type also taken into account and knight in front also removed ///@todo better?
        if ( shadowTarget ){
           kdanger[C]  -= countBit(shadowTarget & kingZone[C])  * EvalConfig::kingAttWeight[EvalConfig::katt_defence][T-1];
           kdanger[~C] += countBit(shadowTarget & kingZone[~C]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack][T-1];
        }
    }
}
///@todo special version of evalPiece for king ??

template < Piece T ,Color C>
inline void evalMob(const Position & p, BitBoard pieceBBiterator, ScoreAcc & score, const BitBoard safe){
    while (pieceBBiterator){
        const BitBoard mob = BBTools::pfCoverage[T-1](popBit(pieceBBiterator), p.occupancy, C) & ~p.allPieces[C] & safe;
        score[sc_MOB] += EvalConfig::MOB[T-2][countBit(mob)]*ColorSignHelper<C>();
    }
}

template < Color C >
inline void evalMobQ(const Position & p, BitBoard pieceBBiterator, ScoreAcc & score, const BitBoard safe){
    while (pieceBBiterator){
        const Square s = popBit(pieceBBiterator);
        BitBoard mob = BBTools::pfCoverage[P_wb-1](s, p.occupancy, C) & ~p.allPieces[C] & safe;
        score[sc_MOB] += EvalConfig::MOB[3][countBit(mob)]*ColorSignHelper<C>();
        mob = BBTools::pfCoverage[P_wr-1](s, p.occupancy, C) & ~p.allPieces[C] & safe;
        score[sc_MOB] += EvalConfig::MOB[4][countBit(mob)]*ColorSignHelper<C>();
    }
}

template < Color C>
inline void evalMobK(const Position & p, BitBoard pieceBBiterator, ScoreAcc & score, const BitBoard safe){
    while (pieceBBiterator){
        const BitBoard mob = BBTools::pfCoverage[P_wk-1](popBit(pieceBBiterator), p.occupancy, C) & ~p.allPieces[C] & safe;
        score[sc_MOB] += EvalConfig::MOB[5][countBit(mob)]*ColorSignHelper<C>();
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
    ScoreAcc score;

    // king captured
    const bool white2Play = p.c == Co_White;
    if ( p.king[Co_White] == INVALIDSQUARE ){
      STOP_AND_SUM_TIMER(Eval)
      return data.gp=0,(white2Play?-1:+1)* MATE;
    }
    if ( p.king[Co_Black] == INVALIDSQUARE ) {
      STOP_AND_SUM_TIMER(Eval)
      return data.gp=0,(white2Play?+1:-1)* MATE;
    }

    // level for the poor ...
    const int lra = std::max(0, 500 - int(10*DynamicConfig::level));
    if ( lra > 0 ) { score[sc_Rand] += Zobrist::randomInt<int>(-lra,lra); }

    context.prefetchPawn(computeHash(p));

    // Material evaluation
    const Hash matHash = MaterialHash::getMaterialHash(p.mat);
    if ( matHash ){
       ++context.stats.counters[Stats::sid_materialTableHits];
       // Hash data
       const MaterialHash::MaterialHashEntry & MEntry = MaterialHash::materialHashTable[matHash];
       data.gp = MEntry.gp;
       score[sc_Mat][EG] += MEntry.score[EG];
       score[sc_Mat][MG] += MEntry.score[MG];
       // end game knowledge (helper or scaling)
       if ( safeMatEvaluator && (p.mat[Co_White][M_t]+p.mat[Co_Black][M_t]<6) ){
          const Color winningSideEG = score[sc_Mat][EG]>0?Co_White:Co_Black;
          if      ( MEntry.t == MaterialHash::Ter_WhiteWinWithHelper || MEntry.t == MaterialHash::Ter_BlackWinWithHelper ){
            STOP_AND_SUM_TIMER(Eval)
            return (white2Play?+1:-1)*(MaterialHash::helperTable[matHash](p,winningSideEG,score[sc_Mat][EG]));
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
          else if ( MEntry.t == MaterialHash::Ter_WhiteWin || MEntry.t == MaterialHash::Ter_BlackWin) score.scalingFactor = 5 - 5*p.fifty/100.f;
          else if ( MEntry.t == MaterialHash::Ter_HardToWin)  score.scalingFactor = 0.5f - 0.5f*(p.fifty/100.f);
          else if ( MEntry.t == MaterialHash::Ter_LikelyDraw) score.scalingFactor = 0.3f - 0.3f*(p.fifty/100.f);
       }
    }
    else{ // game phase and material scores out of table
       ScoreType matScoreW = 0;
       ScoreType matScoreB = 0;
       data.gp = gamePhase(p,matScoreW, matScoreB);
       score[sc_Mat][EG] += (p.mat[Co_White][M_q] - p.mat[Co_Black][M_q]) * *absValuesEG[P_wq] + (p.mat[Co_White][M_r] - p.mat[Co_Black][M_r]) * *absValuesEG[P_wr] + (p.mat[Co_White][M_b] - p.mat[Co_Black][M_b]) * *absValuesEG[P_wb] + (p.mat[Co_White][M_n] - p.mat[Co_Black][M_n]) * *absValuesEG[P_wn] + (p.mat[Co_White][M_p] - p.mat[Co_Black][M_p]) * *absValuesEG[P_wp];
       score[sc_Mat][MG] += matScoreW - matScoreB;
       ++context.stats.counters[Stats::sid_materialTableMiss];
    }

#ifdef WITH_TEXEL_TUNING
    score[sc_Mat] += MaterialHash::Imbalance(p.mat, Co_White) - MaterialHash::Imbalance(p.mat, Co_Black);
#endif

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
    evalPiece<P_wn,Co_White>(p,p.pieces_const<P_wn>(Co_White),kingZone,score[sc_PST],attFromPiece[Co_White][P_wn-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wn-1]);
    evalPiece<P_wb,Co_White>(p,p.pieces_const<P_wb>(Co_White),kingZone,score[sc_PST],attFromPiece[Co_White][P_wb-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wb-1]);
    evalPiece<P_wr,Co_White>(p,p.pieces_const<P_wr>(Co_White),kingZone,score[sc_PST],attFromPiece[Co_White][P_wr-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wr-1]);
    evalPiece<P_wq,Co_White>(p,p.pieces_const<P_wq>(Co_White),kingZone,score[sc_PST],attFromPiece[Co_White][P_wq-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wq-1]);
    evalPiece<P_wk,Co_White>(p,p.pieces_const<P_wk>(Co_White),kingZone,score[sc_PST],attFromPiece[Co_White][P_wk-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wk-1]);
    evalPiece<P_wn,Co_Black>(p,p.pieces_const<P_wn>(Co_Black),kingZone,score[sc_PST],attFromPiece[Co_Black][P_wn-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wn-1]);
    evalPiece<P_wb,Co_Black>(p,p.pieces_const<P_wb>(Co_Black),kingZone,score[sc_PST],attFromPiece[Co_Black][P_wb-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wb-1]);
    evalPiece<P_wr,Co_Black>(p,p.pieces_const<P_wr>(Co_Black),kingZone,score[sc_PST],attFromPiece[Co_Black][P_wr-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wr-1]);
    evalPiece<P_wq,Co_Black>(p,p.pieces_const<P_wq>(Co_Black),kingZone,score[sc_PST],attFromPiece[Co_Black][P_wq-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wq-1]);
    evalPiece<P_wk,Co_Black>(p,p.pieces_const<P_wk>(Co_Black),kingZone,score[sc_PST],attFromPiece[Co_Black][P_wk-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wk-1]);

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
       // pawn backward
       pe.score -= EvalConfig::backwardPawnMalus[EvalConfig::Close]    * countBit(backward[Co_White] & ~semiOpenPawn[Co_White]);
       pe.score -= EvalConfig::backwardPawnMalus[EvalConfig::SemiOpen] * countBit(backward[Co_White] &  semiOpenPawn[Co_White]);
       pe.score += EvalConfig::backwardPawnMalus[EvalConfig::Close]    * countBit(backward[Co_Black] & ~semiOpenPawn[Co_Black]);
       pe.score += EvalConfig::backwardPawnMalus[EvalConfig::SemiOpen] * countBit(backward[Co_Black] &  semiOpenPawn[Co_Black]);
       // double pawn malus
       pe.score -= EvalConfig::doublePawnMalus[EvalConfig::Close]      * countBit(doubled[Co_White]  & ~semiOpenPawn[Co_White]);
       pe.score -= EvalConfig::doublePawnMalus[EvalConfig::SemiOpen]   * countBit(doubled[Co_White]  &  semiOpenPawn[Co_White]);
       pe.score += EvalConfig::doublePawnMalus[EvalConfig::Close]      * countBit(doubled[Co_Black]  & ~semiOpenPawn[Co_Black]);
       pe.score += EvalConfig::doublePawnMalus[EvalConfig::SemiOpen]   * countBit(doubled[Co_Black]  &  semiOpenPawn[Co_Black]);
       // isolated pawn malus
       pe.score -= EvalConfig::isolatedPawnMalus[EvalConfig::Close]    * countBit(isolated[Co_White] & ~semiOpenPawn[Co_White]);
       pe.score -= EvalConfig::isolatedPawnMalus[EvalConfig::SemiOpen] * countBit(isolated[Co_White] &  semiOpenPawn[Co_White]);
       pe.score += EvalConfig::isolatedPawnMalus[EvalConfig::Close]    * countBit(isolated[Co_Black] & ~semiOpenPawn[Co_Black]);
       pe.score += EvalConfig::isolatedPawnMalus[EvalConfig::SemiOpen] * countBit(isolated[Co_Black] &  semiOpenPawn[Co_Black]);
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

       ++context.stats.counters[Stats::sid_ttPawnInsert];
       pe.h = Hash64to32(computePHash(p)); // set the pawn entry
    }
    assert(pePtr);
    const Searcher::PawnEntry & pe = *pePtr;
    score[sc_PawnTT] += pe.score;
    // update global things with pawn entry stuff
    kdanger[Co_White] += pe.danger[Co_White];
    kdanger[Co_Black] += pe.danger[Co_Black];
    checkers[Co_White][0] = BBTools::pawnAttacks<Co_Black>(p.pieces_const<P_wk>(Co_Black)) & pawns[Co_White];
    checkers[Co_Black][0] = BBTools::pawnAttacks<Co_White>(p.pieces_const<P_wk>(Co_White)) & pawns[Co_Black];
    att2[Co_White] |= att[Co_White] & pe.pawnTargets[Co_White];
    att2[Co_Black] |= att[Co_Black] & pe.pawnTargets[Co_Black];
    att[Co_White]  |= pe.pawnTargets[Co_White];
    att[Co_Black]  |= pe.pawnTargets[Co_Black];

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
    score[sc_PieceBlockPawn] += EvalConfig::pieceFrontPawn * countBit( BBTools::shiftN<Co_White>(pawns[Co_White]) & nonPawnMat[Co_White] );
    score[sc_PieceBlockPawn] -= EvalConfig::pieceFrontPawn * countBit( BBTools::shiftN<Co_Black>(pawns[Co_Black]) & nonPawnMat[Co_Black] );

    // center control
    score[sc_Center] += EvalConfig::centerControl * countBit(protectedSquare[Co_White] & extendedCenter);
    score[sc_Center] -= EvalConfig::centerControl * countBit(protectedSquare[Co_Black] & extendedCenter);

    // pawn hole, unprotected
    score[sc_Holes] += EvalConfig::holesMalus * countBit(pe.holes[Co_White] & ~protectedSquare[Co_White]);
    score[sc_Holes] -= EvalConfig::holesMalus * countBit(pe.holes[Co_Black] & ~protectedSquare[Co_Black]);

    // free passer bonus
    evalPawnFreePasser<Co_White>(p,pe.passed[Co_White], score[sc_FreePasser]);
    evalPawnFreePasser<Co_Black>(p,pe.passed[Co_Black], score[sc_FreePasser]);

    // rook behind passed
    score[sc_RookBehindPassed] += EvalConfig::rookBehindPassed * (countBit(p.pieces_const<P_wr>(Co_White) & BBTools::rearSpan<Co_White>(pe.passed[Co_White])) - countBit(p.pieces_const<P_wr>(Co_Black) & BBTools::rearSpan<Co_White>(pe.passed[Co_White])));
    score[sc_RookBehindPassed] -= EvalConfig::rookBehindPassed * (countBit(p.pieces_const<P_wr>(Co_Black) & BBTools::rearSpan<Co_Black>(pe.passed[Co_Black])) - countBit(p.pieces_const<P_wr>(Co_White) & BBTools::rearSpan<Co_Black>(pe.passed[Co_Black])));

    // protected minor blocking openfile
    score[sc_MinorOnOpenFile] += EvalConfig::minorOnOpenFile * countBit(pe.openFiles & (p.whiteBishop()|p.whiteKnight()) & pe.pawnTargets[Co_White]);
    score[sc_MinorOnOpenFile] -= EvalConfig::minorOnOpenFile * countBit(pe.openFiles & (p.blackBishop()|p.blackKnight()) & pe.pawnTargets[Co_Black]);

    // knight on opponent hole, protected
    score[sc_Outpost] += EvalConfig::outpost * countBit(pe.holes[Co_Black] & p.whiteKnight() & pe.pawnTargets[Co_White]);
    score[sc_Outpost] -= EvalConfig::outpost * countBit(pe.holes[Co_White] & p.blackKnight() & pe.pawnTargets[Co_Black]);

    // reward safe checks
    for (Piece pp = P_wp ; pp < P_wk ; ++pp) {
        kdanger[Co_White] += EvalConfig::kingAttSafeCheck[pp-1] * countBit( checkers[Co_Black][pp-1] & safeSquare[Co_White] );
        kdanger[Co_Black] += EvalConfig::kingAttSafeCheck[pp-1] * countBit( checkers[Co_White][pp-1] & safeSquare[Co_Black] );
    }

    // danger : use king danger score. **DO NOT** apply this in end-game
    score[sc_ATT][MG] -=  EvalConfig::kingAttTable[std::min(std::max(ScoreType(kdanger[Co_White]/32),ScoreType(0)),ScoreType(63))];
    score[sc_ATT][MG] +=  EvalConfig::kingAttTable[std::min(std::max(ScoreType(kdanger[Co_Black]/32),ScoreType(0)),ScoreType(63))];
    data.danger[Co_White] = kdanger[Co_White];
    data.danger[Co_Black] = kdanger[Co_Black];

    // number of hanging pieces (complexity ...)
    const BitBoard hanging[2] = {nonPawnMat[Co_White] & weakSquare[Co_White] , nonPawnMat[Co_Black] & weakSquare[Co_Black] };
    score[sc_Hanging] += EvalConfig::hangingPieceMalus * (countBit(hanging[Co_White]) - countBit(hanging[Co_Black]));

    // threats by minor
    BitBoard targetThreat = (nonPawnMat[Co_White] | (pawns[Co_White] & weakSquare[Co_White]) ) & (attFromPiece[Co_Black][P_wn-1] | attFromPiece[Co_Black][P_wb-1]);
    while (targetThreat) score[sc_Threat] += EvalConfig::threatByMinor[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    targetThreat = (nonPawnMat[Co_Black] | (pawns[Co_Black] & weakSquare[Co_Black]) ) & (attFromPiece[Co_White][P_wn-1] | attFromPiece[Co_White][P_wb-1]);
    while (targetThreat) score[sc_Threat] -= EvalConfig::threatByMinor[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    // threats by rook
    targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wr-1];
    while (targetThreat) score[sc_Threat] += EvalConfig::threatByRook[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wr-1];
    while (targetThreat) score[sc_Threat] -= EvalConfig::threatByRook[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    // threats by queen
    targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wq-1];
    while (targetThreat) score[sc_Threat] += EvalConfig::threatByQueen[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wq-1];
    while (targetThreat) score[sc_Threat] -= EvalConfig::threatByQueen[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    // threats by king
    targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wk-1];
    while (targetThreat) score[sc_Threat] += EvalConfig::threatByKing[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wk-1];
    while (targetThreat) score[sc_Threat] -= EvalConfig::threatByKing[PieceTools::getPieceType(p, popBit(targetThreat))-1];

    // threat by safe pawn
    const BitBoard safePawnAtt[2]  = {nonPawnMat[Co_Black] & BBTools::pawnAttacks<Co_White>(pawns[Co_White] & safeSquare[Co_White]), nonPawnMat[Co_White] & BBTools::pawnAttacks<Co_Black>(pawns[Co_Black] & safeSquare[Co_Black])};
    score[sc_PwnSafeAtt] += EvalConfig::pawnSafeAtt * (countBit(safePawnAtt[Co_White]) - countBit(safePawnAtt[Co_Black]));

    // safe pawn push (protected once or not attacked)
    const BitBoard safePawnPush[2]  = {BBTools::shiftN<Co_White>(pawns[Co_White]) & ~p.occupancy & safeSquare[Co_White], BBTools::shiftN<Co_Black>(pawns[Co_Black]) & ~p.occupancy & safeSquare[Co_Black]};
    score[sc_PwnPush] += EvalConfig::pawnMobility * (countBit(safePawnPush[Co_White]) - countBit(safePawnPush[Co_Black]));

    // threat by safe pawn push
    score[sc_PwnPushAtt] += EvalConfig::pawnSafePushAtt * (countBit(nonPawnMat[Co_Black] & BBTools::pawnAttacks<Co_White>(safePawnPush[Co_White])) - countBit(nonPawnMat[Co_White] & BBTools::pawnAttacks<Co_Black>(safePawnPush[Co_Black])));

    // pieces mobility
    evalMob <P_wn,Co_White>(p,p.pieces_const<P_wn>(Co_White),score,safeSquare[Co_White]);
    evalMob <P_wb,Co_White>(p,p.pieces_const<P_wb>(Co_White),score,safeSquare[Co_White]);
    evalMob <P_wr,Co_White>(p,p.pieces_const<P_wr>(Co_White),score,safeSquare[Co_White]);
    evalMobQ<     Co_White>(p,p.pieces_const<P_wq>(Co_White),score,safeSquare[Co_White]);
    evalMobK<     Co_White>(p,p.pieces_const<P_wk>(Co_White),score,~att[Co_Black]);
    evalMob <P_wn,Co_Black>(p,p.pieces_const<P_wn>(Co_Black),score,safeSquare[Co_Black]);
    evalMob <P_wb,Co_Black>(p,p.pieces_const<P_wb>(Co_Black),score,safeSquare[Co_Black]);
    evalMob <P_wr,Co_Black>(p,p.pieces_const<P_wr>(Co_Black),score,safeSquare[Co_Black]);
    evalMobQ<     Co_Black>(p,p.pieces_const<P_wq>(Co_Black),score,safeSquare[Co_Black]);
    evalMobK<     Co_Black>(p,p.pieces_const<P_wk>(Co_Black),score,~att[Co_White]);

    // rook on open file
    score[sc_OpenFile] += EvalConfig::rookOnOpenFile         * countBit(p.whiteRook() & pe.openFiles);
    score[sc_OpenFile] += EvalConfig::rookOnOpenSemiFileOur  * countBit(p.whiteRook() & pe.semiOpenFiles[Co_White]);
    score[sc_OpenFile] += EvalConfig::rookOnOpenSemiFileOpp  * countBit(p.whiteRook() & pe.semiOpenFiles[Co_Black]);
    score[sc_OpenFile] -= EvalConfig::rookOnOpenFile         * countBit(p.blackRook() & pe.openFiles);
    score[sc_OpenFile] -= EvalConfig::rookOnOpenSemiFileOur  * countBit(p.blackRook() & pe.semiOpenFiles[Co_Black]);
    score[sc_OpenFile] -= EvalConfig::rookOnOpenSemiFileOpp  * countBit(p.blackRook() & pe.semiOpenFiles[Co_White]);

    // enemy rook facing king
    score[sc_RookFrontKing] += EvalConfig::rookFrontKingMalus * countBit(BBTools::frontSpan<Co_White>(p.whiteKing()) & p.blackRook());
    score[sc_RookFrontKing] -= EvalConfig::rookFrontKingMalus * countBit(BBTools::frontSpan<Co_Black>(p.blackKing()) & p.whiteRook());

    // enemy rook facing queen
    score[sc_RookFrontQueen] += EvalConfig::rookFrontQueenMalus * countBit(BBTools::frontSpan<Co_White>(p.whiteQueen()) & p.blackRook());
    score[sc_RookFrontQueen] -= EvalConfig::rookFrontQueenMalus * countBit(BBTools::frontSpan<Co_Black>(p.blackQueen()) & p.whiteRook());

    // queen aligned with own rook
    score[sc_RookQueenSameFile] += EvalConfig::rookQueenSameFile * countBit(BBTools::fillFile(p.whiteQueen()) & p.whiteRook());
    score[sc_RookQueenSameFile] -= EvalConfig::rookQueenSameFile * countBit(BBTools::fillFile(p.blackQueen()) & p.blackRook());

    const Square whiteQueenSquare = p.whiteQueen() ? BBTools::SquareFromBitBoard(p.whiteQueen()) : INVALIDSQUARE;
    const Square blackQueenSquare = p.blackQueen() ? BBTools::SquareFromBitBoard(p.blackQueen()) : INVALIDSQUARE;

    // pins on king and queen
    const BitBoard pinnedK [2] = { getPinned<Co_White>(p,p.king[Co_White]), getPinned<Co_Black>(p,p.king[Co_Black]) };
    const BitBoard pinnedQ [2] = { getPinned<Co_White>(p,whiteQueenSquare), getPinned<Co_Black>(p,blackQueenSquare) };
    for (Piece pp = P_wp ; pp < P_wk ; ++pp) {
        const BitBoard bw = p.pieces_const(Co_White, pp);
        if (bw) {
            if (pinnedK[Co_White] & bw) score[sc_PinsK] -= EvalConfig::pinnedKing [pp - 1] * countBit(pinnedK[Co_White] & bw);
            if (pinnedQ[Co_White] & bw) score[sc_PinsQ] -= EvalConfig::pinnedQueen[pp - 1] * countBit(pinnedQ[Co_White] & bw);
        }
        const BitBoard bb = p.pieces_const(Co_Black, pp);
        if (bb) {
            if (pinnedK[Co_Black] & bb) score[sc_PinsK] += EvalConfig::pinnedKing [pp - 1] * countBit(pinnedK[Co_Black] & bb);
            if (pinnedQ[Co_Black] & bb) score[sc_PinsQ] += EvalConfig::pinnedQueen[pp - 1] * countBit(pinnedQ[Co_Black] & bb);
        }
    }

    // attack : queen distance to opponent king (wrong if multiple queens ...)
    if ( blackQueenSquare != INVALIDSQUARE ) score[sc_QueenNearKing] -= EvalConfig::queenNearKing * (7 - chebyshevDistance(p.king[Co_White], blackQueenSquare) );
    if ( whiteQueenSquare != INVALIDSQUARE ) score[sc_QueenNearKing] += EvalConfig::queenNearKing * (7 - chebyshevDistance(p.king[Co_Black], whiteQueenSquare) );

    // number of pawn and piece type value
    score[sc_Adjust] += EvalConfig::adjRook  [p.mat[Co_White][M_p]] * ScoreType(p.mat[Co_White][M_r]);
    score[sc_Adjust] -= EvalConfig::adjRook  [p.mat[Co_Black][M_p]] * ScoreType(p.mat[Co_Black][M_r]);
    score[sc_Adjust] += EvalConfig::adjKnight[p.mat[Co_White][M_p]] * ScoreType(p.mat[Co_White][M_n]);
    score[sc_Adjust] -= EvalConfig::adjKnight[p.mat[Co_Black][M_p]] * ScoreType(p.mat[Co_Black][M_n]);

    // bad bishop
    if (p.whiteBishop() & whiteSquare) score[sc_Adjust] -= EvalConfig::badBishop[countBit(pawns[Co_White] & whiteSquare)];
    if (p.whiteBishop() & blackSquare) score[sc_Adjust] -= EvalConfig::badBishop[countBit(pawns[Co_White] & blackSquare)];
    if (p.blackBishop() & whiteSquare) score[sc_Adjust] += EvalConfig::badBishop[countBit(pawns[Co_Black] & whiteSquare)];
    if (p.blackBishop() & blackSquare) score[sc_Adjust] += EvalConfig::badBishop[countBit(pawns[Co_Black] & blackSquare)];

    // adjust piece pair score
    score[sc_Adjust] += ( (p.mat[Co_White][M_b] > 1 ? EvalConfig::bishopPairBonus[p.mat[Co_White][M_p]] : 0)-(p.mat[Co_Black][M_b] > 1 ? EvalConfig::bishopPairBonus[p.mat[Co_Black][M_p]] : 0) );
    score[sc_Adjust] += ( (p.mat[Co_White][M_n] > 1 ? EvalConfig::knightPairMalus : 0)-(p.mat[Co_Black][M_n] > 1 ? EvalConfig::knightPairMalus : 0) );
    score[sc_Adjust] += ( (p.mat[Co_White][M_r] > 1 ? EvalConfig::rookPairMalus   : 0)-(p.mat[Co_Black][M_r] > 1 ? EvalConfig::rookPairMalus   : 0) );

    // initiative
    const EvalScore initiativeBonus = EvalConfig::initiative[0] * countBit(allPawns) + EvalConfig::initiative[1] * ((allPawns & queenSide) && (allPawns & kingSide)) + EvalConfig::initiative[2] * (countBit(p.occupancy & ~allPawns) == 2) - EvalConfig::initiative[3];
    score[sc_initiative][MG] += sgn(score.score[MG]) * std::max(initiativeBonus[MG], ScoreType(-std::abs(score.score[MG])));
    score[sc_initiative][EG] += sgn(score.score[EG]) * std::max(initiativeBonus[EG], ScoreType(-std::abs(score.score[EG])));

    // tempo
    score[sc_Tempo] += EvalConfig::tempo*(white2Play?+1:-1);

    if ( display ) score.Display(p,data.gp);
    ScoreType ret = (white2Play?+1:-1)*score.Score(p,data.gp); // scale both phase and 50 moves rule
    STOP_AND_SUM_TIMER(Eval)
    return ret;
}
