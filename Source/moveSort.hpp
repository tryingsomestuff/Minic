#pragma once

#include "definition.hpp"

#include "tables.hpp"
#include "timers.hpp"
#include "transposition.hpp"

/*!
 * Minic is not sorting move on the fly, but once and for all
 * MoveSorter is storing needed information and computeScore function
 * will give each move a score. After that, sort is called on the MoveList
 * */

struct MoveSortOperator{
    inline constexpr bool operator()(const Move & a, const Move & b)const{
      assert( a != INVALIDMOVE);
      assert( b != INVALIDMOVE);
      return Move2Score(a) > Move2Score(b);
    }
};

struct MoveSorter{

    MoveSorter(const Searcher & _context, const Position & _p, float _gp, DepthType _height, 
               const CMHPtrArray & _cmhPtr, bool _useSEE = true, bool _isInCheck = false, 
               const TT::Entry * _e = NULL, const MiniMove _refutation = INVALIDMINIMOVE)
          :context(_context),p(_p),gp(_gp),height(_height),cmhPtr(_cmhPtr),useSEE(_useSEE),isInCheck(_isInCheck),e(_e),refutation(_refutation){
            assert(e==NULL||e->h!=nullHash);
    }

    template<Color C>
    void computeScore(Move & m)const;

    const Position & p;
    const TT::Entry * e;
    const Searcher & context;
    const bool useSEE;
    const bool isInCheck;
    const MiniMove refutation;
    const DepthType height;
    const CMHPtrArray & cmhPtr;
    float gp;

    static void score(const Searcher & context, MoveList & moves, const Position & p, float gp, DepthType height, const CMHPtrArray & cmhPtr, bool useSEE = true, bool isInCheck = false, const TT::Entry * e = NULL, const MiniMove refutation = INVALIDMINIMOVE);
    static void scoreAndSort(const Searcher & context, MoveList & moves, const Position & p, float gp, DepthType height, const CMHPtrArray & cmhPtr, bool useSEE = true, bool isInCheck = false, const TT::Entry * e = NULL, const MiniMove refutation = INVALIDMINIMOVE);
    static void sort(MoveList & moves);
    [[nodiscard]] static const Move * pickNext(MoveList & moves, size_t & begin);
};
