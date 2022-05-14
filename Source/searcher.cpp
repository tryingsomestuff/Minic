#include "searcher.hpp"

#include "com.hpp"
#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "xboard.hpp"

TimeType Searcher::getCurrentMoveMs()const{
   if (ThreadPool::instance().main().getData().isPondering || ThreadPool::instance().main().getData().isAnalysis) { return INFINITETIME; }

   TimeType ret = currentMoveMs;
   if (TimeMan::msecUntilNextTC > 0) {
      switch (moveDifficulty) {
         case MoveDifficultyUtil::MD_forced: ret = (ret >> 4); break; // only one move in movelist !
         case MoveDifficultyUtil::MD_easy: ret = (ret >> 3); break;   ///@todo this is not used anymore
         case MoveDifficultyUtil::MD_std: break;                      // nothing special
         case MoveDifficultyUtil::MD_hardAttack:
            break; // score is decreasing but still quite high ///@todo something ?
         case MoveDifficultyUtil::MD_hardDefense:
            ret = static_cast<TimeType>(std::min(TimeMan::msecUntilNextTC / MoveDifficultyUtil::maxStealDivisor, ret * MoveDifficultyUtil::emergencyFactor));
            break; // something bad is happening
      }
   }
   // take variability into account
   ret = std::min(TimeMan::maxTime, static_cast<TimeType>(static_cast<float>(ret) * MoveDifficultyUtil::variabilityFactor())); // inside [0.5 .. 2]
   return std::max(ret, TimeType(20)); // if not much time left, let's try something ...;
}

void Searcher::getCMHPtr(const unsigned int ply, CMHPtrArray& cmhPtr) {
   cmhPtr.fill(nullptr);
   for (unsigned int k = 0; k < MAX_CMH_PLY; ++k) {
      assert(int(ply) - int(2*k) < MAX_PLY && int(ply) - int(2*k) >= 0);
      if (ply > 2*k && isValidMove(stack[ply - 2*k].p.lastMove)) {
         const Position & pref = stack[ply - 2*k].p;
         const Square to = correctedMove2ToKingDest(pref.lastMove);
         cmhPtr[k] = historyT.counter_history[PieceIdx(pref.board_const(to))][to];
      }
   }
}

ScoreType Searcher::getCMHScore(const Position& p, const Square from, const Square to, const CMHPtrArray& cmhPtr) const {
   ScoreType ret = 0;
   for (int i = 0; i < MAX_CMH_PLY; i++) {
      if (cmhPtr[i]) { ret += cmhPtr[i][PieceIdx(p.board_const(from)) * NbSquare + to]; }
   }
   return ret/MAX_CMH_PLY;
}

bool Searcher::isCMHGood(const Position& p, const Square from, const Square to, const CMHPtrArray& cmhPtr, const ScoreType threshold) const {
   for (int i = 0; i < MAX_CMH_PLY; i++) {
      if (cmhPtr[i]) {
         if (cmhPtr[i][PieceIdx(p.board_const(from)) * NbSquare + to] >= threshold) return true;
      }
   }
   return false;
}

bool Searcher::isCMHBad(const Position& p, const Square from, const Square to, const CMHPtrArray& cmhPtr, const ScoreType threshold) const {
   int nbBad = 0;
   for (int i = 0; i < MAX_CMH_PLY; i++) {
      if (cmhPtr[i]) {
         if (cmhPtr[i][PieceIdx(p.board_const(from)) * NbSquare + to] < threshold) ++nbBad;
      }
   }
   return nbBad == MAX_CMH_PLY;
}

ScoreType Searcher::drawScore(const Position& p, DepthType height) {
   if (DynamicConfig::armageddon) {
      if (p.c == Co_White) return matedScore(height);
      else
         return matingScore(height-1);
   }
   return static_cast<ScoreType>(-1 + 2 * ((stats.counters[Stats::sid_nodes] + stats.counters[Stats::sid_qnodes]) % 2));
}

