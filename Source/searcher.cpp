#include "searcher.hpp"

#include "logging.hpp"

TimeType Searcher::getCurrentMoveMs() {
    if (TimeMan::isUCIPondering || TimeMan::isUCIAnalysis) {
        return INFINITETIME;
    }
    TimeType ret = currentMoveMs;
    if (TimeMan::msecUntilNextTC > 0){
        switch (moveDifficulty) {
        case MoveDifficultyUtil::MD_forced:      ret = (ret >> 4); break; // only one move in movelist !
        case MoveDifficultyUtil::MD_easy:        ret = (ret >> 3); break; // a short depth open search shows one move is far behind others
        case MoveDifficultyUtil::MD_std:         break; // nothing special
        case MoveDifficultyUtil::MD_hardAttack:  ret = (std::min(TimeType(TimeMan::msecUntilNextTC*MoveDifficultyUtil::maxStealFraction), ret*MoveDifficultyUtil::emergencyFactor)); break; // something bad is happening
        case MoveDifficultyUtil::MD_hardDefense: ret = (std::min(TimeType(TimeMan::msecUntilNextTC*MoveDifficultyUtil::maxStealFraction), ret*MoveDifficultyUtil::emergencyFactor)); break; // something bad is happening
        }
    }
    // take variability into account
    ret = std::min(TimeMan::maxTime,TimeType(ret * MoveDifficultyUtil::variabilityFactor())); // inside [0.5 .. 2]
    return std::max(ret, TimeType(20));// if not much time left, let's try something ...;
}

void Searcher::getCMHPtr(const unsigned int ply, CMHPtrArray & cmhPtr){
    cmhPtr.fill(0);
    for( unsigned int k = 0 ; k < MAX_CMH_PLY ; ++k){
        assert(ply-k < MAX_PLY && int(ply)-int(k) >= 0);
        if( ply > k && VALIDMOVE(stack[ply-k].p.lastMove)){
           const Square to = Move2To(stack[ply-k].p.lastMove);
           cmhPtr[k] = historyT.counter_history[stack[ply-k-1].p.board_const(to)+PieceShift][to];
        }
    }
}

ScoreType Searcher::getCMHScore(const Position & p, const Square from, const Square to, const CMHPtrArray & cmhPtr) const {
    ScoreType ret = 0;
    for (int i = 0; i < MAX_CMH_PLY; i ++){ if (cmhPtr[i]){ ret += cmhPtr[i][(p.board_const(from)+PieceShift) * NbSquare + to]; } }
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
    if ( isMainThread() ){ ThreadPool::instance().stop(); } // stop other threads
}

size_t Searcher::id()const {
    return _index;
}

bool   Searcher::isMainThread()const {
    return id() == 0 ;
}

Searcher::Searcher(size_t n):_index(n),_exit(false),_searching(true),_stdThread(&Searcher::idleLoop, this){
    startTime   = Clock::now();
    wait();
}

Searcher::~Searcher(){
    _exit = true;
    start();
    Logging::LogIt(Logging::logInfo) << "Waiting for workers to join...";
    _stdThread.join();
}

void Searcher::setData(const ThreadData & d){
    _data = d; // this is a copy
}

ThreadData & Searcher::getData(){
    return _data;
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
    Logging::LogIt(Logging::logInfo) << "Size of Pawn TT " << ttSizePawn * sizeof(PawnEntry) / 1024 << "Kb" ;
}

void Searcher::clearPawnTT() {
    for (unsigned int k = 0; k < ttSizePawn; ++k) tablePawn[k].h = 0;
}

void Searcher::clearGame(){
     clearPawnTT();
     stats.init();
     killerT.initKillers();
     historyT.initHistory();
     counterT.initCounter();
     previousBest = INVALIDMOVE;
}

void Searcher::clearSearch(bool forceCounterClear){
     //clearPawnTT(); // to be used for reproductible results ///@todo verify again
     stats.init();
     killerT.initKillers();
     historyT.initHistory(!forceCounterClear);
     counterT.initCounter();
     previousBest = INVALIDMOVE;
}

