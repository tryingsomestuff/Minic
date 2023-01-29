#pragma once

#include "definition.hpp"
#include "searchConfig.hpp"
#include "tables.hpp"
#include "timers.hpp"
#include "transposition.hpp"

/*!
 * Minic is not sorting move on the fly, but once and for all
 * MoveSorter is storing needed information and computeScore function
 * will give each move a score. After that, sort is called on the MoveList
 * */

struct MoveSortOperator {
   constexpr bool operator()(const Move& a, const Move& b) const {
      assert(a != INVALIDMOVE);
      assert(b != INVALIDMOVE);
      return Move2Score(a) > Move2Score(b);
   }
};

struct MoveSorter {
   MoveSorter(const Searcher&    _context,
              const Position&    _p,
              float              _gp,
              DepthType          _height,
              const CMHPtrArray& _cmhPtr,
              bool               _useSEE     = true,
              bool               _isInCheck  = false,
              const TT::Entry*   _e          = nullptr,
              const MiniMove     _refutation = INVALIDMINIMOVE):
       context(_context), p(_p), gp(_gp), height(_height), cmhPtr(_cmhPtr), useSEE(_useSEE), isInCheck(_isInCheck), e(_e), refutation(_refutation) {
      assert(e == nullptr || e->h != nullHash);
   }

   template<Color C> void computeScore(Move& m) const;

   const Position&    p;
   const TT::Entry*   e;
   const Searcher&    context;
   const bool         useSEE;
   const bool         isInCheck;
   const MiniMove     refutation;
   const DepthType    height;
   const CMHPtrArray& cmhPtr;
   const float        gp;
   
   bool skipSort = false;

   inline void score(MoveList & moves){
      score(context, moves, p, gp, height, cmhPtr, useSEE, isInCheck, e, refutation);
   }

   static void score(const Searcher&    context,
                     MoveList&          moves,
                     const Position&    p,
                     const float        gp,
                     const DepthType    height,
                     const CMHPtrArray& cmhPtr,
                     const bool         useSEE     = true,
                     const bool         isInCheck  = false,
                     const TT::Entry*   e          = nullptr,
                     const MiniMove     refutation = INVALIDMINIMOVE);

   inline void scoreAndSort(MoveList & moves){
      scoreAndSort(context, moves, p, gp, height, cmhPtr, useSEE, isInCheck, e, refutation);
   }

   static void scoreAndSort(const Searcher&    context,
                            MoveList&          moves,
                            const Position&    p,
                            const float        gp,
                            const DepthType    height,
                            const CMHPtrArray& cmhPtr,
                            const bool         useSEE     = true,
                            const bool         isInCheck  = false,
                            const TT::Entry*   e          = nullptr,
                            const MiniMove     refutation = INVALIDMINIMOVE);

   static void sort(MoveList& moves);

   [[nodiscard]] inline const Move* pickNextLazy(MoveList& moves, 
                                                 size_t& begin, 
                                                 bool sorted = false, 
                                                 ScoreType threshold = SearchConfig::lazySortThreshold){
      // if move has very low chance of raising alpha, no need to sort anymore ...
      skipSort = skipSort || sorted || (begin != 0 && Move2Score(*(moves.begin() + begin - 1)) < threshold);
      return pickNext(moves, begin, skipSort);
   }

   [[nodiscard]] static const Move* pickNext(MoveList& moves, size_t& begin, bool skipSort = false);
};
