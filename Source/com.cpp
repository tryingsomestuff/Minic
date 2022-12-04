#include "com.hpp"

#include "distributed.h"
#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "searcher.hpp"
#include "transposition.hpp"

namespace COM {

Protocol     protocol;
State        state;
std::string  command;
RootPosition position;
DepthType    depth;

GameInfo gameInfo;

GameInfo & GetGameInfo(){ return gameInfo; }

#ifdef WITH_NNUE
NNUEEvaluator evaluator;
#endif

std::mutex mutex;

void GameInfo::write(std::ostream & os) const{
   if(_gameStates.empty()) return;
   Logging::LogIt(Logging::logInfo) << "Writing game states (" + std::to_string(size()) + ")";

   os << "[Event \"Minic game\"]" << std::endl;
   os << "[FEN \"" + GetFEN(initialPos) + "\"]" << std::endl;

   uint16_t halfmoves = initialPos.halfmoves;
   uint16_t moves = initialPos.moves;
   auto prev = initialPos;
   for(const auto & gs : _gameStates){
      os << ((halfmoves++)%2?(std::to_string((moves++))+". ") : "") << showAlgAbr(gs.lastMove,prev) << " ";
      prev = gs.p;
   }
   os << std::endl;
   os << std::endl;
}

void GameInfo::clear(const Position & initial){
   Logging::LogIt(Logging::logInfo) << "Clearing game states";
   _gameStates.clear();
   initialPos = initial;
}

void GameInfo::append(const GameStateInfo & stateInfo){
   _gameStates.push_back(stateInfo);
   _gameStates.back().p.h = computeHash(_gameStates.back().p); // be sure to have an updated hash
}

size_t GameInfo::size() const {
   return _gameStates.size();
}

std::vector<Move> GameInfo::getMoves() const{
   std::vector<Move> moves;
   for(const auto & gs : _gameStates){
      moves.push_back(gs.lastMove);
   }
   return moves;
}

std::optional<Hash> GameInfo::getHash(uint16_t halfmove) const{
   const uint16_t startingPly = initialPos.halfmoves;
   const uint16_t offset = halfmove - startingPly;
   if( offset >= size()) return {};
   return offset == 0 ? computeHash(initialPos) : _gameStates[offset-1].p.h; // halfmove starts at 1, not 0 ...
}

std::optional<Move> GameInfo::getMove(uint16_t halfmove) const{
   const uint16_t startingPly = initialPos.halfmoves;
   const uint16_t offset = halfmove - startingPly;
   if( offset >= size()) return {};
   return offset == 0 ? INVALIDMOVE : _gameStates[offset-1].lastMove; // halfmove starts at 1, not 0 ...
}

std::optional<Position> GameInfo::getPosition(uint16_t halfmove) const{
   const uint16_t startingPly = initialPos.halfmoves;
   const uint16_t offset = halfmove - startingPly;
   if( offset >= size()) return {};
   return offset == 0 ? initialPos : _gameStates[offset-1].p; // halfmove starts at 1, not 0 ...
}

void newGame() {

   // write previous game
   if (DynamicConfig::pgnOut){
      std::ofstream os("games_" + std::to_string(GETPID()) + "_" + std::to_string(0) + ".pgn", std::ofstream::app);
      GetGameInfo().write(os);
      os.close();
   }

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
      const Position::MoveInfo moveInfo(p2, move);
      if (!(applyMove(p2, moveInfo) && isPseudoLegal(p2, ponderMove))) {
         Logging::LogIt(Logging::logInfo) << "Illegal ponder move " << ToString(ponderMove) << " " << ToString(p2);
         ponderMove = INVALIDMOVE; // do be sure ...
      }
   }

   bool ret = true;

   Logging::LogIt(Logging::logInfo) << "search async done (state " << static_cast<int>(state) << ")";
   
   // in searching only mode we have to return a move to GUI
   // but uci protocol also expect a bestMove when pondering or analysing to wake up the GUI
   if (state == st_searching || state == st_pondering || state == st_analyzing) {
      const std::string tag = Logging::ct == Logging::CT_uci ? "bestmove" : "move";
      Logging::LogIt(Logging::logInfo) << "sending move to GUI " << ToString(move);
      if (move == INVALIDMOVE) {
         // game ends (check mated / stalemate) **or** stop at the very begining of pondering
         ret = false;
         Logging::LogIt(Logging::logGUI) << tag << " " << "0000";
      }
      else {
         // update current position with given move (in fact this is not good for UCI protocol)
         if (!makeMove(move, true, tag, ponderMove)) {
            Logging::LogIt(Logging::logGUI) << "info string Bad move ! " << ToString(move);
            Logging::LogIt(Logging::logInfo) << ToString(position);
            ret = false;
         }
      }
   }
   Logging::LogIt(Logging::logInfo) << "Putting state to none (state was " << static_cast<int>(state) << ")";
   state = st_none;

   return ret;
}

bool makeMove(const Move m, const bool disp, const std::string & tag, const Move pondermove) {
#ifdef WITH_NNUE
   position.resetNNUEEvaluator(position.evaluator());
#endif
   const Position::MoveInfo moveInfo(position, m);
   const bool b = applyMove(position, moveInfo); // this update the COM::position position status
   if (disp && m != INVALIDMOVE) {
      Logging::LogIt(Logging::logGUI) << tag << " " << ToString(m)
                                      << (Logging::ct == Logging::CT_uci && isValidMove(pondermove) ? (" ponder " + ToString(pondermove)) : "");
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

Move moveFromCOM(const std::string & mstr) {
   Square from  = INVALIDSQUARE;
   Square to    = INVALIDSQUARE;
   MType  mtype = T_std;
   if (!readMove(position, trim(mstr), from, to, mtype)) return INVALIDMOVE;
   return ToMove(from, to, mtype);
}
} // namespace COM
