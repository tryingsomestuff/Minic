#include "transposition.hpp"

#include "distributed.h"
#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "position.hpp"
#include "searcher.hpp"
#include "tools.hpp"

namespace {
template<class T> struct DeleteAligned {
   void operator()(T *ptr) const {
      if (ptr) std_aligned_free(ptr);
   }
};
uint64_t ttSize = 0;
std::unique_ptr<TT::Entry[], DeleteAligned<TT::Entry>> table(nullptr);
} // namespace
namespace TT {

GenerationType curGen = 0;

#ifdef __linux__
#include <sys/mman.h>
#endif

void initTable() {
   Logging::LogIt(Logging::logInfo) << "Init TT";
   Logging::LogIt(Logging::logInfo) << "Entry size " << sizeof(Entry);
   ttSize = powerFloor((1024ull * 1024ull * DynamicConfig::ttSizeMb) / (uint64_t)sizeof(Entry));
   assert(BB::countBit(ttSize) == 1); // a power of 2
   table.reset((Entry *)std_aligned_alloc(1024, ttSize * sizeof(Entry)));
   Logging::LogIt(Logging::logInfo) << "Size of TT " << ttSize * sizeof(Entry) / 1024 / 1024 << "Mb";
   clearTT();
}

void clearTT() {
   TT::curGen = 0;
   Logging::LogIt(Logging::logInfo) << "Now zeroing memory using " << DynamicConfig::threads << " threads";
   auto worker = [&](size_t begin, size_t end) { std::fill(&table[0] + begin, &table[0] + end, Entry()); };
   threadedWork(worker, DynamicConfig::threads, ttSize);
   Logging::LogIt(Logging::logInfo) << "... done ";
}

int hashFull() {
   unsigned int count = 0;
   const unsigned int samples = 1023 * 64;
   for (unsigned int k = 0; k < samples; ++k)
      if (table[(k * 67) % ttSize].h != nullHash) ++count;
   return int((count * 1000) / samples);
}

void age() {
   TT::curGen = static_cast<GenerationType>((TT::curGen + 1) % 8); // see Bound::B_gen
}

void prefetch(Hash h) {
   void *addr = (&table[h & (ttSize - 1)]);
#if defined(__INTEL_COMPILER)
   __asm__("");
#elif defined(_MSC_VER)
   _mm_prefetch((char *)addr, _MM_HINT_T0);
#else
   __builtin_prefetch(addr);
#endif
}

// will return true only if depth is enough
// e.h is nullHash is the TT entry is not usable
bool getEntry(Searcher &context, const Position &p, Hash h, DepthType d, Entry &e) {
   assert(h != nullHash);
   assert((h & (ttSize - 1)) == (h % ttSize));
   if (DynamicConfig::disableTT) return false;
   // update entry immediatly to avoid further race condition and invalidate it later if needed
   e = table[h & (ttSize - 1)];
#ifdef DEBUG_HASH_ENTRY
   e.d = randomInt<unsigned int, 666>(0, UINT32_MAX);
#endif
   if (e.h == nullHash) return false; //early exit
   if (
#ifndef DEBUG_HASH_ENTRY
       ((e.h ^ e._data1 ^ e._data2) != Hash64to32(h)) ||
#endif
       (isValidMove(e.m) && !isPseudoLegal(p, e.m))) {
      // move is filled, but wrong in this position, invalidate returned entry.
      e.h = nullHash;
      return false;
   }

   if (e.d >= d) {
      // valid entry only if depth is ok
      context.stats.incr(Stats::sid_tthits);
      return true;
   }
   else
      return false;
}

// always replace
void setEntry(Searcher &context, Hash h, Move m, ScoreType s, ScoreType eval, Bound b, DepthType d) {
   assert(h != nullHash); // can really happen in fact ... but rarely
   if (DynamicConfig::disableTT) return;
   Entry e(h, m, s, eval, b, d);
   e.h ^= e._data1;
   e.h ^= e._data2;
   context.stats.incr(Stats::sid_ttInsert);
   table[h & (ttSize - 1)] = e; // always replace (favour leaf)
   Distributed::setEntry(h, e);
}

void _setEntry(Hash h, const Entry &e) { table[h & (ttSize - 1)] = e; }

void getPV(const Position &p, Searcher &context, PVList &pv) {
   TT::Entry e;
   Hash hashStack[MAX_PLY] = {nullHash};
   Position  p2 = p;
#ifdef WITH_NNUE
   NNUEEvaluator evaluator;
   p2.associateEvaluator(evaluator);
   p2.resetNNUEEvaluator(p2.evaluator());
#endif
   bool stop = false;
   for (int k = 0; k < MAX_PLY && !stop; ++k) {
      if (!TT::getEntry(context, p2, computeHash(p2), 0, e)) break;
      if (e.h != nullHash) {
         hashStack[k] = computeHash(p2);
         if (!isValidMove(e.m) || !applyMove(p2, e.m)) break;
         pv.push_back(e.m);
         const Hash h = computeHash(p2);
         for (int i = k - 1; i >= 0; --i)
            if (hashStack[i] == h) {
               stop = true;
               break;
            }
      }
   }
}

} // namespace TT

ScoreType createHashScore(ScoreType score, DepthType height) {
   if (isMatingScore(score))
      score += height;
   else if (isMatedScore(score))
      score -= height;
   //else
   //   score *= 0.999f;
   return score;
}

ScoreType adjustHashScore(ScoreType score, DepthType height) {
   if (isMatingScore(score))
      score -= height;
   else if (isMatedScore(score))
      score += height;
   //else
   //   score *= 0.999f;
   return score;
}
