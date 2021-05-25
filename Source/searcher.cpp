#include "searcher.hpp"

#include "com.hpp"
#include "logging.hpp"
#include "xboard.hpp"

TimeType Searcher::getCurrentMoveMs() {
   if (ThreadPool::instance().main().getData().isPondering || ThreadPool::instance().main().getData().isAnalysis) { return INFINITETIME; }

   TimeType ret = currentMoveMs;
   if (TimeMan::msecUntilNextTC > 0) {
      switch (moveDifficulty) {
         case MoveDifficultyUtil::MD_forced: ret = (ret >> 4); break; // only one move in movelist !
         case MoveDifficultyUtil::MD_easy: ret = (ret >> 3); break;   // a short depth open search shows one move is far behind others
         case MoveDifficultyUtil::MD_std: break;                      // nothing special
         case MoveDifficultyUtil::MD_hardAttack:
            break; // score is decreasing but still quite high // ret = (std::min(TimeType(TimeMan::msecUntilNextTC*MoveDifficultyUtil::maxStealFraction), ret*MoveDifficultyUtil::emergencyFactor)); break; // something bad is happening
         case MoveDifficultyUtil::MD_hardDefense:
            ret = (std::min(TimeType(TimeMan::msecUntilNextTC * MoveDifficultyUtil::maxStealFraction), ret * MoveDifficultyUtil::emergencyFactor));
            break; // something bad is happening
      }
   }
   // take variability into account
   ret = std::min(TimeMan::maxTime, TimeType(ret * MoveDifficultyUtil::variabilityFactor())); // inside [0.5 .. 2]
   return std::max(ret, TimeType(20));                                                        // if not much time left, let's try something ...;
}

void Searcher::getCMHPtr(const unsigned int ply, CMHPtrArray& cmhPtr) {
   cmhPtr.fill(0);
   for (unsigned int k = 0; k < MAX_CMH_PLY; ++k) {
      assert(ply - k < MAX_PLY && int(ply) - int(k) >= 0);
      if (ply > k && VALIDMOVE(stack[ply - k].p.lastMove)) {
         const Square to = Move2To(stack[ply - k].p.lastMove);
         cmhPtr[k]       = historyT.counter_history[stack[ply - k - 1].p.board_const(to) + PieceShift][to];
      }
   }
}

ScoreType Searcher::getCMHScore(const Position& p, const Square from, const Square to, const CMHPtrArray& cmhPtr) const {
   ScoreType ret = 0;
   for (int i = 0; i < MAX_CMH_PLY; i++) {
      if (cmhPtr[i]) { ret += cmhPtr[i][(p.board_const(from) + PieceShift) * NbSquare + to]; }
   }
   return ret;
}

ScoreType Searcher::drawScore(const Position& p, DepthType height) {
   if (DynamicConfig::armageddon) {
      if (p.c == Co_White) return -MATE + height;
      else
         return MATE - height + 1;
   }
   return -1 + 2 * ((stats.counters[Stats::sid_nodes] + stats.counters[Stats::sid_qnodes]) % 2);
}

void Searcher::idleLoop() {
   while (true) {
      std::unique_lock<std::mutex> lock(_mutex);
      Logging::LogIt(Logging::logInfo) << "beging of idleloop " << id();
      _searching = false;
      _cv.notify_one(); // Wake up anyone waiting for search finished
      _cv.wait(lock, [&] { return _searching; });
      if (_exit) {
         Logging::LogIt(Logging::logInfo) << "Exiting thread loop " << id();
         return;
      }
      lock.unlock();
      searchLauncher(); // blocking
      Logging::LogIt(Logging::logInfo) << "end of idleloop " << id();
   }
}

void Searcher::startThread() {
   std::lock_guard<std::mutex> lock(_mutex);
   Logging::LogIt(Logging::logInfo) << "Starting worker " << id();
   _searching = true;
   stopFlag   = false;
   _cv.notify_one(); // Wake up the thread in idleLoop()
}

void Searcher::wait() {
   std::unique_lock<std::mutex> lock(_mutex);
   Logging::LogIt(Logging::logInfo) << "Thread waiting " << id();
   _cv.wait(lock, [&] { return !_searching; });
}

