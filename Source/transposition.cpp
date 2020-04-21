#include "transposition.hpp"

#include "logging.hpp"
#include "position.hpp"
#include "searcher.hpp"

namespace{
    unsigned long long int ttSize = 0;
    std::unique_ptr<TT::Entry[]> table;
}

namespace TT{

GenerationType curGen = 0;

unsigned long long int powerFloor(unsigned long long int x) {
    unsigned long long int power = 1;
    while (power < x) power *= 2;
    return power/2;
}

void initTable(){
    assert(table==nullptr);
    Logging::LogIt(Logging::logInfo) << "Init TT" ;
    Logging::LogIt(Logging::logInfo) << "Entry size " << sizeof(Entry);
    ttSize = 1024 * powerFloor((DynamicConfig::ttSizeMb * 1024) / (unsigned long long int)sizeof(Entry));
    table.reset(new Entry[ttSize]);
    Logging::LogIt(Logging::logInfo) << "Size of TT " << ttSize * sizeof(Entry) / 1024 / 1024 << "Mb" ;
}

void clearTT() {
    TT::curGen = 0;
    for (unsigned int k = 0; k < ttSize; ++k) table[k] = { 0, 0, 0, INVALIDMINIMOVE, B_none, -1 };
}

int hashFull(){
    unsigned long long count = 0;
    const unsigned int samples = 1023*64;
    for (unsigned int k = 0; k < samples; ++k) if ( table[(k*67)%ttSize].h ) ++count;
    return int((count*1000)/samples);
}

void age(){
    ++TT::curGen;
}

void prefetch(Hash h) {
   void * addr = (&table[h&(ttSize-1)]);
#  if defined(__INTEL_COMPILER)
   __asm__ ("");
#  elif defined(_MSC_VER)
   _mm_prefetch((char*)addr, _MM_HINT_T0);
#  else
   __builtin_prefetch(addr);
#  endif
}

bool getEntry(Searcher & context, const Position & p, Hash h, DepthType d, Entry & e) {
    assert(h > 0);
    if ( DynamicConfig::disableTT  ) return false;
    e = table[h&(ttSize-1)]; // update entry immediatly to avoid further race condition and invalidate it later if needed
#ifdef DEBUG_HASH_ENTRY
    e.d = Zobrist::randomInt<unsigned int>(0, UINT32_MAX);
#endif
    if ( e.h == 0 ) return false; //early exist cause next ones are also empty ...
    if ( !VALIDMOVE(e.m) ||
#ifndef DEBUG_HASH_ENTRY
        (e.h ^ e._data) != Hash64to32(h) ||
#endif
        !isPseudoLegal(p, e.m)) { e.h = 0; return false; }
    
    if ( e.d >= d ){ ++context.stats.counters[Stats::sid_tthits]; return true; } // valid entry if depth is ok
    else return false;
}

// always replace
void setEntry(Searcher & context, Hash h, Move m, ScoreType s, ScoreType eval, Bound b, DepthType d){
    assert(h > 0);
    if ( DynamicConfig::disableTT ) return;
    Entry e = {h,m,s,eval,b,d};
    e.h ^= e._data;
    ++context.stats.counters[Stats::sid_ttInsert];
    table[h&(ttSize-1)] = e; // always replace (favour leaf)
}

void getPV(const Position & p, Searcher & context, PVList & pv){
    TT::Entry e;
    Hash hashStack[MAX_PLY] = { nullHash };
    Position p2 = p;
    bool stop = false;
    for( int k = 0 ; k < MAX_PLY && !stop; ++k){
      if ( !TT::getEntry(context, p2, computeHash(p2), 0, e)) break;
      if (e.h != 0) {
        hashStack[k] = computeHash(p2);
        pv.push_back(e.m);
        if ( !VALIDMOVE(e.m) || !apply(p2,e.m) ) break;
        const Hash h = computeHash(p2);
        for (int i = k-1; i >= 0; --i) if (hashStack[i] == h) {stop=true;break;}
      }
    }
}

} // TT

ScoreType createHashScore(ScoreType score, DepthType ply){
  if      (isMatingScore(score)) score += ply;
  else if (isMatedScore (score)) score -= ply;
  return score;
}

ScoreType adjustHashScore(ScoreType score, DepthType ply){
  if      (isMatingScore(score)) score -= ply;
  else if (isMatedScore (score)) score += ply;
  return score;
}
