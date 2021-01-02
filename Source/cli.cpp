#include "moveGen.hpp"

#include "cli.hpp"

#include "evalDef.hpp"
#include "logging.hpp"
#include "searcher.hpp"
#include "timeMan.hpp"
#include "tools.hpp"
#include "tables.hpp"
#include "transposition.hpp"
#include "uci.hpp"
#include "xboard.hpp"

void help(){

}

struct PerftAccumulator{
    PerftAccumulator(): pseudoNodes(0), validNodes(0), captureNodes(0), epNodes(0), checkNode(0), checkMateNode(0),castling(0),promotion(0){}
    Counter pseudoNodes,validNodes,captureNodes,epNodes,checkNode,checkMateNode,castling,promotion;
    void Display();
    PerftAccumulator & operator+=(const PerftAccumulator & acc){
        pseudoNodes   += acc.pseudoNodes;
        validNodes    += acc.validNodes;
        captureNodes  += acc.captureNodes;
        epNodes       += acc.epNodes;
        checkNode     += acc.checkNode;
        checkMateNode += acc.checkMateNode;
        castling      += acc.castling;
        promotion     += acc.promotion;
        return *this;
    }
};

void PerftAccumulator::Display(){
    Logging::LogIt(Logging::logInfo) << "pseudoNodes   " << pseudoNodes   ;
    Logging::LogIt(Logging::logInfo) << "validNodes    " << validNodes    ;
    Logging::LogIt(Logging::logInfo) << "captureNodes  " << captureNodes  ;
    Logging::LogIt(Logging::logInfo) << "epNodes       " << epNodes       ;
    Logging::LogIt(Logging::logInfo) << "castling      " << castling       ;
    Logging::LogIt(Logging::logInfo) << "checkNode     " << checkNode     ;
    Logging::LogIt(Logging::logInfo) << "checkMateNode " << checkMateNode ;
}

