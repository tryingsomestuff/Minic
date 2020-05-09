#pragma once

inline ScoreType qDeltaMargin(const Position & p) {
   ScoreType delta = (p.pieces_const(p.c,P_wp) & seventhRank[p.c]) ? Values[P_wq+PieceShift] : Values[P_wp+PieceShift];
   return delta + Values[P_wq+PieceShift];
}

template < bool qRoot, bool pvnode >
ScoreType Searcher::qsearch(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth){
    if (stopFlag) return STOPSCORE; // no time verification in qsearch, too slow
    ++stats.counters[Stats::sid_qnodes];

    alpha = std::max(alpha, (ScoreType)(-MATE + ply));
    beta  = std::min(beta , (ScoreType)( MATE - ply + 1));
    if (alpha >= beta) return alpha;

    if ((int)ply > seldepth) seldepth = ply;

    EvalData data;
    if (ply >= MAX_DEPTH - 1) return eval(p, data, *this);
    Move bestMove = INVALIDMOVE;

    const bool isInCheck = isAttacked(p, kingSquare(p));
    const bool specialQSearch = isInCheck || qRoot;
    DepthType hashDepth = specialQSearch ? 0 : -1;

    debug_king_cap(p);

    TT::Entry e;
    const Hash pHash = computeHash(p);
    if (TT::getEntry(*this, p, pHash, hashDepth, e)) {
        if (!pvnode && e.h != 0 && ((e.b == TT::B_alpha && e.s <= alpha) || (e.b == TT::B_beta  && e.s >= beta) || (e.b == TT::B_exact))) { return adjustHashScore(e.s, ply); }
        if ( e.m!=INVALIDMINIMOVE && (isInCheck || isCapture(e.m)) ) bestMove = e.m;
    }
    if ( qRoot && interiorNodeRecognizer<true,false,true>(p) == MaterialHash::Ter_Draw) return drawScore(); ///@todo is that gain elo ???

    ScoreType evalScore;
    if (isInCheck) evalScore = -MATE + ply;
    else if ( p.lastMove == NULLMOVE && ply > 0 ) evalScore = ScaleScore(EvalConfig::tempo,stack[p.halfmoves-1].data.gp) - stack[p.halfmoves-1].eval; // skip eval if nullmove just applied ///@todo wrong ! gp is 0 here
    else{
        if (e.h != 0){
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
           data.gp = 0.5; // force mid game value in sorting ...
        }
        else {
            ++stats.counters[Stats::sid_ttscmiss];
            evalScore = eval(p, data, *this);
        }
    }
    bool evalScoreIsHashScore = false;
    // use tt score if possible and not in check
    if ( !isInCheck){
       if ( e.h != 0 && ((e.b == TT::B_alpha && e.s <= evalScore) || (e.b == TT::B_beta && e.s >= evalScore) || (e.b == TT::B_exact)) ) evalScore = e.s, evalScoreIsHashScore = true;
       //else if ( e.h == 0 ) TT::setEntry(*this,pHash,INVALIDMOVE,createHashScore(evalScore,ply),createHashScore(evalScore,ply),TT::B_none,-2); // already insert an eval here in case of pruning ...
    }

    TT::Bound b = TT::B_alpha;
    if ( evalScore >= beta ){
        if ( e.h == 0 ) TT::setEntry(*this,pHash,INVALIDMOVE,createHashScore(evalScore,ply),createHashScore(evalScore,ply),TT::B_beta,-2); 
        return evalScore;
    }
    if ( !isInCheck && SearchConfig::doQDeltaPruning && evalScore + qDeltaMargin(p) < alpha ) return ++stats.counters[Stats::sid_delta],alpha;
    if ( /*pvnode &&*/ evalScore > alpha) alpha = evalScore; ///@todo try it ??
    ScoreType bestScore = evalScore;

    MoveList moves;
    if ( isInCheck ) MoveGen::generate<MoveGen::GP_all>(p,moves); ///@todo generate only evasion !
    else             MoveGen::generate<MoveGen::GP_cap>(p,moves);
    CMHPtrArray cmhPtr;
    getCMHPtr(p.halfmoves,cmhPtr);
    MoveSorter::sort(*this,moves,p,data.gp,ply,cmhPtr,false,isInCheck,e.h?&e:NULL); ///@todo warning gp is often = 0 here !

    const ScoreType alphaInit = alpha;

    int validMoveCount = 0;

    for(auto it = moves.begin() ; it != moves.end() ; ++it){
        if (!isInCheck) {
            if (!SEE_GE(p,*it,0)) {++stats.counters[Stats::sid_qsee];continue;}
            if (SearchConfig::doQFutility && evalScore + SearchConfig::qfutilityMargin[evalScoreIsHashScore] + (Move2Type(*it)==T_ep ? Values[P_wp+PieceShift] : PieceTools::getAbsValue(p, Move2To(*it))) <= alphaInit) {++stats.counters[Stats::sid_qfutility];continue;}
        }
        Position p2 = p;
        if ( ! apply(p2,*it) ) continue;
        ++validMoveCount;
        TT::prefetch(computeHash(p2));
        const ScoreType score = -qsearch<false,false>(-beta,-alpha,p2,ply+1,seldepth);
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
    TT::setEntry(*this,pHash,bestMove,createHashScore(bestScore,ply),createHashScore(evalScore,ply),b,hashDepth);
    return bestScore;
}