void Searcher::idleLoop() {
   while (true) {
      std::unique_lock<std::mutex> lock(_mutex);
      Logging::LogIt(Logging::logInfo) << "begin of idleloop " << id();
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
   Logging::LogIt(Logging::logInfo) << "Setting stopflag to false on worker " << id();
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

void Searcher::clearSearch(bool forceHistoryClear) {
#ifdef REPRODUCTIBLE_RESULTS
   clearPawnTT();
   forceHistoryClear = true;
#endif
   stats.init();
   killerT.initKillers();
   if(forceHistoryClear) historyT.initHistory();
   counterT.initCounter();
   previousBest = INVALIDMOVE;
}

bool Searcher::getPawnEntry(Hash h, PawnEntry*& pe) {
   assert(h != nullHash);
   PawnEntry& _e = tablePawn[h & (ttSizePawn - 1)];
   pe = &_e;
   if (_e.h != Hash64to32(h)) return false;
   ///@todo check for hash collision ?
   stats.incr(Stats::sid_ttPawnhits);
   return !DynamicConfig::disableTT;
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

Searcher& Searcher::getCoSearcher(size_t id) {
   static std::map<size_t, std::unique_ptr<Searcher>> coSearchers;
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
   pQuiet.resetNNUEEvaluator(pQuiet.evaluator());

   // go for a qsearch (no pruning, open bounds)
   cos.stopFlag  = false;
   cos.subSearch = true;
   cos.currentMoveMs = INFINITETIME;
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

struct GenFENEntry{
   std::string fen;
   Move m;
   ScoreType s;
   uint16_t ply;
   Color stm;
   void write(std::ofstream & genStream, int result) const {
      genStream << "fen " << fen << "\n"
                << "move " << ToString(m) << "\n"
                << "score " << s
                << "\n"
                //<< "eval "   << e << "\n"
                << "ply " << ply << "\n"
                << "result " << (stm == Co_White ? result : -result) << "\n"
                << "e"
                << "\n";
   }
};

void Searcher::writeToGenFile(const Position& p, bool getQuietPos, const ThreadData & d, const std::optional<int> result) {
   static uint64_t sfensWritten = 0;

   static std::vector<GenFENEntry> buffer;

   ThreadData data = d; // copy data from PV
   Position pLeaf = p; // copy current pos

   if(getQuietPos){

      Searcher& cos = getCoSearcher(id());

      const int          oldMinOutLvl  = DynamicConfig::minOutputLevel;
      const bool         oldDisableTT  = DynamicConfig::disableTT;
      const unsigned int oldLevel      = DynamicConfig::level;
      const unsigned int oldRandomOpen = DynamicConfig::randomOpen;
      const unsigned int oldRandomPly  = DynamicConfig::randomPly;

      // init sub search
      cos.subSearch                 = true;
      DynamicConfig::minOutputLevel = Logging::logMax;
      DynamicConfig::disableTT      = true; // do not use TT in order to get qsearch leaf node
      DynamicConfig::level          = 100;
      DynamicConfig::randomOpen     = 0;
      DynamicConfig::randomPly      = 0;
      cos.clearSearch(true);

      // look for a quiet position using qsearch
      ScoreType qScore = 0;
      pLeaf = getQuiet(p, this, &qScore);

      ScoreType  e = 0;
      if (std::abs(qScore) < 1000) {
         // evaluate quiet leaf position
         EvalData eData;
         e = eval(pLeaf, eData, cos, true, false);

         DynamicConfig::disableTT = false; // use TT here
         if (std::abs(e) < 1000) {
            const Hash matHash = MaterialHash::getMaterialHash(p.mat);
            float gp = 1;
            if (matHash != nullHash) {
               const MaterialHash::MaterialHashEntry& MEntry = MaterialHash::materialHashTable[matHash];
               gp                                            = MEntry.gamePhase();
            }
            DepthType depth = static_cast<DepthType>(clampDepth(DynamicConfig::genFenDepth) * gp + clampDepth(DynamicConfig::genFenDepthEG) * (1.f - gp));
            DynamicConfig::randomPly = 0;
            data.p                   = pLeaf;
            data.depth               = depth;
            cos.setData(data);
            cos.stopFlag = false;
            cos.currentMoveMs = INFINITETIME;
            // do not update COM::position here
            cos.searchDriver(false);
            data = cos.getData();
            // std::cout << data << std::endl; // debug
         }
      }

      DynamicConfig::minOutputLevel = oldMinOutLvl;
      DynamicConfig::disableTT      = oldDisableTT;
      DynamicConfig::level          = oldLevel;
      DynamicConfig::randomOpen     = oldRandomOpen;
      DynamicConfig::randomPly      = oldRandomPly;
      cos.subSearch                 = false;

      // end of sub search

   }

   if (data.best != INVALIDMOVE &&
      //pLeaf.halfmoves >= DynamicConfig::randomPly &&
      std::abs(data.score) < 1000) {
      buffer.push_back({GetFEN(pLeaf), data.best, data.score, pLeaf.halfmoves, pLeaf.c});
      sfensWritten++;
      if (sfensWritten % 100'000 == 0) Logging::LogIt(Logging::logInfoPrio) << "Sfens written " << sfensWritten;
   }

   if (result.has_value()){
      Logging::LogIt(Logging::logInfoPrio) << "Game ended, result " << result.value();
      for(const auto & entry : buffer){
         entry.write(genStream,result.value());
      }
      buffer.clear();
   }

}
#endif
