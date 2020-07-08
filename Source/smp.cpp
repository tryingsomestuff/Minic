#include "smp.hpp"

#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "searcher.hpp"

namespace{
    std::unique_ptr<ThreadPool> _pool = 0;
}

ThreadPool & ThreadPool::instance(){ 
    if ( !_pool ) _pool.reset(new ThreadPool);
    return *_pool;
}

ThreadPool::~ThreadPool(){ 
    // risk of static variable dependency fiasco ... ??
    Logging::LogIt(Logging::logInfo) << "... ok threadPool deleted"; 
}

void ThreadPool::setup(){
    assert(DynamicConfig::threads > 0);
    Logging::LogIt(Logging::logInfo) << "Using " << DynamicConfig::threads << " threads";
    resize(0);
    /*
    unsigned int maxThreads = std::max(1u,std::thread::hardware_concurrency());
    if (DynamicConfig::threads > maxThreads) {
        Logging::LogIt(Logging::logWarn) << "Trying to use more threads than hardware core, I don't like that and will use only " << maxThreads << " threads";
        DynamicConfig::threads = maxThreads;
    }
    */
    while (size() < DynamicConfig::threads) { // init other threads (for main see below)
       push_back(std::unique_ptr<Searcher>(new Searcher(size())));
       back()->initPawnTable();
       back()->clearGame();
    }
}

Searcher & ThreadPool::main() { return *(front()); }

void ThreadPool::wait(bool otherOnly) {
    Logging::LogIt(Logging::logInfo) << "Wait for workers to be ready";
    for (auto & s : *this) { if (!otherOnly || !(*s).isMainThread()) (*s).wait(); }
    Logging::LogIt(Logging::logInfo) << "...ok";
}

Move ThreadPool::search(const ThreadData & d){ // distribute data and call main thread search
    Logging::LogIt(Logging::logInfo) << "Search Sync" ;
    wait();
    Logging::LogIt(Logging::logInfo) << "Locking other threads";
    Searcher::startLock.store(true);
    for (auto & s : *this){
        (*s).setData(d); // this is a copy
        (*s).currentMoveMs = currentMoveMs; // propagate time control
    }
    Logging::LogIt(Logging::logInfo) << "Calling main thread search";
    main().search(); ///@todo 1 thread for nothing here
    stop(); // propagate stop flag to all threads
    wait();
    return main().getData().best;
}

void ThreadPool::startOthers(){ for (auto & s : *this) if (!(*s).isMainThread()) (*s).start();}

void ThreadPool::clearGame(){
    TT::clearTT();
    for (auto & s : *this) (*s).clearGame();
}

void ThreadPool::clearSearch(){ 
    //TT::clearTT(); // to be forced for reproductible results
    for (auto & s : *this) (*s).clearSearch();
}

void ThreadPool::stop(){ for (auto & s : *this) (*s).stopFlag = true;}

void ThreadPool::DisplayStats()const{
    for(size_t k = 0 ; k < Stats::sid_maxid ; ++k){
        Logging::LogIt(Logging::logInfo) << Stats::Names[k] << " " << counter((Stats::StatId)k);
    }
}

Counter ThreadPool::counter(Stats::StatId id) const { 
    Counter n = 0; 
    for (auto & it : *this ){ 
        n += it->stats.counters[id];  
    } 
    return n;
}
