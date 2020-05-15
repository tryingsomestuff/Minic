#include "searcher.hpp"

ScoreType Searcher::qsearchNoPruning(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth){
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
    MoveGen::generate<MoveGen::GP_cap>(p,moves);
    MoveSorter::scoreAndSort(*this,moves,p,data.gp,ply,cmhPtr,true);

    for(auto it = moves.begin() ; it != moves.end() ; ++it){
        Position p2 = p;
        if ( ! apply(p2,*it) ) continue;
        const ScoreType score = -qsearchNoPruning(-beta,-alpha,p2,ply+1,seldepth);
        if ( score > bestScore){
           bestScore = score;
           if ( score > alpha ){
              if ( score >= beta ) return score;
              alpha = score;
           }
        }
    }
    return bestScore;
}
