#pragma once

#include "definition.hpp"

struct Position;

/*!
 * This KPK implementation idea was taken from public-domain code from H.G. Muller
 * http://talkchess.com/forum3/viewtopic.php?f=7&t=47557#p511740
 */
namespace KPK{

Square normalizeSquare(const Position& p, Color strongSide, Square sq);

enum kpk_result : unsigned char { kpk_invalid = 0, kpk_unknown = 1, kpk_draw = 2, kpk_win = 4};
inline kpk_result& operator|=(kpk_result& r, kpk_result v) { return r = kpk_result(r | v); }

#pragma pack(push, 1)
struct KPKPosition {
    KPKPosition() = default;
    explicit KPKPosition(unsigned idx);
    inline operator kpk_result() const { return result; }
    [[nodiscard]] inline kpk_result preCompute(const std::vector<KPKPosition>& db);
    template<Color Us> [[nodiscard]] kpk_result preCompute(const std::vector<KPKPosition>& db);
    Square ksq[2], psq;
    kpk_result result;
    Color us;
};
#pragma pack(pop)

[[nodiscard]] bool probe(Square wksq, Square wpsq, Square bksq, Color us);

void init();

} // KPK
