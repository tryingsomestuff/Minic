#include "attack.hpp"
#include "bitboardTools.hpp"
#include "dynamicConfig.hpp"
#include "evalConfig.hpp"
#include "hash.hpp"
#include "nn.hpp"
#include "positionTools.hpp"
#include "searcher.hpp"
#include "score.hpp"
#include "timers.hpp"

namespace{ // some Color / Piece helpers
   template<Color C> inline constexpr ScoreType ColorSignHelper()          { return C==Co_White?+1:-1;}
   template<Color C> inline Square PromotionSquare(const Square k)         { return C==Co_White? (SQFILE(k) + 56) : SQFILE(k);}
   template<Color C> inline Rank ColorRank(const Square k)                 { return Rank(C==Co_White? SQRANK(k) : (7-SQRANK(k)));}
}

template < Piece T , Color C, bool withForwardness = false>
inline void evalPiece(const Position & p, BitBoard pieceBBiterator, const BitBoard (& kingZone)[2], const BitBoard nonPawnMat, const BitBoard occupancy, EvalScore & score, BitBoard & attBy, BitBoard & att, BitBoard & att2, ScoreType (& kdanger)[2], BitBoard & checkers){
    while (pieceBBiterator) {
        const Square k = popBit(pieceBBiterator);
        const Square kk = ColorSquarePstHelper<C>(k);
        score += EvalConfig::PST[T-1][kk] * ColorSignHelper<C>();
        if (withForwardness) score += EvalScore{ScoreType(((DynamicConfig::styleForwardness-50)*SQRANK(kk))/8),0} * ColorSignHelper<C>();
        const BitBoard shadowTarget = BBTools::pfCoverage[T-1](k, occupancy ^ nonPawnMat, C); // aligned threats removing own piece (not pawn) in occupancy
        if ( shadowTarget ){
           const BitBoard target = BBTools::pfCoverage[T-1](k, occupancy, C); // real targets
           if ( target ){
              attBy |= target;
              att2  |= att & target;
              att   |= target;
              if ( target & p.pieces_const<P_wk>(~C) ) checkers |= SquareToBitboard(k);
           }
           kdanger[C]  -= countBit(target       & kingZone[C])  * EvalConfig::kingAttWeight[EvalConfig::katt_defence][T-1];
           kdanger[~C] += countBit(shadowTarget & kingZone[~C]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack][T-1];
        }
    }
}
///@todo special version of evalPiece for king ??

template < Piece T ,Color C>
inline void evalMob(const Position & p, BitBoard pieceBBiterator, EvalScore & score, const BitBoard safe, const BitBoard occupancy, EvalData & data){
    while (pieceBBiterator){
        const unsigned short int mob = countBit(BBTools::pfCoverage[T-1](popBit(pieceBBiterator), occupancy, C) & ~p.allPieces[C] & safe);
        data.mobility[C] += mob;
        score += EvalConfig::MOB[T-2][mob]*ColorSignHelper<C>();
    }
}

template < Color C >
inline void evalMobQ(const Position & p, BitBoard pieceBBiterator, EvalScore & score, const BitBoard safe, const BitBoard occupancy, EvalData & data){
    while (pieceBBiterator){
        const Square s = popBit(pieceBBiterator);
        unsigned short int mob = countBit(BBTools::pfCoverage[P_wb-1](s, occupancy, C) & ~p.allPieces[C] & safe);
        data.mobility[C] += mob;
        score += EvalConfig::MOB[3][mob]*ColorSignHelper<C>();
        mob = countBit(BBTools::pfCoverage[P_wr-1](s, occupancy, C) & ~p.allPieces[C] & safe);
        data.mobility[C] += mob;
        score += EvalConfig::MOB[4][mob]*ColorSignHelper<C>();
    }
}

template < Color C>
inline void evalMobK(const Position & p, BitBoard pieceBBiterator, EvalScore & score, const BitBoard safe, const BitBoard occupancy, EvalData & data){
    while (pieceBBiterator){
        const unsigned short int mob = countBit(BBTools::pfCoverage[P_wk-1](popBit(pieceBBiterator), occupancy, C) & ~p.allPieces[C] & safe);
        data.mobility[C] += mob;
        score += EvalConfig::MOB[5][mob]*ColorSignHelper<C>();
    }
}

template< Color C>
inline void evalPawnPasser(const Position & p, BitBoard pieceBBiterator, EvalScore & score){
    while (pieceBBiterator) {
        const Square k = popBit(pieceBBiterator);
        const EvalScore kingNearBonus   = EvalConfig::kingNearPassedPawnSupport[chebyshevDistance(p.king[C], k)][ColorRank<C>(k)] + EvalConfig::kingNearPassedPawnDefend[chebyshevDistance(p.king[~C], k)][ColorRank<~C>(k)];
        const bool unstoppable          = (p.mat[~C][M_t] == 0)&&((chebyshevDistance(p.king[~C],PromotionSquare<C>(k))-int(p.c!=C)) > std::min(Square(5), chebyshevDistance(PromotionSquare<C>(k),k)));
        if (unstoppable) score += ColorSignHelper<C>()*(Values[P_wr+PieceShift] - Values[P_wp+PieceShift]); // yes rook not queen to force promotion asap
        else             score += (EvalConfig::passerBonus[ColorRank<C>(k)] + kingNearBonus)*ColorSignHelper<C>();
    }
}

