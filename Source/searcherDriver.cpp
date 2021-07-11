#include "com.hpp"
#include "distributed.h"
#include "logging.hpp"
#include "searcher.hpp"
#include "skill.hpp"
#include "uci.hpp"
#include "xboard.hpp"

namespace {
// Sizes and phases of the skip-blocks, used for distributing search depths across the threads, from stockfish
const unsigned int threadSkipSize            = 20;
const int          skipSize[threadSkipSize]  = {1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
const int          skipPhase[threadSkipSize] = {0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7};
} // namespace

// Output following chosen protocol
void Searcher::displayGUI(DepthType          depth,
                          DepthType          seldepth,
                          ScoreType          bestScore,
                          unsigned int       ply,
                          const PVList&      pv,
                          int                multipv,
                          const std::string& mark) {
   const auto     now           = Clock::now();
   const TimeType ms            = std::max((TimeType)1, (TimeType)std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count());
   getSearchData().times[depth] = ms;
   if (subSearch) return; // no need to display stuff for subsearch
   std::stringstream str;
   const Counter     nodeCount = ThreadPool::instance().counter(Stats::sid_nodes) + ThreadPool::instance().counter(Stats::sid_qnodes);
   if (Logging::ct == Logging::CT_xboard) {
      str << int(depth) << " " << bestScore << " " << ms / 10 << " " << nodeCount << " ";
      if (DynamicConfig::fullXboardOutput)
         str << (int)seldepth << " " << Counter(nodeCount / (ms / 1000.f) / 1000.) << " "
             << ThreadPool::instance().counter(Stats::sid_tbHit1) + ThreadPool::instance().counter(Stats::sid_tbHit2);
      str << "\t" << ToString(pv);
      if (!mark.empty()) str << mark;
   }
   else if (Logging::ct == Logging::CT_uci) {
      const std::string multiPVstr = DynamicConfig::multiPV > 1 ? (" multipv " + std::to_string(multipv)) : "";
      str << "info" << multiPVstr << " depth " << int(depth) << " score " << UCI::uciScore(bestScore, ply) << " time " << ms << " nodes " << nodeCount
          << " nps " << Counter(nodeCount / (ms / 1000.f)) << " seldepth " << (int)seldepth << " tbhits "
          << ThreadPool::instance().counter(Stats::sid_tbHit1) + ThreadPool::instance().counter(Stats::sid_tbHit2);
      static auto lastHashFull = Clock::now();
      if ((int)std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHashFull).count() > 500 &&
          (TimeType)std::max(1, int(std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() * 2)) <
              ThreadPool::instance().main().getCurrentMoveMs()) {
         lastHashFull = now;
         str << " hashfull " << TT::hashFull();
      }
      str << " pv " << ToString(pv); //only add pv at the end (c-chess-cli doesn't like to read something after pv...)
   }
   Logging::LogIt(Logging::logGUI) << str.str();
}

