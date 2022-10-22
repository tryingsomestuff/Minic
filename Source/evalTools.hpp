#pragma once

#include "definition.hpp"
#include "attack.hpp"
#include "bitboard.hpp"
#include "bitboardTools.hpp"

template<Piece T, Color C>
FORCE_FINLINE void evalMob(const Position &p, BitBoard pieceBBiterator, EvalScore &score, const BitBoard safe, const BitBoard occupancy, uint16_t & mobility) {
   BB::applyOn(pieceBBiterator, [&](const Square & k){
      const uint16_t mob = BB::countBit(BBTools::pfCoverage[T - 1](k, occupancy, C) & ~p.allPieces[C] & safe);
      mobility += mob;
      score += EvalConfig::MOB[T - 2][mob] * ColorSignHelper<C>();
   });
}

template<Color C>
FORCE_FINLINE void evalMobQ(const Position &p, BitBoard pieceBBiterator, EvalScore &score, const BitBoard safe, const BitBoard occupancy, uint16_t & mobility) {
   BB::applyOn(pieceBBiterator, [&](const Square & k){
      uint16_t mob = BB::countBit(BBTools::pfCoverage[P_wb - 1](k, occupancy, C) & ~p.allPieces[C] & safe);
      mobility += mob;
      score += EvalConfig::MOB[3][mob] * ColorSignHelper<C>();
      mob = BB::countBit(BBTools::pfCoverage[P_wr - 1](k, occupancy, C) & ~p.allPieces[C] & safe);
      mobility += mob;
      score += EvalConfig::MOB[4][mob] * ColorSignHelper<C>();
   });
}

template<Color C>
FORCE_FINLINE void evalMobK(const Position &p, BitBoard pieceBBiterator, EvalScore &score, const BitBoard safe, const BitBoard occupancy, uint16_t & mobility) {
   BB::applyOn(pieceBBiterator, [&](const Square & k){
      const uint16_t mob = BB::countBit(BBTools::pfCoverage[P_wk - 1](k, occupancy, C) & ~p.allPieces[C] & safe);
      mobility += mob;
      score += EvalConfig::MOB[5][mob] * ColorSignHelper<C>();
   });
}
