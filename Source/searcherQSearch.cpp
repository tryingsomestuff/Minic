#include "searcher.hpp"

#ifdef WITH_NNUE
#include "nnue_accumulator.h"
#endif

ScoreType Searcher::qsearchNoPruning(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth, PVList * pv){
    EvalData data;
    ++stats.counters[Stats::sid_qnodes];
    const ScoreType evalScore = eval(p,data,*this);

    if ( evalScore >= beta ) return evalScore;
    if ( evalScore > alpha ) alpha = evalScore;

    const bool isInCheck = isAttacked(p, kingSquare(p));
    ScoreType bestScore = isInCheck?-MATE+ply:evalScore;

    CMHPtrArray cmhPtr;
    getCMHPtr(p.halfmoves,cmhPtr);

    MoveList moves;
    if ( isInCheck ) MoveGen::generate<MoveGen::GP_all>(p,moves); ///@todo generate only evasion !
    else             MoveGen::generate<MoveGen::GP_cap>(p,moves);
    MoveSorter::scoreAndSort(*this,moves,p,data.gp,ply,cmhPtr,false,isInCheck);

    for(auto it = moves.begin() ; it != moves.end() ; ++it){
        Position p2 = p;
#ifdef WITH_NNUE        
        NNUE::Accumulator acc;
        p2.setAccumulator(acc,p);
#endif
        if ( ! applyMove(p2,*it) ) continue;
        PVList childPV;
        const ScoreType score = -qsearchNoPruning(-beta,-alpha,p2,ply+1,seldepth, pv ? &childPV : nullptr);
        if ( score > bestScore){
           bestScore = score;
           if ( score > alpha ){
              if (pv) updatePV(*pv, *it, childPV);
              if ( score >= beta ) return score;
              alpha = score;
           }
        }
    }
    return bestScore;
}

inline ScoreType qDeltaMargin(const Position & p) {
   const ScoreType delta = (p.pieces_const(p.c,P_wp) & seventhRank[p.c]) ? Values[P_wq+PieceShift] : Values[P_wp+PieceShift];
   return delta + Values[P_wq+PieceShift];
}

