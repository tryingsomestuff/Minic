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
        COM::position.associateEvaluator(evaluator);
#endif
        readFEN(startPosition, COM::position);
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
           Logging::LogIt(Logging::logInfo) << "Received command : " << command;
           strcpy(buffer, command.c_str()); // only usefull if WITH_MPI 
        }
        // don't rely on Bcast to do a "passive wait", most implementation is doing a busy-wait, so use 100% cpu
        Distributed::asyncBcast(buffer,4096,Distributed::_requestInput); 
        Distributed::waitRequest(Distributed::_requestInput);
        // other slave rank event loop
        if ( !Distributed::isMainProcess()){ 
           command = buffer;
        }
        Distributed::sync(); // let ensure everyone is sync here
    }

    SideToMove opponent(SideToMove & s) {
        return s == stm_white ? stm_black : stm_white;
    }

    bool sideToMoveFromFEN(const std::string & fen) {
        const bool b = readFEN(fen, COM::position,true);
        stm = COM::position.c == Co_White ? stm_white : stm_black;
        if (!b) Logging::LogIt(Logging::logFatal) << "Illegal FEN " << fen;
        return b;
    }

    Move thinkUntilTimeUp(TimeType forcedMs = -1) { // think and when threads stop searching, return best move
        Logging::LogIt(Logging::logInfo) << "Thinking... (state " << (int)COM::state << ")";
        ScoreType score = 0;
        Move m = INVALIDMOVE;
        if (depth < 0) depth = MAX_DEPTH;
        Logging::LogIt(Logging::logInfo) << "depth          " << (int)depth;
        ThreadPool::instance().currentMoveMs = forcedMs <= 0 ? TimeMan::GetNextMSecPerMove(position) : forcedMs;
        Logging::LogIt(Logging::logInfo) << "currentMoveMs  " << ThreadPool::instance().currentMoveMs;
        Logging::LogIt(Logging::logInfo) << ToString(position);
        DepthType seldepth = 0;
        PVList pv;
        const ThreadData d = { depth,seldepth,score,position,m,pv,SearchData()}; // only input coef here is depth
        ThreadPool::instance().search(d);
        m = ThreadPool::instance().main().getData().best; // here output results
        Logging::LogIt(Logging::logInfo) << "...done returning move " << ToString(m) << " (state " << (int)COM::state << ")";;
        return m;
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

    void thinkAsync(State st, TimeType forcedMs) { // fork a future that runs a synchorous search, if needed send returned move to GUI
        f = std::async(std::launch::async, [st,forcedMs] {
            COM::move = COM::thinkUntilTimeUp(forcedMs);
            const PVList & pv = ThreadPool::instance().main().getData().pv; ///@todo take PV from the deepest thread not main ???
            COM::ponderMove = INVALIDMOVE;
            if ( pv.size() > 1) {
               Position p2 = COM::position;
#ifdef WITH_NNUE
               NNUEEvaluator evaluator2;
               p2.associateEvaluator(evaluator2);
               p2.resetNNUEEvaluator(p2.Evaluator());
#endif
               if ( applyMove(p2,pv[0]) && isPseudoLegal(p2,pv[1])) COM::ponderMove = pv[1];
            }
            Logging::LogIt(Logging::logInfo) << "search async done (state " << (int)st << ")";
            if (st == st_searching) {
                Logging::LogIt(Logging::logInfo) << "sending move to GUI " << ToString(COM::move);
                if (COM::move == INVALIDMOVE) { COM::mode = COM::m_force; } // game ends
                else {
                    if (!COM::makeMove(COM::move, true, Logging::ct == Logging::CT_uci ? "bestmove" : "move", COM::ponderMove)) {
                        Logging::LogIt(Logging::logGUI) << "info string Bad computer move !";
                        Logging::LogIt(Logging::logInfo) << ToString(COM::position);
                        COM::mode = COM::m_force;
                    }
                    else{
                        COM::stm = COM::opponent(COM::stm);
                        COM::moves.push_back(COM::move);
                    }
                }
            }
            Logging::LogIt(Logging::logInfo) << "Putting state to none (state " << (int)st << ")";
            state = st_none;
        });
    }

    Move moveFromCOM(std::string mstr) { // copy string on purpose
        Square from = INVALIDSQUARE;
        Square to   = INVALIDSQUARE;
        MType mtype = T_std;
        if (!readMove(COM::position, trim(mstr), from, to, mtype)) return INVALIDMOVE;
        return ToMove(from, to, mtype);
    }
}