bool Searcher::getPawnEntry(Hash h, PawnEntry *& pe){
    assert(h > 0);
    PawnEntry & _e = tablePawn[h&(ttSizePawn-1)];
    pe = &_e;
    if ( _e.h != Hash64to32(h) ) return false;
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

std::atomic<bool> Searcher::startLock;
const unsigned long long int Searcher::ttSizePawn = 1024*32;

#ifdef WITH_GENFILE
void Searcher::writeToGenFile(const Position & p){
    static std::map<int,std::unique_ptr<Searcher> > coSearchers; 
    static unsigned long long int sfensWritten = 0;
    if (!genFen || id() >= MAX_THREADS) return;

    if ( coSearchers.find(id()) == coSearchers.end()){
        coSearchers[id()] = std::unique_ptr<Searcher>(new Searcher(id()+MAX_THREADS));
        coSearchers[id()]->initPawnTable();
    }

    Searcher & cos = *coSearchers[id()];

    cos.genFen = false;
    const bool oldQuiet = DynamicConfig::quiet;
    const bool oldDisableTT = DynamicConfig::disableTT;
    const unsigned int oldLevel = DynamicConfig::level;

    // init sub search
    cos.subSearch = true;
    DynamicConfig::quiet = true;
    DynamicConfig::disableTT = true; // do not use TT in order to get qsearch leaf node
    DynamicConfig::level = 100;
    cos.clearSearch(true);

    // look for a quiet position using qsearch
    PVList pv;
    DepthType ply = 1;
    DepthType seldepth = 0;
    cos.stopFlag = false;
    ScoreType qScore = cos.qsearchNoPruning(-10000,10000,p,ply,seldepth,&pv);
    Position pQuiet = p;
    NNUEEvaluator evaluator;
    pQuiet.associateEvaluator(evaluator);
    pQuiet.resetNNUEEvaluator(pQuiet.Evaluator());
    for ( auto & m : pv){
        Position p2 = pQuiet;
        //std::cout << "Applying move " << ToString(m) << std::endl;
        if (!applyMove(p2,m,true)) break; // will always be ok because noValidation=true
        pQuiet = p2;
    }

    Move m = INVALIDMOVE;
    ScoreType s = 0, e = 0;
    if ( std::abs(qScore) < 1000 ){
        // evaluate quiet leaf position
        EvalData data;
        //std::ostringstream str;
        e = eval(pQuiet,data,cos,true,false/*,&str*/);

        DynamicConfig::disableTT = false; // use TT here
        if ( std::abs(e) < 1000 ){
            seldepth = 0;
            DepthType depth(DynamicConfig::genFenDepth);
            unsigned int oldRandomPly = DynamicConfig::randomPly;
            DynamicConfig::randomPly = 0;
            pv = cos.search(pQuiet,m,depth,s,seldepth);
            DynamicConfig::randomPly = oldRandomPly;
        }
    }

    cos.genFen = true;
    DynamicConfig::quiet = oldQuiet;
    DynamicConfig::disableTT = oldDisableTT;
    DynamicConfig::level = oldLevel;
    cos.subSearch = false;
    // end of sub search

    if ( m != INVALIDMOVE && pQuiet.halfmoves >= DynamicConfig::randomPly && std::abs(s) < 2000){

        /*
        // epd format
            genStream << GetFEN(pQuiet) << " c0 \"" << e << "\" ;"          // eval
                                << " c1 \"" << s << "\" ;"             // score (search)
                                << " c2 \"" << ToString(m) << "\" ;"   // best move
                                //<< " c3 \"" << str.str() << "\" ;"   // features break down
                                << "\n";
        */

        // "plain" format (**not** taking result into account, also draw!)
        genStream << "fen "    << GetFEN(pQuiet) << "\n"
                  << "move "   << ToString(m) << "\n"
                  << "score "  << s << "\n"
                  //<< "eval "   << e << "\n"
                  << "ply "    << pQuiet.halfmoves << "\n"
                  << "result " << 0 << "\n"
                  << "e" << "\n";

        sfensWritten++;
        if ( sfensWritten % 100'000 == 0) Logging::LogIt(Logging::logInfo) << "Sfens written " << sfensWritten; 
    }
}
#endif