void Searcher::searchDriver(bool postMove) {
   //stopFlag = false; // shall be only done outside to avoid race condition
   _height = 0;

   if (isMainThread()) Distributed::sync(Distributed::_commStat2, __PRETTY_FUNCTION__);

   // we start by a copy, because position object must be mutable here.
   Position p(_data.p);
#ifdef WITH_NNUE
   // Create an evaluator and reset it with the current position
   NNUEEvaluator nnueEvaluator;
   p.associateEvaluator(nnueEvaluator);
   p.resetNNUEEvaluator(nnueEvaluator);
#endif

   //if (isMainThread()) p.initCaslingPermHashTable(); // let's be sure ...

   // requested depth can be changed according to level or skill parameter
   DynamicConfig::level = DynamicConfig::limitStrength ? Skill::convertElo2Level() : DynamicConfig::level;
   DepthType maxDepth   = _data.depth; // _data.depth will be reset to zero later
   if (Distributed::isMainProcess()) {
      maxDepth = std::max((DepthType)1, (Skill::enabled() && !DynamicConfig::nodesBasedLevel) ? std::min(maxDepth, Skill::limitedDepth()) : maxDepth);
   }
   else {
      // other process performs infinite search
      maxDepth      = MAX_DEPTH;
      currentMoveMs = INFINITETIME; // overrides currentMoveMs
   }

   // when limiting skill by nodes only, only apply that to main process
   // this doesn't make much sense to apply limited nodes on a multi-process run anyway ...
   if (Distributed::isMainProcess() && DynamicConfig::nodesBasedLevel && Skill::enabled()) {
      TimeMan::maxNodes = Skill::limitedNodes();
      Logging::LogIt(Logging::logDebug) << "Limited nodes to fit level: " << TimeMan::maxNodes;
   }

   // initialize basic search variable
   moveDifficulty = MoveDifficultyUtil::MD_std;
   startTime      = Clock::now();

#ifdef WITH_GENFILE
   // open the genfen file for output if needed
   if (DynamicConfig::genFen && id() < MAX_THREADS && !genStream.is_open()) {
      genStream.open("genfen_" + std::to_string(::getpid()) + "_" + std::to_string(id()) + ".epd", std::ofstream::app);
   }
#endif

   // Main thread only will reset tables
   if (isMainThread() || id() >= MAX_THREADS) {
      if (isMainThread()) {
         TT::age();
         MoveDifficultyUtil::variability = 1.f; // not usefull for co-searcher threads that won't depend on time
         ThreadPool::instance().clearSearch();  // reset tables for all threads !
      }
      Logging::LogIt(Logging::logInfo) << "Search params :";
      Logging::LogIt(Logging::logInfo) << "requested time  " << getCurrentMoveMs() << " (" << currentMoveMs << ")"; // won't exceed TimeMan::maxTime
      Logging::LogIt(Logging::logInfo) << "requested depth " << (int)maxDepth;
   }
   // other threads will wait here for start signal
   else {
      Logging::LogIt(Logging::logInfo) << "helper thread waiting ... " << id();
      while (startLock.load()) { ; }
      Logging::LogIt(Logging::logInfo) << "... go for id " << id();
   }

   // fill "root" position stack data
   {
      EvalData  eData;
      ScoreType e = eval(p, eData, *this, false);
      assert(p.halfmoves < MAX_PLY && p.halfmoves >= 0);
      stack[p.halfmoves] = {p, computeHash(p), eData, e, INVALIDMINIMOVE};
   }

   // reset output search results
   _data.score    = 0;
   _data.best     = INVALIDMOVE;
   _data.seldepth = 0;
   _data.depth    = 0;
   _data.pv.clear();

   const bool isInCheck = isAttacked(p, kingSquare(p));
   const DepthType easyMoveDetectionDepth = 5;
   const DepthType startDepth = 1; //std::min(d,easyMoveDetectionDepth);

   // initialize multiPV stuff
   DynamicConfig::multiPV = (Logging::ct == Logging::CT_uci ? DynamicConfig::multiPV : 1);
   if (Skill::enabled() && !DynamicConfig::nodesBasedLevel) { DynamicConfig::multiPV = std::max(DynamicConfig::multiPV, 4u); }
   Logging::LogIt(Logging::logInfo) << "MultiPV " << DynamicConfig::multiPV;
   std::vector<RootScores> multiPVMoves(DynamicConfig::multiPV, {INVALIDMOVE, -MATE});
   // in multipv mode _data.score cannot be use a the aspiration loop score
   std::vector<ScoreType> currentScore(DynamicConfig::multiPV, 0);

   // handle "maxNodes" style search (will always complete depth 1 search)
   const auto maxNodes = TimeMan::maxNodes;
   // reset this for depth 1 to be sure to iterate at least once ...
   // on main process, requested value will be restored, but not on other process
   TimeMan::maxNodes = 0;

   // using MAX_DEPTH-6 so that draw can be found for sure ///@todo I don't understand this -6 anymore ..
   const DepthType targetMaxDepth = std::min(maxDepth, DepthType(MAX_DEPTH - 6));

   // random mover can be forced for the few first moves of a game or by setting level to 0
   if (DynamicConfig::level == 0 || p.halfmoves < DynamicConfig::randomPly) {
      if (p.halfmoves < DynamicConfig::randomPly) Logging::LogIt(Logging::logInfo) << "Randomized ply";
      _data.score = randomMover(p, _data.pv, isInCheck);
      goto pvsout;
   }

   // easy move detection (shallow open window search)
   // only main thread here (stopflag will be triggered anyway for other threads if needed)
   if (isMainThread() && DynamicConfig::multiPV == 1 && targetMaxDepth > easyMoveDetectionDepth + 5 && maxNodes == 0 &&
       currentMoveMs < INFINITETIME && currentMoveMs > 1000 && TimeMan::msecUntilNextTC > 0 && !getData().isPondering && !getData().isAnalysis) {
      rootScores.clear();
      _data.score = pvs<true>(-MATE, MATE, p, easyMoveDetectionDepth, 0, _data.pv, _data.seldepth, isInCheck, false, false);
      std::sort(rootScores.begin(), rootScores.end(), [](const RootScores& r1, const RootScores& r2) { return r1.s > r2.s; });
      if (stopFlag) { // no more time, this is strange ...
         goto pvsout;
      }
      if (rootScores.size() == 1) {
         moveDifficulty = MoveDifficultyUtil::MD_forced; // only one : check evasion or zugzwang
      }
      else if (rootScores.size() > 1 && rootScores[0].s > rootScores[1].s + MoveDifficultyUtil::easyMoveMargin) {
         moveDifficulty = MoveDifficultyUtil::MD_easy;
      }
   }

   // ID loop
   for (DepthType depth = startDepth; depth <= targetMaxDepth && !stopFlag; ++depth) {

      // MultiPV loop
      std::vector<MiniMove> skipMoves;
      for (unsigned int multi = 0; multi < DynamicConfig::multiPV && !stopFlag; ++multi) {
         // No need to continue multiPV loop if a mate is found
         if (!skipMoves.empty() && isMatedScore(currentScore[multi])) break;

         if (isMainThread()) {
            if (depth > 1) {
               // delayed other thread start (can use a depth condition...)
               if (startLock.load()) {
                  Logging::LogIt(Logging::logInfo) << "Unlocking other threads";
                  startLock.store(false);
               }
            }
         }
         // stockfish like thread management (not for co-searcher)
         else if (!subSearch) {
            const int i = (id() - 1) % threadSkipSize;
            if (((depth + skipPhase[i]) / skipSize[i]) % 2) continue; // next depth
         }

         Logging::LogIt(Logging::logInfo) << "Thread " << id() << " searching depth " << (int)depth;

#ifndef WITH_EVAL_TUNING
         contempt = {ScoreType((p.c == Co_White ? +1 : -1) * (DynamicConfig::contempt + DynamicConfig::contemptMG) * DynamicConfig::ratingFactor),
                     ScoreType((p.c == Co_White ? +1 : -1) * DynamicConfig::contempt * DynamicConfig::ratingFactor)};
#else
         contempt = 0;
#endif
         // dynamic contempt ///@todo tune this
         contempt += ScoreType(std::round(25 * std::tanh(currentScore[multi] / 400.f))); // slow but ok here
         Logging::LogIt(Logging::logInfo) << "Dynamic contempt " << contempt;

         // initialize aspiration window loop variables
         PVList    pvLoc;
         ScoreType delta =
             (SearchConfig::doWindow && depth > SearchConfig::aspirationMinDepth)
                 ? SearchConfig::aspirationInit + std::max(0, SearchConfig::aspirationDepthInit - SearchConfig::aspirationDepthCoef * depth)
                 : MATE; // MATE not INFSCORE in order to enter the loop below once
         ScoreType alpha       = std::max(ScoreType(currentScore[multi] - delta), ScoreType(-MATE));
         ScoreType beta        = std::min(ScoreType(currentScore[multi] + delta), MATE);
         ScoreType score       = 0;
         DepthType windowDepth = depth;

         // Aspiration loop
         while (!stopFlag) {
            pvLoc.clear();
            score =
                pvs<true>(alpha, beta, p, windowDepth, 0, pvLoc, _data.seldepth, isInCheck, false, false, skipMoves.empty() ? nullptr : &skipMoves);
            if (stopFlag) break;
            delta += 2 + delta / 2; // formula from xiphos ...
            if (alpha > -MATE && score <= alpha) {
               beta  = std::min(MATE, ScoreType((alpha + beta) / 2));
               alpha = std::max(ScoreType(score - delta), ScoreType(-MATE));
               Logging::LogIt(Logging::logInfo) << "Increase window alpha " << alpha << ".." << beta;
               if (isMainThread() && DynamicConfig::multiPV == 1) {
                  PVList pv2;
                  TT::getPV(p, *this, pv2);
                  displayGUI(depth, _data.seldepth, score, p.halfmoves, pv2, multi + 1, "!");
                  windowDepth = depth;
               }
            }
            else if (beta < MATE && score >= beta) {
               --windowDepth; // from Ethereal
               beta = std::min(ScoreType(score + delta), ScoreType(MATE));
               Logging::LogIt(Logging::logInfo) << "Increase window beta " << alpha << ".." << beta;
               if (isMainThread() && DynamicConfig::multiPV == 1) {
                  PVList pv2;
                  TT::getPV(p, *this, pv2);
                  displayGUI(depth, _data.seldepth, score, p.halfmoves, pv2, multi + 1, "?");
               }
            }
            else
               break;

         } // Aspiration loop end

         if (stopFlag) {
            if (multi != 0 && isMainThread()) {
               // handle multiPV display only based on previous ID iteration data
               PVList pvMulti;
               pvMulti.push_back(multiPVMoves[multi].m);
               displayGUI(depth - 1, 0, multiPVMoves[multi].s, p.halfmoves, pvMulti, multi + 1); ///@todo store pv and seldepth in multiPVMoves ?
            }
         }
         else {
            // this aspirasion multipv loop was fully done, let's update results
            _data.depth         = depth;
            currentScore[multi] = score;

            // In multiPV mode, fill skipmove for next multiPV iteration, and backup multiPV info
            if (!pvLoc.empty() && DynamicConfig::multiPV > 1) {
               skipMoves.push_back(Move2MiniMove(pvLoc[0]));
               multiPVMoves[multi].m = pvLoc[0];
               multiPVMoves[multi].s = score;
            }

            // update the outputed pv only with the best move line
            if (multi == 0) {
               std::unique_lock<std::mutex> lock(_mutexPV);
               _data.pv    = pvLoc;
               _data.depth = depth;
               _data.score = score;
            }

            if (isMainThread()) {
               // output to GUI
               displayGUI(depth, _data.seldepth, _data.score, p.halfmoves, pvLoc, multi + 1);
            }

            if (isMainThread() && multi == 0) {
               // store current depth info
               getSearchData().scores[depth] = _data.score;
               getSearchData().nodes[depth]  = ThreadPool::instance().counter(Stats::sid_nodes) + ThreadPool::instance().counter(Stats::sid_qnodes);
               if (pvLoc.size()) getSearchData().moves[depth] = Move2MiniMove(pvLoc[0]);

               // check for an emergency
               if (TimeMan::isDynamic && depth > MoveDifficultyUtil::emergencyMinDepth &&
                   _data.score < getSearchData().scores[depth - 1] - MoveDifficultyUtil::emergencyMargin) {
                  moveDifficulty = _data.score > MoveDifficultyUtil::emergencyAttackThreashold ? MoveDifficultyUtil::MD_hardAttack
                                                                                               : MoveDifficultyUtil::MD_hardDefense;
                  Logging::LogIt(Logging::logInfo) << "Emergency mode activated : " << _data.score << " < "
                                                   << getSearchData().scores[depth - 1] - MoveDifficultyUtil::emergencyMargin;
               }

               // update a "variability" measure to scale remaining time on it ///@todo tune this more
               if (depth > 12 && pvLoc.size()) {
                  if (getSearchData().moves[depth] != getSearchData().moves[depth - 1]) MoveDifficultyUtil::variability *= (1.f + float(depth) / 100); // slow but ok here
                  else
                     MoveDifficultyUtil::variability *= 0.97f;
                  Logging::LogIt(Logging::logInfo) << "Variability :" << MoveDifficultyUtil::variability;
                  Logging::LogIt(Logging::logInfo) << "Variability time factor :" << MoveDifficultyUtil::variabilityFactor();
               }

               // check for remaining time
               if (TimeMan::isDynamic &&
                   (TimeType)std::max(1, int(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count() * 1.8)) >
                       getCurrentMoveMs()) {
                  stopFlag = true;
                  Logging::LogIt(Logging::logInfo) << "stopflag triggered, not enough time for next depth";
                  break;
               }

               // compute EBF
               if (depth > 1) {
                  Logging::LogIt(Logging::logInfo) << "EBF  "
                                                   << float(getSearchData().nodes[depth]) / (std::max(Counter(1), getSearchData().nodes[depth - 1]));
                  Logging::LogIt(Logging::logInfo) << "EBF2 "
                                                   << float(ThreadPool::instance().counter(Stats::sid_qnodes)) /
                                                          std::max(Counter(1), ThreadPool::instance().counter(Stats::sid_nodes));
               }

               // sync (pull) stopflag in other process
               if (!Distributed::isMainProcess()) { Distributed::get(&ThreadPool::instance().main().stopFlag, 1, Distributed::_winStop, 0); }
            }
         }
      } // multiPV loop end

      // check for a node count stop
      if (isMainThread()) {
         // restore real value (only on main processus!), was discarded for depth 1 search
         if (Distributed::isMainProcess()) TimeMan::maxNodes = maxNodes; 
         const Counter nodeCount = ThreadPool::instance().counter(Stats::sid_nodes) + ThreadPool::instance().counter(Stats::sid_qnodes);
         if (TimeMan::maxNodes > 0 && nodeCount > TimeMan::maxNodes) {
            stopFlag = true;
            Logging::LogIt(Logging::logInfo) << "stopFlag triggered in search driver (nodes limits) in thread " << id();
         }
      }

   }    // iterative deepening loop end

pvsout:

   // for all thread, best move is set to first pv move (if there is one ...)
   // this will be changed later for main thread to take skill or best move into account
   if (_data.pv.empty()) _data.best = INVALIDMOVE; 
   else                  _data.best = _data.pv[0];

   if (isMainThread()) {
      // in case of very very short depth or time, "others" threads may still be blocked
      Logging::LogIt(Logging::logInfo) << "Unlocking other threads (end of search)";
      startLock.store(false);

      // all threads are updating there output values but main one is looking for the longest pv
      // note that depth, score, seldepth and pv are already updated on-the-fly
      if (_data.pv.empty()) {
         if (!subSearch) Logging::LogIt(Logging::logWarn) << "Empty pv";
      }
      else {
         // !!! warning: when skill uses multiPV returned move shall be used and not first move of pv in receiveMoves !!!
         if (Skill::enabled() && !DynamicConfig::nodesBasedLevel) { _data.best = Skill::pick(multiPVMoves); }
         else {
            // get pv from best (deepest) threads
            DepthType bestDepth    = _data.depth;
            size_t    bestThreadId = 0;
            for (auto& s : ThreadPool::instance()) {
               std::unique_lock<std::mutex> lock(_mutexPV);
               if (s->getData().depth > bestDepth) {
                  bestThreadId = s->id();
                  bestDepth    = s->getData().depth;
                  Logging::LogIt(Logging::logInfo) << "Better thread ! " << bestThreadId << ", depth " << (int)bestDepth;
               }
            }
            // update data with best data available
            _data      = ThreadPool::instance()[bestThreadId]->getData();
            _data.best = _data.pv[0];
         }
      }

#ifdef WITH_GENFILE
      // calling writeToGenFile at each root node
      if (DynamicConfig::genFen && p.halfmoves >= DynamicConfig::randomPly && DynamicConfig::level != 0) writeToGenFile(p);
#endif

      // wait for "ponderhit" or "stop" in case search returned too soon
      if (!stopFlag && (getData().isPondering || getData().isAnalysis)) {
         Logging::LogIt(Logging::logInfo) << "Waiting for ponderhit or stop ...";
         while (!stopFlag && (getData().isPondering || getData().isAnalysis)) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
         Logging::LogIt(Logging::logInfo) << "... ok";
      }

      // now send stopflag to all threads
      ThreadPool::instance().stop();
      // and wait for them
      ThreadPool::instance().wait(true);

      // sync point for distributed search
      Distributed::winFence(Distributed::_winStop);
      Distributed::syncStat();
      Distributed::syncTT();

      if (Distributed::isMainProcess()) {
         for (auto r = 1; r < Distributed::worldSize; ++r) {
            Distributed::get(&ThreadPool::instance().main().stopFlag, 1, Distributed::_winStop, r);
            Distributed::winFence(Distributed::_winStop);
            while (ThreadPool::instance().main().stopFlag) {
               Distributed::get(&ThreadPool::instance().main().stopFlag, 1, Distributed::_winStop, r);
               Distributed::winFence(Distributed::_winStop);
            }
         }
      }

      // display search statistics (only when all threads and process are done and sync)
      if (Distributed::moreThanOneProcess()) { Distributed::showStat(); }
      else {
         ThreadPool::instance().DisplayStats();
      }

      if (postMove) {
         // send move and ponder move to GUI
         const bool success = COM::receiveMoves(_data.best, _data.pv.size() > 1 ? _data.pv[1] : INVALIDMOVE);
         // update position state
         XBoard::moveApplied(success);
      }

   } // isMainThread()
}
