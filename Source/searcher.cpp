#include "searcher.hpp"

#include "logging.hpp"

TimeType Searcher::getCurrentMoveMs() {
    if (TimeMan::isUCIPondering) {
        return INFINITETIME;
    }
    TimeType ret = currentMoveMs;
    if (TimeMan::msecUntilNextTC > 0){
        switch (moveDifficulty) {
        case MoveDifficultyUtil::MD_forced:      ret = (ret >> 4); break;
        case MoveDifficultyUtil::MD_easy:        ret = (ret >> 3); break;
        case MoveDifficultyUtil::MD_std:         break;
        case MoveDifficultyUtil::MD_hardDefense: ret = (std::min(TimeType(TimeMan::msecUntilNextTC*MoveDifficultyUtil::maxStealFraction), ret*MoveDifficultyUtil::emergencyFactor)); break;
        case MoveDifficultyUtil::MD_hardAttack:  break;
        }
    }
    return std::max(ret, TimeType(20));// if not much time left, let's try that ...;
}

void Searcher::getCMHPtr(const unsigned int ply, CMHPtrArray & cmhPtr){
    cmhPtr.fill(0);
    for( unsigned int k = 0 ; k < MAX_CMH_PLY ; ++k){
        if( ply > k && VALIDMOVE(stack[ply-k].p.lastMove)){
           const Square to = Move2To(stack[ply-k].p.lastMove);
           cmhPtr[k] = historyT.counter_history[stack[ply-k-1].p.board_const(to)+PieceShift][to];
        }
    }
}

ScoreType Searcher::getCMHScore(const Position & p, const Square from, const Square to, DepthType ply, const CMHPtrArray & cmhPtr) const {
    ScoreType ret = 0;
    for (int i = 0; i < MAX_CMH_PLY; i ++){ if (cmhPtr[i]){ ret += cmhPtr[i][(p.board_const(from)+PieceShift) * 64 + to]; } }
    return ret;
}

ScoreType Searcher::drawScore() { return -1 + 2*((stats.counters[Stats::sid_nodes]+stats.counters[Stats::sid_qnodes]) % 2); }

void Searcher::idleLoop(){
    while (true){
        std::unique_lock<std::mutex> lock(_mutex);
        _searching = false;
        _cv.notify_one(); // Wake up anyone waiting for search finished
        _cv.wait(lock, [&]{ return _searching; });
        if (_exit) return;
        lock.unlock();
        search();
    }
}

void Searcher::start(){
    std::lock_guard<std::mutex> lock(_mutex);
    Logging::LogIt(Logging::logInfo) << "Starting worker " << id() ;
    _searching = true;
    _cv.notify_one(); // Wake up the thread in IdleLoop()
}

void Searcher::wait(){
    std::unique_lock<std::mutex> lock(_mutex);
    _cv.wait(lock, [&]{ return !_searching; });
}

void Searcher::search(){
    Logging::LogIt(Logging::logInfo) << "Search launched for thread " << id() ;
    if ( isMainThread() ){ ThreadPool::instance().startOthers(); } // started other threads but locked for now ...
    _data.pv = search(_data.p, _data.best, _data.depth, _data.sc, _data.seldepth);
}

size_t Searcher::id()const {
    return _index;
}

bool   Searcher::isMainThread()const {
    return id() == 0 ;
}

Searcher::Searcher(size_t n):_index(n),_exit(false),_searching(true),_stdThread(&Searcher::idleLoop, this){
    wait();
}

Searcher::~Searcher(){
    _exit = true;
    start();
    Logging::LogIt(Logging::logInfo) << "Waiting for workers to join...";
    _stdThread.join();
}

void Searcher::setData(const ThreadData & d){
    _data = d;
}

const ThreadData & Searcher::getData()const{
    return _data;
}

bool Searcher::searching()const{
    return _searching;
}

void Searcher::initPawnTable(){
    assert(tablePawn==0);
    assert(ttSizePawn>0);
    Logging::LogIt(Logging::logInfo) << "Init Pawn TT : " << ttSizePawn;
    Logging::LogIt(Logging::logInfo) << "PawnEntry size " << sizeof(PawnEntry);
    tablePawn.reset(new PawnEntry[ttSizePawn]);
    Logging::LogIt(Logging::logInfo) << "Size of Pawn TT " << ttSizePawn * sizeof(PawnEntry) / 1024 / 1024 << "Mb" ;
}

void Searcher::clearPawnTT() {
    for (unsigned int k = 0; k < ttSizePawn; ++k) tablePawn[k].h = 0;
}

bool Searcher::getPawnEntry(Hash h, PawnEntry *& pe){
    assert(h > 0);
    PawnEntry & _e = tablePawn[h&(ttSizePawn-1)];
    pe = &_e;
    if ( _e.h != Hash64to32(h) )     return false;
    ++stats.counters[Stats::sid_ttPawnhits];
    return true;
}

void Searcher::prefetchPawn(Hash h) {
    void * addr = (&tablePawn[h&(ttSizePawn-1)]);
    #  if defined(__INTEL_COMPILER)
    __asm__ ("");
    #  elif defined(_MSC_VER)
    _mm_prefetch((char*)addr, _MM_HINT_T0);
    #  else
    __builtin_prefetch(addr);
    #  endif
}

bool Searcher::stopFlag           = true;
TimeType  Searcher::currentMoveMs = 777; // a dummy initial value, useful for debug
MoveDifficultyUtil::MoveDifficulty Searcher::moveDifficulty = MoveDifficultyUtil::MD_std;
std::atomic<bool> Searcher::startLock;
const unsigned long long int Searcher::ttSizePawn = 1024*32;
