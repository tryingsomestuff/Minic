#include "moveSort.hpp"

#include "logging.hpp"
#include "searcher.hpp"

/* Moves are sorted this way
 * 1°) previous best (from current thread previousBest)
 * 2°) TT move
 * 3°) king evasion is in check and king moves
 * 4°) prom cap
 * 5°) good capture (not prom) based on SEE if not in check, based on MVV LVA if in check
 * 6°) prom
 * 7°) killer 0, then killer 1, then killer 0 from previous move, then counter
 * 8°) other quiet based on various history score (from/to, piece/to, CMH)
 * 9°) bad cap
 */

template< Color C>
void MoveSorter::computeScore(Move & m)const{
    assert(VALIDMOVE(m));
    if ( Move2Score(m) != 0 ) return; // prob cut already computed captures score
    const MType  t    = Move2Type(m); assert(moveTypeOK(t));
    const Square from = Move2From(m); assert(squareOK(from));
    const Square to   = Move2To(m); assert(squareOK(to));
    ScoreType s = MoveScoring[t];
    if ( ply == 0 && sameMove(context.previousBest,m)) s += 20000; // previous root best
    else if (e && sameMove(e->m,m)) s += 15000; // TT move
    else{
        if (isInCheck && from == p.king[C]) s += 10000; // king evasion
        if ( isCapture(t) && !isPromotion(t)){
            if ( useSEE && !isInCheck ){
                const ScoreType see = context.SEE(p,m);
                s += see;
                if ( VALIDMOVE(p.lastMove) && isCapture(p.lastMove) && to == Move2To(p.lastMove) ) s += 500; // recapture bonus
                else if ( see < -80 ) s -= 2*MoveScoring[T_capture]; // too bad capture
            }
            else{ // MVVLVA
                if ( VALIDMOVE(p.lastMove) && isCapture(p.lastMove) && to == Move2To(p.lastMove) ) s += 500; // recapture bonus
                else{
                   const Piece victim   = (t != T_ep) ? PieceTools::getPieceType(p,to) : P_wp;
                   const Piece attacker = PieceTools::getPieceType(p,from);
                   assert(victim>0); assert(attacker>0);
                   s += SearchConfig::MvvLvaScores[victim-1][attacker-1]; //[0 400]
                }                
            }
        }
        else if ( t == T_std ){
            if      (sameMove(m, context.killerT.killers[ply][0])) s += 1900; // quiet killer
            else if (sameMove(m, context.killerT.killers[ply][1])) s += 1700; // quiet killer
            else if (ply > 1 && sameMove(m, context.killerT.killers[ply-2][0])) s += 1500; // quiet killer
            else if (VALIDMOVE(p.lastMove) && sameMove(context.counterT.counter[Move2From(p.lastMove)][Move2To(p.lastMove)],m)) s+= 1300; // quiet counter
            else {
                ///@todo give another try to tune those !
                const Piece pp = p.board_const(from);
                s += context.historyT.history[p.c][from][to] /3; // +/- HISTORY_MAX = 1000
                s += context.historyT.historyP[pp +PieceShift][to] /3 ; // +/- HISTORY_MAX = 1000
                s += context.getCMHScore(p, from, to, cmhPtr) /3; // +/- HISTORY_MAX = 1000
                if ( !isInCheck ){
                   if ( refutation != INVALIDMOVE && from == Move2To(refutation) && context.SEE_GE(p,m,-80)) s += 1000; // move (safely) leaving threat square from null move search
                   const EvalScore * const  pst = EvalConfig::PST[std::abs(pp) - 1];
                   s += ScaleScore(pst[ColorSquarePstHelper<C>(to)] - pst[ColorSquarePstHelper<C>(from)],gp);
                }
            }
        }
    }
    m = ToMove(from, to, t, s);
}

void MoveSorter::score(const Searcher & context, MoveList & moves, const Position & p, float gp, DepthType ply, const CMHPtrArray & cmhPtr, bool useSEE, bool isInCheck, const TT::Entry * e, const Move refutation){
    START_TIMER
    if ( moves.size() < 2) return;
    const MoveSorter ms(context,p,gp,ply,cmhPtr,useSEE,isInCheck,e,refutation);
    if ( p.c == Co_White ){
       for(auto it = moves.begin() ; it != moves.end() ; ++it){ 
          ms.computeScore<Co_White>(*it); 
       }
    }
    else{
       for(auto it = moves.begin() ; it != moves.end() ; ++it){ 
          ms.computeScore<Co_Black>(*it); 
       }
    }
    STOP_AND_SUM_TIMER(MoveScoring)
}

void MoveSorter::scoreAndSort(const Searcher & context, MoveList & moves, const Position & p, float gp, DepthType ply, const CMHPtrArray & cmhPtr, bool useSEE, bool isInCheck, const TT::Entry * e, const Move refutation){
   score(context,moves,p,gp,ply,cmhPtr,useSEE,isInCheck,e,refutation);
   sort(moves);
}

void MoveSorter::sort(MoveList & moves){
    START_TIMER
    std::sort(moves.begin(),moves.end(),MoveSortOperator());
    STOP_AND_SUM_TIMER(MoveSorting)
}

const Move * MoveSorter::pickNext(MoveList & moves, size_t & begin){
    if ( moves.begin()+begin == moves.end()) return nullptr;
    START_TIMER
    auto it = std::min_element(moves.begin()+begin,moves.end(),MoveSortOperator());
    std::iter_swap(moves.begin()+begin,it);
    STOP_AND_SUM_TIMER(MoveSorting)
    return &*(moves.begin()+(begin++)); // increment begin !
}
