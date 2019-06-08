struct PerftAccumulator{
    PerftAccumulator(): pseudoNodes(0), validNodes(0), captureNodes(0), epNodes(0), checkNode(0), checkMateNode(0){}
    Counter pseudoNodes,validNodes,captureNodes,epNodes,checkNode,checkMateNode;

    void Display(){
        Logging::LogIt(Logging::logInfo) << "pseudoNodes   " << pseudoNodes   ;
        Logging::LogIt(Logging::logInfo) << "validNodes    " << validNodes    ;
        Logging::LogIt(Logging::logInfo) << "captureNodes  " << captureNodes  ;
        Logging::LogIt(Logging::logInfo) << "epNodes       " << epNodes       ;
        Logging::LogIt(Logging::logInfo) << "checkNode     " << checkNode     ;
        Logging::LogIt(Logging::logInfo) << "checkMateNode " << checkMateNode ;
    }
};

void pstMean(){
    for (int i = 0 ; i < 6; ++i){
        int divisor = (i==0)?48:64;
        ScoreType sum = 0;
        for(int k = 0 ; k < 64 ; ++k){
           sum += EvalConfig::PST[i][k];
        }
        Logging::LogIt(Logging::logInfo) << PieceNames[i+1+PieceShift] << " " << sum/divisor;
        for(int k = ((i==0)?8:0) ; k < ((i==0)?56:64) ; ++k){
            if (k%8==0) std::cout << std::endl;
            std::cout << EvalConfig::PST[i][k]-sum/divisor << ",";
        }
        std::cout << std::endl;
        sum = 0;
        for(int k = 0 ; k < 64 ; ++k){
           sum += EvalConfig::PSTEG[i][k];
        }
        Logging::LogIt(Logging::logInfo) << PieceNames[i+1+PieceShift] << "EG " << sum/divisor;
        for(int k = ((i==0)?8:0) ; k < ((i==0)?56:64) ; ++k){
            if (k%8==0) std::cout << std::endl;
            std::cout << EvalConfig::PSTEG[i][k]-sum/divisor << ",";
        }
        std::cout << std::endl;
    }
}

Counter perft(const Position & p, DepthType depth, PerftAccumulator & acc, bool divide = false){
    if ( depth == 0) return 0;
    static TT::Entry e;
    MoveList moves;
    generate<GP_all>(p,moves);
    int validMoves = 0;
    int allMoves = 0;
    for (auto it = moves.begin() ; it != moves.end(); ++it){
        const Move m = *it;
        ++allMoves;
        Position p2 = p;
        if ( ! apply(p2,m) ) continue;
        ++validMoves;
        if ( divide && depth == 2 ) Logging::LogIt(Logging::logInfo) << ToString(p2) ;
        Counter nNodes = perft(p2,depth-1,acc,divide);
        if ( divide && depth == 2 ) Logging::LogIt(Logging::logInfo) << "=> after " << ToString(m) << " " << nNodes ;
        if ( divide && depth == 1 ) Logging::LogIt(Logging::logInfo) << (int)depth << " " <<  ToString(m) ;
    }
    if ( depth == 1 ) { acc.pseudoNodes += allMoves; acc.validNodes += validMoves; }
    if ( divide && depth == 2 ) Logging::LogIt(Logging::logInfo) << "********************" ;
    return acc.validNodes;
}

void perft_test(const std::string & fen, DepthType d, unsigned long long int expected) {
    Position p;
    readFEN(fen, p);
    Logging::LogIt(Logging::logInfo) << ToString(p) ;
    PerftAccumulator acc;
    if (perft(p, d, acc, false) != expected) Logging::LogIt(Logging::logInfo) << "Error !! " << fen << " " << expected ;
    acc.Display();
    Logging::LogIt(Logging::logInfo) << "#########################" ;
}

