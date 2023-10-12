#include "searcher.hpp"

#include "com.hpp"
#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "moveApply.hpp"
#include "xboard.hpp"

TimeType Searcher::getCurrentMoveMs()const{
   if (ThreadPool::instance().main().getData().isPondering || ThreadPool::instance().main().getData().isAnalysis) { return INFINITETIME; }

   TimeType ret = currentMoveMs;
   if (TimeMan::msecUntilNextTC > 0) {
      bool extented = false;
      switch (moveDifficulty) {
         case MoveDifficultyUtil::MD_forced: 
            // only one move in movelist !
            ret = (ret >> 4); 
            break; 
         case MoveDifficultyUtil::MD_easy: 
            ///@todo this is not used anymore
            ret = (ret >> 3); 
            break;   
         case MoveDifficultyUtil::MD_std: 
            // nothing special
            break;
         case MoveDifficultyUtil::MD_moobAttackIID:
            // score is decreasing during IID but still quite high (IID moob, sharp position)
            ret = static_cast<TimeType>(std::min(TimeMan::msecUntilNextTC / MoveDifficultyUtil::maxStealDivisor, ret * MoveDifficultyUtil::emergencyFactorIIDGood));
            extented = true;
            break; 
         case MoveDifficultyUtil::MD_moobDefenceIID:
            // score is decreasing during IID and it's not smelling good (sharp position)
            extented = true;
            ret = static_cast<TimeType>(std::min(TimeMan::msecUntilNextTC / MoveDifficultyUtil::maxStealDivisor, ret * MoveDifficultyUtil::emergencyFactorIID));
            break; 
      }
      if (!extented) {
         switch (positionEvolution) {
            case MoveDifficultyUtil::PE_none:
            case MoveDifficultyUtil::PE_std: 
               // nothing special
               break;
            case MoveDifficultyUtil::PE_boomingAttack:
               // let's validate this a little more
               ret = static_cast<TimeType>(std::min(TimeMan::msecUntilNextTC / MoveDifficultyUtil::maxStealDivisor, ret * MoveDifficultyUtil::emergencyFactorBoomHistory));
               break; 
            case MoveDifficultyUtil::PE_boomingDefence:
               // let's validate this a little more
               ret = static_cast<TimeType>(std::min(TimeMan::msecUntilNextTC / MoveDifficultyUtil::maxStealDivisor, ret * MoveDifficultyUtil::emergencyFactorBoomHistory));
               break; 
            case MoveDifficultyUtil::PE_moobingAttack:
               // let's try to understand better
               ret = static_cast<TimeType>(std::min(TimeMan::msecUntilNextTC / MoveDifficultyUtil::maxStealDivisor, ret * MoveDifficultyUtil::emergencyFactorMoobHistory));
               break; 
            case MoveDifficultyUtil::PE_moobingDefence:
               // let's try to defend
               ret = static_cast<TimeType>(std::min(TimeMan::msecUntilNextTC / MoveDifficultyUtil::maxStealDivisor, ret * MoveDifficultyUtil::emergencyFactorMoobHistory));
               break; 
         }
      }
   }
   // take variability into account
   ret = std::min(TimeMan::maxTime, static_cast<TimeType>(static_cast<float>(ret) * MoveDifficultyUtil::variabilityFactor())); // inside [0.5 .. 2]
   return std::max(ret, static_cast<TimeType>(20)); // if not much time left, let's try something ...;
}

void Searcher::getCMHPtr(const unsigned int ply, CMHPtrArray& cmhPtr) {
   cmhPtr.fill(nullptr);
   for (unsigned int k = 0; k < MAX_CMH_PLY; ++k) {
      assert(static_cast<int>(ply) - static_cast<int>(2*k) < MAX_PLY && static_cast<int>(ply) - static_cast<int>(2*k) >= 0);
      if (ply > 2*k && isValidMove(stack[ply - 2*k].p.lastMove)) {
         const Position & pRef = stack[ply - 2*k].p;
         const Square to = correctedMove2ToKingDest(pRef.lastMove);
         const int ptIdx = isEnPassant(pRef.lastMove) ? PieceIdx(P_wp) : PieceIdx(pRef.board_const(to));
         cmhPtr[k] = &historyT.counter_history[ptIdx][to];
      }
   }
}

bool Searcher::isBooming(uint16_t halfmove){
   assert(halfmove >= 0);
   if (halfmove < 4) { return false; }  // no booming at the beginning of the game of course
   if (stack[halfmove-2].h == nullHash) { return false; } // no record in previous state
   if (stack[halfmove-4].h == nullHash) { return false; } // no record in former state
   constexpr ScoreType boomMargin = 80;
   return stack[halfmove-4].eval <= stack[halfmove-2].eval + boomMargin;
}