template < Color C>
inline void evalPawn(BitBoard pieceBBiterator, EvalScore & score){
    while (pieceBBiterator) { 
        const Square k = popBit(pieceBBiterator);
        const Square kk = ColorSquarePstHelper<C>(k);
        score += EvalConfig::PST[0][kk] * ColorSignHelper<C>(); 
    }
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

bool isLazyHigh(ScoreType lazyThreshold, const EvalFeatures & features, EvalScore & score) {
    score = features.SumUp();
    return std::abs(score[MG] + score[EG]) / 2 > lazyThreshold;
}

ScoreType eval(const Position & p, EvalData & data, Searcher &context, bool safeMatEvaluator, bool display, std::ostream * of){
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

    ///@todo activate (or modulate!) features based on skill level
    
    // main features
    EvalFeatures features;

    // Material evaluation
    const Hash matHash = MaterialHash::getMaterialHash(p.mat);
    if ( matHash ){
       ++context.stats.counters[Stats::sid_materialTableHits];
       // Hash data
       const MaterialHash::MaterialHashEntry & MEntry = MaterialHash::materialHashTable[matHash];
       data.gp = MEntry.gp;
       features.scores[F_material] += MEntry.score;
       // end game knowledge (helper or scaling)
       if ( safeMatEvaluator && (p.mat[Co_White][M_t]+p.mat[Co_Black][M_t]<6) ){
          const Color winningSideEG = features.scores[F_material][EG]>0?Co_White:Co_Black;
          if ( MEntry.t == MaterialHash::Ter_WhiteWinWithHelper || MEntry.t == MaterialHash::Ter_BlackWinWithHelper ){
            STOP_AND_SUM_TIMER(Eval)
            ++context.stats.counters[Stats::sid_materialTableHelper];
            return (white2Play?+1:-1)*(MaterialHash::helperTable[matHash](p,winningSideEG,features.scores[F_material][EG]));
          }
          else if ( MEntry.t == MaterialHash::Ter_Draw){ 
              if (!isAttacked(p, kingSquare(p))) {
                 STOP_AND_SUM_TIMER(Eval)
                 ++context.stats.counters[Stats::sid_materialTableDraw];
                 return context.drawScore();
              }
          }
          else if ( MEntry.t == MaterialHash::Ter_MaterialDraw) {
              if (!isAttacked(p, kingSquare(p))){
                  STOP_AND_SUM_TIMER(Eval)
                  ++context.stats.counters[Stats::sid_materialTableDraw2];
                  return context.drawScore();
              }
          }
          else if ( MEntry.t == MaterialHash::Ter_WhiteWin || MEntry.t == MaterialHash::Ter_BlackWin) features.scalingFactor = 5 - 5*p.fifty/100.f;
          else if ( MEntry.t == MaterialHash::Ter_HardToWin)  features.scalingFactor = 0.5f - 0.5f*(p.fifty/100.f);
          else if ( MEntry.t == MaterialHash::Ter_LikelyDraw) features.scalingFactor = 0.3f - 0.3f*(p.fifty/100.f);
       }
    }
    else{ // game phase and material scores out of table
       ///@todo don't care about imbalance here ?
       ScoreType matScoreW = 0;
       ScoreType matScoreB = 0;
       data.gp = gamePhase(p,matScoreW, matScoreB);
       features.scores[F_material] += EvalScore((p.mat[Co_White][M_q] - p.mat[Co_Black][M_q]) * *absValuesEG[P_wq] + (p.mat[Co_White][M_r] - p.mat[Co_Black][M_r]) * *absValuesEG[P_wr] + (p.mat[Co_White][M_b] - p.mat[Co_Black][M_b]) * *absValuesEG[P_wb] + (p.mat[Co_White][M_n] - p.mat[Co_Black][M_n]) * *absValuesEG[P_wn] + (p.mat[Co_White][M_p] - p.mat[Co_Black][M_p]) * *absValuesEG[P_wp], matScoreW - matScoreB);
       ++context.stats.counters[Stats::sid_materialTableMiss];
    }

#ifdef WITH_TEXEL_TUNING
    features.scores[F_material] += MaterialHash::Imbalance(p.mat, Co_White) - MaterialHash::Imbalance(p.mat, Co_Black);
#endif

#ifdef WITH_NNUE
    if (DynamicConfig::useNNUE && !context.noNNUE){
        EvalScore score;
        if ( ! isLazyHigh(600,features,score)){ // stay to classic eval when the game is already decided
           ScoreType nnueScore = NNUEWrapper::evaluate(p);
           // take tempo and contempt into account
           nnueScore += ScaleScore( /*EvalConfig::tempo*(white2Play?+1:-1) +*/ context.contempt, data.gp);
           nnueScore = (Score(nnueScore,features.scalingFactor,p) * NNUEWrapper::NNUEscaling) / 64;
           ++context.stats.counters[Stats::sid_evalNNUE];
           STOP_AND_SUM_TIMER(Eval)
           return nnueScore;
        }
        ++context.stats.counters[Stats::sid_evalStd];
    }
#endif

    STOP_AND_SUM_TIMER(Eval1)

    context.prefetchPawn(computeHash(p));

    // usefull bitboards 
    const BitBoard pawns[2]   = {p.whitePawn()  , p.blackPawn()};
    const BitBoard knights[2] = {p.whiteKnight(), p.blackKnight()};
    const BitBoard bishops[2] = {p.whiteBishop(), p.blackBishop()};
    const BitBoard rooks[2]   = {p.whiteRook()  , p.blackRook()};
    const BitBoard queens[2]  = {p.whiteQueen() , p.blackQueen()};
    const BitBoard kings[2]   = {p.whiteKing()  , p.blackKing()};    
    const BitBoard allPawns          = pawns[Co_White] | pawns[Co_Black];
    const BitBoard nonPawnMat[2]     = {p.allPieces[Co_White] & ~pawns[Co_White] , p.allPieces[Co_Black] & ~pawns[Co_Black]};
    const BitBoard occupancy         = p.occupancy();
    const BitBoard minor[2]          = { knights[Co_White] | bishops[Co_White] , knights[Co_Black] | bishops[Co_Black]};
    ScoreType kdanger[2]             = {0, 0};
    BitBoard att[2]                  = {empty, empty}; // bitboard of squares attacked by color
    BitBoard att2[2]                 = {empty, empty}; // bitboard of squares attacked twice by color
    BitBoard attFromPiece[2][6]      = {{empty}};      // bitboard of squares attacked by specific piece of color
    BitBoard checkers[2][6]          = {{empty}};      // bitboard of color pieces squares attacking king

    const BitBoard kingZone[2]   = { BBTools::mask[p.king[Co_White]].kingZone, BBTools::mask[p.king[Co_Black]].kingZone};
    const BitBoard kingShield[2] = { kingZone[Co_White] & ~BBTools::shiftS<Co_White>(ranks[SQRANK(p.king[Co_White])]) , kingZone[Co_Black] & ~BBTools::shiftS<Co_Black>(ranks[SQRANK(p.king[Co_Black])]) };

    // PST, attack, danger
    if ( DynamicConfig::styleForwardness != 50 ){
       evalPiece<P_wn,Co_White,true>(p,knights[Co_White],kingZone,nonPawnMat[Co_White],occupancy,features.scores[F_positional],attFromPiece[Co_White][P_wn-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wn-1]);
       evalPiece<P_wb,Co_White,true>(p,bishops[Co_White],kingZone,nonPawnMat[Co_White],occupancy,features.scores[F_positional],attFromPiece[Co_White][P_wb-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wb-1]);
       evalPiece<P_wr,Co_White,true>(p,rooks  [Co_White],kingZone,nonPawnMat[Co_White],occupancy,features.scores[F_positional],attFromPiece[Co_White][P_wr-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wr-1]);
       evalPiece<P_wq,Co_White,true>(p,queens [Co_White],kingZone,nonPawnMat[Co_White],occupancy,features.scores[F_positional],attFromPiece[Co_White][P_wq-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wq-1]);
       evalPiece<P_wk,Co_White,true>(p,kings  [Co_White],kingZone,nonPawnMat[Co_White],occupancy,features.scores[F_positional],attFromPiece[Co_White][P_wk-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wk-1]);
       evalPiece<P_wn,Co_Black,true>(p,knights[Co_Black],kingZone,nonPawnMat[Co_Black],occupancy,features.scores[F_positional],attFromPiece[Co_Black][P_wn-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wn-1]);
       evalPiece<P_wb,Co_Black,true>(p,bishops[Co_Black],kingZone,nonPawnMat[Co_Black],occupancy,features.scores[F_positional],attFromPiece[Co_Black][P_wb-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wb-1]);
       evalPiece<P_wr,Co_Black,true>(p,rooks  [Co_Black],kingZone,nonPawnMat[Co_Black],occupancy,features.scores[F_positional],attFromPiece[Co_Black][P_wr-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wr-1]);
       evalPiece<P_wq,Co_Black,true>(p,queens [Co_Black],kingZone,nonPawnMat[Co_Black],occupancy,features.scores[F_positional],attFromPiece[Co_Black][P_wq-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wq-1]);
       evalPiece<P_wk,Co_Black,true>(p,kings  [Co_Black],kingZone,nonPawnMat[Co_Black],occupancy,features.scores[F_positional],attFromPiece[Co_Black][P_wk-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wk-1]);
    }
    else{
       evalPiece<P_wn,Co_White>(p,knights[Co_White],kingZone,nonPawnMat[Co_White],occupancy,features.scores[F_positional],attFromPiece[Co_White][P_wn-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wn-1]);
       evalPiece<P_wb,Co_White>(p,bishops[Co_White],kingZone,nonPawnMat[Co_White],occupancy,features.scores[F_positional],attFromPiece[Co_White][P_wb-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wb-1]);
       evalPiece<P_wr,Co_White>(p,rooks  [Co_White],kingZone,nonPawnMat[Co_White],occupancy,features.scores[F_positional],attFromPiece[Co_White][P_wr-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wr-1]);
       evalPiece<P_wq,Co_White>(p,queens [Co_White],kingZone,nonPawnMat[Co_White],occupancy,features.scores[F_positional],attFromPiece[Co_White][P_wq-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wq-1]);
       evalPiece<P_wk,Co_White>(p,kings  [Co_White],kingZone,nonPawnMat[Co_White],occupancy,features.scores[F_positional],attFromPiece[Co_White][P_wk-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wk-1]);
       evalPiece<P_wn,Co_Black>(p,knights[Co_Black],kingZone,nonPawnMat[Co_Black],occupancy,features.scores[F_positional],attFromPiece[Co_Black][P_wn-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wn-1]);
       evalPiece<P_wb,Co_Black>(p,bishops[Co_Black],kingZone,nonPawnMat[Co_Black],occupancy,features.scores[F_positional],attFromPiece[Co_Black][P_wb-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wb-1]);
       evalPiece<P_wr,Co_Black>(p,rooks  [Co_Black],kingZone,nonPawnMat[Co_Black],occupancy,features.scores[F_positional],attFromPiece[Co_Black][P_wr-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wr-1]);
       evalPiece<P_wq,Co_Black>(p,queens [Co_Black],kingZone,nonPawnMat[Co_Black],occupancy,features.scores[F_positional],attFromPiece[Co_Black][P_wq-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wq-1]);
       evalPiece<P_wk,Co_Black>(p,kings  [Co_Black],kingZone,nonPawnMat[Co_Black],occupancy,features.scores[F_positional],attFromPiece[Co_Black][P_wk-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wk-1]);
    }

    STOP_AND_SUM_TIMER(Eval2)

    Searcher::PawnEntry * pePtr = nullptr;
#ifdef WITH_TEXEL_TUNING
    Searcher::PawnEntry dummy; // used for texel tuning
    pePtr = &dummy;
    {
#else
    if ( !context.getPawnEntry(computePHash(p), pePtr) || DynamicConfig::disableTT ){
#endif
       assert(pePtr);
       Searcher::PawnEntry & pe = *pePtr;
       pe.reset();
       pe.pawnTargets   [Co_White] = BBTools::pawnAttacks   <Co_White>(pawns[Co_White]); 
       pe.pawnTargets   [Co_Black] = BBTools::pawnAttacks   <Co_Black>(pawns[Co_Black]);
       pe.semiOpenFiles [Co_White] = BBTools::fillFile(pawns[Co_White]) & ~BBTools::fillFile(pawns[Co_Black]); // semiOpen white means with white pawn, and without black pawn
       pe.semiOpenFiles [Co_Black] = BBTools::fillFile(pawns[Co_Black]) & ~BBTools::fillFile(pawns[Co_White]); 
       pe.passed        [Co_White] = BBTools::pawnPassed    <Co_White>(pawns[Co_White],pawns[Co_Black]); 
       pe.passed        [Co_Black] = BBTools::pawnPassed    <Co_Black>(pawns[Co_Black],pawns[Co_White]);
       pe.holes         [Co_White] = BBTools::pawnHoles     <Co_White>(pawns[Co_White]) & holesZone[Co_White]; 
       pe.holes         [Co_Black] = BBTools::pawnHoles     <Co_Black>(pawns[Co_Black]) & holesZone[Co_White];
       pe.openFiles =  BBTools::openFiles(pawns[Co_White], pawns[Co_Black]);

       // PST, attack
       evalPawn<Co_White>(pawns[Co_White],pe.score);
       evalPawn<Co_Black>(pawns[Co_Black],pe.score);
       
       // danger in king zone
       pe.danger[Co_White] -= countBit(pe.pawnTargets[Co_White] & kingZone[Co_White]) * EvalConfig::kingAttWeight[EvalConfig::katt_defence][0]; // 0 means pawn here
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
       const BitBoard candidates    [2] = {BBTools::pawnCandidates<Co_White>(pawns[Co_White],pawns[Co_Black]) , BBTools::pawnCandidates<Co_Black>(pawns[Co_Black],pawns[Co_White])};
       evalPawnCandidate<Co_White>(candidates[Co_White],pe.score);
       evalPawnCandidate<Co_Black>(candidates[Co_Black],pe.score);
       ///@todo hidden passed

       // bad pawns
       const BitBoard semiOpenPawn  [2] = {BBTools::pawnSemiOpen  <Co_White>(pawns[Co_White],pawns[Co_Black]), 
                                           BBTools::pawnSemiOpen  <Co_Black>(pawns[Co_Black],pawns[Co_White])};       
       const BitBoard backward[2] = {BBTools::pawnBackward<Co_White>(pawns[Co_White],pawns[Co_Black]), 
                                     BBTools::pawnBackward<Co_Black>(pawns[Co_Black],pawns[Co_White])};
       const BitBoard backwardOpenW  = backward[Co_White] &  semiOpenPawn[Co_White];
       const BitBoard backwardCloseW = backward[Co_White] & ~semiOpenPawn[Co_White];
       const BitBoard backwardOpenB  = backward[Co_Black] &  semiOpenPawn[Co_Black];
       const BitBoard backwardCloseB = backward[Co_Black] & ~semiOpenPawn[Co_Black];
       const BitBoard doubled[2] = {BBTools::pawnDoubled<Co_White>(pawns[Co_White]), 
                                    BBTools::pawnDoubled<Co_Black>(pawns[Co_Black])};
       const BitBoard doubledOpenW   = doubled[Co_White]  &  semiOpenPawn[Co_White];
       const BitBoard doubledCloseW  = doubled[Co_White]  & ~semiOpenPawn[Co_White];
       const BitBoard doubledOpenB   = doubled[Co_Black]  &  semiOpenPawn[Co_Black];
       const BitBoard doubledCloseB  = doubled[Co_Black]  & ~semiOpenPawn[Co_Black];       
       const BitBoard isolated[2] = {BBTools::pawnIsolated(pawns[Co_White]), 
                                     BBTools::pawnIsolated(pawns[Co_Black])};
       const BitBoard isolatedOpenW  = isolated[Co_White] &  semiOpenPawn[Co_White];
       const BitBoard isolatedCloseW = isolated[Co_White] & ~semiOpenPawn[Co_White];
       const BitBoard isolatedOpenB  = isolated[Co_Black] &  semiOpenPawn[Co_Black];
       const BitBoard isolatedCloseB = isolated[Co_Black] & ~semiOpenPawn[Co_Black];   
       const BitBoard detached[2] = {BBTools::pawnDetached<Co_White>(pawns[Co_White],pawns[Co_Black]), 
                                     BBTools::pawnDetached<Co_Black>(pawns[Co_Black],pawns[Co_White])};
       const BitBoard detachedOpenW  = detached[Co_White] &  semiOpenPawn[Co_White] & ~backward[Co_White];
       const BitBoard detachedCloseW = detached[Co_White] & ~semiOpenPawn[Co_White] & ~backward[Co_White];   
       const BitBoard detachedOpenB  = detached[Co_Black] &  semiOpenPawn[Co_Black] & ~backward[Co_Black];
       const BitBoard detachedCloseB = detached[Co_Black] & ~semiOpenPawn[Co_Black] & ~backward[Co_Black];   
       for (Rank r = Rank_2 ; r <= Rank_7 ; ++r){ // simply r2..r7 for classic chess works
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
       pe.score -= EvalConfig::pawnFawnMalusKS * (countBit((pawns[Co_White] & (BBSq_h2 | BBSq_g3)) | (pawns[Co_Black] & BBSq_h3) | (kings[Co_White] & kingSide))/4);
       pe.score += EvalConfig::pawnFawnMalusKS * (countBit((pawns[Co_Black] & (BBSq_h7 | BBSq_g6)) | (pawns[Co_White] & BBSq_h6) | (kings[Co_Black] & kingSide))/4);
       pe.score -= EvalConfig::pawnFawnMalusQS * (countBit((pawns[Co_White] & (BBSq_a2 | BBSq_b3)) | (pawns[Co_Black] & BBSq_a3) | (kings[Co_White] & queenSide))/4);
       pe.score += EvalConfig::pawnFawnMalusQS * (countBit((pawns[Co_Black] & (BBSq_a7 | BBSq_b6)) | (pawns[Co_White] & BBSq_a6) | (kings[Co_Black] & queenSide))/4);

       ++context.stats.counters[Stats::sid_ttPawnInsert];
       pe.h = Hash64to32(computePHash(p)); // set the pawn entry
    }
    assert(pePtr);
    const Searcher::PawnEntry & pe = *pePtr;
    features.scores[F_pawnStruct] += pe.score;

/*
    // lazy eval
    {
        EvalScore score = 0;    
        if ( isLazyHigh(1000,features,score)){
            STOP_AND_SUM_TIMER(Eval)
            return (white2Play?+1:-1)*Score(score, features.scalingFactor, p, data.gp);
        }
    }
*/

    // update global things with pawn entry stuff
    kdanger[Co_White] += pe.danger[Co_White];
    kdanger[Co_Black] += pe.danger[Co_Black];
    checkers[Co_White][0] = BBTools::pawnAttacks<Co_Black>(kings[Co_Black]) & pawns[Co_White];
    checkers[Co_Black][0] = BBTools::pawnAttacks<Co_White>(kings[Co_White]) & pawns[Co_Black];
    att2[Co_White] |= att[Co_White] & pe.pawnTargets[Co_White];
    att2[Co_Black] |= att[Co_Black] & pe.pawnTargets[Co_Black];
    att[Co_White]  |= pe.pawnTargets[Co_White];
    att[Co_Black]  |= pe.pawnTargets[Co_Black];

    STOP_AND_SUM_TIMER(Eval3)

    //const BitBoard attackedOrNotDefended[2]    = {att[Co_White]  | ~att[Co_Black]  , att[Co_Black]  | ~att[Co_White] };
    const BitBoard attackedAndNotDefended[2]   = {att[Co_White]  & ~att[Co_Black]  , att[Co_Black]  & ~att[Co_White] };
    const BitBoard attacked2AndNotDefended2[2] = {att2[Co_White] & ~att2[Co_Black] , att2[Co_Black] & ~att2[Co_White]};

    const BitBoard weakSquare[2]       = {att[Co_Black] & ~att2[Co_White] & (~att[Co_White] | attFromPiece[Co_White][P_wk-1] | attFromPiece[Co_White][P_wq-1]) , att[Co_White] & ~att2[Co_Black] & (~att[Co_Black] | attFromPiece[Co_Black][P_wk-1] | attFromPiece[Co_Black][P_wq-1])};
    const BitBoard safeSquare[2]       = {~att[Co_Black] | ( weakSquare[Co_Black] & att2[Co_White] ) , ~att[Co_White] | ( weakSquare[Co_White] & att2[Co_Black] ) };
    const BitBoard protectedSquare[2]  = {pe.pawnTargets[Co_White] | attackedAndNotDefended[Co_White] | attacked2AndNotDefended2[Co_White] , pe.pawnTargets[Co_Black] | attackedAndNotDefended[Co_Black] | attacked2AndNotDefended2[Co_Black] };

    // own piece in front of pawn
    features.scores[F_development] += EvalConfig::pieceFrontPawn * countBit( BBTools::shiftN<Co_White>(pawns[Co_White]) & nonPawnMat[Co_White] );
    features.scores[F_development] -= EvalConfig::pieceFrontPawn * countBit( BBTools::shiftN<Co_Black>(pawns[Co_Black]) & nonPawnMat[Co_Black] );

    // pawn in front of own minor 
    BitBoard wpminor = BBTools::shiftS<Co_White>(pawns[Co_White]) & minor[Co_White];
    while(wpminor){
       features.scores[F_development] += EvalConfig::pawnFrontMinor[ColorRank<Co_White>(popBit(wpminor))];
    }
    BitBoard bpminor = BBTools::shiftS<Co_Black>(pawns[Co_Black]) & minor[Co_Black];
    while(bpminor){
       features.scores[F_development] -= EvalConfig::pawnFrontMinor[ColorRank<Co_Black>(popBit(bpminor))];
    }

    // center control
    features.scores[F_development] += EvalConfig::centerControl * countBit(protectedSquare[Co_White] & extendedCenter);
    features.scores[F_development] -= EvalConfig::centerControl * countBit(protectedSquare[Co_Black] & extendedCenter);

    // pawn hole, unprotected
    features.scores[F_positional] += EvalConfig::holesMalus * countBit(pe.holes[Co_White] & ~protectedSquare[Co_White])/4;
    features.scores[F_positional] -= EvalConfig::holesMalus * countBit(pe.holes[Co_Black] & ~protectedSquare[Co_Black])/4;

    // free passer bonus
    evalPawnFreePasser<Co_White>(p,pe.passed[Co_White], features.scores[F_pawnStruct]);
    evalPawnFreePasser<Co_Black>(p,pe.passed[Co_Black], features.scores[F_pawnStruct]);

    // rook behind passed
    features.scores[F_pawnStruct] += EvalConfig::rookBehindPassed * (countBit(rooks[Co_White] & BBTools::rearSpan<Co_White>(pe.passed[Co_White])) - countBit(rooks[Co_Black] & BBTools::rearSpan<Co_White>(pe.passed[Co_White])));
    features.scores[F_pawnStruct] -= EvalConfig::rookBehindPassed * (countBit(rooks[Co_Black] & BBTools::rearSpan<Co_Black>(pe.passed[Co_Black])) - countBit(rooks[Co_White] & BBTools::rearSpan<Co_Black>(pe.passed[Co_Black])));

    // protected minor blocking openfile
    features.scores[F_positional] += EvalConfig::minorOnOpenFile * countBit(pe.openFiles & (minor[Co_White]) & pe.pawnTargets[Co_White]);
    features.scores[F_positional] -= EvalConfig::minorOnOpenFile * countBit(pe.openFiles & (minor[Co_Black]) & pe.pawnTargets[Co_Black]);

    // knight on opponent hole, protected by pawn
    features.scores[F_positional] += EvalConfig::outpostN * countBit(pe.holes[Co_Black] & knights[Co_White] & pe.pawnTargets[Co_White]);
    features.scores[F_positional] -= EvalConfig::outpostN * countBit(pe.holes[Co_White] & knights[Co_Black] & pe.pawnTargets[Co_Black]);

    // bishop on opponent hole, protected by pawn
    features.scores[F_positional] += EvalConfig::outpostB * countBit(pe.holes[Co_Black] & bishops[Co_White] & pe.pawnTargets[Co_White]);
    features.scores[F_positional] -= EvalConfig::outpostB * countBit(pe.holes[Co_White] & bishops[Co_Black] & pe.pawnTargets[Co_Black]);

    // knight far from both kings gets a penalty
    BitBoard knight = knights[Co_White];
    while(knight){
        const Square knighSq = popBit(knight);
        features.scores[F_positional] += EvalConfig::knightTooFar[std::min(chebyshevDistance(p.king[Co_White],knighSq),chebyshevDistance(p.king[Co_Black],knighSq))];
    }
    knight = knights[Co_Black];
    while(knight){
        const Square knighSq = popBit(knight);
        features.scores[F_positional] -= EvalConfig::knightTooFar[std::min(chebyshevDistance(p.king[Co_White],knighSq),chebyshevDistance(p.king[Co_Black],knighSq))];
    }

    // reward safe checks
    kdanger[Co_White] += EvalConfig::kingAttSafeCheck[0] * countBit( checkers[Co_Black][0] & att[Co_Black] );
    kdanger[Co_Black] += EvalConfig::kingAttSafeCheck[0] * countBit( checkers[Co_White][0] & att[Co_White] );
    for (Piece pp = P_wn ; pp < P_wk ; ++pp) {
        kdanger[Co_White] += EvalConfig::kingAttSafeCheck[pp-1] * countBit( checkers[Co_Black][pp-1] & safeSquare[Co_Black] );
        kdanger[Co_Black] += EvalConfig::kingAttSafeCheck[pp-1] * countBit( checkers[Co_White][pp-1] & safeSquare[Co_White] );
    }

    // less danger if no enemy queen
    const Square whiteQueenSquare = queens[Co_White] ? BBTools::SquareFromBitBoard(queens[Co_White]) : INVALIDSQUARE;
    const Square blackQueenSquare = queens[Co_Black] ? BBTools::SquareFromBitBoard(queens[Co_Black]) : INVALIDSQUARE;
    kdanger[Co_White] -= blackQueenSquare==INVALIDSQUARE ? EvalConfig::kingAttNoQueen : 0;
    kdanger[Co_Black] -= whiteQueenSquare==INVALIDSQUARE ? EvalConfig::kingAttNoQueen : 0;

    // danger : use king danger score. **DO NOT** apply this in end-game
    const ScoreType dw = EvalConfig::kingAttTable[std::min(std::max(ScoreType(kdanger[Co_White]/32),ScoreType(0)),ScoreType(63))];
    const ScoreType db = EvalConfig::kingAttTable[std::min(std::max(ScoreType(kdanger[Co_Black]/32),ScoreType(0)),ScoreType(63))];
    features.scores[F_attack] -=  EvalScore(dw,0);
    features.scores[F_attack] +=  EvalScore(db,0);
    data.danger[Co_White] = kdanger[Co_White];
    data.danger[Co_Black] = kdanger[Co_Black];

    // number of hanging pieces
    const BitBoard hanging[2] = {nonPawnMat[Co_White] & weakSquare[Co_White] , nonPawnMat[Co_Black] & weakSquare[Co_Black] };
    features.scores[F_attack] += EvalConfig::hangingPieceMalus * (countBit(hanging[Co_White]) - countBit(hanging[Co_Black]));

/*
    // lazy eval
    {
        EvalScore score = 0;    
        if ( isLazyHigh(900,features,score)){
            STOP_AND_SUM_TIMER(Eval)
            return (white2Play?+1:-1)*Score(score, features.scalingFactor, p, data.gp);
        }
    }
*/

    // threats by minor
    BitBoard targetThreat = (nonPawnMat[Co_White] | (pawns[Co_White] & weakSquare[Co_White]) ) & (attFromPiece[Co_Black][P_wn-1] | attFromPiece[Co_Black][P_wb-1]);
    while (targetThreat) features.scores[F_attack] += EvalConfig::threatByMinor[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    targetThreat = (nonPawnMat[Co_Black] | (pawns[Co_Black] & weakSquare[Co_Black]) ) & (attFromPiece[Co_White][P_wn-1] | attFromPiece[Co_White][P_wb-1]);
    while (targetThreat) features.scores[F_attack] -= EvalConfig::threatByMinor[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    // threats by rook
    targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wr-1];
    while (targetThreat) features.scores[F_attack] += EvalConfig::threatByRook[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wr-1];
    while (targetThreat) features.scores[F_attack] -= EvalConfig::threatByRook[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    // threats by queen
    targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wq-1];
    while (targetThreat) features.scores[F_attack] += EvalConfig::threatByQueen[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wq-1];
    while (targetThreat) features.scores[F_attack] -= EvalConfig::threatByQueen[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    // threats by king
    targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wk-1];
    while (targetThreat) features.scores[F_attack] += EvalConfig::threatByKing[PieceTools::getPieceType(p, popBit(targetThreat))-1];
    targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wk-1];
    while (targetThreat) features.scores[F_attack] -= EvalConfig::threatByKing[PieceTools::getPieceType(p, popBit(targetThreat))-1];

    // threat by safe pawn
    const BitBoard safePawnAtt[2]  = {nonPawnMat[Co_Black] & BBTools::pawnAttacks<Co_White>(pawns[Co_White] & safeSquare[Co_White]), nonPawnMat[Co_White] & BBTools::pawnAttacks<Co_Black>(pawns[Co_Black] & safeSquare[Co_Black])};
    features.scores[F_attack] += EvalConfig::pawnSafeAtt * (countBit(safePawnAtt[Co_White]) - countBit(safePawnAtt[Co_Black]));

    // safe pawn push (protected once or not attacked)
    const BitBoard safePawnPush[2]  = {BBTools::shiftN<Co_White>(pawns[Co_White]) & ~occupancy & safeSquare[Co_White], BBTools::shiftN<Co_Black>(pawns[Co_Black]) & ~occupancy & safeSquare[Co_Black]};
    features.scores[F_mobility] += EvalConfig::pawnMobility * (countBit(safePawnPush[Co_White]) - countBit(safePawnPush[Co_Black]));

    // threat by safe pawn push
    features.scores[F_attack] += EvalConfig::pawnSafePushAtt * (countBit(nonPawnMat[Co_Black] & BBTools::pawnAttacks<Co_White>(safePawnPush[Co_White])) - countBit(nonPawnMat[Co_White] & BBTools::pawnAttacks<Co_Black>(safePawnPush[Co_Black])));

    STOP_AND_SUM_TIMER(Eval4)

    // pieces mobility (second attack loop needed, knowing safeSquare ...)
    evalMob <P_wn,Co_White>(p,knights[Co_White],features.scores[F_mobility],safeSquare[Co_White],occupancy,data);
    evalMob <P_wb,Co_White>(p,bishops[Co_White],features.scores[F_mobility],safeSquare[Co_White],occupancy,data);
    evalMob <P_wr,Co_White>(p,rooks  [Co_White],features.scores[F_mobility],safeSquare[Co_White],occupancy,data);
    evalMobQ<     Co_White>(p,queens [Co_White],features.scores[F_mobility],safeSquare[Co_White],occupancy,data);
    evalMobK<     Co_White>(p,kings  [Co_White],features.scores[F_mobility],~att[Co_Black]      ,occupancy,data);
    evalMob <P_wn,Co_Black>(p,knights[Co_Black],features.scores[F_mobility],safeSquare[Co_Black],occupancy,data);
    evalMob <P_wb,Co_Black>(p,bishops[Co_Black],features.scores[F_mobility],safeSquare[Co_Black],occupancy,data);
    evalMob <P_wr,Co_Black>(p,rooks  [Co_Black],features.scores[F_mobility],safeSquare[Co_Black],occupancy,data);
    evalMobQ<     Co_Black>(p,queens [Co_Black],features.scores[F_mobility],safeSquare[Co_Black],occupancy,data);
    evalMobK<     Co_Black>(p,kings  [Co_Black],features.scores[F_mobility],~att[Co_White]      ,occupancy,data);

    STOP_AND_SUM_TIMER(Eval5)

    // rook on open file
    features.scores[F_positional] += EvalConfig::rookOnOpenFile         * countBit(rooks[Co_White] & pe.openFiles);
    features.scores[F_positional] += EvalConfig::rookOnOpenSemiFileOur  * countBit(rooks[Co_White] & pe.semiOpenFiles[Co_White]);
    features.scores[F_positional] += EvalConfig::rookOnOpenSemiFileOpp  * countBit(rooks[Co_White] & pe.semiOpenFiles[Co_Black]);
    features.scores[F_positional] -= EvalConfig::rookOnOpenFile         * countBit(rooks[Co_Black] & pe.openFiles);
    features.scores[F_positional] -= EvalConfig::rookOnOpenSemiFileOur  * countBit(rooks[Co_Black] & pe.semiOpenFiles[Co_Black]);
    features.scores[F_positional] -= EvalConfig::rookOnOpenSemiFileOpp  * countBit(rooks[Co_Black] & pe.semiOpenFiles[Co_White]);

    // enemy rook facing king
    features.scores[F_positional] += EvalConfig::rookFrontKingMalus * countBit(BBTools::frontSpan<Co_White>(kings[Co_White]) & rooks[Co_Black]);
    features.scores[F_positional] -= EvalConfig::rookFrontKingMalus * countBit(BBTools::frontSpan<Co_Black>(kings[Co_Black]) & rooks[Co_White]);

    // enemy rook facing queen
    features.scores[F_positional] += EvalConfig::rookFrontQueenMalus * countBit(BBTools::frontSpan<Co_White>(queens[Co_White]) & rooks[Co_Black]);
    features.scores[F_positional] -= EvalConfig::rookFrontQueenMalus * countBit(BBTools::frontSpan<Co_Black>(queens[Co_Black]) & rooks[Co_White]);

    // queen aligned with own rook
    features.scores[F_positional] += EvalConfig::rookQueenSameFile * countBit(BBTools::fillFile(queens[Co_White]) & rooks[Co_White]);
    features.scores[F_positional] -= EvalConfig::rookQueenSameFile * countBit(BBTools::fillFile(queens[Co_Black]) & rooks[Co_Black]);

    // pins on king and queen
    const BitBoard pinnedK [2] = { getPinned<Co_White>(p,p.king[Co_White]), getPinned<Co_Black>(p,p.king[Co_Black]) };
    const BitBoard pinnedQ [2] = { getPinned<Co_White>(p,whiteQueenSquare), getPinned<Co_Black>(p,blackQueenSquare) };
    for (Piece pp = P_wp ; pp < P_wk ; ++pp) {
        const BitBoard bw = p.pieces_const(Co_White, pp);
        if (bw) {
            if (pinnedK[Co_White] & bw) features.scores[F_attack] -= EvalConfig::pinnedKing [pp - 1] * countBit(pinnedK[Co_White] & bw);
            if (pinnedQ[Co_White] & bw) features.scores[F_attack] -= EvalConfig::pinnedQueen[pp - 1] * countBit(pinnedQ[Co_White] & bw);
        }
        const BitBoard bb = p.pieces_const(Co_Black, pp);
        if (bb) {
            if (pinnedK[Co_Black] & bb) features.scores[F_attack] += EvalConfig::pinnedKing [pp - 1] * countBit(pinnedK[Co_Black] & bb);
            if (pinnedQ[Co_Black] & bb) features.scores[F_attack] += EvalConfig::pinnedQueen[pp - 1] * countBit(pinnedQ[Co_Black] & bb);
        }
    }

    // attack : queen distance to opponent king (wrong if multiple queens ...)
    if ( blackQueenSquare != INVALIDSQUARE ) features.scores[F_positional] -= EvalConfig::queenNearKing * (7 - chebyshevDistance(p.king[Co_White], blackQueenSquare) );
    if ( whiteQueenSquare != INVALIDSQUARE ) features.scores[F_positional] += EvalConfig::queenNearKing * (7 - chebyshevDistance(p.king[Co_Black], whiteQueenSquare) );

    // number of pawn and piece type value  ///@todo closedness instead ?
    features.scores[F_material] += EvalConfig::adjRook  [p.mat[Co_White][M_p]] * ScoreType(p.mat[Co_White][M_r]);
    features.scores[F_material] -= EvalConfig::adjRook  [p.mat[Co_Black][M_p]] * ScoreType(p.mat[Co_Black][M_r]);
    features.scores[F_material] += EvalConfig::adjKnight[p.mat[Co_White][M_p]] * ScoreType(p.mat[Co_White][M_n]);
    features.scores[F_material] -= EvalConfig::adjKnight[p.mat[Co_Black][M_p]] * ScoreType(p.mat[Co_Black][M_n]);

    // bad bishop
    if (p.whiteBishop() & whiteSquare) features.scores[F_material] -= EvalConfig::badBishop[countBit(pawns[Co_White] & whiteSquare)];
    if (p.whiteBishop() & blackSquare) features.scores[F_material] -= EvalConfig::badBishop[countBit(pawns[Co_White] & blackSquare)];
    if (p.blackBishop() & whiteSquare) features.scores[F_material] += EvalConfig::badBishop[countBit(pawns[Co_Black] & whiteSquare)];
    if (p.blackBishop() & blackSquare) features.scores[F_material] += EvalConfig::badBishop[countBit(pawns[Co_Black] & blackSquare)];

    // adjust piece pair score
    features.scores[F_material] += ( (p.mat[Co_White][M_b] > 1 ? EvalConfig::bishopPairBonus[p.mat[Co_White][M_p]] : 0)-(p.mat[Co_Black][M_b] > 1 ? EvalConfig::bishopPairBonus[p.mat[Co_Black][M_p]] : 0) );
    features.scores[F_material] += ( (p.mat[Co_White][M_n] > 1 ? EvalConfig::knightPairMalus : 0)-(p.mat[Co_Black][M_n] > 1 ? EvalConfig::knightPairMalus : 0) );
    features.scores[F_material] += ( (p.mat[Co_White][M_r] > 1 ? EvalConfig::rookPairMalus   : 0)-(p.mat[Co_Black][M_r] > 1 ? EvalConfig::rookPairMalus   : 0) );

    // complexity 
    // an "anti human" trick : winning side wants to keep the position more complex
    features.scores[F_complexity] += EvalScore{1,0} * (white2Play?+1:-1) * (p.mat[Co_White][M_t]+p.mat[Co_Black][M_t]+(p.mat[Co_White][M_p]+p.mat[Co_Black][M_p])/2);

    if ( display ) displayEval(data,features);

    // taking Style into account, giving each score a [0..2] factor
    features.scores[F_material]    = features.scores[F_material]    .scale(1 + (DynamicConfig::styleMaterial    - 50)/50.f, 1.f); 
    features.scores[F_positional]  = features.scores[F_positional]  .scale(1 + (DynamicConfig::stylePositional  - 50)/50.f, 1.f); 
    features.scores[F_development] = features.scores[F_development] .scale(1 + (DynamicConfig::styleDevelopment - 50)/50.f, 1.f); 
    features.scores[F_mobility]    = features.scores[F_mobility]    .scale(1 + (DynamicConfig::styleMobility    - 50)/50.f, 1.f); 
    features.scores[F_pawnStruct]  = features.scores[F_pawnStruct]  .scale(1 + (DynamicConfig::stylePawnStruct  - 50)/50.f, 1.f); 
    features.scores[F_attack]      = features.scores[F_attack]      .scale(1 + (DynamicConfig::styleAttack      - 50)/50.f, 1.f); 
    features.scores[F_complexity]  = features.scores[F_complexity]  .scale(    (DynamicConfig::styleComplexity  - 50)/50.f, 0.f);

    // save in learning file
    if ( of ) (*of) << features;

    // Sum everything
    EvalScore score = features.SumUp();

#ifdef VERBOSE_EVAL
    if ( display ){
        Logging::LogIt(Logging::logInfo) << "Post correction ... ";
        displayEval(data,features);
        Logging::LogIt(Logging::logInfo) << "> All (before 2nd order) " << score;
    }
#endif

    // initiative (kind of second order pawn structure stuff for end-games)
    EvalScore initiativeBonus = EvalConfig::initiative[0] * countBit(allPawns) + EvalConfig::initiative[1] * ((allPawns & queenSide) && (allPawns & kingSide)) + EvalConfig::initiative[2] * (countBit(occupancy & ~allPawns) == 2) - EvalConfig::initiative[3];
    initiativeBonus = EvalScore(sgn(score[MG]) * std::max(initiativeBonus[MG], ScoreType(-std::abs(score[MG]))), sgn(score[EG]) * std::max(initiativeBonus[EG], ScoreType(-std::abs(score[EG]))));
    score += initiativeBonus;

#ifdef VERBOSE_EVAL
    if ( display ){
        Logging::LogIt(Logging::logInfo) << "Initiative    " << initiativeBonus;
        Logging::LogIt(Logging::logInfo) << "> All (with initiative) " << score;
    }
#endif

    // tempo
    score += EvalConfig::tempo*(white2Play?+1:-1);

#ifdef VERBOSE_EVAL
    if ( display ){
        Logging::LogIt(Logging::logInfo) << "> All (with tempo) " << score;
    }
#endif

    // contempt
    score += context.contempt;

#ifdef VERBOSE_EVAL
    if ( display ){
        Logging::LogIt(Logging::logInfo) << "> All (with contempt) " << score;
    }
#endif

    // scale score (MG/EG)
    ScoreType ret = (white2Play?+1:-1)*Score(ScaleScore(score,data.gp),features.scalingFactor,p); // scale both phase and 50 moves rule

    if ( display ){
        Logging::LogIt(Logging::logInfo) << "==> All (fully scaled) " << score;
    }

    // use NN input
    ///@todo this needs stm to be taken into account in EvalNN
#ifdef WITH_MLP
       const float gamma = 0.8f;
       ret = (ScoreType)(gamma*ret + (1-gamma)*NN::EvalNN(features,ret));
#endif

    STOP_AND_SUM_TIMER(Eval)
    return ret;
}