// multi-threaded search (blocking call)
void Searcher::searchLauncher() {
   Logging::LogIt(Logging::logInfo) << "Search launched for thread " << id();
   // starts other threads first but they are locked for now ...
   if (isMainThread()) { ThreadPool::instance().startOthers(); }
   // so here searchDriver() will update the thread _data structure
   searchDriver();
}

size_t Searcher::id() const { return _index; }

bool Searcher::isMainThread() const { return id() == 0; }

Searcher::Searcher(size_t n): _index(n), _exit(false), _searching(true), _stdThread(&Searcher::idleLoop, this) {
   startTime = Clock::now();
   wait(); // wait for idleLoop to start in the _stdThread object
}

Searcher::~Searcher() {
   _exit = true;
   startThread();
   Logging::LogIt(Logging::logInfo) << "Waiting for thread worker to join...";
   _stdThread.join();
}

void Searcher::setData(const ThreadData& d) {
   _data = d; // this is a copy
}

ThreadData& Searcher::getData() { return _data; }

const ThreadData& Searcher::getData() const { return _data; }

SearchData& Searcher::getSearchData() { return _data.datas; }

const SearchData& Searcher::getSearchData() const { return _data.datas; }

bool Searcher::searching() const { return _searching; }

void Searcher::initPawnTable() {
   assert(tablePawn == 0);
   assert(ttSizePawn > 0);
   Logging::LogIt(Logging::logInfo) << "Init Pawn TT : " << ttSizePawn;
   Logging::LogIt(Logging::logInfo) << "PawnEntry size " << sizeof(PawnEntry);
   tablePawn.reset(new PawnEntry[ttSizePawn]);
   Logging::LogIt(Logging::logInfo) << "Size of Pawn TT " << ttSizePawn * sizeof(PawnEntry) / 1024 << "Kb";
}

void Searcher::clearPawnTT() {
   for (unsigned int k = 0; k < ttSizePawn; ++k) tablePawn[k].h = nullHash;
}

void Searcher::clearGame() {
   clearPawnTT();
   stats.init();
   killerT.initKillers();
   historyT.initHistory();
   counterT.initCounter();
   previousBest = INVALIDMOVE;
}

void Searcher::clearSearch(bool forceCounterClear) {
#ifdef REPRODUCTIBLE_RESULTS
   clearPawnTT();
   forceCounterClear = true;
#endif
   stats.init();
   killerT.initKillers();
   historyT.initHistory(!forceCounterClear);
   counterT.initCounter();
   previousBest = INVALIDMOVE;
}

bool Searcher::getPawnEntry(Hash h, PawnEntry*& pe) {
   assert(h != nullHash);
   PawnEntry& _e = tablePawn[h & (ttSizePawn - 1)];
   pe = &_e;
   if (_e.h != Hash64to32(h)) return false;
   stats.incr(Stats::sid_ttPawnhits);
   return true;
}

void Searcher::prefetchPawn(Hash h) {
   void* addr = (&tablePawn[h & (ttSizePawn - 1)]);
#if defined(__INTEL_COMPILER)
   __asm__("");
#elif defined(_MSC_VER)
   _mm_prefetch((char*)addr, _MM_HINT_T0);
#else
   __builtin_prefetch(addr);
#endif
}

std::atomic<bool> Searcher::startLock;
const uint64_t    Searcher::ttSizePawn = 1024 * 16 * 8;

Searcher& Searcher::getCoSearcher(size_t id) {
   static std::map<int, std::unique_ptr<Searcher>> coSearchers;
   // init new co-searcher if not already present
   if (coSearchers.find(id) == coSearchers.end()) {
      coSearchers[id] = std::unique_ptr<Searcher>(new Searcher(id + MAX_THREADS));
      coSearchers[id]->initPawnTable();
   }
   return *coSearchers[id];
}