bool Searcher::isMoobing(uint16_t halfmove){
   assert(halfmove >= 0);
   if (halfmove < 4) { return false; }  // no moobing at the beginning of the game of course
   if (stack[halfmove-2].h == nullHash) { return false; } // no record in previous state
   if (stack[halfmove-4].h == nullHash) { return false; } // no record in former state
   constexpr ScoreType moobMargin = 80;
   return stack[halfmove-4].eval >= stack[halfmove-2].eval + moobMargin;
}

ScoreType Searcher::getCMHScore(const Position& p, const Square from, const Square to, const CMHPtrArray& cmhPtr) const {
   ScoreType ret = 0;
   for (int i = 0; i < MAX_CMH_PLY; i++) {
      if (cmhPtr[i]) { ret += (*cmhPtr[i])[PieceIdx(p.board_const(from)) * NbSquare + to]; }
   }
   return ret/MAX_CMH_PLY;
}

bool Searcher::isCMHGood(const Position& p, const Square from, const Square to, const CMHPtrArray& cmhPtr, const ScoreType threshold) const {
   for (int i = 0; i < MAX_CMH_PLY; i++) {
      if (cmhPtr[i]) {
         const auto cmhScore = (*cmhPtr[i])[PieceIdx(p.board_const(from)) * NbSquare + to];
         if (cmhScore >= threshold){
               /*
               std::cout << ToString(p) << std::endl;
               std::cout << SquareNames[from] << std::endl;
               std::cout << SquareNames[to] << std::endl;
               std::cout << cmhScore << std::endl;
               */
               return true;
         }
      }
   }
   return false;
}

bool Searcher::isCMHBad(const Position& p, const Square from, const Square to, const CMHPtrArray& cmhPtr, const ScoreType threshold) const {
   int nbBad = 0;
   for (int i = 0; i < MAX_CMH_PLY; i++) {
      if (cmhPtr[i]) {
         if ((*cmhPtr[i])[PieceIdx(p.board_const(from)) * NbSquare + to] < threshold) ++nbBad;
      }
   }
   return nbBad == MAX_CMH_PLY;
}

