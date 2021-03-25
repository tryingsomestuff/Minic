#include "com.hpp"

#include "distributed.h"
#include "logging.hpp"
#include "searcher.hpp"
#include "transposition.hpp"

namespace COM {

    State state; // this is redundant with Mode & Ponder...
    Ponder ponder;
    std::string command;
    Position position;
    Move move, ponderMove;
    DepthType depth;
    Mode mode;
    SideToMove stm; ///@todo isn't this redundant with position.c ??
    Position initialPos;
    std::vector<Move> moves;
    std::future<void> f;
#ifdef WITH_NNUE
    NNUEEvaluator evaluator;
#endif

    void newgame() {
        mode = m_force;
        stm = stm_white;
#ifdef WITH_NNUE
        position.associateEvaluator(evaluator);
#endif
        readFEN(startPosition, position);
        ThreadPool::instance().clearGame(); // re-init all threads data
    }

    void init() {
        Logging::LogIt(Logging::logInfo) << "Init COM";
        ponderMove = INVALIDMOVE;
        move = INVALIDMOVE;
        ponder = p_off;
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

    SideToMove opponent(SideToMove & s) {
        return s == stm_white ? stm_black : stm_white;
    }

    bool sideToMoveFromFEN(const std::string & fen) {
        const bool b = readFEN(fen, position,true);
        stm = position.c == Co_White ? stm_white : stm_black;
        if (!b) Logging::LogIt(Logging::logFatal) << "Illegal FEN " << fen;
        return b;
    }

    void receivePv(const PVList & pv){
        move = INVALIDMOVE;
        if ( pv.size()) move = pv[0];

        const Move m = ThreadPool::instance().main().getData().best; // here output results
        assert(m == pv[0]);
        Logging::LogIt(Logging::logInfo) << "...done returning move " << ToString(m) << " (state " << (int)state << ")";

        // share the same move with all process 
        if ( Distributed::worldSize > 1 ){
           Distributed::sync(Distributed::_commMove,__PRETTY_FUNCTION__);
           // don't rely on Bcast to do a "passive wait", most implementation is doing a busy-wait, so use 100% cpu
           Distributed::asyncBcast(&m,1,Distributed::_requestMove,Distributed::_commMove);
           Distributed::waitRequest(Distributed::_requestMove);
        }

        // if position get a ponder move
        ponderMove = INVALIDMOVE;
        if ( pv.size() > 1) {
            Position p2 = position;
#ifdef WITH_NNUE
            NNUEEvaluator evaluator2;
            p2.associateEvaluator(evaluator2);
            p2.resetNNUEEvaluator(p2.Evaluator());
#endif
            if ( applyMove(p2,pv[0]) && isPseudoLegal(p2,pv[1])) ponderMove = pv[1];
        }

        Logging::LogIt(Logging::logInfo) << "search async done (state " << (int)state << ")";
        if (state == st_searching) { // in searching mode we have to return a move to GUI
            Logging::LogIt(Logging::logInfo) << "sending move to GUI " << ToString(move);
            if (move == INVALIDMOVE) { mode = m_force; } // game ends
            else {
                if (!makeMove(move, true, Logging::ct == Logging::CT_uci ? "bestmove" : "move", ponderMove)) {
                    Logging::LogIt(Logging::logGUI) << "info string Bad computer move !";
                    Logging::LogIt(Logging::logInfo) << ToString(position);
                    mode = m_force;
                }
                else{
                    // switch stm
                    stm = opponent(stm);
                    // backup move (mainly for takeback feature)
                    moves.push_back(move);
                }
            }
        }
        Logging::LogIt(Logging::logInfo) << "Putting state to none (state was" << (int)state << ")";
        state = st_none;
    }

    bool makeMove(Move m, bool disp, std::string tag, Move pMove) {
#ifdef WITH_NNUE
        position.resetNNUEEvaluator(position.Evaluator());
#endif
        bool b = applyMove(position, m, true);
        if (disp && m != INVALIDMOVE) Logging::LogIt(Logging::logGUI) << tag << " " << ToString(m) << (Logging::ct==Logging::CT_uci && VALIDMOVE(pMove) ? (" ponder " + ToString(pMove)) : "");
        Logging::LogIt(Logging::logInfo) << ToString(position);
        return b;
    }

    void stop() {
        Logging::LogIt(Logging::logInfo) << "stopping previous search";
        ThreadPool::instance().stop();
        if ( f.valid() ){
           Logging::LogIt(Logging::logInfo) << "wait for future to land ...";
           f.wait(); // synchronous wait of current future
           Logging::LogIt(Logging::logInfo) << "...ok future is terminated";
        }
    }

    void stopPonder() {
        if (state == st_pondering) {
            stop();
        }
    }

    ///@todo should not use an async, instead just call main().start() and treat return move at the end of search
    void thinkAsync(TimeType forcedMs) { 
        f = std::async(std::launch::async, [forcedMs] { ///@todo do not use asyncs
            Logging::LogIt(Logging::logInfo) << "Thinking... (state " << (int)state << ")";

            if (depth < 0) depth = MAX_DEPTH;
            Logging::LogIt(Logging::logInfo) << "depth          " << (int)depth;

            // here is computed the time for next search (stored in the Threadpool for now)
            ThreadPool::instance().currentMoveMs = forcedMs <= 0 ? TimeMan::GetNextMSecPerMove(position) : forcedMs;
            Logging::LogIt(Logging::logInfo) << "currentMoveMs  " << ThreadPool::instance().currentMoveMs;

            Logging::LogIt(Logging::logInfo) << ToString(position);

            // will later be copied on all thread data and thus "reset" them
            ScoreType score = 0;
            Move m = INVALIDMOVE;
            DepthType seldepth = 0;
            PVList pv;

            ///@todo the only input coef here is depth...
            const ThreadData data = { depth,seldepth,score,position,m,pv,SearchData()}; 
            ThreadPool::instance().search(data); ///@todo blocking call for now
            
            ///@todo move this at the end of search
            receivePv(ThreadPool::instance().main().getData().pv); ///@todo take PV from the deepest thread not main ???
        });
    }

    Move moveFromCOM(std::string mstr) { // copy string on purpose
        Square from = INVALIDSQUARE;
        Square to   = INVALIDSQUARE;
        MType mtype = T_std;
        if (!readMove(position, trim(mstr), from, to, mtype)) return INVALIDMOVE;
        return ToMove(from, to, mtype);
    }
}
