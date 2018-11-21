int debug(std::string cli, int argc, char ** argv){

    if ( cli == "-xboard" ){
        XBoard::init();
        TimeMan::init();
        XBoard::xboard();
        return 0;
    }

    LogIt(logInfo) << "You can use -xboard command line option to enter xboard mode";

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
        LogIt(logInfo) << "Error reading fen" ;
        return 1;
    }

    LogIt(logInfo) << ToString(p) ;

    if (cli == "-attacked") {
        Square k = Sq_e4;
        if (argc >= 3) k = atoi(argv[3]);
        LogIt(logInfo) << showBitBoard(BB::isAttackedBB(p, k, p.c));
        return 0;
    }

    if (cli == "-cov") {
        Square k = Sq_e4;
        if (argc >= 3) k = atoi(argv[3]);
        switch (p.b[k]) {
        case P_wp:
            LogIt(logInfo) << showBitBoard((BB::coverage<P_wp>(k, p.occupancy, p.c) + BB::mask[k].push[p.c]) & ~p.whitePiece);
            break;
        case P_wn:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wn>(k, p.occupancy, p.c) & ~p.whitePiece);
            break;
        case P_wb:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wb>(k, p.occupancy, p.c) & ~p.whitePiece);
            break;
        case P_wr:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wr>(k, p.occupancy, p.c) & ~p.whitePiece);
            break;
        case P_wq:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wq>(k, p.occupancy, p.c) & ~p.whitePiece);
            break;
        case P_wk:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wk>(k, p.occupancy, p.c) & ~p.whitePiece);
            break;
        case P_bk:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wk>(k, p.occupancy, p.c) & ~p.blackPiece);
            break;
        case P_bq:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wq>(k, p.occupancy, p.c )& ~p.blackPiece);
            break;
        case P_br:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wr>(k, p.occupancy, p.c) & ~p.blackPiece);
            break;
        case P_bb:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wb>(k, p.occupancy, p.c) & ~p.blackPiece);
            break;
        case P_bn:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wn>(k, p.occupancy, p.c) & ~p.blackPiece);
            break;
        case P_bp:
            LogIt(logInfo) << showBitBoard((BB::coverage<P_wp>(k, p.occupancy, p.c) + BB::mask[k].push[p.c])& ~p.blackPiece);
            break;
        default:
            LogIt(logInfo) << showBitBoard(0ull);
        }
        return 0;
    }

    if ( cli == "-eval" ){
        float gp = 0;
        int score = eval(p,gp);
        LogIt(logInfo) << "eval " << score << " phase " << gp ;
        return 0;
    }

    if ( cli == "-gen" ){
        std::vector<Move> moves;
        generate(p,moves);
        sort(ThreadPool::instance().main(),moves,p,0);
        LogIt(logInfo) << "nb moves : " << moves.size() ;
        for(auto it = moves.begin(); it != moves.end(); ++it){
            LogIt(logInfo) << ToString(*it,true) ;
        }
        return 0;
    }

    if ( cli == "-testmove" ){
        Move m = ToMove(8,16,T_std);
        Position p2 = p;
        apply(p2,m);
        LogIt(logInfo) << ToString(p2) ;
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
        ScoreType s;
        TimeMan::isDynamic                = false;
        TimeMan::nbMoveInTC               = -1;
        TimeMan::msecPerMove              = 60*60*1000*24; // 1 day == infinity ...
        TimeMan::msecWholeGame            = -1;
        TimeMan::msecInc                  = -1;
        currentMoveMs = TimeMan::GetNextMSecPerMove(p);
        DepthType seldepth = 0;
        std::vector<Move> pv;
        ThreadData d = {depth,seldepth/*dummy*/,s/*dummy*/,p,bestMove/*dummy*/,pv/*dummy*/}; // only input coef
        ThreadPool::instance().searchSync(d);
        bestMove = ThreadPool::instance().main().getData().best; // here output results
        s = ThreadPool::instance().main().getData().sc; // here output results
        pv = ThreadPool::instance().main().getData().pv; // here output results
        LogIt(logInfo) << "Best move is " << ToString(bestMove) << " " << (int)depth << " " << s << " pv : " << ToString(pv) ;
        return 0;
    }

    if ( cli == "-mateFinder" ){
        mateFinder = true;
        DepthType depth = 5;
        if ( argc >= 3 ) depth = atoi(argv[3]);
        Move bestMove = INVALIDMOVE;
        ScoreType s;
        TimeMan::isDynamic                = false;
        TimeMan::nbMoveInTC               = -1;
        TimeMan::msecPerMove              = 60*60*1000*24; // 1 day == infinity ...
        TimeMan::msecWholeGame            = -1;
        TimeMan::msecInc                  = -1;
        currentMoveMs = TimeMan::GetNextMSecPerMove(p);
        DepthType seldepth = 0;
        std::vector<Move> pv;
        ThreadData d = {depth,seldepth/*dummy*/,s/*dummy*/,p,bestMove/*dummy*/,pv/*dummy*/}; // only input coef
        ThreadPool::instance().searchSync(d);
        bestMove = ThreadPool::instance().main().getData().best; // here output results
        s = ThreadPool::instance().main().getData().sc; // here output results
        pv = ThreadPool::instance().main().getData().pv; // here output results
        LogIt(logInfo) << "Best move is " << ToString(bestMove) << " " << (int)depth << " " << s << " pv : " << ToString(pv) ;
        return 0;
    }

    LogIt(logInfo) << "Error : unknown command line" ;
    return 1;
}
