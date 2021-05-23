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
   std::array<ScoreType, MAX_DEPTH> scores;
   std::array<MiniMove, MAX_DEPTH>  moves;
   std::array<Counter, MAX_DEPTH>   nodes;
   std::array<TimeType, MAX_DEPTH>  times;
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
   void                    setup();
   [[nodiscard]] Searcher& main();
   void                    distributeData(const ThreadData& data);
   void                    startSearch(const ThreadData& d); // non-blocking
   void                    startOthers();                    // non-blocking
   void                    wait(bool otherOnly = false);
   void                    stop(); // non-blocking

   // gathering counter information from all threads
   [[nodiscard]] Counter counter(Stats::StatId id, bool forceLocal = false) const;
   void                  DisplayStats() const;

   void clearGame();
   void clearSearch();

   TimeType currentMoveMs = 999;
};
