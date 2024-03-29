#pragma once

#include "definition.hpp"
#include "position.hpp"
#include "stats.hpp"

struct Searcher; // forward decl

struct SearchData {
   SearchData() {
      scores.fill(0);
      moves.fill(INVALIDMINIMOVE);
      nodes.fill(0ull);
      times.fill(0ull);
   }
   array1d<ScoreType, MAX_DEPTH> scores;
   array1d<MiniMove, MAX_DEPTH>  moves;
   array1d<Counter, MAX_DEPTH>   nodes;
   array1d<TimeType, MAX_DEPTH>  times;
};

// an little input/output structure for each thread
struct ThreadData {
   DepthType  depth = MAX_DEPTH, seldepth = 0;
   ScoreType  score = 0;
   Position   p;
   Move       best = INVALIDMOVE;
   PVList     pv;
   SearchData datas;
   bool       isPondering = false;
   bool       isAnalysis  = false;
   void reset(){
      score    = 0;
      best     = INVALIDMOVE;
      seldepth = 0;
      depth    = 0;
      pv.clear();
   }
};

std::ostream& operator<<(std::ostream& of, const ThreadData& d);

/*!
 * This is the singleton pool of threads
 * The search function here is the main entry point for an analysis
 * This is based on the former Stockfish design
 */
class ThreadPool : public std::vector<std::unique_ptr<Searcher>> {
  public:
   static ThreadPool& instance();
   ~ThreadPool();
   ThreadPool() = default;
   // non copyable
   ThreadPool(const ThreadPool&) = delete;
   ThreadPool(const ThreadPool&&) = delete;
   ThreadPool& operator=(const ThreadPool&) = delete;
   ThreadPool& operator=(const ThreadPool&&) = delete;

   static void initPawnTables();

   [[nodiscard]] Searcher& main();
   void setup();
   void distributeData(const ThreadData& data) const;
   void startSearch(const ThreadData& d); // non-blocking
   void startOthers() const;              // non-blocking
   void wait(bool otherOnly = false) const;
   void stop() const; // non-blocking

   // gathering counter information from all threads
   [[nodiscard]] Counter counter(Stats::StatId id, bool forceLocal = false) const;

   void displayStats() const;
   void clearGame() const;
   void clearSearch() const;

   TimeType currentMoveMs = 999;
};
