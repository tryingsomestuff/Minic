#include "threading.hpp"

#include "com.hpp"
#include "distributed.h"
#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "searcher.hpp"

namespace {
std::unique_ptr<ThreadPool> _pool = 0;
}

ThreadPool& ThreadPool::instance() {
   if (!_pool) _pool.reset(new ThreadPool);
   return *_pool;
}

ThreadPool::~ThreadPool() {
   // risk of static variable dependency fiasco ... ??
   Logging::LogIt(Logging::logInfo) << "... ok threadPool deleted";
}

void ThreadPool::setup() {
   assert(DynamicConfig::threads > 0);
   Logging::LogIt(Logging::logInfo) << "Using " << DynamicConfig::threads << " threads";
   resize(0);
#ifdef LIMIT_THREADS_TO_PHYSICAL_CORES
   unsigned int maxThreads = std::max(1u, std::thread::hardware_concurrency());
   if (DynamicConfig::threads > maxThreads) {
      Logging::LogIt(Logging::logWarn) << "Trying to use more threads than hardware physical cores";
      Logging::LogIt(Logging::logWarn) << "I don't like that and will use only " << maxThreads << " threads";
      DynamicConfig::threads = maxThreads;
   }
#endif
   // init other threads (for main see below)
   while (size() < DynamicConfig::threads) {
      push_back(std::unique_ptr<Searcher>(new Searcher(size())));
      back()->initPawnTable();
      back()->clearGame();
   }
}

Searcher& ThreadPool::main() { return *(front()); }

void ThreadPool::wait(bool otherOnly) {
   Logging::LogIt(Logging::logInfo) << "Wait for workers to be ready";
   for (auto& s : *this) {
      if (!otherOnly || !(*s).isMainThread()) (*s).wait();
   }
   Logging::LogIt(Logging::logInfo) << "...ok";
}

void ThreadPool::distributeData(const ThreadData& data) {
   // send input data and time control to all threads
   ThreadData dataOther = data;
   dataOther.depth      = MAX_DEPTH; // helper threads go for infinite search (need to receive stopFlag to stop)
   for (auto& s : *this) {
      (*s).setData((*s).isMainThread() ? data : dataOther); // this is a copy
      (*s).currentMoveMs = currentMoveMs;                   // propagate time control from Threadpool to each Searcher
   }
}

// distribute data and call main thread search (this is a non-blocking function)
void ThreadPool::startSearch(const ThreadData& data) {
   Logging::LogIt(Logging::logInfo) << "Search Sync";
   main().wait();
   // COM state must be updated quite late, when all threads or done
   COM::state = data.isAnalysis ? COM::st_analyzing : data.isPondering ? COM::st_pondering : COM::st_searching; // update COM state
   Logging::LogIt(Logging::logInfo) << "state set to " << (int)COM::state;
   Logging::LogIt(Logging::logInfo) << "Locking other threads";
   Searcher::startLock.store(true);
   distributeData(data);
   Logging::LogIt(Logging::logInfo) << "Calling main thread startThread";
   main().startThread(); // non blocking call
}

void ThreadPool::startOthers() {
   for (auto& s : *this)
      if (!(*s).isMainThread()) (*s).startThread();
}

void ThreadPool::clearGame() {
   TT::clearTT();
   for (auto& s : *this) (*s).clearGame();
}

void ThreadPool::clearSearch() {
#ifdef REPRODUCTIBLE_RESULTS
   TT::clearTT();
#endif
   for (auto& s : *this) (*s).clearSearch();
   Distributed::initStat();
}

void ThreadPool::stop() {
   Logging::LogIt(Logging::logInfo) << "Setting stopflag to true on every threads";
   for (auto& s : *this) (*s).stopFlag = true;
}

void ThreadPool::displayStats() const {
   for (size_t k = 0; k < Stats::sid_maxid; ++k) { Logging::LogIt(Logging::logInfo) << Stats::Names[k] << " " << counter((Stats::StatId)k); }
}

Counter ThreadPool::counter(Stats::StatId id, bool forceLocal) const {
   if (!forceLocal && Distributed::moreThanOneProcess()) { return Distributed::counter(id); }
   else {
      Counter n = 0;
      for (auto& it : *this) { n += it->stats.counters[id]; }
      return n;
   }
}

std::ostream& operator<<(std::ostream& of, const ThreadData& d) {
   of << GetFEN(d.p) << " " << d.score << " " << ToString(d.best) << " " << ToString(d.pv);
   return of;
}