int cliManagement(std::string cli, int argc, char ** argv){

    if ( cli == "-xboard" ){
        XBoard::init();
        TimeMan::init();
        XBoard::xboard();
        return 0;
    }

#ifdef WITH_UCI
    if (cli == "-uci") {
        UCI::init();
        TimeMan::init();
        UCI::uci();
        return 0;
    }
#endif
    Logging::LogIt(Logging::logInfo) << "You can use -xboard command line option to enter xboard mode";

    if ( cli == "-perft_test" ){
        perft_test(startPosition, 5, 4865609);
        perft_test("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ", 4, 4085603);
        perft_test("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ", 6, 11030083);
        perft_test("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5, 15833292);
    }

    if ( argc < 3 ) return 1;

#ifdef IMPORTBOOK
    if ( cli == "-buildBook"){
        Book::readBook(argv[2]);
        return 0;
    }
#endif

    std::string fen = argv[2];

    if ( fen == "start")  fen = startPosition;
    if ( fen == "fine70") fen = fine70;
    if ( fen == "shirov") fen = shirov;

    Position p;
    if ( ! readFEN(fen,p) ){
        Logging::LogIt(Logging::logInfo) << "Error reading fen" ;
        return 1;
    }

    Logging::LogIt(Logging::logInfo) << ToString(p) ;

    if (cli == "-qsearch"){
        DepthType seldepth = 0;
        double s = ThreadPool::instance().main().qsearchNoPruning(-10000,10000,p,1,seldepth);
        Logging::LogIt(Logging::logInfo) << "Score " << s;
        return 1;
    }

    if (cli == "-attacked") {
        Square k = Sq_e4;
        if (argc >= 3) k = atoi(argv[3]);
        Logging::LogIt(Logging::logInfo) << showBitBoard(BB::isAttackedBB(p, k, p.c));
        return 0;
    }

    if (cli == "-cov") {
        Square k = Sq_e4;
        if (argc >= 3) k = atoi(argv[3]);
        switch (p.b[k]) {
        case P_wp:
            Logging::LogIt(Logging::logInfo) << showBitBoard((BB::coverage<P_wp>(k, p.occupancy, p.c) + BB::mask[k].push[p.c]) & ~p.allPieces[Co_White]);
            break;
        case P_wn:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BB::coverage<P_wn>(k, p.occupancy, p.c) & ~p.allPieces[Co_White]);
            break;
        case P_wb:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BB::coverage<P_wb>(k, p.occupancy, p.c) & ~p.allPieces[Co_White]);
            break;
        case P_wr:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BB::coverage<P_wr>(k, p.occupancy, p.c) & ~p.allPieces[Co_White]);
            break;
        case P_wq:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BB::coverage<P_wq>(k, p.occupancy, p.c) & ~p.allPieces[Co_White]);
            break;
        case P_wk:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BB::coverage<P_wk>(k, p.occupancy, p.c) & ~p.allPieces[Co_White]);
            break;
        case P_bk:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BB::coverage<P_wk>(k, p.occupancy, p.c) & ~p.allPieces[Co_Black]);
            break;
        case P_bq:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BB::coverage<P_wq>(k, p.occupancy, p.c )& ~p.allPieces[Co_Black]);
            break;
        case P_br:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BB::coverage<P_wr>(k, p.occupancy, p.c) & ~p.allPieces[Co_Black]);
            break;
        case P_bb:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BB::coverage<P_wb>(k, p.occupancy, p.c) & ~p.allPieces[Co_Black]);
            break;
        case P_bn:
            Logging::LogIt(Logging::logInfo) << showBitBoard(BB::coverage<P_wn>(k, p.occupancy, p.c) & ~p.allPieces[Co_Black]);
            break;
        case P_bp:
            Logging::LogIt(Logging::logInfo) << showBitBoard((BB::coverage<P_wp>(k, p.occupancy, p.c) + BB::mask[k].push[p.c])& ~p.allPieces[Co_Black]);
            break;
        default:
            Logging::LogIt(Logging::logInfo) << showBitBoard(0ull);
        }
        return 0;
    }

    if ( cli == "-eval" ){
        float gp = 0;
        int score = eval<true>(p,gp);
        Logging::LogIt(Logging::logInfo) << "eval " << score << " phase " << gp ;
        return 0;
    }

    if ( cli == "-gen" ){
        MoveList moves;
        generate<GP_all>(p,moves);
        sort(ThreadPool::instance().main(),moves,p,0);
        Logging::LogIt(Logging::logInfo) << "nb moves : " << moves.size() ;
        for(auto it = moves.begin(); it != moves.end(); ++it){
            Logging::LogIt(Logging::logInfo) << ToString(*it,true) ;
        }
        return 0;
    }

    if ( cli == "-testmove" ){
        Move m = ToMove(8,16,T_std);
        Position p2 = p;
        apply(p2,m);
        Logging::LogIt(Logging::logInfo) << ToString(p2) ;
        return 0;
    }

    if ( cli == "-perft" ){
        DepthType d = 5;
        if ( argc >= 3 ) d = atoi(argv[3]);
        PerftAccumulator acc;
        perft(p,d,acc,false);
        acc.Display();
        return 0;
    }

    if ( cli == "-analyze" ){
        DepthType depth = 5;
        if ( argc >= 3 ) depth = atoi(argv[3]);
        Move bestMove = INVALIDMOVE;
        ScoreType s = 0;
        TimeMan::isDynamic       = false;
        TimeMan::nbMoveInTC      = -1;
        TimeMan::msecPerMove     = 60*60*1000*24; // 1 day == infinity ...
        TimeMan::msecInTC        = -1;
        TimeMan::msecInc         = -1;
        TimeMan::msecUntilNextTC = -1;
        ThreadContext::currentMoveMs = TimeMan::GetNextMSecPerMove(p);
        DepthType seldepth = 0;
        PVList pv;
        ThreadData d = {depth,seldepth/*dummy*/,s/*dummy*/,p,bestMove/*dummy*/,pv/*dummy*/}; // only input coef
        ThreadPool::instance().searchSync(d);
        bestMove = ThreadPool::instance().main().getData().best; // here output results
        s = ThreadPool::instance().main().getData().sc; // here output results
        pv = ThreadPool::instance().main().getData().pv; // here output results
        Logging::LogIt(Logging::logInfo) << "Best move is " << ToString(bestMove) << " " << (int)depth << " " << s << " pv : " << ToString(pv) ;
        return 0;
    }

    if ( cli == "-mateFinder" ){
        DynamicConfig::mateFinder = true;
        DepthType depth = 5;
        if ( argc >= 3 ) depth = atoi(argv[3]);
        Move bestMove = INVALIDMOVE;
        ScoreType s = 0;
        TimeMan::isDynamic       = false;
        TimeMan::nbMoveInTC      = -1;
        TimeMan::msecPerMove     = 60*60*1000*24; // 1 day == infinity ...
        TimeMan::msecInTC        = -1;
        TimeMan::msecInc         = -1;
        TimeMan::msecUntilNextTC = -1;
        ThreadContext::currentMoveMs = TimeMan::GetNextMSecPerMove(p);
        DepthType seldepth = 0;
        PVList pv;
        ThreadData d = {depth,seldepth/*dummy*/,s/*dummy*/,p,bestMove/*dummy*/,pv/*dummy*/}; // only input coef
        ThreadPool::instance().searchSync(d);
        bestMove = ThreadPool::instance().main().getData().best; // here output results
        s = ThreadPool::instance().main().getData().sc; // here output results
        pv = ThreadPool::instance().main().getData().pv; // here output results
        Logging::LogIt(Logging::logInfo) << "Best move is " << ToString(bestMove) << " " << (int)depth << " " << s << " pv : " << ToString(pv) ;
        return 0;
    }

    Logging::LogIt(Logging::logInfo) << "Error : unknown command line" ;
    return 1;
}