Position Searcher::getQuiet(const Position& p, Searcher* searcher, ScoreType* qScore) {
   // fixed co-searcher if not given
   Searcher& cos = getCoSearcher(searcher ? searcher->id() : 2 * MAX_THREADS);

   PVList    pv;
   DepthType height   = 1;
   DepthType seldepth = 0;
   ScoreType s        = 0;

   Position      pQuiet = p; // because p is given const
   NNUEEvaluator evaluator;
   pQuiet.associateEvaluator(evaluator);
   pQuiet.resetNNUEEvaluator(pQuiet.Evaluator());

   // go for a qsearch (no pruning, open bounds)
   cos.stopFlag  = false;
   cos.subSearch = true;
   s = cos.qsearchNoPruning(-10000, 10000, pQuiet, height, seldepth, &pv);
   cos.subSearch = false;
   if (qScore) *qScore = s;

   //std::cout << "pv : " << ToString(pv) << std::endl;

   // goto qsearch leaf
   for (auto& m : pv) {
      Position p2 = pQuiet;
      //std::cout << "Applying move " << ToString(m) << std::endl;
      if (!applyMove(p2, m, true)) break; // will always be ok because noValidation=true
      pQuiet = p2;
   }

   return pQuiet;
}

#ifdef WITH_GENFILE
void Searcher::writeToGenFile(const Position& p) {
   static uint64_t sfensWritten = 0;
   if (!genFen || id() >= MAX_THREADS) return;

   Searcher& cos = getCoSearcher(id());

   cos.genFen                       = false;
   const bool         oldQuiet      = DynamicConfig::quiet;
   const bool         oldDisableTT  = DynamicConfig::disableTT;
   const unsigned int oldLevel      = DynamicConfig::level;
   const unsigned int oldRandomOpen = DynamicConfig::randomOpen;
   const unsigned int oldRandomPly  = DynamicConfig::randomPly;

   // init sub search
   cos.subSearch             = true;
   DynamicConfig::quiet      = true;
   DynamicConfig::disableTT  = true; // do not use TT in order to get qsearch leaf node
   DynamicConfig::level      = 100;
   DynamicConfig::randomOpen = 0;
   cos.clearSearch(true);

   // look for a quiet position using qsearch
   ScoreType qScore = 0;
   Position  pQuiet = getQuiet(p, this, &qScore);

   ThreadData data;
   ScoreType  e = 0;
   if (std::abs(qScore) < 1000) {
      // evaluate quiet leaf position
      EvalData eData;
      e = eval(pQuiet, eData, cos, true, false);

      DynamicConfig::disableTT = false; // use TT here
      if (std::abs(e) < 1000) {
         const Hash matHash = MaterialHash::getMaterialHash(p.mat);
         float gp = 1;
         if (matHash != nullHash) {
            const MaterialHash::MaterialHashEntry& MEntry = MaterialHash::materialHashTable[matHash];
            gp                                            = MEntry.gamePhase();
         }
         DepthType depth(DynamicConfig::genFenDepth * gp + DynamicConfig::genFenDepthEG * (1.f - gp));
         DynamicConfig::randomPly = 0;
         data.p                   = pQuiet;
         data.depth               = depth;
         cos.setData(data);
         // do not update COM::position here
         cos.searchDriver();
         data = cos.getData();

         // std::cout << data << std::endl; // debug
      }
   }

   cos.genFen                = true;
   DynamicConfig::quiet      = oldQuiet;
   DynamicConfig::disableTT  = oldDisableTT;
   DynamicConfig::level      = oldLevel;
   DynamicConfig::randomOpen = oldRandomOpen;
   DynamicConfig::randomPly  = oldRandomPly;
   cos.subSearch             = false;

   // end of sub search

   if (data.best != INVALIDMOVE && pQuiet.halfmoves >= DynamicConfig::randomPly && std::abs(data.score) < 1000) {
      /*
        // epd format
            genStream << GetFEN(pQuiet) << " c0 \"" << e << "\" ;"          // eval
                                << " c1 \"" << s << "\" ;"             // score (search)
                                << " c2 \"" << ToString(m) << "\" ;"   // best move
                                //<< " c3 \"" << str.str() << "\" ;"   // features break down
                                << "\n";
        */

      // "plain" format (**not** taking result into account, also draw!)
      genStream << "fen " << GetFEN(pQuiet) << "\n"
                << "move " << ToString(data.best) << "\n"
                << "score " << data.score
                << "\n"
                //<< "eval "   << e << "\n"
                << "ply " << pQuiet.halfmoves << "\n"
                << "result " << 0 << "\n"
                << "e"
                << "\n";

      sfensWritten++;
      if (sfensWritten % 100'000 == 0) Logging::LogIt(Logging::logInfo) << "Sfens written " << sfensWritten;
   }
}
#endif
