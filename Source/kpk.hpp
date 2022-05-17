#pragma once

#include "definition.hpp"

struct Position;

/*!
 * This KPK implementation idea was taken from public-domain code from H.G. Muller
 * http://talkchess.com/forum3/viewtopic.php?f=7&t=47557#p511740
 */
namespace KPK {

[[nodiscard]] Square normalizeSquare(const Position& p, const Color strongSide, const Square sq);

enum kpk_result : uint8_t { kpk_invalid = 0, kpk_unknown = 1, kpk_draw = 2, kpk_win = 4 };
inline constexpr kpk_result& operator|=(kpk_result& r, kpk_result v) { return r = kpk_result(r | v); }

constexpr unsigned KPKmaxIndex = 2 * 24 * NbSquare * NbSquare; // Color x pawn x wk x bk

#pragma pack(push, 1)
struct KPKPosition {
   KPKPosition() = default;
   explicit KPKPosition(const unsigned idx);
   inline operator kpk_result() const { return result; }
   [[nodiscard]] inline kpk_result preCompute(const std::array<KPKPosition, KPKmaxIndex>& db);
   template<Color Us> [[nodiscard]] kpk_result preCompute(const std::array<KPKPosition, KPKmaxIndex>& db);
   Square ksq[2], psq;
   kpk_result result;
   Color us;
};
#pragma pack(pop)

[[nodiscard]] bool probe(const Square wksq, const Square wpsq, const Square bksq, const Color us);

void init();

} // namespace KPK
