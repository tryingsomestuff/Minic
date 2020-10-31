#include "transposition.hpp"

#include "logging.hpp"
#include "position.hpp"
#include "searcher.hpp"
#include "tools.hpp"

namespace{
    template<class T>
    struct DeleteAligned{
        void operator()(T * ptr) const { if (ptr) std_aligned_free(ptr); }
    };    
    unsigned long long int ttSize = 0;
    std::unique_ptr<TT::Entry[],DeleteAligned<TT::Entry>> table(nullptr);
}
namespace TT{

GenerationType curGen = 0;

#ifdef __linux__
#include <sys/mman.h>
#endif

void initTable(){
    Logging::LogIt(Logging::logInfo) << "Init TT" ;
    Logging::LogIt(Logging::logInfo) << "Entry size " << sizeof(Entry);
    ttSize = powerFloor((1024 * 1024 * DynamicConfig::ttSizeMb) / (unsigned long long int)sizeof(Entry));
    assert(countBit(ttSize) == 1); // a power of 2
    table.reset((Entry *) std_aligned_alloc(1024,ttSize*sizeof(Entry)));
    Logging::LogIt(Logging::logInfo) << "Size of TT " << ttSize * sizeof(Entry) / 1024 / 1024 << "Mb" ;
    clearTT();
}

void clearTT() {
    TT::curGen = 0;
    Logging::LogIt(Logging::logInfo) << "Now zeroing memory using " << DynamicConfig::threads << " threads" ;
    auto worker = [&] (size_t begin, size_t end){
       std::fill(&table[0]+begin,&table[0]+end,Entry());
    };
    threadedWork(worker,DynamicConfig::threads,ttSize);
    Logging::LogIt(Logging::logInfo) << "... done ";    
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

// will return true only if depth is enough
// e.h is nullHash is the TT entry is not usable
bool getEntry(Searcher & context, const Position & p, Hash h, DepthType d, Entry & e) {
    assert(h > 0);
    assert((h&(ttSize-1))==(h%ttSize));
    if ( DynamicConfig::disableTT ) return false;
    e = table[h&(ttSize-1)]; // update entry immediatly to avoid further race condition and invalidate it later if needed
#ifdef DEBUG_HASH_ENTRY
    e.d = randomInt<unsigned int,666>(0, UINT32_MAX);
#endif
    if ( e.h == nullHash ) return false; //early exit
    if ( 
#ifndef DEBUG_HASH_ENTRY
        ((e.h ^ e._data) != Hash64to32(h)) || 
#endif
        ( VALIDMOVE(e.m) && !isPseudoLegal(p, e.m))
       ) { e.h = nullHash; return false; } // move is filled, but wrong in this position, invalidate returned entry.
    
    if ( e.d >= d ){ ++context.stats.counters[Stats::sid_tthits]; return true; } // valid entry only if depth is ok
    else return false;
}

// always replace
void setEntry(Searcher & context, Hash h, Move m, ScoreType s, ScoreType eval, Bound b, DepthType d){
    assert(h > 0);
    if ( DynamicConfig::disableTT ) return;
    Entry e(h,m,s,eval,b,d);
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
      if (!TT::getEntry(context, p2, computeHash(p2), 0, e)) break;
      if (e.h != nullHash) {
        hashStack[k] = computeHash(p2);
        if ( !VALIDMOVE(e.m) || !applyMove(p2,e.m) ) break;
        pv.push_back(e.m);
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
