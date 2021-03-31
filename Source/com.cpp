#include "com.hpp"

#include "distributed.h"
#include "logging.hpp"
#include "searcher.hpp"
#include "transposition.hpp"

namespace COM {

    State state; // this is redundant with Mode & Ponder...
    std::string command;
    Position position;
    DepthType depth;
    std::vector<Move> moves;
#ifdef WITH_NNUE
    NNUEEvaluator evaluator;
#endif

    std::mutex mutex;

    void newgame() {
#ifdef WITH_NNUE
        position.associateEvaluator(evaluator);
#endif
        readFEN(startPosition, position);
        ThreadPool::instance().clearGame(); // re-init all threads data
    }

    void init() {
        Logging::LogIt(Logging::logInfo) << "Init COM";
        state = st_none;
        depth = -1;
        newgame();
    }

    void readLine() {
        char buffer[4096]; // only usefull if WITH_MPI
        // only main process read stdin
        if ( Distributed::isMainProcess()){ 
           command.clear();
           std::getline(std::cin, command);
           Logging::LogIt(Logging::logInfo) << "Received command : \"" << command << "\"";
           strcpy(buffer, command.c_str()); // only usefull if WITH_MPI 
        }
        if ( Distributed::worldSize > 1 ){
           // don't rely on Bcast to do a "passive wait", most implementation is doing a busy-wait, so use 100% cpu
           Distributed::asyncBcast(buffer,4096,Distributed::_requestInput,Distributed::_commInput); 
           Distributed::waitRequest(Distributed::_requestInput);
        }
        // other slave rank event loop
        if ( !Distributed::isMainProcess()){ 
           command = buffer;
        }
    }

    bool receiveMoves(Move move, Move ponderMove){
        Logging::LogIt(Logging::logInfo) << "...done returning move " << ToString(move) << " (state " << (int)state << ")";
        Logging::LogIt(Logging::logInfo) << "ponder move " << ToString(ponderMove);

        // share the same move with all process 
        if ( Distributed::worldSize > 1 ){
           Distributed::sync(Distributed::_commMove,__PRETTY_FUNCTION__);
           // don't rely on Bcast to do a "passive wait", most implementation is doing a busy-wait, so use 100% cpu
           Distributed::asyncBcast(&move,1,Distributed::_requestMove,Distributed::_commMove);
           Distributed::waitRequest(Distributed::_requestMove);
        }

        // if possible get a ponder move
        if ( ponderMove != INVALIDMOVE) {
            Position p2 = position;
#ifdef WITH_NNUE
            NNUEEvaluator evaluator2;
            p2.associateEvaluator(evaluator2);
            p2.resetNNUEEvaluator(p2.Evaluator());
#endif
            if ( !(applyMove(p2,move) && isPseudoLegal(p2,ponderMove))){
                Logging::LogIt(Logging::logInfo) << "Illegal ponder move" << ToString(ponderMove) << ToString(p2);
                ponderMove = INVALIDMOVE; // do be sure ...
            }
        }

        bool ret = true;

        Logging::LogIt(Logging::logInfo) << "search async done (state " << (int)state << ")";
        if (state == st_searching) { // in searching mode we have to return a move to GUI
            Logging::LogIt(Logging::logInfo) << "sending move to GUI " << ToString(move);
            if (move == INVALIDMOVE) { 
                ret = false;
            } // game ends
            else {
                if (!makeMove(move, true, Logging::ct == Logging::CT_uci ? "bestmove" : "move", ponderMove)) {
                    Logging::LogIt(Logging::logGUI) << "info string Bad computer move !";
                    Logging::LogIt(Logging::logInfo) << ToString(position);
                    ret = false;
                }
                else{
                    // backup move (mainly for takeback feature)
                    moves.push_back(move);
                }
            }
        }
        Logging::LogIt(Logging::logInfo) << "Putting state to none (state was " << (int)state << ")";
        state = st_none;

        return ret;
    }

    bool makeMove(Move m, bool disp, std::string tag, Move pMove) {
#ifdef WITH_NNUE
        position.resetNNUEEvaluator(position.Evaluator());
#endif
        bool b = applyMove(position, m, true);
        if (disp && m != INVALIDMOVE){
            Logging::LogIt(Logging::logGUI) << tag << " " << ToString(m) 
                                                   << (Logging::ct==Logging::CT_uci && VALIDMOVE(pMove) ? (" ponder " + ToString(pMove)) : "");
        }
        Logging::LogIt(Logging::logInfo) << ToString(position);
        return b;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(mutex); // cannot stop and start at the same time
        Logging::LogIt(Logging::logInfo) << "Stopping previous search";
        ThreadPool::instance().stop();
        // some state shall be reset here
        ThreadPool::instance().main().isAnalysis = false;
        ThreadPool::instance().main().isPondering = false;
    }

    void stopPonder() {
        if (state == st_pondering) {
            stop();
        }
    }

    // this is a non-blocking call (search wise)
    void thinkAsync() {
        std::lock_guard<std::mutex> lock(mutex); // cannot stop and start at the same time
        Logging::LogIt(Logging::logInfo) << "Thinking... (state " << (int)state << ")";
        if (depth < 0) depth = MAX_DEPTH;
        Logging::LogIt(Logging::logInfo) << "depth          " << (int)depth;
        // here is computed the time for next search (and store it in the Threadpool for now)
        ThreadPool::instance().currentMoveMs = TimeMan::GetNextMSecPerMove(position);
        Logging::LogIt(Logging::logInfo) << "currentMoveMs  " << ThreadPool::instance().currentMoveMs;
        Logging::LogIt(Logging::logInfo) << ToString(position);

        // will later be copied on all thread data and thus "reset" them
        ThreadData d;
        d.p = position;
        d.depth = depth;        
        ThreadPool::instance().startSearch(d); // data is copied !
    }

    Move moveFromCOM(std::string mstr) { // copy string on purpose
        Square from = INVALIDSQUARE;
        Square to   = INVALIDSQUARE;
        MType mtype = T_std;
        if (!readMove(position, trim(mstr), from, to, mtype)) return INVALIDMOVE;
        return ToMove(from, to, mtype);
    }
}
