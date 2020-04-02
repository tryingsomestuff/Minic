#pragma once

#include "definition.hpp"

#include "tables.hpp"
#include "timers.hpp"
#include "transposition.hpp"

/* Minic is not sorting move on the fly, but once and for all
 * MoveSorter is storing needed information and computeScore function
 * will give each move a score. After that, sort is called on the MoveList
 * */

struct MoveSorter{

    MoveSorter(const Searcher & context, const Position & p, float gp, DepthType ply, const CMHPtrArray & cmhPtr, bool useSEE = true, bool isInCheck = false, const TT::Entry * e = NULL, const Move refutation = INVALIDMOVE):context(context),p(p),gp(gp),ply(ply),cmhPtr(cmhPtr),useSEE(useSEE),isInCheck(isInCheck),e(e),refutation(refutation){assert(e==NULL||e->h!=nullHash);}

    void computeScore(Move & m)const;

    inline bool operator()(const Move & a, const Move & b)const{
      assert( a != INVALIDMOVE);
      assert( b != INVALIDMOVE);
      return Move2Score(a) > Move2Score(b);
    }

    const Position & p;
    const TT::Entry * e;
    const Searcher & context;
    const bool useSEE;
    const bool isInCheck;
    const Move refutation;
    const DepthType ply;
    const CMHPtrArray & cmhPtr;
    float gp;

    static void sort(const Searcher & context, MoveList & moves, const Position & p, float gp, DepthType ply, const CMHPtrArray & cmhPtr, bool useSEE = true, bool isInCheck = false, const TT::Entry * e = NULL, const Move refutation = INVALIDMOVE);
};