ScoreType Searcher::drawScore(const Position& p, DepthType height) const {
   // handles chess variants
   ///@todo other chess variants
   if (DynamicConfig::armageddon) {
      if (p.c == Co_White) return matedScore(height);
      else return matingScore(height-1);
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

void Searcher::clearGame() {
   clearPawnTT();
   stats.init();
   killerT.initKillers();
   historyT.initHistory();
   counterT.initCounter();
   previousBest = INVALIDMOVE;

   // clear stack data
   for (auto & d : stack){
      d = {Position(), nullHash, 0, INVALIDMINIMOVE};
   }
}

void Searcher::clearSearch(bool forceHistoryClear) {
#ifdef REPRODUCTIBLE_RESULTS
   clearPawnTT();
   forceHistoryClear = true;
#endif
   stats.init();
   killerT.initKillers();
   if (forceHistoryClear) historyT.initHistory();
   counterT.initCounter();
   previousBest = INVALIDMOVE;
}

void Searcher::initPawnTable() {
   Logging::LogIt(Logging::logInfo) << "Init Pawn TT (one per thread)";
   Logging::LogIt(Logging::logInfo) << "PawnEntry size " << sizeof(PawnEntry);
   ttSizePawn = powerFloor((SIZE_MULTIPLIER * DynamicConfig::ttPawnSizeMb / DynamicConfig::threads) / sizeof(PawnEntry));
   assert(BB::countBit(ttSizePawn) == 1); // a power of 2 and not 0 ...
   tablePawn.reset(new PawnEntry[ttSizePawn]);
   Logging::LogIt(Logging::logInfo) << "Size of Pawn TT " << ttSizePawn * sizeof(PawnEntry) / 1024 << "Kb (" << ttSizePawn << " entries)";
}

void Searcher::clearPawnTT() {
   for (unsigned int k = 0; k < ttSizePawn; ++k) tablePawn[k].h = nullHash;
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
   if (coSearchers.contains(id)) {
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
#ifdef WITH_NNUE
   NNUEEvaluator evaluator;
   pQuiet.associateEvaluator(evaluator);
   pQuiet.resetNNUEEvaluator(pQuiet.evaluator());
#endif

   // go for a qsearch (no pruning, open bounds)
   cos.stopFlag  = false;
   cos.subSearch = true;
   cos.currentMoveMs = INFINITETIME;
   s = cos.qsearchNoPruning(-10000, 10000, pQuiet, height, seldepth, &pv);
   cos.subSearch = false;
   if (qScore) *qScore = s;

   //std::cout << "pv : " << ToString(pv) << std::endl;

   // goto qsearch leaf
   for (const auto& m : pv) {
      Position p2 = pQuiet;
      //std::cout << "Applying move " << ToString(m) << std::endl;
      if (const MoveInfo moveInfo(p2,m); !applyMove(p2, moveInfo)) break;
      pQuiet = p2;
   }

   pQuiet.dissociateEvaluator();
   return pQuiet;
}

#ifdef WITH_GENFILE

struct GenFENEntry{
   std::string fen;
   Move m;
   ScoreType s;
   uint16_t ply;
   Color stm;
   void write(std::ofstream & stream, int result) const {
      stream << "fen " << fen << "\n"
             << "move " << ToString(m) << "\n"
             << "score " << s << "\n"
             //<< "eval "   << e << "\n"
             << "ply " << ply << "\n"
             << "result " << (stm == Co_White ? result : -result) << "\n"
             << "e" << "\n";
   }
   ///@todo writeBinary
};

void Searcher::writeToGenFile(const Position& p, bool getQuietPos, const ThreadData & d, const std::optional<int> result) {
   static uint64_t sfensWritten = 0;

   static std::vector<GenFENEntry> buffer;

   ThreadData data = d; // copy data from PV
   Position pLeaf = p; // copy current pos

   if (getQuietPos){

      Searcher& cos = getCoSearcher(id());

      const int          oldMinOutLvl  = DynamicConfig::minOutputLevel;
      const bool         oldDisableTT  = DynamicConfig::disableTT;
      const unsigned int oldLevel      = DynamicConfig::level;
      const unsigned int oldRandomOpen = DynamicConfig::randomOpen;
      const unsigned int oldRandomPly  = DynamicConfig::randomPly;

      // init sub search
      DynamicConfig::minOutputLevel = Logging::logMax;
      DynamicConfig::disableTT      = true; // do not use TT in order to get qsearch leaf node
      DynamicConfig::level          = 100;
      DynamicConfig::randomOpen     = 0;
      DynamicConfig::randomPly      = 0;
      cos.clearSearch(true);

      ///@todo in the following code the evaluator will be reset 3 times ! (getQuiet, before eval, at the beginning of searchDriver) ...

      // look for a quiet position using qsearch
      ScoreType qScore = 0;
      pLeaf = getQuiet(p, this, &qScore);

      ScoreType  e = 0;
      if (Abs(qScore) < 1000) {
#ifdef WITH_NNUE
         NNUEEvaluator evaluator;
         pLeaf.associateEvaluator(evaluator);
         pLeaf.resetNNUEEvaluator(pLeaf.evaluator());
#endif
         // evaluate quiet leaf position
         EvalData eData;
         e = eval(pLeaf, eData, cos, true, false);

         DynamicConfig::disableTT = false; // use TT here
         if (Abs(e) < 1000) {
            const Hash matHash = MaterialHash::getMaterialHash(p.mat);
            float gp = 1;
            if (matHash != nullHash) {
               const MaterialHash::MaterialHashEntry& MEntry = MaterialHash::materialHashTable[matHash];
               gp                                            = MEntry.gamePhase();
            }
            DepthType depth = static_cast<DepthType>(clampDepth(DynamicConfig::genFenDepth) * gp + clampDepth(DynamicConfig::genFenDepthEG) * (1.f - gp));
            DynamicConfig::randomPly = 0;
            data.p     = pLeaf;
            data.depth = depth;
            cos.setData(data);
            cos.stopFlag = false;
            cos.currentMoveMs = INFINITETIME;
            // do not update COM::position here
            cos.subSearch = true;
            cos.isStoppableCoSearcher = true;
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
   // skip when bestmove is capture or when you have not reached randomPly limit yet
   else{
      if(isCapture(data.best) || pLeaf.halfmoves <= DynamicConfig::randomPly || pLeaf.halfmoves <= 10){
         ///@todo research for early move impacted with randomOpen ?
         data.best = INVALIDMOVE;
      }
   }

   if (data.best != INVALIDMOVE && Abs(data.score) < 1500) {
      buffer.emplace_back(GetFEN(pLeaf), data.best, data.score, pLeaf.halfmoves, pLeaf.c);
      ++sfensWritten;
      if (sfensWritten % 10'000 == 0) Logging::LogIt(Logging::logInfoPrio) << "Sfens written " << sfensWritten;
   }

   if (result.has_value()){
      Logging::LogIt(Logging::logInfo) << "Game ended, result " << result.value();
      for (const auto & entry : buffer){
         entry.write(genStream,result.value());
      }
      buffer.clear();
   }

}
#endif
