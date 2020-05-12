#include "smp.hpp"

#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "searcher.hpp"

ThreadPool & ThreadPool::instance(){ 
    static ThreadPool pool; 
    return pool;
}

ThreadPool::~ThreadPool(){ 
    // risk of static variable dependency fiasco ... ??
    Logging::LogIt(Logging::logInfo) << "... ok threadPool deleted"; 
}

void ThreadPool::setup(){
    assert(DynamicConfig::threads > 0);
    clear();
    Logging::LogIt(Logging::logInfo) << "Using " << DynamicConfig::threads << " threads";
    /*
    unsigned int maxThreads = std::max(1u,std::thread::hardware_concurrency());
    if (DynamicConfig::threads > maxThreads) {
        Logging::LogIt(Logging::logWarn) << "Trying to use more threads than hardware core, I don't like that and will use only " << maxThreads << " threads";
        DynamicConfig::threads = maxThreads;
    }
    */
    while (size() < DynamicConfig::threads) {
       push_back(std::unique_ptr<Searcher>(new Searcher(size())));
       back()->initPawnTable();
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

void ThreadPool::clearPawnTT(){ for (auto & s : *this) (*s).clearPawnTT();}

void ThreadPool::stop(){ for (auto & s : *this) (*s).stopFlag = true;}

ThreadPool::ThreadPool():std::vector<std::unique_ptr<Searcher>>(){ 
    push_back(std::unique_ptr<Searcher>(new Searcher(size()))); // this one will be called "Main" thread (id=0)
} 

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
