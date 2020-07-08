#pragma once

#include "definition.hpp"

#include "position.hpp"
#include "transposition.hpp"

/* This file defines classic tables management such as :
 *  - Killers
 *  - History
 *     - color/from/to
 *     - Piece/to
 *     - CMH history : previous moved piece/previous to/cureet moved piece & to
 *  - Counter : from/to
 */

static const int MAX_CMH_PLY = 2;
typedef std::array<ScoreType*,MAX_CMH_PLY> CMHPtrArray;

struct KillerT{
    Move killers[MAX_DEPTH][2];

    void initKillers();
    bool isKiller(const Move m, const DepthType ply);
    void update(Move m, DepthType ply);
};

struct HistoryT{
    ScoreType history[2][64][64]; // color, from, to
    ScoreType historyP[13][64]; // Piece, to
    ScoreType counter_history[13][64][13*64]; //previous moved piece, previous to, current moved piece * boardsize + current to

    void initHistory(bool noCleanCounter = false);

    template<int S>
    inline void update(DepthType depth, Move m, const Position & p, CMHPtrArray & cmhPtr){
        if ( Move2Type(m) == T_std ){
           const Color c = p.c;
           const Square from = Move2From(m); assert(squareOK(from));
           const Square to = Move2To(m); assert(squareOK(to));
           const ScoreType s = S * HSCORE(depth);
           const Piece pp = p.board_const(from);
           history[c][from][to] += s - HISTORY_DIV(history[c][from][to] * std::abs(s));
           historyP[pp+PieceShift][to] += s - HISTORY_DIV(historyP[pp+PieceShift][to] * std::abs(s));
           for (int i = 0; i < MAX_CMH_PLY; ++i){
               if (cmhPtr[i]){
                  ScoreType & item = cmhPtr[i][(p.board_const(from)+PieceShift) * 64 + to];
                  item += s - HISTORY_DIV(item * std::abs(s));
               }
           }
        }
    }
};

struct CounterT{
    ScoreType counter[64][64];

    void initCounter();
    void update(Move m, const Position & p);
};

void updateTables(Searcher & context, const Position & p, DepthType depth, DepthType ply, const Move m, TT::Bound bound, CMHPtrArray & cmhPtr);
