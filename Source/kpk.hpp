#pragma once

#include "definition.hpp"

struct Position;

/*!
 * This KPK implementation idea was taken from public-domain code from H.G. Muller
 * http://talkchess.com/forum3/viewtopic.php?f=7&t=47557#p511740
 */
namespace KPK {

#if !defined(WITH_SMALL_MEMORY)
[[nodiscard]] Square normalizeSquare(const Position& p, const Color strongSide, const Square sq);

enum kpk_result : uint8_t { kpk_invalid = 0, kpk_unknown = 1, kpk_draw = 2, kpk_win = 4 };
constexpr kpk_result& operator|=(kpk_result& r, kpk_result v) { return r = kpk_result(r | v); }

constexpr unsigned KPKmaxIndex = 2 * 24 * NbSquare * NbSquare; // Color x pawn x wk x bk

struct KPKPosition {
   KPKPosition() = default;
   explicit KPKPosition(const unsigned idx);
   FORCE_FINLINE operator kpk_result() const { return result; }
   [[nodiscard]] kpk_result preCompute(const array1d<KPKPosition, KPKmaxIndex>& db);
   template<Color Us> [[nodiscard]] kpk_result preCompute(const array1d<KPKPosition, KPKmaxIndex>& db);
   colored<Square> ksq;
   Square psq;
   kpk_result result;
   Color us;
};

[[nodiscard]] bool probe(const Square wksq, const Square wpsq, const Square bksq, const Color us);
#endif

void init();

} // namespace KPK
