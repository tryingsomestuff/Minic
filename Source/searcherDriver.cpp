#include "com.hpp"
#include "distributed.h"
#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "searcher.hpp"
#include "skill.hpp"
#include "timeMan.hpp"
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
   const TimeType ms            = getTimeDiff(startTime);
   getSearchData().times[depth] = ms;
   if (subSearch) return; // no need to display stuff for subsearch
   std::stringstream str;
   const Counter nodeCount = ThreadPool::instance().counter(Stats::sid_nodes) + ThreadPool::instance().counter(Stats::sid_qnodes);
   if (Logging::ct == Logging::CT_xboard) {
      str << static_cast<int>(depth) << " "
          << bestScore << " "
          << ms / 10 << " " // csec
          << nodeCount << " ";
      if (DynamicConfig::fullXboardOutput)
         str << static_cast<int>(seldepth) << " "
             << static_cast<Counter>(nodeCount / ms) << " "  // knps
             << ThreadPool::instance().counter(Stats::sid_tbHit1) + ThreadPool::instance().counter(Stats::sid_tbHit2);
      str << "\t" << ToString(pv);
      if (!mark.empty()) str << mark;
   }
   else if (Logging::ct == Logging::CT_uci) {
      const std::string multiPVstr = DynamicConfig::multiPV > 1 ? (" multipv " + std::to_string(multipv)) : "";
      str << "info" << multiPVstr
          << " depth " << static_cast<int>(depth)
          << " score " << UCI::uciScore(bestScore, ply)
          << " time " << ms // msec
          << " nodes " << nodeCount
          << " nps " << static_cast<Counter>(static_cast<double>(nodeCount) / msec2sec(ms)) // nps
          << " seldepth " << static_cast<int>(seldepth) << " tbhits "
          << ThreadPool::instance().counter(Stats::sid_tbHit1) + ThreadPool::instance().counter(Stats::sid_tbHit2);
      static auto lastHashFull = Clock::now(); ///@todo slow here because of guard variable
      if (getTimeDiff(now,lastHashFull) > 500 &&
          getTimeDiff(now,startTime)*2 < ThreadPool::instance().main().getCurrentMoveMs()) {
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
   bool depthLimitedSearch = false;
   if (Distributed::isMainProcess()) {
      maxDepth = asLeastOne( (Skill::enabled() && !DynamicConfig::nodesBasedLevel) ? std::min(maxDepth, Skill::limitedDepth()) : maxDepth );
      depthLimitedSearch = maxDepth != _data.depth;
   }
   else {
      // other process performs infinite search
      maxDepth      = MAX_DEPTH;
      currentMoveMs = INFINITETIME; // overrides currentMoveMs
   }

   // when limiting skill by nodes only, only apply that to main process
   // this doesn't make much sense to apply limited nodes on a multi-process run anyway ...
   if (Distributed::isMainProcess() && Skill::enabled() && DynamicConfig::nodesBasedLevel) {
      TimeMan::maxNodes = Skill::limitedNodes();
      Logging::LogIt(Logging::logDebug) << "Limited nodes to fit level: " << TimeMan::maxNodes;
   }

   // initialize some search variable
   moveDifficulty = MoveDifficultyUtil::MD_std;
   positionEvolution = MoveDifficultyUtil::PE_std;
   startTime      = Clock::now();

   // check game history for potential situations (boom/moob)
   if (isMainThread()) {
      if (isBooming(p.halfmoves)) 
         positionEvolution = stack[p.halfmoves-2].eval > MoveDifficultyUtil::emergencyAttackThreashold ? MoveDifficultyUtil::PE_boomingAttack : 
                                                                                                         MoveDifficultyUtil::PE_boomingDefence;
      else if (isMoobing(p.halfmoves))
         positionEvolution = stack[p.halfmoves-2].eval > MoveDifficultyUtil::emergencyAttackThreashold ? MoveDifficultyUtil::PE_moobingAttack : 
                                                                                                         MoveDifficultyUtil::PE_moobingDefence;
   }

   // Main thread only will reset tables
   if (isMainThread()) {
      TT::age();
      MoveDifficultyUtil::variability = 1.f; // not usefull for co-searcher threads that won't depend on time
      ThreadPool::instance().clearSearch();  // reset tables for all threads !
   }
   if (isMainThread() || id() >= MAX_THREADS) {
      Logging::LogIt(Logging::logInfo) << "Search params :";
      Logging::LogIt(Logging::logInfo) << "requested time  " << getCurrentMoveMs() << " (" << currentMoveMs << ")"; // won't exceed TimeMan::maxTime
      Logging::LogIt(Logging::logInfo) << "requested depth " << static_cast<int>(maxDepth);
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
      ScoreType e = eval(p, eData, *this, true);
      assert(p.halfmoves < MAX_PLY && p.halfmoves >= 0);
      stack[p.halfmoves] = {p, computeHash(p), /*eData,*/ e, INVALIDMINIMOVE};
   }

   // reset output search results
   _data.reset();

   const bool isInCheck = isPosInCheck(p);

   // initialize multiPV stuff
   DynamicConfig::multiPV = (Logging::ct == Logging::CT_uci ? DynamicConfig::multiPV : 1);
   if (Skill::enabled() && !DynamicConfig::nodesBasedLevel) { DynamicConfig::multiPV = std::max(DynamicConfig::multiPV, 4u); }
   Logging::LogIt(Logging::logInfo) << "MultiPV " << DynamicConfig::multiPV;
   std::vector<MultiPVScores> multiPVMoves(DynamicConfig::multiPV, {INVALIDMOVE, matedScore(0), {}, 0});
   // in multipv mode _data.score cannot be use a the aspiration loop score
   std::vector<ScoreType> currentScore(DynamicConfig::multiPV, 0);

   // handle "maxNodes" style search (will always complete depth 1 search)
   const auto maxNodes = TimeMan::maxNodes;
   // reset this for depth 1 to be sure to iterate at least once ...
   // on main process, requested value will be restored, but not on other process
   TimeMan::maxNodes = 0;

   // using MAX_DEPTH-6 so that draw can be found for sure ///@todo I don't understand this -6 anymore ..
   const DepthType targetMaxDepth = std::min(maxDepth, DepthType(MAX_DEPTH - 6));

   const bool isFiniteTimeSearch = maxNodes == 0 && !depthLimitedSearch && currentMoveMs < INFINITETIME && TimeMan::msecUntilNextTC > 0 && !getData().isPondering && !getData().isAnalysis;

   // forced bongcloud
   if (DynamicConfig::bongCloud && (p.castling & (p.c == Co_White ? C_w_all : C_b_all)) ){
      constexpr Move wbc[5] = { ToMove(Sq_e1,Sq_e2,T_std), ToMove(Sq_e1,Sq_d1,T_std), ToMove(Sq_e1,Sq_f1,T_std), ToMove(Sq_e1,Sq_d2,T_std), ToMove(Sq_e1,Sq_f2,T_std)};
      constexpr Move bbc[5] = { ToMove(Sq_e8,Sq_e7,T_std), ToMove(Sq_e8,Sq_d8,T_std), ToMove(Sq_e8,Sq_f8,T_std), ToMove(Sq_e8,Sq_d7,T_std), ToMove(Sq_e8,Sq_f7,T_std)};
      MoveList moves;
      MoveGen::generate(p,moves);
      for (int i = 0 ; i < 5; ++i){
         const Move m = p.c == Co_White ? wbc[i] : bbc[i];
         for (const auto & it : moves){
            if(sameMove(m,it)){
               _data.score = 0;
               _data.pv.push_back(m);
               goto pvsout;
            }
         }
      }
   }

   // random mover can be forced for the few first moves of a game or by setting level to 0
   if (DynamicConfig::level == 0 || p.halfmoves < DynamicConfig::randomPly || currentMoveMs <= TimeMan::msecMinimal) {
      if (p.halfmoves < DynamicConfig::randomPly) Logging::LogIt(Logging::logInfo) << "Randomized ply";
      _data.score = randomMover(p, _data.pv, isInCheck);
      goto pvsout;
   }

   // forced move detection
   // only main thread here (stopflag will be triggered anyway for other threads if needed)
   if (isMainThread() && DynamicConfig::multiPV == 1 && isFiniteTimeSearch && currentMoveMs > 100) { ///@todo should work with nps here
      _data.score = pvs<true>(matedScore(0), matingScore(0), p, 1, 0, _data.pv, _data.seldepth, 0, isInCheck, false); // depth 1 search to get real valid moves
      // only one : check evasion or zugzwang
      if (rootScores.size() == 1) {
         moveDifficulty = MoveDifficultyUtil::MD_forced;
      }
   }

   // ID loop
   for (DepthType depth = 1; depth <= targetMaxDepth && !stopFlag; ++depth) {

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
            const auto i = (id() - 1) % threadSkipSize;
            if (((depth + skipPhase[i]) / skipSize[i]) % 2){
               Logging::LogIt(Logging::logInfo) << "Thread " << id() << " skipping depth " << static_cast<int>(depth);
               continue; // next depth
            }
         }

         Logging::LogIt(Logging::logInfo) << "Thread " << id() << " searching depth " << static_cast<int>(depth);

#ifndef WITH_EVAL_TUNING
         contempt = {ScoreType((p.c == Co_White ? +1 : -1) * (DynamicConfig::contempt + DynamicConfig::contemptMG) * DynamicConfig::ratingFactor),
                     ScoreType((p.c == Co_White ? +1 : -1) * DynamicConfig::contempt * DynamicConfig::ratingFactor)};
#else
         contempt = 0;
#endif
         // dynamic contempt ///@todo tune this
         contempt += static_cast<ScoreType>(std::round(25 * std::tanh(currentScore[multi] / 400.f))); // slow but ok here
         Logging::LogIt(Logging::logInfo) << "Dynamic contempt " << contempt;

         // initialize aspiration window loop variables
         PVList    pvLoc;
         ScoreType delta =
             (SearchConfig::doWindow && depth > SearchConfig::aspirationMinDepth)
                 ? SearchConfig::aspirationInit + std::max(0, SearchConfig::aspirationDepthInit - SearchConfig::aspirationDepthCoef * depth)
                 : matingScore(0); // MATE not INFSCORE in order to enter the loop below once
         ScoreType alpha       = std::max(static_cast<ScoreType>(currentScore[multi] - delta), matedScore(0));
         ScoreType beta        = std::min(static_cast<ScoreType>(currentScore[multi] + delta), matingScore(0));
         ScoreType score       = 0;
         DepthType windowDepth = depth;
         Logging::LogIt(Logging::logInfo) << "Inital window: " << alpha << ".." << beta;

         // Aspiration loop
         while (!stopFlag) {
            pvLoc.clear();
            score = pvs<true>(alpha, beta, p, windowDepth, 0, pvLoc, _data.seldepth, 0, isInCheck, false, skipMoves.empty() ? nullptr : &skipMoves);
            if (stopFlag) break;
            ScoreType matW =0, matB = 0;
            delta += ScoreType((2 + delta / 2) * exp(1.f - gamePhase(p.mat,matW,matB))); // in end-game, open window faster
            if (delta > 1000 ) delta = matingScore(0);
            if (alpha > matedScore(0) && score <= alpha) {
               beta  = std::min(matingScore(0), ScoreType((alpha + beta) / 2));
               alpha = std::max(ScoreType(score - delta), matedScore(0));
               Logging::LogIt(Logging::logInfo) << "Increase window alpha " << alpha << ".." << beta;
               if (isMainThread() && DynamicConfig::multiPV == 1) {
                  PVList pv2;
                  TT::getPV(p, *this, pv2);
                  displayGUI(depth, _data.seldepth, score, p.halfmoves, pv2, multi + 1, "?");
                  windowDepth = depth;
               }
            }
            else if (beta < matingScore(0) && score >= beta) {
               --windowDepth; // from Ethereal
               beta = std::min(ScoreType(score + delta), matingScore(0));
               Logging::LogIt(Logging::logInfo) << "Increase window beta " << alpha << ".." << beta;
               if (isMainThread() && DynamicConfig::multiPV == 1) {
                  PVList pv2;
                  TT::getPV(p, *this, pv2);
                  displayGUI(depth, _data.seldepth, score, p.halfmoves, pv2, multi + 1, "!");
               }
            }
            else
               break;

         } // Aspiration loop end

         if (stopFlag) {
            if (multi != 0 && isMainThread()) {
               // handle multiPV display only based on previous ID iteration data
               displayGUI(depth - 1, multiPVMoves[multi].seldepth, multiPVMoves[multi].s, p.halfmoves, multiPVMoves[multi].pv, multi + 1);
            }
         }
         else {
            // this aspiration multipv loop was fully done, let's update results
            _data.depth         = depth;
            currentScore[multi] = score;

            // In multiPV mode, fill skipmove for next multiPV iteration
            if (!pvLoc.empty() && DynamicConfig::multiPV > 1) {
               skipMoves.push_back(Move2MiniMove(pvLoc[0]));
            }

            // backup multiPV info
            if (!pvLoc.empty()){
               multiPVMoves[multi].m = pvLoc[0];
            }
            multiPVMoves[multi].s = score;
            multiPVMoves[multi].pv = pvLoc;
            multiPVMoves[multi].seldepth = _data.seldepth;

            // update the outputed pv only with the best move line
            if (multi == 0) {
               std::unique_lock<std::mutex> lock(_mutexPV);
               _data.pv    = pvLoc;
               _data.depth = depth;
               _data.score = score;
            }

            if (isMainThread()) {
               // output to GUI
               displayGUI(depth, _data.seldepth, multiPVMoves[multi].s, p.halfmoves, pvLoc, multi + 1);
            }

            if (isMainThread() && multi == 0) {
               // store current depth info
               getSearchData().scores[depth] = _data.score;
               getSearchData().nodes[depth]  = ThreadPool::instance().counter(Stats::sid_nodes) + ThreadPool::instance().counter(Stats::sid_qnodes);
               if (!pvLoc.empty()) getSearchData().moves[depth] = Move2MiniMove(pvLoc[0]);

               // check for an emergency : 
               // if IID reports decreasing score, we have to take action (like search for longer)
               if (TimeMan::isDynamic && 
                   depth > MoveDifficultyUtil::emergencyMinDepth && 
                   moveDifficulty == MoveDifficultyUtil::MD_std &&
                   _data.score < getSearchData().scores[depth - 1] - MoveDifficultyUtil::emergencyMargin) {
                  moveDifficulty = _data.score > MoveDifficultyUtil::emergencyAttackThreashold ? MoveDifficultyUtil::MD_moobAttackIID
                                                                                               : MoveDifficultyUtil::MD_moobDefenceIID;
                  Logging::LogIt(Logging::logInfo) << "Emergency mode activated : " << _data.score << " < "
                                                   << getSearchData().scores[depth - 1] - MoveDifficultyUtil::emergencyMargin;
               }

               // update a "variability" measure to scale remaining time on it ///@todo tune this more
               if (depth > 12 && !pvLoc.empty()) {
                  if (getSearchData().moves[depth] != getSearchData().moves[depth - 1] &&
                  std::fabs(getSearchData().scores[depth] - getSearchData().scores[depth-1]) > MoveDifficultyUtil::emergencyMargin/4 )
                     MoveDifficultyUtil::variability *= (1.f + float(depth) / 100);
                  else
                     MoveDifficultyUtil::variability *= 0.98f;
                  Logging::LogIt(Logging::logInfo) << "Variability :" << MoveDifficultyUtil::variability;
                  Logging::LogIt(Logging::logInfo) << "Variability time factor :" << MoveDifficultyUtil::variabilityFactor();
               }

               // check for remaining time
               if (TimeMan::isDynamic && static_cast<TimeType>(static_cast<double>(getTimeDiff(startTime))*1.8) > getCurrentMoveMs()) {
                  stopFlag = true;
                  Logging::LogIt(Logging::logInfo) << "stopflag triggered, not enough time for next depth";
                  break;
               }

               // compute EBF
               if (depth > 1) {
                  Logging::LogIt(Logging::logInfo) << "EBF  "
                                                   << getSearchData().nodes[depth] /
                                                      static_cast<double>(asLeastOne(getSearchData().nodes[depth - 1]));
                  Logging::LogIt(Logging::logInfo) << "EBF2 "
                                                   << ThreadPool::instance().counter(Stats::sid_qnodes) /
                                                      static_cast<double>(asLeastOne(ThreadPool::instance().counter(Stats::sid_nodes)));
               }

               // sync (pull) stopflag in other process
               if (!Distributed::isMainProcess()) {
                  bool masterStopFlag;
                  Distributed::get(&masterStopFlag, 1, Distributed::_winStopFromR0, 0, Distributed::_requestStopFromR0);
	               Distributed::waitRequest(Distributed::_requestStopFromR0);
                  ThreadPool::instance().main().stopFlag = masterStopFlag;
               }
            }
         }
      } // multiPV loop end

      // check for a node count stop
      if (isMainThread() || isStoppableCoSearcher) {
         // restore real value (only on main processus!), was discarded for depth 1 search
         if (Distributed::isMainProcess()) TimeMan::maxNodes = maxNodes;
         const Counter nodeCount = isStoppableCoSearcher ? stats.counters[Stats::sid_nodes] + stats.counters[Stats::sid_qnodes]
                                 : ThreadPool::instance().counter(Stats::sid_nodes) + ThreadPool::instance().counter(Stats::sid_qnodes);
         if (TimeMan::maxNodes > 0 && nodeCount > TimeMan::maxNodes) {
            stopFlag = true;
            Logging::LogIt(Logging::logInfo) << "stopFlag triggered in search driver (nodes limits) in thread " << id();
         }
      }

   } // iterative deepening loop end

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
         // !!! warning: when skill uses multiPV, returned move shall be used and not first move of pv in receiveMoves !!!
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
                  Logging::LogIt(Logging::logInfo) << "Better thread ! " << bestThreadId << ", depth " << static_cast<int>(bestDepth);
               }
            }
            // update data with best data available
            _data      = ThreadPool::instance()[bestThreadId]->getData();
            _data.best = _data.pv[0]; ///@todo this can lead to best move not being coherent with last reported PV
         }
         // update stack data on all searcher with "real" score
         // this way all stack[k (with k < p.halfmove)] will be "history" of the game accessible to searcher
         for (auto& s : ThreadPool::instance()) {
            s->stack[p.halfmoves].eval = _data.score;
         }
      }

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

      // gather various process stats
      Distributed::syncStat();
      // share our TT
      Distributed::syncTT();

      // display search statistics (only when all threads and process are done and sync)
      if (Distributed::moreThanOneProcess()) { Distributed::showStat(); }
      else {
         ThreadPool::instance().displayStats();
      }

      if (postMove) {
         // send move and ponder move to GUI
         const bool success = COM::receiveMoves(_data.best, _data.pv.size() > 1 && !(Skill::enabled() && !DynamicConfig::nodesBasedLevel) ? _data.pv[1] : INVALIDMOVE);
         if ( COM::protocol == COM::p_xboard){
            // update position state and history (this is only a xboard need)
            XBoard::moveApplied(success, _data.best);
         }
      }

   } // isMainThread()
}
