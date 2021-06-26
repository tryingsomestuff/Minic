#pragma once

#include "definition.hpp"

struct Position;
struct Searcher;

/*!
 * TT in Minic is a very classic 1 entry per bucket cache,
 * It stores a 32 bits hash and thus move from TT must be validating before being used
 * An entry is storing both static and evaluation score
 * as well as move, bound and depth.
 * ///@todo test using aging !
 */
namespace TT {

// curGen is coded inside Bound using only 3bits, shifted by 5 (224 = 0x111)
extern GenerationType curGen;
enum Bound : uint8_t {
   B_none          = 0,
   B_alpha         = 1,
   B_beta          = 2,
   B_exact         = 3,
   B_ttPVFlag      = 4,
   B_isCheckFlag   = 8,
   B_isInCheckFlag = 16,
   B_gen           = 224,
   B_allFlags      = B_ttPVFlag | B_isCheckFlag | B_isInCheckFlag | B_gen
};
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#endif // defined(__GNUC__)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpedantic"
#pragma clang diagnostic ignored "-Wignored-qualifiers"
#endif // defined(__clang__)
#pragma pack(push, 1)
struct Entry {
   Entry(): m(INVALIDMINIMOVE), h(nullHash), s(0), e(0), b(B_none), d(-2) {}
   Entry(Hash _h, Move _m, ScoreType _s, ScoreType _e, Bound _b, DepthType _d):
       h(Hash64to32(_h)), m(Move2MiniMove(_m)), s(_s), e(_e), b(Bound(_b | (curGen << 5))), d(_d) {}
   MiniHash h; //32
   union {
      MiniHash _data1; //32
      struct {
         ScoreType s; //16
         ScoreType e; //16
      };
   };
   union {
      MiniHash _data2; //32
      struct {
         MiniMove  m; //16
         Bound     b; //8
         DepthType d; //8
      };
   };
};
#pragma pack(pop)
#if defined(__clang__)
#pragma clang diagnostic pop
#endif // defined(__clang__)
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif // defined(__GNUC__)

void initTable();

void clearTT();

[[nodiscard]] int hashFull();

void age();

void prefetch(Hash h);

bool getEntry(Searcher& context, const Position& p, Hash h, DepthType d, Entry& e);

void setEntry(Searcher& context, Hash h, Move m, ScoreType s, ScoreType eval, Bound b, DepthType d);

void _setEntry(Hash h, const Entry& e);

void getPV(const Position& p, Searcher& context, PVList& pv);

} // namespace TT

[[nodiscard]] ScoreType createHashScore(ScoreType score, DepthType height);

[[nodiscard]] ScoreType adjustHashScore(ScoreType score, DepthType height);
