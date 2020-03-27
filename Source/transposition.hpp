#pragma once

#include "definition.hpp"

struct Position;
struct Searcher;

/* TT in Minic is a very classic 1 entry per bucket cache,
 * It stores a 32 bits hash and thus move from TT must be validating before being used
 * An entry is storing both static and evaluation score
 * as well as move, bound and depth.
 * ///@todo test generation !
 */

namespace TT{

extern GenerationType curGen;
enum Bound : unsigned char{ B_exact = 0, B_alpha = 1, B_beta = 2, B_none = 3};
#pragma pack(push, 1)
struct Entry{
    Entry():m(INVALIDMINIMOVE),h(0),s(0),e(0),b(B_none),d(-1)/*,generation(curGen)*/{}
    Entry(Hash h, Move m, ScoreType s, ScoreType e, Bound b, DepthType d) : h(Hash64to32(h)), m(Move2MiniMove(m)), s(s), e(e), /*generation(curGen),*/ b(b), d(d){}
    MiniHash h;            //32
    ScoreType s, e;        //16 + 16
    union{
        MiniHash _d;       //32
        struct{
           MiniMove m;     //16
           Bound b;        //8
           DepthType d;    //8
        };
    };
    //GenerationType generation;
};
#pragma pack(pop)

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
