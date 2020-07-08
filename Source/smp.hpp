#pragma once

#include "definition.hpp"

#include "position.hpp"
#include "stats.hpp"

struct Searcher; // forward decl

struct SearchData{
    SearchData(){
        scores.fill(0);
        moves.fill(INVALIDMINIMOVE);
        nodes.fill(0ull);
        times.fill(0ull);
    }
    std::array<ScoreType,MAX_DEPTH> scores;
    std::array<MiniMove,MAX_DEPTH>  moves;
    std::array<Counter,MAX_DEPTH>   nodes;
    std::array<TimeType,MAX_DEPTH>  times;
};

struct ThreadData{
    DepthType depth = 0, seldepth = 0;
    ScoreType sc = 0;
    Position p;
    Move best = INVALIDMOVE;
    PVList pv;
    SearchData datas;
};

/* This is the singleton pool of threads
 * The search function here is the main entry point for an analysis
 * This is based on the former Stockfish design
 */
class ThreadPool : public std::vector<std::unique_ptr<Searcher>> {
public:
    static ThreadPool & instance();
    ~ThreadPool();
    void setup();
    Searcher & main();
    Move search(const ThreadData & d);
    void startOthers();
    void wait(bool otherOnly = false);
    void stop();
    // gathering counter information from all threads
    Counter counter(Stats::StatId id) const;
    void DisplayStats()const;
    void clearGame();
    void clearSearch();

    TimeType currentMoveMs = 999;
};

