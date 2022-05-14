#include "com.hpp"

#include "distributed.h"
#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "searcher.hpp"
#include "transposition.hpp"

namespace COM {
Protocol          protocol;
State             state;
std::string       command;
RootPosition      position;
DepthType         depth;
std::vector<Move> moves;
#ifdef WITH_NNUE
NNUEEvaluator evaluator;
#endif

std::mutex mutex;

void newGame() {
#ifdef WITH_NNUE
   position.associateEvaluator(evaluator);
#endif
   readFEN(startPosition, position); // this set the COM::position position status

   // reset dynamic state depending on opponent
   DynamicConfig::ratingFactor      = 1.;
   DynamicConfig::ratingAdv         = 0;
   DynamicConfig::opponent          = "";
   DynamicConfig::ratingAdvReceived = false;

   // re-init all threads data
   ThreadPool::instance().clearGame();
}

void init(const Protocol pr) {
   Logging::LogIt(Logging::logInfo) << "Init COM";
   state = st_none;
   depth = -1;
   protocol = pr;
   newGame();
}

void readLine() {
   char buffer[4096]; // only usefull if WITH_MPI
   // only main process read stdin
   if (Distributed::isMainProcess()) {
      Logging::LogIt(Logging::logInfo) << "Waiting for input ...";
      command.clear();
      std::getline(std::cin, command);
      Logging::LogIt(Logging::logInfo) << "Received command : \"" << command << "\"";
      strcpy(buffer, command.c_str()); // only usefull if WITH_MPI
   }
   if (Distributed::moreThanOneProcess()) {
      // don't rely on Bcast to do a "passive wait", most implementation is doing a busy-wait, so use 100% cpu
      Distributed::asyncBcast(buffer, 4096, Distributed::_requestInput, Distributed::_commInput);
      Distributed::waitRequest(Distributed::_requestInput);
   }
   // other slave rank event loop
   if (!Distributed::isMainProcess()) { command = buffer; }
}

bool receiveMoves(const Move move, Move ponderMove) {
   Logging::LogIt(Logging::logInfo) << "...done returning move " << ToString(move) << " (state " << static_cast<int>(state) << ")";
   Logging::LogIt(Logging::logInfo) << "ponder move " << ToString(ponderMove);

   // share the same move with all process
   if (Distributed::moreThanOneProcess()) {
      Distributed::sync(Distributed::_commMove, __PRETTY_FUNCTION__);
      // don't rely on Bcast to do a "passive wait", most implementation is doing a busy-wait, so use 100% cpu
      Distributed::asyncBcast(&move, 1, Distributed::_requestMove, Distributed::_commMove);
      Distributed::waitRequest(Distributed::_requestMove);
   }

   // if possible, get a ponder move
   if (ponderMove != INVALIDMOVE) {
      Position p2 = position;
#ifdef WITH_NNUE
      NNUEEvaluator evaluator2;
      p2.associateEvaluator(evaluator2);
      p2.resetNNUEEvaluator(p2.evaluator());
#endif
      // apply best move and verify ponder move is ok
      if (!(applyMove(p2, move) && isPseudoLegal(p2, ponderMove))) {
         Logging::LogIt(Logging::logInfo) << "Illegal ponder move " << ToString(ponderMove) << " " << ToString(p2);
         ponderMove = INVALIDMOVE; // do be sure ...
      }
   }

   bool ret = true;

   Logging::LogIt(Logging::logInfo) << "search async done (state " << static_cast<int>(state) << ")";
   // in searching only mode we have to return a move to GUI
   // but curiously uci protocol also expect a bestMove when pondering to wake up the GUI
   if (state == st_searching || state == st_pondering) {
      const std::string tag = Logging::ct == Logging::CT_uci ? "bestmove" : "move";
      Logging::LogIt(Logging::logInfo) << "sending move to GUI " << ToString(move);
      if (move == INVALIDMOVE) {
         // game ends (check mated / stalemate) **or** stop at the very begining of pondering
         ret = false;
         Logging::LogIt(Logging::logGUI) << tag << " " << "0000";
      }
      else {
         if (!makeMove(move, true, tag, ponderMove)) {
            Logging::LogIt(Logging::logGUI) << "info string Bad move ! " << ToString(move);
            Logging::LogIt(Logging::logInfo) << ToString(position);
            ret = false;
         }
         else {
            // backup move (mainly for takeback feature)
            moves.push_back(move);
         }
      }
   }
   Logging::LogIt(Logging::logInfo) << "Putting state to none (state was " << static_cast<int>(state) << ")";
   state = st_none;

   return ret;
}

bool makeMove(const Move m, const bool disp, const std::string & tag, const Move pMove) {
#ifdef WITH_NNUE
   position.resetNNUEEvaluator(position.evaluator());
#endif
   bool b = applyMove(position, m, true); // this update the COM::position position status
   if (disp && m != INVALIDMOVE) {
      Logging::LogIt(Logging::logGUI) << tag << " " << ToString(m)
                                      << (Logging::ct == Logging::CT_uci && isValidMove(pMove) ? (" ponder " + ToString(pMove)) : "");
   }
   Logging::LogIt(Logging::logInfo) << ToString(position);
   return b;
}

void stop() {
   std::lock_guard<std::mutex> lock(mutex); // cannot stop and start at the same time
   Logging::LogIt(Logging::logInfo) << "Stopping previous search";
   ThreadPool::instance().stop();
}

void stopPonder() {
   if (state == st_pondering) { stop(); }
}

// this is a non-blocking call (search wise)
void thinkAsync(const COM::State givenState) {
   std::lock_guard<std::mutex> lock(mutex); // cannot stop and start at the same time
   Logging::LogIt(Logging::logInfo) << "Thinking... (state was " << static_cast<int>(state) << " going for " << static_cast<int>(givenState) << ")";
   if (depth < 0) depth = MAX_DEPTH;
   Logging::LogIt(Logging::logInfo) << "depth          " << static_cast<int>(depth);
   // here is computed the time for next search (and store it in the Threadpool for now)
   ThreadPool::instance().currentMoveMs = TimeMan::getNextMSecPerMove(position);
   Logging::LogIt(Logging::logInfo) << "currentMoveMs  " << ThreadPool::instance().currentMoveMs;
   Logging::LogIt(Logging::logInfo) << ToString(position);

   // will later be copied on all thread data and thus "reset" them
   ThreadData d;
   d.p           = position;
   d.depth       = depth;
   d.isAnalysis  = givenState == st_analyzing;
   d.isPondering = givenState == st_pondering;
   ThreadPool::instance().startSearch(d); // data is copied !
}

Move moveFromCOM(std::string mstr) { // copy string on purpose
   Square from  = INVALIDSQUARE;
   Square to    = INVALIDSQUARE;
   MType  mtype = T_std;
   if (!readMove(position, trim(mstr), from, to, mtype)) return INVALIDMOVE;
   return ToMove(from, to, mtype);
}
} // namespace COM