ScoreType Searcher::qsearch(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth, unsigned int qply, bool qRoot, bool pvnode, signed char isInCheckHint){
    if (stopFlag) return STOPSCORE; // no time verification in qsearch, too slow
    ++stats.counters[Stats::sid_qnodes];

    alpha = std::max(alpha, (ScoreType)(-MATE + ply));
    beta  = std::min(beta , (ScoreType)( MATE - ply + 1));
    if (alpha >= beta) return alpha;

    if ((int)ply > seldepth) seldepth = ply;

    EvalData data;
    if (ply >= MAX_DEPTH - 1) return eval(p, data, *this);
    Move bestMove = INVALIDMOVE;

    debug_king_cap(p);

    // probe TT
    TT::Entry e;
    const Hash pHash = computeHash(p);
    const bool ttDepthOk = TT::getEntry(*this, p, pHash, 0, e);
    const TT::Bound bound = TT::Bound(e.b & ~TT::B_allFlags);
    const bool ttHit = e.h != nullHash;
    const bool validTTmove = ttHit && e.m != INVALIDMINIMOVE;
    const bool ttPV = pvnode || (validTTmove && (e.b&TT::B_ttFlag));    
    const bool ttIsInCheck = validTTmove && (e.b&TT::B_isInCheckFlag);
    const bool isInCheck = isInCheckHint!=-1 ? isInCheckHint : ttIsInCheck || isAttacked(p, kingSquare(p));
    const bool specialQSearch = isInCheck || qRoot;
    const DepthType hashDepth = specialQSearch ? 0 : -1;
    if (ttDepthOk && e.d >= hashDepth) { // if depth of TT entry is enough
        if (!pvnode && ((bound == TT::B_alpha && e.s <= alpha) || (bound == TT::B_beta  && e.s >= beta) || (bound == TT::B_exact))) { 
            return adjustHashScore(e.s, ply); 
        }
    }

#ifdef WITH_GENFILE
    if ( DynamicConfig::genFen && ((stats.counters[Stats::sid_nodes]+stats.counters[Stats::sid_qnodes])%DynamicConfig::genFenSkip==0)) writeToGenFile(p);
#endif

    if ( qRoot && interiorNodeRecognizer<true,false,true>(p) == MaterialHash::Ter_Draw) return drawScore(); ///@todo is that gain elo ???

    if ( validTTmove && (isInCheck || isCapture(e.m)) ) bestMove = e.m;
    
    assert(p.halfmoves - 1 < MAX_PLY && p.halfmoves - 1 >= 0);

    // get a static score for the position.
    ScoreType evalScore;
    if (isInCheck) evalScore = -MATE + ply;
    else if ( p.lastMove == NULLMOVE && ply > 0 ) evalScore = 2*ScaleScore(EvalConfig::tempo,stack[p.halfmoves-1].data.gp) - stack[p.halfmoves-1].eval; // skip eval if nullmove just applied ///@todo wrong ! gp is 0 here so tempoMG must be == tempoEG
    else{
        if (ttHit){ // if we had a TT hit (with or without associated move), we can use its eval instead of calling eval()
            ++stats.counters[Stats::sid_ttschits];
            evalScore = e.e;
            /*
            const Hash matHash = MaterialHash::getMaterialHash(p.mat);
            if ( matHash ){
               ++stats.counters[Stats::sid_materialTableHits];
               const MaterialHash::MaterialHashEntry & MEntry = MaterialHash::materialHashTable[matHash];
               data.gp = MEntry.gp;
            }
            else{
               ScoreType matScoreW = 0;
               ScoreType matScoreB = 0;
               data.gp = gamePhase(p,matScoreW,matScoreB);
               ++stats.counters[Stats::sid_materialTableMiss];
            }
            */
            data.gp = 0.5; // force mid game value in sorting ... affect only quiet move, so here check evasion ...
        }
        else {
            ++stats.counters[Stats::sid_ttscmiss];
            evalScore = eval(p, data, *this);
        }
    }
    const ScoreType staticScore = evalScore;
    bool evalScoreIsHashScore = false;
    // use tt score if possible and not in check
    if ( !isInCheck){
       if ( ttHit && ((bound == TT::B_alpha && e.s <= evalScore) || (bound == TT::B_beta && e.s >= evalScore) || (bound == TT::B_exact)) ) evalScore = e.s, evalScoreIsHashScore = true;
       if ( !ttHit ) TT::setEntry(*this,pHash,INVALIDMOVE,createHashScore(evalScore,ply),createHashScore(evalScore,ply),TT::B_none,-2); // already insert an eval here in case of pruning ...
    }

    // early cut-off based on eval score (static or from TT score)
    if ( evalScore >= beta ){
        //if ( !ttHit ) TT::setEntry(*this,pHash,INVALIDMOVE,createHashScore(evalScore,ply),createHashScore(evalScore,ply),TT::B_none,-2); 
        return evalScore;
    }
    if ( !isInCheck && SearchConfig::doQDeltaPruning && staticScore + qDeltaMargin(p) < alpha ) return ++stats.counters[Stats::sid_delta],alpha;
    if ( evalScore > alpha) alpha = evalScore;

    TT::Bound b = TT::B_alpha;
    ScoreType bestScore = evalScore;
    const ScoreType alphaInit = alpha;
    int validMoveCount = 0;
    
    // try the tt move before move generation
    if ( validTTmove && (isInCheck || isCapture(e.m)) ){
        Position p2 = p;
#ifdef WITH_NNUE        
        NNUE::Accumulator acc;
        p2.setAccumulator(acc,p);
#endif        
        //Position & p2 = stack[p.halfmoves+1].p;
        //p2 = p;
        if ( applyMove(p2,e.m) ){;
            ++validMoveCount;
            //stack[p2.halfmoves].p = p2; ///@todo another expensive copy !!!!
            //stack[p2.halfmoves].h = p2.h;
            TT::prefetch(computeHash(p2));
            const ScoreType score = -qsearch(-beta,-alpha,p2,ply+1,seldepth,isInCheck?0:qply+1,false,false);
            if ( score > bestScore){
                bestMove = e.m;
                bestScore = score;
                if ( score > alpha ){
                    if (score >= beta) {
                        b = TT::B_beta;
                        TT::setEntry(*this,pHash,bestMove,createHashScore(bestScore,ply),createHashScore(evalScore,ply),TT::Bound(b|(ttPV?TT::B_ttFlag:TT::B_none)|(isInCheck?TT::B_isInCheckFlag:TT::B_none)),hashDepth);
                        ///@todo try to update history here also ??
                        return bestScore;
                    }
                    b = TT::B_exact;
                    alpha = score;
                }
            }
        }
    }

    MoveList moves;
    if ( isInCheck ) MoveGen::generate<MoveGen::GP_all>(p,moves); ///@todo generate only evasion !
    else             MoveGen::generate<MoveGen::GP_cap>(p,moves);
    CMHPtrArray cmhPtr;
    getCMHPtr(p.halfmoves,cmhPtr);

    const Square recapture = VALIDMOVE(p.lastMove) ? Move2To(p.lastMove) : INVALIDSQUARE;
    const bool onlyRecapture = qply > 5 && isCapture(p.lastMove) && recapture != INVALIDSQUARE;

#ifdef USE_PARTIAL_SORT
    MoveSorter::score(*this,moves,p,data.gp,ply,cmhPtr,false,isInCheck,validTTmove?&e:NULL); ///@todo warning gp is often = 0.5 here !
    size_t offset = 0;
    const Move * it = nullptr;
    while( (it = MoveSorter::pickNext(moves,offset))){
#else
    MoveSorter::scoreAndSort(*this,moves,p,data.gp,ply,cmhPtr,false,isInCheck,validTTmove?&e:NULL); ///@todo warning gp is often = 0.5 here !
    for(auto it = moves.begin() ; it != moves.end() ; ++it){
#endif
        if (validTTmove && sameMove(e.m, *it)) continue; // already tried
        if (!isInCheck) {
            if (onlyRecapture && Move2To(*it) != recapture ) continue; // only recapture now ...
            if (!SEE_GE(p,*it,0)) {++stats.counters[Stats::sid_qsee];continue;}
            if (SearchConfig::doQFutility && staticScore + SearchConfig::qfutilityMargin[evalScoreIsHashScore] + (isPromotionCap(*it) ? (Values[P_wq+PieceShift]-Values[P_wp+PieceShift]) : 0 ) + (Move2Type(*it)==T_ep ? Values[P_wp+PieceShift] : PieceTools::getAbsValue(p, Move2To(*it))) <= alphaInit) {++stats.counters[Stats::sid_qfutility];continue;}
        }
        Position p2 = p;
#ifdef WITH_NNUE        
        NNUE::Accumulator acc;
        p2.setAccumulator(acc,p);
#endif
        //Position & p2 = stack[p.halfmoves+1].p;
        //p2 = p;
        if ( ! applyMove(p2,*it) ) continue;
        ++validMoveCount;
        //stack[p2.halfmoves].p = p2; ///@todo another expensive copy !!!!
        //stack[p2.halfmoves].h = p2.h;
        TT::prefetch(computeHash(p2));
        const ScoreType score = -qsearch(-beta,-alpha,p2,ply+1,seldepth,isInCheck?0:qply+1,false,false);
        if ( score > bestScore){
           bestMove = *it;
           bestScore = score;
           if ( score > alpha ){
               if (score >= beta) {
                   b = TT::B_beta;
                   break;
               }
               b = TT::B_exact;
               alpha = score;
           }
        }
    }
    if ( validMoveCount==0 && isInCheck) bestScore = -MATE + ply;  
    TT::setEntry(*this,pHash,bestMove,createHashScore(bestScore,ply),createHashScore(evalScore,ply),TT::Bound(b|(ttPV?TT::B_ttFlag:TT::B_none)|(isInCheck?TT::B_isInCheckFlag:TT::B_none)),hashDepth);
    return bestScore;
}