Counter perft(const Position & p, DepthType depth, PerftAccumulator & acc){
    if ( depth == 0) return 0;
    MoveList moves;
    PerftAccumulator accLoc;
#ifndef DEBUG_PERFT
    MoveGen::generate<MoveGen::GP_all>(p,moves);
    const size_t n = moves.size();
    for (size_t k = 0 ; k < n; ++k){
        const Move & m = moves[k];
#else
    for ( MiniMove m = std::numeric_limits<MiniMove>::min(); m < std::numeric_limits<MiniMove>::max(); ++m){
        if( !isPseudoLegal(p,m) ) continue;
#endif
        ++accLoc.pseudoNodes;
        Position p2 = p;
#if defined(WITH_NNUE) && defined(DEBUG_NNUE_UPDATE)
        NNUEEvaluator evaluator;
        p2.associateEvaluator(evaluator);
        p2.resetNNUEEvaluator(p2.Evaluator());        
#endif        
        if ( ! applyMove(p2,m) ) continue;
        ++accLoc.validNodes;
        MType t = Move2Type(m);
        if ( t == T_ep) ++accLoc.epNodes;
        if ( isCapture(t)) ++accLoc.captureNodes;
        if ( isCastling(t)) ++accLoc.castling;
        if ( isPromotion(t)) ++accLoc.promotion;
        perft(p2,depth-1,acc);
    }
    if ( depth == 1 ) acc+=accLoc;
    return acc.validNodes;
}

void perft_test(const std::string & fen, DepthType d, unsigned long long int expected) {
    Position p;
    readFEN(fen, p);
    Logging::LogIt(Logging::logInfo) << ToString(p) ;
    PerftAccumulator acc;
    unsigned long long int n = perft(p, d, acc);
    acc.Display();
    if (n != expected) Logging::LogIt(Logging::logFatal) << "Error !! " << fen << " " << expected ;
    Logging::LogIt(Logging::logInfo) << "#########################" ;
}

Move analyze(const Position & p, DepthType depth, bool openBenchOutput = false){
    Move bestMove = INVALIDMOVE;
    ScoreType s = 0;
    TimeMan::isDynamic       = false;
    TimeMan::nbMoveInTC      = -1;
    TimeMan::msecPerMove     = INFINITETIME;
    TimeMan::msecInTC        = -1;
    TimeMan::msecInc         = -1;
    TimeMan::msecUntilNextTC = -1;
    ThreadPool::instance().currentMoveMs = TimeMan::GetNextMSecPerMove(p);
    DepthType seldepth = 0;
    PVList pv;
    ThreadData d = {depth,seldepth,s,p,bestMove,pv,SearchData()}; // only input coef is depth here
    ThreadPool::instance().search(d);
    bestMove = ThreadPool::instance().main().getData().best; // here output results
    s = ThreadPool::instance().main().getData().sc; // here output results
    pv = ThreadPool::instance().main().getData().pv; // here output results
    Logging::LogIt(Logging::logInfo) << "Best move is " << ToString(bestMove) << " " << (int)depth << " " << s << " pv : " << ToString(pv);
    if ( openBenchOutput ){
        Logging::LogIt(Logging::logInfo) << "Next two lines are for OpenBench";
        const TimeType ms = std::max(1,(int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - ThreadPool::instance().main().startTime).count());
        const Counter nodeCount = ThreadPool::instance().main().stats.counters[Stats::sid_nodes] + ThreadPool::instance().main().stats.counters[Stats::sid_qnodes];
        std::cerr << "NODES " << nodeCount << std::endl;
        std::cerr << "NPS " << int(nodeCount/(ms/1000.f)) << std::endl;
    }
    return bestMove;
}

void selfPlay(DepthType depth){
    DynamicConfig::genFen = true;
    DynamicConfig::FRC = true;
    Position p(chess960::positions[std::rand()%960]);
    NNUEEvaluator evaluator;
    p.associateEvaluator(evaluator);
    p.resetNNUEEvaluator(p.Evaluator());
    Move move = INVALIDMOVE;
#ifdef WITH_GENFILE
        if ( DynamicConfig::genFen && ! ThreadPool::instance().main().genStream.is_open() ){
            ThreadPool::instance().main().genStream.open("genfen_" + std::to_string(::getpid()) + "_" + std::to_string(0) + ".epd",std::ofstream::app);
        }
#endif    
    while( true ){
        DynamicConfig::genFen = false;
        ThreadPool::instance().main().subSearch = true;
        move = analyze(p,depth); // selfplay using a specific depth
        ThreadPool::instance().main().subSearch = false;
        DynamicConfig::genFen = true;
        if (move == INVALIDMOVE){
            //Logging::LogIt(Logging::logInfo) << "End of game"; 
            break;
        }
        Position p2 = p;
        applyMove(p2,move,true);
        p = p2;
#ifdef WITH_GENFILE
        if ( DynamicConfig::genFen ) ThreadPool::instance().main().writeToGenFile(p); // writeToGenFile using genFenDepth from this root position
#endif
        if (p.halfmoves > MAX_PLY/4 ){
            //Logging::LogIt(Logging::logInfo) << "Too long game"; 
            break;
        }
    }    
}

int cliManagement(std::string cli, int argc, char ** argv){

    // first we parse options that do not need extra parameters

#ifdef WITH_UCI
    if (cli == "-uci") {
        UCI::init();
        TimeMan::init();
        UCI::uci();
        return 0;
    }
    Logging::LogIt(Logging::logInfo) << "You can use -uci command line option to enter uci mode";
#endif
#ifdef WITH_XBOARD
    if ( cli == "-xboard" ){
        XBoard::init();
        TimeMan::init();
        XBoard::xboard();
        return 0;
    }
    Logging::LogIt(Logging::logInfo) << "You can use -xboard command line option to enter xboard mode";
#endif

    if ( cli == "-selfplay"){
        DepthType d = 15; // this is "search depth", not genFenDepth !
        if ( argc > 2 ) d = atoi(argv[2]); 
        unsigned long long int n = 1;
        if ( argc > 3 ) n = atoll(argv[3]); 
        Logging::LogIt(Logging::logInfo) << "Let's go for " << n << " selfplay games ..."; 
        while ( n-- > 0 ){
            if ( n%1000 == 0 ) Logging::LogIt(Logging::logInfo) << "Remaining games " << n;
            selfPlay(d);
        }
        return 0;
    }

    if ( cli == "-perft_test" ){
        perft_test(startPosition, 5, 4865609);
        perft_test("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ", 4, 4085603);
        perft_test("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ", 6, 11030083);
        perft_test("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5, 15833292);
        return 0;
    }

    if ( cli == "-perft_test_long_fischer" ){
        DynamicConfig::FRC = true;
        std::ifstream infile("Book_and_Test/TestSuite/fischer.txt");
        std::string line;
        while (std::getline(infile, line)){
            std::size_t found = line.find_first_of(",");
            const std::string fen = line.substr(0, found);
            while (found != std::string::npos ){
                const std::size_t start = found+1;
                found=line.find_first_of(",",found + 1 );
                const unsigned long long ull = std::stoull (line.substr(start, found-start));
                perft_test(fen, 6, ull);
            }
        }
        return 0;
    }

    if ( cli == "-perft_test_long" ){
        std::ifstream infile("Book_and_Test/TestSuite/perft.txt");
        std::string line;
        while (std::getline(infile, line)){
            std::size_t found = line.find_first_of(",");
            const std::string fen = line.substr(0, found);
            unsigned int i = 0;
            while (found != std::string::npos ){
                const std::size_t start = found+1;
                found=line.find_first_of(",",found + 1 );
                const unsigned long long ull = std::stoull (line.substr(start, found-start));
                perft_test(fen, ++i, ull);
            }
        }
        return 0;
    }

    if (cli == "-see_test"){

        struct SEETest{
            const std::string fen;
            const Move m;
            const ScoreType threshold;
        };

        const ScoreType P = Values[P_wp+PieceShift];
        const ScoreType N = Values[P_wn+PieceShift];
        const ScoreType B = Values[P_wb+PieceShift];
        const ScoreType R = Values[P_wr+PieceShift];
        const ScoreType Q = Values[P_wq+PieceShift];

        std::cout << P << "\t" << N << "\t" << B << "\t" << R << "\t" << Q << "\t" << std::endl;

        // from Vajolet
        std::list<SEETest> posList;
        /* capture initial move */
        posList.push_back( SEETest{"3r3k/3r4/2n1n3/8/3p4/2PR4/1B1Q4/3R3K w - - 0 1", 				ToMove(Sq_d3,Sq_d4, T_capture), ScoreType(P - R + N - P )});
        posList.push_back( SEETest{"1k1r4/1ppn3p/p4b2/4n3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - 0 1",			ToMove(Sq_d3,Sq_e5, T_capture), ScoreType(N - N + B - R + N )});
        posList.push_back( SEETest{"1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - 0 1",                  ToMove(Sq_d3,Sq_e5, T_capture), ScoreType(P - N )});
        posList.push_back( SEETest{"rnb2b1r/ppp2kpp/5n2/4P3/q2P3B/5R2/PPP2PPP/RN1QKB2 w Q - 1 1",		ToMove(Sq_h4,Sq_f6, T_capture), ScoreType(N - B + P )});
        posList.push_back( SEETest{"r2q1rk1/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 b - - 1 1",	ToMove(Sq_g4,Sq_f3, T_capture), ScoreType(N - B )});
        posList.push_back( SEETest{"r1bqkb1r/2pp1ppp/p1n5/1p2p3/3Pn3/1B3N2/PPP2PPP/RNBQ1RK1 b kq - 2 1",	ToMove(Sq_c6,Sq_d4, T_capture), ScoreType(P - N + N - P)});
        posList.push_back( SEETest{"r1bq1r2/pp1ppkbp/4N1p1/n3P1B1/8/2N5/PPP2PPP/R2QK2R w KQ - 2 1",		ToMove(Sq_e6,Sq_g7, T_capture), ScoreType(B - N )});
        posList.push_back( SEETest{"r1bq1r2/pp1ppkbp/4N1pB/n3P3/8/2N5/PPP2PPP/R2QK2R w KQ - 2 1",		ToMove(Sq_e6,Sq_g7, T_capture), ScoreType(B )});
        posList.push_back( SEETest{"rnq1k2r/1b3ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq - 0 1",	ToMove(Sq_d6,Sq_h2, T_capture), ScoreType(P - B )});
        posList.push_back( SEETest{"rn2k2r/1bq2ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq - 5 1",	ToMove(Sq_d6,Sq_h2, T_capture), ScoreType(P )});
        posList.push_back( SEETest{"r2qkbn1/ppp1pp1p/3p1rp1/3Pn3/4P1b1/2N2N2/PPP2PPP/R1BQKB1R b KQq - 2 1",	ToMove(Sq_g4,Sq_f3, T_capture), ScoreType(N - B + P )});
        posList.push_back( SEETest{"rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/P1N5/1PQ1PPPP/R1B1KBNR b KQ - 1 1",          ToMove(Sq_b4,Sq_c3, T_capture), ScoreType(N - B )});
        posList.push_back( SEETest{"r4rk1/3nppbp/bq1p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - - 1 1",		ToMove(Sq_b6,Sq_b2, T_capture), ScoreType(P - Q )});
        posList.push_back( SEETest{"r4rk1/1q1nppbp/b2p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - - 1 1",		ToMove(Sq_f6,Sq_d5, T_capture), ScoreType(P - N )});
        posList.push_back( SEETest{"1r3r2/5p2/4p2p/2k1n1P1/2PN1nP1/1P3P2/8/2KR1B1R b - - 0 29",                 ToMove(Sq_b8,Sq_b3, T_capture), ScoreType(P - R )});
        posList.push_back( SEETest{"1r3r2/5p2/4p2p/4n1P1/kPPN1nP1/5P2/8/2KR1B1R b - - 0 1",			ToMove(Sq_b8,Sq_b4, T_capture), ScoreType(P )});
        posList.push_back( SEETest{"2r2rk1/5pp1/pp5p/q2p4/P3n3/1Q3NP1/1P2PP1P/2RR2K1 b - - 1 22",		ToMove(Sq_c8,Sq_c1, T_capture), ScoreType(R - R )});
        posList.push_back( SEETest{"1r3r1k/p4pp1/2p1p2p/qpQP3P/2P5/3R4/PP3PP1/1K1R4 b - - 0 1",                 ToMove(Sq_a5,Sq_a2, T_capture), ScoreType(P - Q )});
        posList.push_back( SEETest{"1r5k/p4pp1/2p1p2p/qpQP3P/2P2P2/1P1R4/P4rP1/1K1R4 b - - 0 1",		ToMove(Sq_a5,Sq_a2, T_capture), ScoreType(P )});
        posList.push_back( SEETest{"r2q1rk1/1b2bppp/p2p1n2/1ppNp3/3nP3/P2P1N1P/BPP2PP1/R1BQR1K1 w - - 4 14",	ToMove(Sq_d5,Sq_e7, T_capture), ScoreType(B - N )});
        posList.push_back( SEETest{"rnbqrbn1/pp3ppp/3p4/2p2k2/4p3/3B1K2/PPP2PPP/RNB1Q1NR w - - 0 1",		ToMove(Sq_d3,Sq_e4, T_capture), ScoreType(P )});
        /* non capture initial move */
        posList.push_back( SEETest{"rnb1k2r/p3p1pp/1p3p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq - 0 1",	ToMove(Sq_e4,Sq_d6, T_std), ScoreType(- N + P )});
        posList.push_back( SEETest{"r1b1k2r/p4npp/1pp2p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq - 0 1",	ToMove(Sq_e4,Sq_d6, T_std), ScoreType(- N + N )});
        posList.push_back( SEETest{"2r1k2r/pb4pp/5p1b/2KB3n/4N3/2NP1PB1/PPP1P1PP/R2Q3R w k - 0 1",		ToMove(Sq_d5,Sq_c6, T_std), ScoreType(- B )});
        posList.push_back( SEETest{"2r1k2r/pb4pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w k - 0 1",		ToMove(Sq_d5,Sq_c6, T_std), ScoreType(- B + B )});
        posList.push_back( SEETest{"2r1k3/pbr3pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w - - 0 1",		ToMove(Sq_d5,Sq_c6, T_std), ScoreType(- B + B - N )});
        /* initial move promotion */
        posList.push_back( SEETest{"5k2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ - 0 1",			ToMove(Sq_d7,Sq_d8, T_promq), ScoreType( -P + Q  )});
        posList.push_back( SEETest{"r4k2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ - 0 1",			ToMove(Sq_d7,Sq_d8, T_promq), ScoreType( -P + Q - Q )});
        posList.push_back( SEETest{"5k2/p2P2pp/1b6/1p6/1Nn1P1n1/8/PPP4P/R2QK1NR w KQ - 0 1",			ToMove(Sq_d7,Sq_d8, T_promq), ScoreType( -P + Q - Q + B )});
        posList.push_back( SEETest{"4kbnr/p1P1pppp/b7/4q3/7n/8/PP1PPPPP/RNBQKBNR w KQk - 0 1",                  ToMove(Sq_c7,Sq_c8, T_promq), ScoreType( -P + Q - Q )});
        posList.push_back( SEETest{"4kbnr/p1P1pppp/b7/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk - 0 1",                  ToMove(Sq_c7,Sq_c8, T_promq), ScoreType( -P + Q - Q + B )});
        posList.push_back( SEETest{"4kbnr/p1P1pppp/b7/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk - 0 1",                  ToMove(Sq_c7,Sq_c8, T_promn), ScoreType( -P + N )});
        posList.push_back( SEETest{"4kbnr/p1P1pppp/1b6/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk - 0 1",                 ToMove(Sq_c7,Sq_c8, T_promn), ScoreType( -P + N )});
        /* initial move En Passant */
        posList.push_back( SEETest{"4kbnr/p1P4p/b1q5/5pP1/4n3/5Q2/PP1PPP1P/RNB1KBNR w KQk f6 0 2",		ToMove(Sq_g5,Sq_f6, T_ep), ScoreType(P - P )});
        posList.push_back( SEETest{"4kbnr/p1P4p/b1q5/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk f6 0 2",		ToMove(Sq_g5,Sq_f6, T_ep), ScoreType(P - P )});
        /* initial move capture promotion */
        posList.push_back( SEETest{"1n2kb1r/p1P4p/2qb4/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk - 0 1",		ToMove(Sq_c7,Sq_b8, T_cappromq ), ScoreType(N + ( -P + Q ) -Q )});

        posList.push_back( SEETest{"rnbqk2r/pp3ppp/2p1pn2/3p4/3P4/N1P1BN2/PPB1PPPb/R2Q1RK1 w kq - 0 1",         ToMove(Sq_g1,Sq_h2, T_capture), ScoreType(B) });

        posList.push_back( SEETest{"3N4/2K5/2n1n3/1k6/8/8/8/8 b - - 0 1",					ToMove(Sq_c6,Sq_d8, T_capture), ScoreType(N) });
        posList.push_back( SEETest{"3N4/2K5/2n5/1k6/8/8/8/8 b - - 0 1",                                         ToMove(Sq_c6,Sq_d8, T_capture), ScoreType(0) });

        /* promotion inside the loop */
        posList.push_back( SEETest{"3N4/2P5/2n5/1k6/8/8/8/4K3 b - - 0 1",					ToMove(Sq_c6,Sq_d8, T_capture), ScoreType(N - N - Q + P ) });
        posList.push_back( SEETest{"3N4/2P5/2n5/1k6/8/8/8/4K3 b - - 0 1",					ToMove(Sq_c6,Sq_b8, T_capture), ScoreType(- N - Q + P ) });
        posList.push_back( SEETest{"3n3r/2P5/8/1k6/8/8/3Q4/4K3 w - - 0 1",					ToMove(Sq_d2,Sq_d8, T_capture), ScoreType(N) });
        posList.push_back( SEETest{"3n3r/2P5/8/1k6/8/8/3Q4/4K3 w - - 0 1",					ToMove(Sq_c7,Sq_d8, T_promq)  , ScoreType( (N - P + Q ) - Q + R) });
        /* double promotion inside the loop */
        posList.push_back( SEETest{"r2n3r/2P1P3/4N3/1k6/8/8/8/4K3 w - - 0 1",					ToMove(Sq_e6,Sq_d8, T_capture), ScoreType(N) });
        posList.push_back( SEETest{"8/8/8/1k6/6b1/4N3/2p3K1/3n4 w - - 0 1",					ToMove(Sq_e3,Sq_d1, T_capture), ScoreType(0) });
        posList.push_back( SEETest{"8/8/1k6/8/8/2N1N3/2p1p1K1/3n4 w - - 0 1",					ToMove(Sq_c3,Sq_d1, T_capture), ScoreType(N - N - Q + P) });

        posList.push_back( SEETest{"8/8/1k6/8/8/2N1N3/4p1K1/3n4 w - - 0 1",					ToMove(Sq_c3,Sq_d1, T_capture), ScoreType(N - (N - P + Q ) + Q) });

        posList.push_back( SEETest{"r1bqk1nr/pppp1ppp/2n5/1B2p3/1b2P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",	ToMove(Sq_e1,Sq_g1, T_wks) , ScoreType(0) });

        for (auto & t : posList) {
            Position p;
            readFEN(t.fen, p, true);
            Logging::LogIt(Logging::logError) << "============================== " << t.fen << " ==";
            const ScoreType s = ThreadPool::instance().main().SEE(p,t.m);
            bool b = s == t.threshold;
            if ( !b ) Logging::LogIt(Logging::logError)  << "wrong SEE value == " << ToString(p) << "\n" << ToString(t.m) << " " << s << " " << t.threshold;
        }

        return 0;
    }

    if ( cli == "bench" ){
        Position p;
        DepthType d = 15;
        if ( argc > 2 ) d = atoi(argv[2]);        
        readFEN(startPosition,p);
        analyze(p,d,true);
        readFEN(fine70,p);
        analyze(p,d,true);
        readFEN(shirov,p);
        analyze(p,d,true);
        return 0;
    }

    if ( cli == "-evalSpeed"){
        DynamicConfig::disableTT = true;
        std::string filename = "evalSpeed.epd";
        if (argc > 2) filename = argv[2];
        std::vector<Position> data;
        Logging::LogIt(Logging::logInfo) << "Running eval speed with file " << filename;
        std::vector<std::string> positions;
        readEPDFile(filename,positions);
        for(size_t k = 0 ; k < positions.size() ; ++k){
            data.push_back(Position(positions[k],false));
            if (k % 50000 == 0) Logging::LogIt(Logging::logInfo) << k << " position read";
        }
        Logging::LogIt(Logging::logInfo) << "Data size : " << data.size();

        std::chrono::time_point<Clock> startTime = Clock::now();
        for(int k = 0; k < 10 ; ++k)
        for(auto p : data){
            EvalData d;
            eval(p,d,ThreadPool::instance().main(),true);
        }
        int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count();
        Logging::LogIt(Logging::logInfo) << "Eval speed (with EG material hash): " << data.size()*10.f/(ms*1000) << " Meval/s";

        startTime = Clock::now();
        for(int k = 0; k < 10 ; ++k)
        for(auto p : data){
            EvalData d;
            eval(p,d,ThreadPool::instance().main(),false);
        }
        ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count();
        Logging::LogIt(Logging::logInfo) << "Eval speed : " << data.size()*10.f/(ms*1000) << " Meval/s"; 

        ThreadPool::instance().DisplayStats();
        return 0;
    }

    // next option needs at least one argument more
    if ( argc < 3 ){
        help();
        return 1;
    }

    // in other cases, argv[2] is always the fen string
    std::string fen = argv[2];

    // some "short cuts" !
    if ( fen == "start")  fen = startPosition;
    if ( fen == "fine70") fen = fine70;
    if ( fen == "shirov") fen = shirov;

    // instantiate the position
    Position p;
#ifdef WITH_NNUE
    NNUEEvaluator evaluator;
    p.associateEvaluator(evaluator);
    p.resetNNUEEvaluator(evaluator);
#endif
    if ( ! readFEN(fen,p) ){
        Logging::LogIt(Logging::logInfo) << "Error reading fen" ;
        return 1;
    }

    Logging::LogIt(Logging::logInfo) << ToString(p) ;

    if (cli == "-qsearch"){
        DepthType seldepth = 0;
        ScoreType s = ThreadPool::instance().main().qsearchNoPruning(-10000,
                                                                      10000,
                                                                      p,
                                                                      1,
                                                                      seldepth);
        Logging::LogIt(Logging::logInfo) << "Score " << s;
        return 1;
    }

    if (cli == "-see"){
        Square from = INVALIDSQUARE;
        Square to = INVALIDSQUARE;
        MType mtype = T_std;
        const std::string move = argc>3?argv[3]:"e2e4";
        readMove(p,move,from,to,mtype);
        Move m = ToMove(from,to,mtype);
        ScoreType t = atoi(argv[4]);
        bool b = ThreadPool::instance().main().SEE_GE(p,m,t);
        Logging::LogIt(Logging::logInfo) << "SEE ? " << (b?"ok":"not");
        return 1;
    }

    if (cli == "-attacked") {
        Square k = Sq_e4;
        if (argc > 3) k = atoi(argv[3]);
        Logging::LogIt(Logging::logInfo) << SquareNames[k];
        Logging::LogIt(Logging::logInfo) << showBitBoard(BBTools::allAttackedBB(p, k, p.c));
        return 0;
    }

    if (cli == "-cov") {
        Square k = Sq_e4;
        if (argc > 3) k = atoi(argv[3]);
        switch (p.board_const(k)) {
        case P_wp:
            Logging::LogIt(Logging::logInfo) << showBitBoard((BBTools::coverage<P_wp>(k, p.occupancy(), p.c) + BBTools::mask[k].push[p.c]) & ~p.allPieces[Co_White]);
            break;
        case P_wn:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BBTools::coverage<P_wn>(k, p.occupancy(), p.c) & ~p.allPieces[Co_White]);
            break;
        case P_wb:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BBTools::coverage<P_wb>(k, p.occupancy(), p.c) & ~p.allPieces[Co_White]);
            break;
        case P_wr:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BBTools::coverage<P_wr>(k, p.occupancy(), p.c) & ~p.allPieces[Co_White]);
            break;
        case P_wq:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BBTools::coverage<P_wq>(k, p.occupancy(), p.c) & ~p.allPieces[Co_White]);
            break;
        case P_wk:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BBTools::coverage<P_wk>(k, p.occupancy(), p.c) & ~p.allPieces[Co_White]);
            break;
        case P_bk:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BBTools::coverage<P_wk>(k, p.occupancy(), p.c) & ~p.allPieces[Co_Black]);
            break;
        case P_bq:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BBTools::coverage<P_wq>(k, p.occupancy(), p.c )& ~p.allPieces[Co_Black]);
            break;
        case P_br:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BBTools::coverage<P_wr>(k, p.occupancy(), p.c) & ~p.allPieces[Co_Black]);
            break;
        case P_bb:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BBTools::coverage<P_wb>(k, p.occupancy(), p.c) & ~p.allPieces[Co_Black]);
            break;
        case P_bn:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BBTools::coverage<P_wn>(k, p.occupancy(), p.c) & ~p.allPieces[Co_Black]);
            break;
        case P_bp:
            Logging::LogIt(Logging::logInfo) << showBitBoard((BBTools::coverage<P_wp>(k, p.occupancy(), p.c) + BBTools::mask[k].push[p.c])& ~p.allPieces[Co_Black]);
            break;
        default:
            Logging::LogIt(Logging::logInfo) << showBitBoard(0ull);
        }
        return 0;
    }

    if ( cli == "-eval" ){
        EvalData data;
        if ( DynamicConfig::useNNUE ) DynamicConfig::forceNNUE = true;
        ScoreType score = eval(p,data,ThreadPool::instance().main(),true,true);
        Logging::LogIt(Logging::logInfo) << "eval " << score << " phase " << data.gp ;
        return 0;
    }

    if ( cli == "-gen" ){
        MoveList moves;
        MoveGen::generate<MoveGen::GP_all>(p,moves);
        CMHPtrArray cmhPtr = {0};
        MoveSorter::scoreAndSort(ThreadPool::instance().main(),moves,p,0.f,0,cmhPtr);
        Logging::LogIt(Logging::logInfo) << "nb moves : " << moves.size() ;
        for(auto it = moves.begin(); it != moves.end(); ++it){
            Logging::LogIt(Logging::logInfo) << ToString(*it,true) ;
        }
        return 0;
    }

    if ( cli == "-testmove" ){
        Move m = ToMove(8,16,T_std);
        Position p2 = p;
#if defined(WITH_NNUE) && defined(DEBUG_NNUE_UPDATE)
        p2.associateEvaluator(evaluator);
        p2.resetNNUEEvaluator(p2.Evaluator());        
#endif
        applyMove(p2,m);
        Logging::LogIt(Logging::logInfo) << ToString(p2) ;
        return 0;
    }

    if ( cli == "-perft" ){
        DepthType d = 5;
        if ( argc > 3 ) d = atoi(argv[3]);
        PerftAccumulator acc;
        auto start = Clock::now();
        perft(p,d,acc);
        auto elapsed = std::max(1, (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
        Logging::LogIt(Logging::logInfo) << "Perft done in " << elapsed << "ms";
        acc.Display();
        Logging::LogIt(Logging::logInfo) << "Speed " << int(acc.validNodes/elapsed) << "KNPS";
        return 0;
    }

    if ( cli == "-analyze" ){
        DepthType depth = 15;
        if ( argc > 3 ) depth = atoi(argv[3]);
        auto start = Clock::now();
        analyze(p,depth);
        auto elapsed = std::max(1, (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
        Logging::LogIt(Logging::logInfo) << "Analysis done in " << elapsed << "ms";
        return 0;
    }

    if ( cli == "-mateFinder" ){
        DynamicConfig::mateFinder = true;
        DepthType depth = 10;
        if ( argc > 3 ) depth = atoi(argv[3]);
        analyze(p,depth);
        return 0;
    }

    Logging::LogIt(Logging::logInfo) << "Error : unknown command line" ;
    return 1;
}
