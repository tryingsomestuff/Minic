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
namespace TT{

extern GenerationType curGen;
enum Bound : unsigned char{ B_none = 0, B_alpha = 1, B_beta = 2, B_exact = 3, B_ttFlag = 4, B_isCheckFlag = 8, B_isInCheckFlag = 16, B_allFlags = B_ttFlag|B_isCheckFlag|B_isInCheckFlag};
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#endif  // defined(__GNUC__)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpedantic"
#pragma clang diagnostic ignored "-Wignored-qualifiers"
#endif  // defined(__clang__)
#pragma pack(push, 1)
struct Entry{
    Entry():m(INVALIDMINIMOVE),h(0),s(0),e(0),b(B_none),d(-2)/*,generation(curGen)*/{}
    Entry(Hash _h, Move _m, ScoreType _s, ScoreType _e, Bound _b, DepthType _d) : h(Hash64to32(_h)), m(Move2MiniMove(_m)), s(_s), e(_e), /*generation(curGen),*/ b(_b), d(_d){}
    MiniHash h;            //32
    ScoreType s, e;        //16 + 16
    union{
        MiniHash _data;    //32
        struct {
           MiniMove m;     //16
           Bound b;        //8
           DepthType d;    //8
        };
    };
    //GenerationType generation;
};
#pragma pack(pop)
#if defined(__clang__)
#pragma clang diagnostic pop
#endif  // defined(__clang__)
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif  // defined(__GNUC__)

void initTable();

void clearTT();

int hashFull();

void age();

void prefetch(Hash h);

bool getEntry(Searcher & context, const Position & p, Hash h, DepthType d, Entry & e);

// always replace
void setEntry(Searcher & context, Hash h, Move m, ScoreType s, ScoreType eval, Bound b, DepthType d);

void getPV(const Position & p, Searcher & context, PVList & pv);

} // TT

ScoreType createHashScore(ScoreType score, DepthType ply);

ScoreType adjustHashScore(ScoreType score, DepthType ply);
