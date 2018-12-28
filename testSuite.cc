
//#include "minic.cc"

void split( std::vector<std::string> & v, const std::string & str, const std::string & sep){
    size_t start = 0, end = 0;
    while ( end != std::string::npos){
        end = str.find( sep, start);
        v.push_back( str.substr( start,(end == std::string::npos) ? std::string::npos : end - start));
        start = (   ( end > (std::string::npos - sep.size()) ) ?  std::string::npos  :  end + sep.size());
    }
}

std::vector<std::string> split2(const std::string & line, char sep, char delim){
   std::vector<std::string> v;
   const char * c = line.c_str();       // prepare to parse the line
   bool inSep{false};
   for (const char* p = c; *p ; ++p) {  // iterate through the string
       if (*p == delim) {               // toggle flag if we're btw delim
           inSep = !inSep;
       }
       else if (*p == sep && !inSep) {  // if sep OUTSIDE delim
           std::string str(c,p-c);
           str.erase(std::remove(str.begin(), str.end(), ':'), str.end());
           v.push_back(str);            // keep the field
           c = p+1;                     // and start parsing next one
       }
   }
   v.push_back(std::string(c));         // last field delimited by end of line instead of sep
   return v;
}

std::string & ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

class ExtendedPosition : public Position{
public:
    ExtendedPosition(const std::string & s, bool withMoveCount = true);
    bool shallFindBest();
    std::vector<std::string> bestMoves();
    std::vector<std::string> badMoves();
    std::string id();
    static bool readEPDFile(const std::string & fileName, std::vector<std::string> & positions);
    static void test(const std::vector<std::string> & positions,
                     const std::vector<int> &         timeControls,
                     bool                             breakAtFirstSuccess,
                     const std::vector<int> &         scores,
                     std::function< int(int) >        eloF,
                     bool                             withMoveCount = true);
    static void testStatic(const std::vector<std::string> & positions,
                           int chunck = 4,
                           bool withMoveCount = false);
private:
    std::map<std::string,std::vector<std::string> > _extendedParams;
};

ExtendedPosition::ExtendedPosition(const std::string & extFEN, bool withMoveCount) : Position(extFEN){
    std::vector<std::string> strList;
    std::stringstream iss(extFEN);
    std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));
    if ( strList.size() < (withMoveCount?7u:5u)) LogIt(logFatal) << "Not an extended position";
    std::vector<std::string>(strList.begin()+(withMoveCount?6:4), strList.end()).swap(strList);
    const std::string extendedParamsStr = std::accumulate(strList.begin(), strList.end(), std::string(""),[](const std::string & a, const std::string & b) {return a + ' ' + b;});
    LogIt(logInfo) << "extended parameters : " << extendedParamsStr;
    std::vector<std::string> strParamList;
    split(strParamList,extendedParamsStr,";");
    for(size_t k = 0 ; k < strParamList.size() ; ++k){
        LogIt(logInfo) << "extended parameters : " << k << " " << strParamList[k];
        strParamList[k] = ltrim(strParamList[k]);
        if ( strParamList[k].empty()) continue;
        std::vector<std::string> pair;
        split(pair,strParamList[k]," ");
        if ( pair.size() < 2 ) LogIt(logFatal) << "Not un extended parameter pair";
        std::vector<std::string> values = pair;
        values.erase(values.begin());
        _extendedParams[pair[0]] = values;
        LogIt(logInfo) << "extended parameters pair : " << pair[0] << " => " << values[0];
    }
}

bool ExtendedPosition::shallFindBest(){ return _extendedParams.find("bm") != _extendedParams.end();}

std::vector<std::string> ExtendedPosition::bestMoves(){ return _extendedParams["bm"];}

std::vector<std::string> ExtendedPosition::badMoves(){ return _extendedParams["am"];}

std::string ExtendedPosition::id(){
    if ( _extendedParams.find("id") != _extendedParams.end() ) return _extendedParams["id"][0];
    else return "";
}

bool ExtendedPosition::readEPDFile(const std::string & fileName, std::vector<std::string> & positions){
    LogIt(logInfo) << "Loading EPD file : " << fileName;
    std::ifstream str(fileName);
    if (str) {
        std::string line;
        while (std::getline(str, line)) positions.push_back(line);
        return true;
    }
    else {
        LogIt(logError) << "Cannot open EPD file " << fileName;
        return false;
    }
}

template<class T>
std::ostream & operator<<(std::ostream & os, const std::vector<T> &v){
    for(auto it = v.begin() ; it != v.end() ; ++it){
        os << *it << " ";
    }
    return os;
}

BitBoard getPieceBitboard(const Position & p, Piece t){
    const BitBoard * const allB[13] = { &(p.blackKing),&(p.blackQueen),&(p.blackRook),&(p.blackBishop),&(p.blackKnight),&(p.blackPawn),&dummy,&(p.whitePawn),&(p.whiteKnight),&(p.whiteBishop),&(p.whiteRook),&(p.whiteQueen),&(p.whiteKing) };
    return *allB[t+PieceShift];
}

int numberOf(const Position & p, Piece t){ return countBit(getPieceBitboard(p,t));}

std::string showAlgAbr(Move m, const Position & p) {
    Square from  = Move2From(m);
    Square to    = Move2To(m);
    MType  mtype = Move2Type(m);
    if ( m == INVALIDMOVE) return "xx";
    if ( mtype == T_wks || mtype == T_bks) return "0-0";
    if ( mtype == T_wqs || mtype == T_bqs) return "0-0-0";

    std::string s;
    Piece t = p.b[from];

    // add piece type if not pawn
    s+= Names[PieceShift + std::abs(t)];
    if ( t==P_wp || t==P_bp ) s.clear(); // no piece symbol for pawn

    // ensure move is not ambiguous
    bool isAmbiguousFile = false;
    bool isAmbiguousRank = false;
    if ( numberOf(p,t)>1 ){
        std::vector<Square> v;
        BitBoard b = getPieceBitboard(p,t);
        while (b) v.push_back(BB::popBit(b));
        for(auto it = v.begin() ; it != v.end() ; ++it){
            if ( *it == from ) continue; // to not compare to myself ...
            std::vector<Move> l;
            generateSquare(p,l,*it);
            for(auto mit = l.begin() ; mit != l.end() ; ++mit){
                if ( *mit == m ) continue; // to not compare to myself ... should no happend thanks to previous verification
                if ( Move2To(*mit) == to ){
                    if ( ! isAmbiguousFile && SQFILE(Move2From(*mit)) == SQFILE(from)){
                        isAmbiguousFile = true;
                    }
                    if ( ! isAmbiguousRank && SQRANK(Move2From(*mit)) == SQRANK(from)){
                       isAmbiguousRank = true;
                    }
                }
            }
        }
    }

    if ( isAmbiguousRank || ((t==P_wp || t==P_bp) && isCapture(m)) ) s+= Files[SQFILE(from)];
    if ( isAmbiguousFile ) s+= Ranks[SQRANK(from)];

    // add 'x' if capture
    if ( isCapture(m)){
        s+="x";
    }

    // add landing position
    s+= Squares[to];

    // and promotion to
    if (isPromotion(m)){
        switch(mtype){
        case T_cappromq:
        case T_promq:
            s += "=";
            s += "Q";
            break;
        case T_cappromr:
        case T_promr:
            s += "=";
            s += "R";
            break;
        case T_cappromb:
        case T_promb:
            s += "=";
            s += "B";
            break;
        case T_cappromn:
        case T_promn:
            s += "=";
            s += "N";
            break;
        }
    }

    Position p2 = p;
    if (apply(p2,m)){
        if ( isAttacked(p2, kingSquare(p2)) ) s += "+";
    }
    else{
        s += "~";
    }

    return s;
}

void ExtendedPosition::test(const std::vector<std::string> & positions,
                            const std::vector<int> &         timeControls,
                            bool                             breakAtFirstSuccess,
                            const std::vector<int> &         scores,
                            std::function< int(int) >        eloF,
                            bool                             withMoveCount){
    struct Results{
        Results():k(0),t(0),score(0){}
        int k;
        int t;
        std::string name;
        std::vector<std::string> bm;
        std::vector<std::string> am;
        std::string computerMove;
        int score;
    };

    if (scores.size() != timeControls.size()){
        LogIt(logFatal) << "Wrong timeControl versus score vector size";
    }

    Results ** results = new Results*[positions.size()];

    // run the test and fill results table
    for (size_t k = 0 ; k < positions.size() ; ++k ){
        LogIt(logInfo) << "Test #" << k << " " << positions[k];
        results[k] = new Results[timeControls.size()];
        ExtendedPosition extP(positions[k],withMoveCount);
        for(size_t t = 0 ; t < timeControls.size() ; ++t){
            LogIt(logInfo) << " " << t;
            currentMoveMs = timeControls[t];
            DepthType seldepth = 0;
            DepthType depth = 64;
            ScoreType s = 0;
            Move bestMove = INVALIDMOVE;
            std::vector<Move> pv;
            ThreadData d = {depth,seldepth,s,extP,bestMove,pv}; // only input coef
            ThreadPool::instance().searchSync(d);
            bestMove = ThreadPool::instance().main().getData().best; // here output results

            results[k][t].name = extP.id();
            results[k][t].k = (int)k;
            results[k][t].t = timeControls[t];

            results[k][t].computerMove = showAlgAbr(bestMove,extP);
            LogIt(logInfo) << "Best move found is  " << results[k][t].computerMove;

            if ( extP.shallFindBest()){
                LogIt(logInfo) << "Best move should be " << extP.bestMoves();
                results[k][t].bm = extP.bestMoves();
                results[k][t].score = 0;
                bool success = false;
                for(size_t i = 0 ; i < results[k][t].bm.size() ; ++i){
                    if ( results[k][t].computerMove == results[k][t].bm[i]){
                        results[k][t].score = scores[t];
                        success = true;
                        break;
                    }
                }
                if ( breakAtFirstSuccess && success ) break;
            }
            else{
                LogIt(logInfo) << "Bad move was " << extP.badMoves();
                results[k][t].am = extP.badMoves();
                results[k][t].score = scores[t];
                bool success = true;
                for(size_t i = 0 ; i < results[k][t].am.size() ; ++i){
                    if ( results[k][t].computerMove == results[k][t].am[i]){
                        results[k][t].score = 0;
                        success = false;
                        break;
                    }
                }
                if ( breakAtFirstSuccess && success ) break;
            }
        }
    }

    // display results
    int totalScore = 0;
    std::cout << std::setw(25) << "Test"
              << std::setw(14) << "Move";
    for(size_t j = 0 ; j < timeControls.size() ; ++j){
        std::cout << std::setw(8) << timeControls[j];
    }
    std::cout << std::setw(6) << "score" << std::endl;
    for (size_t k = 0 ; k < positions.size() ; ++k ){
        int score = 0;
        for(size_t t = 0 ; t < timeControls.size() ; ++t){
            score += results[k][t].score;
        }
        totalScore += score;
        std::stringstream str;
        const std::vector<std::string> v = (results[k][0].bm.empty()?results[k][0].am:results[k][0].bm);
        std::ostream_iterator<std::string> strIt(str," ");
        std::copy(v.begin(), v.end(), strIt);
        std::cout << std::setw(25) << results[k][0].name
                  << std::setw(14) << (results[k][0].bm.empty()?std::string("!")+str.str():str.str());
        for(size_t j = 0 ; j < timeControls.size() ; ++j){
            std::cout << std::setw(8) << results[k][j].computerMove;
        }
        std::cout << std::setw(6) << score << std::endl;
    }

    if ( eloF(100) != 0) {
        LogIt(logInfo) << "Total score " << totalScore << " => ELO " << eloF(totalScore);
    }

    // clear results table
    for (size_t k = 0 ; k < positions.size() ; ++k ){
        delete[] results[k];
    }
    delete[] results;
}

void ExtendedPosition::testStatic(const std::vector<std::string> & positions,
                                  int                              chunck,
                                  bool                             withMoveCount) {
    struct Results {
        Results() :k(0), t(0), score(0){}
        int k;
        int t;
        ScoreType score;
        std::string name;
    };

    Results * results = new Results[positions.size()];

    // run the test and fill results table
    for (size_t k = 0; k < positions.size(); ++k) {
        std::cout << "Test #" << k << " " << positions[k] << std::endl;
        ExtendedPosition extP(positions[k], withMoveCount);
        //std::cout << " " << t << std::endl;
        float gp = 0;
        ScoreType ret = eval(extP,gp);

        results[k].name = extP.id();
        results[k].k = (int)k;
        results[k].score = ret;

        std::cout << "score is  " << ret << std::endl;

    }

    // display results
    std::cout << std::setw(25) << "Test" << std::setw(14) << "score" << std::endl;
    ScoreType score = 0;
    for (size_t k = 0; k < positions.size(); ++k) {
        std::cout << std::setw(25) << results[k].name << std::setw(14) << results[k].score << std::endl;
        if ( k%chunck == 0 ){
            score = results[k].score;
        }
        // only compare unsigned score ...
        if ( std::abs( std::abs(results[k].score) - std::abs(score) ) > 0 ){
            LogIt(logWarn) << "Score differ !";
        }
    }

    // clear results table
    delete[] results;
}

bool test(const std::string & option){

    if (option == "help_test") {
        LogIt(logInfo) << "Available analysis tests";
        LogIt(logInfo) << " BT2630";
        LogIt(logInfo) << " arasan";
        LogIt(logInfo) << " arasan_sym";
        LogIt(logInfo) << " CCROHT";
        LogIt(logInfo) << " BKTest";
        LogIt(logInfo) << " EETest";
        LogIt(logInfo) << " KMTest";
        LogIt(logInfo) << " LCTest";
        LogIt(logInfo) << " sbdTest";
        LogIt(logInfo) << " STS";
        LogIt(logInfo) << "Available unitary tests";
        LogIt(logInfo) << " xortest zhash_1 zhash_2 ... zhash_n : test zobrist hashing";
        LogIt(logInfo) << " gameplay : very old test that forces a game";
        LogIt(logInfo) << " zhash : another hash test";
        LogIt(logInfo) << " movegenerator : test move generator";
        LogIt(logInfo) << " threats : test threat detection";
        LogIt(logInfo) << " see : test static exchange evaluation";
        LogIt(logInfo) << " ordering fen : test move ordering";
        LogIt(logInfo) << " generator fen depth display : test move generator";
        LogIt(logInfo) << " divide (not working yet)";
        LogIt(logInfo) << " staticanalysis fen : test static evaluation of a position";
        LogIt(logInfo) << " analysis fen depth display : test analysis of a position";
        LogIt(logInfo) << " playUI fen depth : console mode game against the UI";
        LogIt(logInfo) << " playAI fen depth : console mode test match AI vs AI";
        LogIt(logInfo) << " texel_tuning inputfile : ru texel tuning based on given file";
        return 0;
    }

    if (option == "BT2630") {
        std::vector<std::string> positions;

        if ( ! ExtendedPosition::readEPDFile("TestSuite/bt2630.epd",positions) ){
            return 1;
        }

        std::vector<int> timeControls = { 15000*60 }; //mseconds
        std::vector<int> scores = { 1 };

        ExtendedPosition::test(positions,timeControls,true,scores,[](int score){return 2630-35*(30-score);},false);
        return true;
    }

    if (option == "WAC") {
        std::vector<std::string> positions;

        if ( ! ExtendedPosition::readEPDFile("TestSuite/WinAtChess.epd",positions) ){
            return 1;
        }

        std::vector<int> timeControls = { 3000 }; //mseconds
        std::vector<int> scores = { 1 };

        ExtendedPosition::test(positions,timeControls,true,scores,[](int score){return 2630-35*(30-score);},false);
        return true;
    }

    if (option == "arasan") {
        std::vector<std::string> positions;

        if ( ! ExtendedPosition::readEPDFile("TestSuite/arasan19a.epd",positions) ){
            return 1;
        }

        std::vector<int> timeControls = { 1000*60 }; //mseconds
        std::vector<int> scores = { 1 };

        ExtendedPosition::test(positions,timeControls,true,scores,[](int score){return score;},false);
        return true;
    }

    if (option == "arasan_sym") {
        std::vector<std::string> positions;

        if (!ExtendedPosition::readEPDFile("TestSuite/arasan_sym_eval.epd", positions)) {
            return 1;
        }

        ExtendedPosition::testStatic(positions);
        return true;
    }

    if (option == "CCROHT") {
        std::vector<std::string> positions;
        positions.push_back("rn1qkb1r/pp2pppp/5n2/3p1b2/3P4/2N1P3/PP3PPP/R1BQKBNR w KQkq - 0 1 id \"CCR01\"; bm Qb3;");
        positions.push_back("rn1qkb1r/pp2pppp/5n2/3p1b2/3P4/1QN1P3/PP3PPP/R1B1KBNR b KQkq - 1 1 id \"CCR02\";bm Bc8;");
        positions.push_back("r1bqk2r/ppp2ppp/2n5/4P3/2Bp2n1/5N1P/PP1N1PP1/R2Q1RK1 b kq - 1 10 id \"CCR03\"; bm Nh6; am Ne5;");
        positions.push_back("r1bqrnk1/pp2bp1p/2p2np1/3p2B1/3P4/2NBPN2/PPQ2PPP/1R3RK1 w - - 1 12 id \"CCR04\"; bm b4;");
        positions.push_back("rnbqkb1r/ppp1pppp/5n2/8/3PP3/2N5/PP3PPP/R1BQKBNR b KQkq - 3 5 id \"CCR05\"; bm e5;");
        positions.push_back("rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/P1N5/1PQ1PPPP/R1B1KBNR b KQ - 1 5 id \"CCR06\"; bm Bcx3+;");
        positions.push_back("r4rk1/3nppbp/bq1p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - - 1 12 id \"CCR07\"; bm Rfb8;");
        positions.push_back("rn1qkb1r/pb1p1ppp/1p2pn2/2p5/2PP4/5NP1/PP2PPBP/RNBQK2R w KQkq c6 1 6 id \"CCR08\"; bm d5;");
        positions.push_back("r1bq1rk1/1pp2pbp/p1np1np1/3Pp3/2P1P3/2N1BP2/PP4PP/R1NQKB1R b KQ - 1 9 id \"CCR09\"; bm Nd4;");
        positions.push_back("rnbqr1k1/1p3pbp/p2p1np1/2pP4/4P3/2N5/PP1NBPPP/R1BQ1RK1 w - - 1 11 id \"CCR10\"; bm a4;");
        positions.push_back("rnbqkb1r/pppp1ppp/5n2/4p3/4PP2/2N5/PPPP2PP/R1BQKBNR b KQkq f3 1 3 id \"CCR11\"; bm d5;");
        positions.push_back("r1bqk1nr/pppnbppp/3p4/8/2BNP3/8/PPP2PPP/RNBQK2R w KQkq - 2 6 id \"CCR12\"; bm Bxf7+;");
        positions.push_back("rnbq1b1r/ppp2kpp/3p1n2/8/3PP3/8/PPP2PPP/RNBQKB1R b KQ d6 1 5 id \"CCR13\"; am Ne4;");
        positions.push_back("rnbqkb1r/pppp1ppp/3n4/8/2BQ4/5N2/PPP2PPP/RNB2RK1 b kq - 1 6 id \"CCR14\"; am Nxc4;");
        positions.push_back("r2q1rk1/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 w - - 1 12 id \"CCR15\"; bm exf6;");
        positions.push_back("r1bqkb1r/2pp1ppp/p1n5/1p2p3/3Pn3/1B3N2/PPP2PPP/RNBQ1RK1 b kq - 2 7 id \"CCR16\"; bm d5;");
        positions.push_back("r2qkbnr/2p2pp1/p1pp4/4p2p/4P1b1/5N1P/PPPP1PP1/RNBQ1RK1 w kq - 1 8 id \"CCR17\"; am hxg4;");
        positions.push_back("r1bqkb1r/pp3ppp/2np1n2/4p1B1/3NP3/2N5/PPP2PPP/R2QKB1R w KQkq e6 1 7 id \"CCR18\"; bm Bxf6+;");
        positions.push_back("rn1qk2r/1b2bppp/p2ppn2/1p6/3NP3/1BN5/PPP2PPP/R1BQR1K1 w kq - 5 10 id \"CCR19\"; am Bxe6;");
        positions.push_back("r1b1kb1r/1pqpnppp/p1n1p3/8/3NP3/2N1B3/PPP1BPPP/R2QK2R w KQkq - 3 8 id \"CCR20\"; am Ndb5;");
        positions.push_back("r1bqnr2/pp1ppkbp/4N1p1/n3P3/8/2N1B3/PPP2PPP/R2QK2R b KQ - 2 11 id \"CCR21\"; am Kxe6;");
        positions.push_back("r3kb1r/pp1n1ppp/1q2p3/n2p4/3P1Bb1/2PB1N2/PPQ2PPP/RN2K2R w KQkq - 3 11 id \"CCR22\"; bm a4;");
        positions.push_back("r1bq1rk1/pppnnppp/4p3/3pP3/1b1P4/2NB3N/PPP2PPP/R1BQK2R w KQ - 3 7 id \"CCR23\"; bm Bxh7+;");
        positions.push_back("r2qkbnr/ppp1pp1p/3p2p1/3Pn3/4P1b1/2N2N2/PPP2PPP/R1BQKB1R w KQkq - 2 6 id \"CCR24\"; bm Nxe5;");
        positions.push_back("rn2kb1r/pp2pppp/1qP2n2/8/6b1/1Q6/PP1PPPBP/RNB1K1NR b KQkq - 1 6 id \"CCR25\"; am Qxb3;");

        std::vector<int> timeControls = { 15000,30000,60000,120000 }; //mseconds
        std::vector<int> scores = { 1,1,1,1 };

        ExtendedPosition::test(positions,timeControls,false,scores,[](int score){return 2000+8*score;});
        return true;
    }

    if (option == "BKTest") {
        std::vector<std::string> positions;
        positions.push_back("1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 0 bm Qd1+; id \"BK.01\";");
        positions.push_back("3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - - 0 0 bm d5; id \"BK.02\";");
        positions.push_back("2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - - 0 0 bm f5; id \"BK.03\";");
        positions.push_back("rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq - - 0 0 bm e6; id \"BK.04\";");
        positions.push_back("r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - - 0 0 bm Nd5 a4; id \"BK.05\";");
        positions.push_back("2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - - 0 0 bm g6; id \"BK.06\";");
        positions.push_back("1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - - 0 0 bm Nf6; id \"BK.07\";");
        positions.push_back("4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - - 0 0 bm f5; id \"BK.08\";");
        positions.push_back("2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - - 0 0 bm f5; id \"BK.09\";");
        positions.push_back("3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - - 0 0 bm Ne5; id \"BK.10\";");
        positions.push_back("2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - - 0 0 bm f4; id \"BK.11\";");
        positions.push_back("r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - - 0 0 bm Bf5; id \"BK.12\";");
        positions.push_back("r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - - 0 0 bm b4; id \"BK.13\";");
        positions.push_back("rnb2r1k/pp2p2p/2pp2p1/q2P1p2/8/1Pb2NP1/PB2PPBP/R2Q1RK1 w - - 0 0 bm Qd2 Qe1; id \"BK.14\";");
        positions.push_back("2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - - 0 0 bm Qxg7+; id \"BK.15\";");
        positions.push_back("r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq - 0 0 bm Ne4; id \"BK.16\";");
        positions.push_back("r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - - 0 0 bm h5; id \"BK.17\";");
        positions.push_back("r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - - 0 0 bm Nb3; id \"BK.18\";");
        positions.push_back("3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - - 0 0 bm Rxe4; id \"BK.19\";");
        positions.push_back("r4k2/pb2bp1r/1p1qp2p/3pNp2/3P1P2/2N3P1/PPP1Q2P/2KRR3 w - - 0 0 bm g4; id \"BK.20\";");
        positions.push_back("3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - - 0 0 bm Nh6; id \"BK.21\";");
        positions.push_back("2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - - 0 0 bm Bxe4; id \"BK.22\";");
        positions.push_back("r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq - 0 0 bm f6; id \"BK.23\";");
        positions.push_back("r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - - 0 0 bm f4; id \"BK.24\";");

        std::vector<int> timeControls = { 10000,30000,60000,120000 }; //mseconds
        std::vector<int> scores = { 1,1,1,1 };

        ExtendedPosition::test(positions,timeControls,false,scores,[](int){return 0;});
        return true;
    }

    if (option == "EETest") {
        std::vector<std::string> positions;

        positions.push_back("8/8/p2p3p/3k2p1/PP6/3K1P1P/8/8 b - - 0 0 bm Kc6; id \"E_E_T 001 - B vs B\"");
        positions.push_back("8/p5pp/3k1p2/3p4/1P3P2/P1K5/5P1P/8 b - - 0 0 bm g5; id \"E_E_T 002 - B vs B\"");
        positions.push_back("8/1p3p2/p7/8/2k5/4P1pP/2P1K1P1/8 w - - 0 0 bm h4; id \"E_E_T 003 - B vs B\"");
        positions.push_back("8/pp5p/3k2p1/8/4Kp2/2P1P2P/P7/8 w - - 0 0 bm exf4; id \"E_E_T 004 - B vs B\"");
        positions.push_back("8/7p/1p3pp1/p2K4/Pk3PPP/8/1P6/8 b - - 0 0 bm Kb3 f5; id \"E_E_T 005 - B vs B\"");
        positions.push_back("2k5/3b4/PP3K2/7p/4P3/1P6/8/8 w - - 0 0 bm Ke7; id \"E_E_T 006 - B vs L\"");
        positions.push_back("8/3Pb1p1/8/3P2P1/5P2/7k/4K3/8 w - - 0 0 bm Kd3; id \"E_E_T 007 - B vs L\"");
        positions.push_back("8/1Pk2Kpp/8/8/4nPP1/7P/8/8 b - - 0 0 bm Nf2; id \"E_E_T 008 - B vs S\"");
        positions.push_back("2n5/4k1p1/P6p/3PK3/7P/8/6P1/8 b - - 0 0 bm g6; id \"E_E_T 009 - B vs S\"");
        positions.push_back("4k3/8/3PP1p1/8/3K3p/8/3n2PP/8 b - - 0 0 am Nf1; id \"E_E_T 010 - B vs S\"");
        positions.push_back("6k1/5p2/P3p1p1/2Qp4/5q2/2K5/8/8 b - - 0 0 am Qc1+ Qe5+; id \"E_E_T 011 - D vs D\"");
        positions.push_back("8/6pk/8/2p2P1p/6qP/5QP1/8/6K1 w - - 0 0 bm Qd3 Qf2; id \"E_E_T 012 - D vs D\"");
        positions.push_back("5q1k/5P1p/Q7/5n1p/6P1/7K/8/8 w - - 0 0 bm Qa1+; id \"E_E_T 013 - D vs D&S\"");
        positions.push_back("4qr2/4p2k/1p2P1pP/5P2/1P3P2/6Q1/8/3B1K2 w - - 0 0 bm Ba4; id \"E_E_T 014 - D&L vs D&T\"");
        positions.push_back("8/kn4b1/P2B4/8/1Q6/6pP/1q4pP/5BK1 w - - 0 0 bm Bc5+; id \"E_E_T 015 - D&L&L vs D&L&S\"");
        positions.push_back("6k1/1p2p1bp/p5p1/4pb2/1q6/4Q3/1P2BPPP/2R3K1 w - - 0 0 bm Qa3; id \"E_E_T 017 - D&T&L vs D&L&L\"");
        positions.push_back("1rr2k2/p1q5/3p2Q1/3Pp2p/8/1P3P2/1KPRN3/8 w - - 0 0 e6 bm Rd1; id \"E_E_T 018 - D&T&S vs D&T&T\"");
        positions.push_back("r5k1/3R2p1/p1r1q2p/P4p2/5P2/2p1P3/5P1P/1R1Q2K1 w - - 0 0 am Rbb7; id \"E_E_T 019 - D&T&T vs D&T&T\"");
        positions.push_back("8/1p4k1/pK5p/2B5/P4pp1/8/7P/8 b - - 0 0 am g3; id \"E_E_T 020 - L vs B\"");
        positions.push_back("8/6p1/6P1/6Pp/B1p1p2K/6PP/3k2P1/8 w - - 0 0 bm Bd1; id \"E_E_T 021 - L vs B\"");
        positions.push_back("8/4k3/8/2Kp3p/B3bp1P/P7/1P6/8 b - - 0 0 bm Bg2; id \"E_E_T 022 - L vs L\"");
        positions.push_back("8/8/2p1K1p1/2k5/p7/P4BpP/1Pb3P1/8 w - - 0 0 am Kd7; id \"E_E_T 023 - L vs L\"");
        positions.push_back("8/3p3B/5p2/5P2/p7/PP5b/k7/6K1 w - - 0 0 bm b4; id \"E_E_T 024 - L vs L\"");
        positions.push_back("8/p4p2/1p2k2p/6p1/P4b1P/1P6/3B1PP1/6K1 w - - 0 0 am Bxf4; id \"E_E_T 025 - L vs L\"");
        positions.push_back("3b3k/1p4p1/p5p1/4B3/8/7P/1P3PP1/5K2 b - - 0 0 am Bf6; id \"E_E_T 027 - L vs L\"");
        positions.push_back("4b1k1/1p3p2/4pPp1/p2pP1P1/P2P4/1P1B4/8/2K5 w - - 0 0 bm b4; id \"E_E_T 028 - L vs L\"");
        positions.push_back("8/3k1p2/n3pP2/1p2P2p/5K2/1PB5/7P/8 b - - 0 0 am Kc6 b4; id \"E_E_T 029 - L vs S\"");
        positions.push_back("8/8/4p1p1/1P1kP3/4n1PK/2B4P/8/8 b - - 0 0 bm Kc5; id \"E_E_T 030 - L vs S\"");
        positions.push_back("8/5k2/4p3/B2p2P1/3K2n1/1P6/8/8 b - - 0 0 bm Kg6; id \"E_E_T 031 - L vs S\"");
        positions.push_back("5b2/p4B2/5B2/1bN5/8/P3r3/4k1K1/8 w - - 0 0 bm Bh5+; id \"E_E_T 032 - L&L&S vs T&L&L\"");
        positions.push_back("8/p5pq/8/p2N3p/k2P3P/8/KP3PB1/8 w - - 0 0 bm Be4; id \"E_E_T 033 - L&S vs D\"");
        positions.push_back("1b6/1P6/8/2KB4/6pk/3N3p/8/8 b - - 0 0 bm Kg3; id \"E_E_T 034 - L&S vs L&B\"");
        positions.push_back("8/p7/7k/1P1K3P/8/1n6/4Bp2/5Nb1 b - - 0 0 bm Na5; id \"E_E_T 035 - L&S vs L&S\"");
        positions.push_back("8/8/8/3K4/2N5/p2B4/p7/k4r2 w - - 0 0 bm Kc5; id \"E_E_T 036 - L&S vs T&B\"");
        positions.push_back("8/8/2kp4/5Bp1/8/5K2/3N4/2rN4 w - - 0 0 bm Nb3; id \"E_E_T 037 - L&S&S vs T&B\"");
        positions.push_back("k2K4/1p4pN/P7/1p3P2/pP6/8/8/8 w - - 0 0 bm f6 Kc7; id \"E_E_T 038 - S vs B\"");
        positions.push_back("k2N4/1qpK1p2/1p6/1P4p1/1P4P1/8/8/8 w - - 0 0 bm Nc6; id \"E_E_T 039 - S vs D\"");
        positions.push_back("6k1/4b3/4p1p1/8/6pP/4PN2/5K2/8 w - - 0 0 bm Ne5 Nh2; id \"E_E_T 040 - S vs L\"");
        positions.push_back("8/8/6Np/2p3kP/1pPbP3/1P3K2/8/8 w - - 0 0 bm e5; id \"E_E_T 041 - S vs L\"");
        positions.push_back("8/3P4/1p3b1p/p7/P7/1P3NPP/4p1K1/3k4 w - - 0 0 bm g4; id \"E_E_T 042 - S vs L\"");
        positions.push_back("8/8/1p2p3/p3p2b/P1PpP2P/kP6/2K5/7N w - - 0 0 bm Nf2; id \"E_E_T 043 - S vs L\"");
        positions.push_back("4N3/8/3P3p/1k2P3/7p/1n1K4/8/8 w - - 0 0 bm d7; id \"E_E_T 044 - S vs S\"");
        positions.push_back("N5n1/2p1kp2/2P3p1/p4PP1/K4P2/8/8/8 w - - 0 0 bm f6 Kb5; id \"E_E_T 045 - S vs S\"");
        positions.push_back("8/8/2pn4/p4p1p/P4N2/1Pk2KPP/8/8 w - - 0 0 bm Ne2 Ne6; id \"E_E_T 046 - S vs S\"");
        positions.push_back("8/7k/2P5/2p4p/P3N2K/8/8/5r2 w - - 0 0 bm Ng5+; id \"E_E_T 047 - S vs T\"");
        positions.push_back("2k1r3/p7/K7/1P6/P2N4/8/P7/8 w - - 0 0 bm Nc6; id \"E_E_T 048 - S vs T\"");
        positions.push_back("1k6/8/8/1K6/5pp1/8/4Pp1p/R7 w - - 0 0 bm Kb6; id \"E_E_T 049 - T vs B\"");
        positions.push_back("6k1/8/8/1K4p1/3p2P1/2pp4/8/1R6 w - - 0 0 bm Kc6; id \"E_E_T 050 - T vs B\"");
        positions.push_back("8/5p2/3pp2p/p5p1/4Pk2/2p2P1P/P1Kb2P1/1R6 w - - 0 0 bm a4 Rb5; id \"E_E_T 051 - T vs L\"");
        positions.push_back("8/8/4pR2/3pP2p/6P1/2p4k/P1Kb4/8 b - - 0 0 bm hxg4; id \"E_E_T 052 - T vs L\"");
        positions.push_back("3k3K/p5P1/P3r3/8/1N6/8/8/8 w - - 0 0 bm Kh7; id \"E_E_T 053 - T vs S\"");
        positions.push_back("8/8/5p2/5k2/p4r2/PpKR4/1P5P/8 w - - 0 0 am Rd4; id \"E_E_T 054 - T vs T\"");
        positions.push_back("5k2/7R/8/4Kp2/5Pp1/P5rp/1P6/8 w - - 0 0 bm Kf6; id \"E_E_T 055 - T vs T\"");
        positions.push_back("2K5/p7/7P/5pR1/8/5k2/r7/8 w - - 0 0 bm Rxf5+; id \"E_E_T 056 - T vs T\"");
        positions.push_back("8/2R4p/4k3/1p2P3/pP3PK1/r7/8/8 b - - 0 0 bm h5 Ra1; id \"E_E_T 057 - T vs T\"");
        positions.push_back("2k1r3/5R2/3KP3/8/1pP3p1/1P5p/8/8 w - - 0 0 bm Rc7+; id \"E_E_T 058 - T vs T\"");
        positions.push_back("8/6p1/1p5p/1R2k3/4p3/1P2P3/1K4PP/3r4 b - - 0 0 am Rd5; id \"E_E_T 059 - T vs T\"");
        positions.push_back("5K2/kp3P2/2p5/2Pp4/3P4/r7/p7/6R1 w - - 0 0 bm Ke7; id \"E_E_T 060 - T vs T\"");
        positions.push_back("8/pp3K2/2P4k/5p2/8/6P1/R7/6r1 w - - 0 0 bm Kf6; id \"E_E_T 061 - T vs T\"");
        positions.push_back("2r3k1/6pp/3pp1P1/1pP5/1P6/P4R2/5K2/8 w - - 0 0 bm c6; id \"E_E_T 062 - T vs T\"");
        positions.push_back("r2k4/8/8/1P4p1/8/p5P1/6P1/1R3K2 w - - 0 0 bm b6; id \"E_E_T 063 - T vs T\"");
        positions.push_back("8/4k3/1p4p1/p7/P1r1P3/1R4Pp/5P1P/4K3 w - - 0 0 bm Ke2; id \"E_E_T 064 - T vs T\"");
        positions.push_back("R7/4kp2/P3p1p1/3pP1P1/3P1P2/p6r/3K4/8 w - - 0 0 bm Kc2; id \"E_E_T 065 - T vs T\"");
        positions.push_back("8/1pp1r3/p1p2k2/6p1/P5P1/1P3P2/2P1RK2/8 b - - 0 0 am Rxe2+ Re5; id \"E_E_T 066 - T vs T\"");
        positions.push_back("8/1p2R3/8/p5P1/3k4/P2p2K1/1P6/5r2 w - - 0 0 bm Kg2; id \"E_E_T 067 - T vs T\"");
        positions.push_back("R7/P5Kp/2P5/k3r2P/8/5p2/p4P2/8 w - - 0 0 bm Rb8; id \"E_E_T 068 - T vs T\"");
        positions.push_back("8/2p4K/4k1p1/p1p1P3/PpP2P2/5R1P/8/6r1 b - - 0 0 bm Kf7; id \"E_E_T 069 - T vs T\"");
        positions.push_back("8/B7/1R6/q3k2p/8/6p1/5P2/5K2 w - - 0 0 bm Rb3; id \"E_E_T 071 - T&L vs D\"");
        positions.push_back("5k2/8/2Pb1B2/8/6RK/7p/5p1P/8 w - - 0 0 bm Be5; id \"E_E_T 072 - T&L vs L&B\"");
        positions.push_back("3kB3/R4P2/8/8/1p6/pp6/5r2/1K6 w - - 0 0 bm f8=Q f8=R; id \"E_E_T 073 - T&L vs T&B\"");
        positions.push_back("5k2/1p6/1P1p4/1K1p2p1/PB1P2P1/3pR2p/1P2p1pr/8 w - - 0 0 bm Ba5; id \"E_E_T 074 - T&L vs T&B\"");
        positions.push_back("6k1/p6p/1p1p2p1/2bP4/P1P5/2B3P1/4r2P/1R5K w - - 0 0 bm a5; id \"E_E_T 075 - T&L vs T&L\"");
        positions.push_back("3R3B/8/1r4b1/8/4pP2/7k/8/7K w - - 0 0 bm Bd4; id \"E_E_T 076 - T&L vs T&L\"");
        positions.push_back("rk1b4/p2p2p1/1P6/2R2P2/8/2K5/8/5B2 w - - 0 0 bm Rc8+; id \"E_E_T 077 - T&L vs T&L\"");
        positions.push_back("3r1k2/8/7R/8/8/pp1B4/P7/n1K5 w - - 0 0 bm Rf6+; id \"E_E_T 078 - T&L vs T&S\"");
        positions.push_back("r5k1/5ppp/1p6/2b1R3/1p3P2/2nP2P1/1B3PKP/5B2 w - - 0 0 bm d4; id \"E_E_T 079 - T&L&L vs T&L&S\"");
        positions.push_back("5k2/3p1b2/4pN2/3PPpp1/6R1/6PK/1B1q1P2/8 w - - 0 0 bm Ba3+; id \"E_E_T 080 - T&L&S vs D&L\"");
        positions.push_back("8/1p5p/6p1/1p4Pp/1PpR4/2P1K1kB/6Np/7b w - - 0 0 bm Rd1; id \"E_E_T 081 - T&L&S vs L&B\"");
        positions.push_back("7k/1p1Nr2P/3Rb3/8/3K4/6b1/8/5B2 w - - 0 0 bm Ne5; id \"E_E_T 082 - T&L&S vs T&L&L\"");
        positions.push_back("8/1B4k1/5pn1/6N1/1P3rb1/P1K4p/3R4/8 w - - 0 0 bm Nxh3; id \"E_E_T 083 - T&L&S vs T&L&S\"");
        positions.push_back("8/7p/6p1/3Np1bk/4Pp2/1R3PPK/5r1P/8 w - - 0 0 bm Nc7; id \"E_E_T 084 - T&S vs T&L\"");
        positions.push_back("1r6/3b1p2/2k4P/1N3p1P/5P2/8/3R4/2K5 w - - 0 0 bm Na7+; id \"E_E_T 085 - T&S vs T&L\"");
        positions.push_back("k6r/8/1R6/8/1pK2p2/8/7N/3b4 w - - 0 0 bm Nf1; id \"E_E_T 086 - T&S vs T&L\"");
        positions.push_back("8/8/8/p1p5/2P1k3/1Pn5/1N1R2K1/1r6 w - - 0 0 bm Kg3; id \"E_E_T 087 - T&S vs T&S\"");
        positions.push_back("5n1k/1r3P1p/p2p3P/P7/8/1N6/5R2/4K3 b - - 0 0 bm Re7+; id \"E_E_T 088 - T&S vs T&S\"");
        positions.push_back("6R1/P2k1N2/r7/7P/r7/p7/7K/8 w - - 0 0 bm Nh6; id \"E_E_T 089 - T&S vs T&T\"");
        positions.push_back("8/1rk1P3/7p/P7/1N2r3/5RKb/8/8 w - - 0 0 bm Na6+; id \"E_E_T 090 - T&S&B vs T&T&L\"");
        positions.push_back("2K5/k3q3/6pR/6p1/6Pp/7P/8/3R4 w - - 0 0 bm Rh7; id \"E_E_T 090 - T&T vs D\"");
        positions.push_back("R5bk/5r2/P7/1P1pR3/3P4/7p/5p1K/8 w - - 0 0 bm Rh5+; id \"E_E_T 092 - T&T vs T&L\"");
        positions.push_back("4k3/7r/3nb3/2R5/8/6n1/1R3K2/8 w - - 0 0 bm Re5; id \"E_E_T 093 - T&T vs T&L&S\"");
        positions.push_back("1r6/1r6/1P1KP3/6k1/1R4p1/7p/7R/8 w - - 0 0 bm Kc6 Rb5; id \"E_E_T 094 - T&T vs T&T\"");
        positions.push_back("1k1K4/1p6/P4P2/2R5/4p2R/r2p4/8/3r4 w - - 0 0 bm Rf4; id \"E_E_T 095 - T&T vs T&T\"");
        positions.push_back("5k2/R1p5/p1R3Pb/2K5/2B5/2q2b2/8/8 w - - 0 0 bm g7+; id \"E_E_T 096 - T&T&L vs D&L&L\"");
        positions.push_back("8/8/k7/n7/p1R5/p7/4r1p1/KB3R2 w - - 0 0 bm Rc3; id \"E_E_T 097 - T&T&L vs T&S&B\"");
        positions.push_back("3r2k1/p1R2ppp/1p6/P1b1PP2/3p4/3R2B1/5PKP/1r6 w - - 0 0 bm f6; id \"E_E_T 098 - T&T&L vs T&T&L\"");
        positions.push_back("8/5p2/5rp1/p2k1r1p/P1pNp2P/RP1bP1P1/5P1R/4K3 b - - 0 0 bm c3; id \"E_E_T 099 - T&T&S vs T&T&L\"");
        positions.push_back("1r4k1/6pp/3Np1b1/p1R1P3/6P1/P2pr2P/1P1R2K1/8 b - - 0 0 bm Rf8; id \"E_E_T 100 - T&T&S vs T&T&L\"");
        positions.push_back("4k1r1/pp2p2p/3pN1n1/2pP4/2P3P1/PP5P/8/5RK1 b - - 0 0 am Nf8; id \"E_E_T 16b -  T&S vs T&S\"");
        positions.push_back("8/3k3p/1p2p3/p4p2/Pb1Pp3/2B3PP/1P3P2/5K2 w - - 0 0 am Bxb4; id \"E_E_T 26b -  L vs L\"");
        positions.push_back("8/1k6/8/Q7/7p/6p1/6pr/6Kb w - - 0 0 bm Qc5; id \"E_E_T 70b - D vs T&L&B\"");

        std::vector<int> timeControls = { 1000,3000,6000,12000 }; //mseconds
        std::vector<int> scores = { 1,1,1,1 };

        ExtendedPosition::test(positions,timeControls,false,scores,[](int){return 0;});
        return true;
    }

    if (option == "KMTest") {
        std::vector<std::string> positions;
        positions.push_back("1rbq1rk1/p1b1nppp/1p2p3/8/1B1pN3/P2B4/1P3PPP/2RQ1R1K w - - 0 0 bm Nf6+; id \"position 01\";");
        positions.push_back("3r2k1/p2r1p1p/1p2p1p1/q4n2/3P4/PQ5P/1P1RNPP1/3R2K1 b - - 0 0 bm Nxd4 id \"position 02\";");
        positions.push_back("3r2k1/1p3ppp/2pq4/p1n5/P6P/1P6/1PB2QP1/1K2R3 w - - 0 0 am Rd1; id \"position 03\";");
        positions.push_back("r1b1r1k1/1ppn1p1p/3pnqp1/8/p1P1P3/5P2/PbNQNBPP/1R2RB1K w - - 0 0 bm Rxb2; id \"position 04\";");
        positions.push_back("2r4k/pB4bp/1p4p1/6q1/1P1n4/2N5/P4PPP/2R1Q1K1 b - - 0 0 bm Qxc1; id \"position 05\";");
        positions.push_back("r5k1/3n1ppp/1p6/3p1p2/3P1B2/r3P2P/PR3PP1/2R3K1 b - - 0 0 am Rxa2; id \"position 06\";");
        positions.push_back("2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - - 0 0 bm Bxe4 id \"position 07\";");
        positions.push_back("5r1k/6pp/1n2Q3/4p3/8/7P/PP4PK/R1B1q3 b - - 0 0 bm h6; id \"position 08\";");
        positions.push_back("r3k2r/pbn2ppp/8/1P1pP3/P1qP4/5B2/3Q1PPP/R3K2R w KQkq - 0 0 bm Be2; id \"position 09\";");
        positions.push_back("3r2k1/ppq2pp1/4p2p/3n3P/3N2P1/2P5/PP2QP2/K2R4 b - - 0 0 bm Nxc3; id \"position 10\";");
        positions.push_back("q3rn1k/2QR4/pp2pp2/8/P1P5/1P4N1/6n1/6K1 w - - 0 0 bm Nf5; id \"position 11\";");
        positions.push_back("6k1/p3q2p/1nr3pB/8/3Q1P2/6P1/PP5P/3R2K1 b - - 0 0 bm Rd6; id \"position 12\";");
        positions.push_back("1r4k1/7p/5np1/3p3n/8/2NB4/7P/3N1RK1 w - - 0 0 bm Nxd5; id \"position 13\";");
        positions.push_back("1r2r1k1/p4p1p/6pB/q7/8/3Q2P1/PbP2PKP/1R3R2 w - - 0 0 bm Rxb2; id \"position 14\";");
        positions.push_back("r2q1r1k/pb3p1p/2n1p2Q/5p2/8/3B2N1/PP3PPP/R3R1K1 w - - 0 0 bm Bxf5; id \"position 15\";");
        positions.push_back("8/4p3/p2p4/2pP4/2P1P3/1P4k1/1P1K4/8 w - - 0 0 bm b4; id \"position 16\";");
        positions.push_back("1r1q1rk1/p1p2pbp/2pp1np1/6B1/4P3/2NQ4/PPP2PPP/3R1RK1 w - - 0 0 bm e5; id \"position 17\";");
        positions.push_back("q4rk1/1n1Qbppp/2p5/1p2p3/1P2P3/2P4P/6P1/2B1NRK1 b - - 0 0 bm Qc8; id \"position 18\";");
        positions.push_back("r2q1r1k/1b1nN2p/pp3pp1/8/Q7/PP5P/1BP2RPN/7K w - - 0 0 bm Qxd7; id \"position 19\";");
        positions.push_back("8/5p2/pk2p3/4P2p/2b1pP1P/P3P2B/8/7K w - - 0 0 bm Bg4; id \"position 20\";");
        positions.push_back("8/2k5/4p3/1nb2p2/2K5/8/6B1/8 w - - 0 0 bm Kxb5; id \"position 21\";");
        positions.push_back("1B1b4/7K/1p6/1k6/8/8/8/8 w - - 0 0 bm Ba7; id \"position 22\";");
        positions.push_back("rn1q1rk1/1b2bppp/1pn1p3/p2pP3/3P4/P2BBN1P/1P1N1PP1/R2Q1RK1 b - - 0 0 bm Ba6; id \"position 23\";");
        positions.push_back("8/p1ppk1p1/2n2p2/8/4B3/2P1KPP1/1P5P/8 w - - 0 0 bm Bxc6; id \"position 24\";");
        positions.push_back("8/3nk3/3pp3/1B6/8/3PPP2/4K3/8 w - - 0 0 bm Bxd7; id \"position 25\";");

        std::vector<int> timeControls = { 1000,3000,6000,12000 }; //mseconds
        std::vector<int> scores = { 1,1,1,1 };

        ExtendedPosition::test(positions,timeControls,false,scores,[](int){return 0;});
        return true;
    }

    if (option == "LCTest") {
        std::vector<std::string> positions;
        positions.push_back("r3kb1r/3n1pp1/p6p/2pPp2q/Pp2N3/3B2PP/1PQ2P2/R3K2R w KQkq - bm d6; id \"LCTII.POS.01\";c0 \"Chernin - Miles, Tunis 1985\";");
        positions.push_back("1k1r3r/pp2qpp1/3b1n1p/3pNQ2/2pP1P2/2N1P3/PP4PP/1K1RR3 b - - bm Bb4; id \"LCTII.POS.02\";c0 \"Lilienthal - Botvinnik, Moskau 1945\";");
        positions.push_back("r6k/pp4p1/2p1b3/3pP3/7q/P2B3r/1PP2Q1P/2K1R1R1 w - - bm Qc5; id \"LCTII.POS.03\";c0 \"Boissel - Boulard, corr. 1994\";");
        positions.push_back("1nr5/2rbkppp/p3p3/Np6/2PRPP2/8/PKP1B1PP/3R4 b - - bm e5; id \"LCTII.POS.04\";c0 \"Kaplan - Kopec, USA 1975\";");
        positions.push_back("2r2rk1/1p1bq3/p3p2p/3pPpp1/1P1Q4/P7/2P2PPP/2R1RBK1 b - - bm Bb5; id \"LCTII.POS.05\";c0 \"Estrin - Pytel, Albena 1973\";");
        positions.push_back("3r1bk1/p4ppp/Qp2p3/8/1P1B4/Pq2P1P1/2r2P1P/R3R1K1 b - - bm e5; id \"LCTII.POS.06\";c0 \"Nimzowitsch - Capablanca, New York 1927\";");
        positions.push_back("r1b2r1k/pp2q1pp/2p2p2/2p1n2N/4P3/1PNP2QP/1PP2RP1/5RK1 w - - bm Nd1; id \"LCTII.POS.07\";c0 \"Tartakower - Rubinstein, Moskau 1925\";");
        positions.push_back("r2qrnk1/pp3ppb/3b1n1p/1Pp1p3/2P1P2N/P5P1/1B1NQPBP/R4RK1 w - - bm Bh3; id \"LCTII.POS.08\";c0 \"Polugaevsky - Unzicker, Kislovodsk 1972\";");
        positions.push_back("5nk1/Q4bpp/5p2/8/P1n1PN2/q4P2/6PP/1R4K1 w - - bm Qd4; id \"LCTII.POS.09\";c0 \"Boissel - Del Gobbo, corr. 1994\";");
        positions.push_back("r3k2r/3bbp1p/p1nppp2/5P2/1p1NP3/5NP1/PPPK3P/3R1B1R b kq - bm Bf8; id \"LCTII.POS.10\";c0 \"Cucka - Jansa, Brno 1960\";");
        positions.push_back("bn6/1q4n1/1p1p1kp1/2pPp1pp/1PP1P1P1/3N1P1P/4B1K1/2Q2N2 w - - bm h4; id \"LCTII.POS.11\";c0 \"Landau - Schmidt, Noordwijk 1938\";");
        positions.push_back("3r2k1/pp2npp1/2rqp2p/8/3PQ3/1BR3P1/PP3P1P/3R2K1 b - - bm Rb6; id \"LCTII.POS.12\";c0 \"Korchnoi - Karpov, Meran 1981\";");
        positions.push_back("1r2r1k1/4ppbp/B5p1/3P4/pp1qPB2/2n2Q1P/P4PP1/4RRK1 b - - bm Nxa2; id \"LCTII.POS.13\";c0 \"Barbero - Kouatly, Budapest 1987\";");
        positions.push_back("r2qkb1r/1b3ppp/p3pn2/1p6/1n1P4/1BN2N2/PP2QPPP/R1BR2K1 w kq - bm d5; id \"LCTII.POS.14\";c0 \"Spasski - Aftonomov, Leningrad 1949\";");
        positions.push_back("1r4k1/1q2bp2/3p2p1/2pP4/p1N4R/2P2QP1/1P3PK1/8 w - - bm Nxd6; id \"LCTII.CMB.01\";c0 \"Romanishin - Gdansky, Polonica Zdroj 1992\";");
        positions.push_back("rn3rk1/pbppq1pp/1p2pb2/4N2Q/3PN3/3B4/PPP2PPP/R3K2R w KQ - bm Qxh7+; id \"LCTII.CMB.02\";c0 \"Lasker,Ed - Thomas, London 1911\";");
        positions.push_back("4r1k1/3b1p2/5qp1/1BPpn2p/7n/r3P1N1/2Q1RPPP/1R3NK1 b - - bm Qf3; id \"LCTII.CMB.03\";c0 \"Andruet - Spassky, BL 1988\";");
        positions.push_back("2k2b1r/1pq3p1/2p1pp2/p1n1PnNp/2P2B2/2N4P/PP2QPP1/3R2K1 w - - bm exf6; id \"LCTII.CMB.04\";c0 \"Vanka - Jansa, Prag 1957\";");
        positions.push_back("2r2r2/3qbpkp/p3n1p1/2ppP3/6Q1/1P1B3R/PBP3PP/5R1K w - - bm Rxh7+; id \"LCTII.CMB.05\";c0 \"Boros - Szabo, Budapest 1937\";");
        positions.push_back("2r1k2r/2pn1pp1/1p3n1p/p3PP2/4q2B/P1P5/2Q1N1PP/R4RK1 w q - bm exf6; id \"LCTII.CMB.06\";c0 \"Lilienthal - Capablanca, Hastings 1934\";");
        positions.push_back("2rr2k1/1b3ppp/pb2p3/1p2P3/1P2BPnq/P1N3P1/1B2Q2P/R4R1K b - - bm Rxc3; id \"LCTII.CMB.07\";c0 \"Rotlewi - Rubinstein, Lodz 1907\";");
        positions.push_back("2b1r1k1/r4ppp/p7/2pNP3/4Q3/q6P/2P2PP1/3RR1K1 w - - bm Nf6+; id \"LCTII.CMB.08\";c0 \"Zarkov - Mephisto, Albuquerque 1991\";");
        positions.push_back("6k1/5p2/3P2p1/7n/3QPP2/7q/r2N3P/6RK b - - bm Rxd2; id \"LCTII.CMB.09\";c0 \"Portisch - Kasparov, Moskau 1981\";");
        positions.push_back("rq2rbk1/6p1/p2p2Pp/1p1Rn3/4PB2/6Q1/PPP1B3/2K3R1 w - - bm Bxh6; id \"LCTII.CMB.10\";c0 \"Tchoudinovskikh - Merchiev, UdSSR 1987\";");
        positions.push_back("rnbq2k1/p1r2p1p/1p1p1Pp1/1BpPn1N1/P7/2P5/6PP/R1B1QRK1 w - - bm Nxh7; id \"LCTII.CMB.11\";c0 \"Vaisser - Genius 2, Aubervilliers, 1994\";");
        positions.push_back("r2qrb1k/1p1b2p1/p2ppn1p/8/3NP3/1BN5/PPP3QP/1K3RR1 w - - bm e5; id \"LCTII.CMB.12\";c0 \"Spassky - Petrosian, Moskau 1969\";");
        positions.push_back("8/1p3pp1/7p/5P1P/2k3P1/8/2K2P2/8 w - - bm f6; id \"LCTII.FIN.01\";c0 \"NN - Lasker,Ed\";");
        positions.push_back("8/pp2r1k1/2p1p3/3pP2p/1P1P1P1P/P5KR/8/8 w - - bm f5; id \"LCTII.FIN.02\";c0 \"Capablanca - Eliskases, Moskau 1936\";");
        positions.push_back("8/3p4/p1bk3p/Pp6/1Kp1PpPp/2P2P1P/2P5/5B2 b - - bm Bxe4; id \"LCTII.FIN.03\";c0 \"Studie 1994\";");
        positions.push_back("5k2/7R/4P2p/5K2/p1r2P1p/8/8/8 b - - bm h3; am h5; id \"LCTII.FIN.04\";c0 \"Karpov - Deep Thought, Analyse 1990\";");
        positions.push_back("6k1/6p1/7p/P1N5/1r3p2/7P/1b3PP1/3bR1K1 w - - bm a6; id \"LCTII.FIN.05\";c0 \"Karpov - Kasparov, Moskau 1985 [Analyse]\";");
        positions.push_back("8/3b4/5k2/2pPnp2/1pP4N/pP1B2P1/P3K3/8 b - - bm f4; id \"LCTII.FIN.06\";c0 \"Minev - Portisch, Halle 1967\";");
        positions.push_back("6k1/4pp1p/3p2p1/P1pPb3/R7/1r2P1PP/3B1P2/6K1 w - - bm Bb4; id \"LCTII.FIN.07\";c0 \"Lengyel - Kaufman, Los Angeles 1974\";");
        positions.push_back("2k5/p7/Pp1p1b2/1P1P1p2/2P2P1p/3K3P/5B2/8 w - - bm c5; id \"LCTII.FIN.08\";c0 \"Spassky - Byrne, 1974\";");
        positions.push_back("8/5Bp1/4P3/6pP/1b1k1P2/5K2/8/8 w - - bm Kg4; id \"LCTII.FIN.09\";c0 \"Klimenok - Kabanov, UdSSR 1969\";");

        std::vector<int> timeControls = { 9000,29000,89000,209000,389000,600000 }; //mseconds
        std::vector<int> scores = { 30,25,20,15,10,5 };

        ExtendedPosition::test(positions,timeControls,true,scores,[](int score){return 1900 + score;},false);
        return true;
    }

    if (option == "sbdTest") {
        std::vector<std::string> positions;
        positions.push_back("1qr3k1/p2nbppp/bp2p3/3p4/3P4/1P2PNP1/P2Q1PBP/1N2R1K1 b - - bm Qc7; id \"sbd.001\";");
        positions.push_back("1r2r1k1/3bnppp/p2q4/2RPp3/4P3/6P1/2Q1NPBP/2R3K1 w - - bm Rc7; id \"sbd.002\";");
        positions.push_back("2b1k2r/2p2ppp/1qp4n/7B/1p2P3/5Q2/PPPr2PP/R2N1R1K b k - bm 0-0; id \"sbd.003\";");
        positions.push_back("2b5/1p4k1/p2R2P1/4Np2/1P3Pp1/1r6/5K2/8 w - - bm Rd8; id \"sbd.004\";");
        positions.push_back("2brr1k1/ppq2ppp/2pb1n2/8/3NP3/2P2P2/P1Q2BPP/1R1R1BK1 w - - bm g3; id \"sbd.005\";");
        positions.push_back("2kr2nr/1pp3pp/p1pb4/4p2b/4P1P1/5N1P/PPPN1P2/R1B1R1K1 b - - bm Bf7; id \"sbd.006\";");
        positions.push_back("2r1k2r/1p1qbppp/p3pn2/3pBb2/3P4/1QN1P3/PP2BPPP/2R2RK1 b k - bm 0-0; id \"sbd.007\";");
        positions.push_back("2r1r1k1/pbpp1npp/1p1b3q/3P4/4RN1P/1P4P1/PB1Q1PB1/2R3K1 w - - bm Rce1; id \"sbd.008\";");
        positions.push_back("2r2k2/r4p2/2b1p1p1/1p1p2Pp/3R1P1P/P1P5/1PB5/2K1R3 w - - bm Kd2; id \"sbd.009\";");
        positions.push_back("2r3k1/5pp1/1p2p1np/p1q5/P1P4P/1P1Q1NP1/5PK1/R7 w - - bm Rd1; id \"sbd.010\";");
        positions.push_back("2r3qk/p5p1/1n3p1p/4PQ2/8/3B4/5P1P/3R2K1 w - - bm e6; id \"sbd.011\";");
        positions.push_back("3b4/3k1pp1/p1pP2q1/1p2B2p/1P2P1P1/P2Q3P/4K3/8 w - - bm Qf3; id \"sbd.012\";");
        positions.push_back("3n1r1k/2p1p1bp/Rn4p1/6N1/3P3P/2N1B3/2r2PP1/5RK1 w - - bm Na4 Nce4; id \"sbd.013\";");
        positions.push_back("3q1rk1/3rbppp/ppbppn2/1N6/2P1P3/BP6/P1B1QPPP/R3R1K1 w - - bm Nd4; id \"sbd.014\";");
        positions.push_back("3r1rk1/p1q4p/1pP1ppp1/2n1b1B1/2P5/6P1/P1Q2PBP/1R3RK1 w - - bm Bh6; id \"sbd.015\";");
        positions.push_back("3r2k1/2q2p1p/5bp1/p1pP4/PpQ5/1P3NP1/5PKP/3R4 b - - bm Qd6; id \"sbd.016\";");
        positions.push_back("3r2k1/p1q1npp1/3r1n1p/2p1p3/4P2B/P1P2Q1P/B4PP1/1R2R1K1 w - - bm Bc4; id \"sbd.017\";");
        positions.push_back("3r4/2k5/p3N3/4p3/1p1p4/4r3/PPP3P1/1K1R4 b - - bm Kd7; id \"sbd.018\";");
        positions.push_back("3r4/2R1np1p/1p1rpk2/p2b1p2/8/PP2P3/4BPPP/2R1NK2 w - - bm b4; id \"sbd.019\";");
        positions.push_back("3rk2r/1b2bppp/p1qppn2/1p6/4P3/PBN2PQ1/1PP3PP/R1B1R1K1 b k - bm 0-0; id \"sbd.020\";");
        positions.push_back("3rk2r/1bq2pp1/2pb1n1p/p3pP2/P1B1P3/8/1P2QBPP/2RN1R1K b k - bm Be7 0-0; id \"sbd.021\";");
        positions.push_back("3rkb1r/pppb1pp1/4n2p/2p5/3NN3/1P5P/PBP2PP1/3RR1K1 w - - bm Nf5; id \"sbd.022\";");
        positions.push_back("3rr1k1/1pq2ppp/p1n5/3p4/6b1/2P2N2/PP1QBPPP/3R1RK1 w - - bm Rfe1; id \"sbd.023\";");
        positions.push_back("4r1k1/1q1n1ppp/3pp3/rp6/p2PP3/N5P1/PPQ2P1P/3RR1K1 w - - bm Rc1; id \"sbd.024\";");
        positions.push_back("4rb1k/1bqn1pp1/p3rn1p/1p2pN2/1PP1p1N1/P1P2Q1P/1BB2PP1/3RR1K1 w - - bm Qe2; id \"sbd.025\";");
        positions.push_back("4rr2/2p5/1p1p1kp1/p6p/P1P4P/6P1/1P3PK1/3R1R2 w - - bm Rfe1; id \"sbd.026\";");
        positions.push_back("5r2/pp1b1kpp/8/2pPp3/2P1p2P/4P1r1/PPRKB1P1/6R1 b - - bm Ke7; id \"sbd.027\";");
        positions.push_back("6k1/1R5p/r2p2p1/2pN2B1/2bb4/P7/1P1K2PP/8 w - - bm Nf6+; id \"sbd.028\";");
        positions.push_back("6k1/pp1q1pp1/2nBp1bp/P2pP3/3P4/8/1P2BPPP/2Q3K1 w - - bm Qc5; id \"sbd.029\";");
        positions.push_back("6k1/pp2rp1p/2p2bp1/1n1n4/1PN3P1/P2rP2P/R3NPK1/2B2R2 w - - bm Rd2; id \"sbd.030\";");
        positions.push_back("8/2p2kpp/p6r/4Pp2/1P2pPb1/2P3P1/P2B1K1P/4R3 w - - bm h4; id \"sbd.031\";");
        positions.push_back("Q5k1/5pp1/5n1p/2b2P2/8/5N1P/5PP1/2q1B1K1 b - - bm Kh7; id \"sbd.032\";");
        positions.push_back("r1b1k1nr/1p3ppp/p1np4/4p1q1/2P1P3/N1NB4/PP3PPP/2RQK2R w Kkq - bm 0-0; id \"sbd.033\";");
        positions.push_back("r1b1k2r/p1pp1ppp/1np1q3/4P3/1bP5/1P6/PB1NQPPP/R3KB1R b KQkq - bm 0-0; id \"sbd.034\";");
        positions.push_back("r1b1k2r/ppppqppp/8/2bP4/3p4/6P1/PPQPPPBP/R1B2RK1 b kq - bm 0-0; id \"sbd.035\";");
        positions.push_back("r1b1k2r/ppq1bppp/2n5/2N1p3/8/2P1B1P1/P3PPBP/R2Q1RK1 b kq - bm 0-0; id \"sbd.036\";");
        positions.push_back("r1b1kb1r/pp2qppp/2pp4/8/4nP2/2N2N2/PPPP2PP/R1BQK2R w KQkq - bm 0-0; id \"sbd.037\";");
        positions.push_back("r1b1qrk1/pp4b1/2pRn1pp/5p2/2n2B2/2N2NPP/PPQ1PPB1/5RK1 w - - bm Rd3; id \"sbd.038\";");
        positions.push_back("r1b2rk1/1pqn1pp1/p2bpn1p/8/3P4/2NB1N2/PPQB1PPP/3R1RK1 w - - bm Rc1; id \"sbd.039\";");
        positions.push_back("r1b2rk1/2qnbp1p/p1npp1p1/1p4PQ/4PP2/1NNBB3/PPP4P/R4RK1 w - - bm Qh6; id \"sbd.040\";");
        positions.push_back("r1b2rk1/pp2ppbp/2n2np1/2q5/5B2/1BN1PN2/PP3PPP/2RQK2R w K - bm 0-0; id \"sbd.041\";");
        positions.push_back("r1b2rk1/pp4pp/1q1Nppn1/2n4B/1P3P2/2B2RP1/P6P/R2Q3K b - - bm Na6; id \"sbd.042\";");
        positions.push_back("r1b2rk1/ppp1qppp/1b1n4/8/B2n4/3NN3/PPPP1PPP/R1BQK2R w KQ - bm 0-0; id \"sbd.043\";");
        positions.push_back("r1b2rk1/ppq1bppp/2p1pn2/8/2NP4/2N1P3/PP2BPPP/2RQK2R w K - bm 0-0; id \"sbd.044\";");
        positions.push_back("r1bq1rk1/1p1n1pp1/p4n1p/2bp4/8/2NBPN2/PPQB1PPP/R3K2R w KQ - bm 0-0; id \"sbd.045\";");
        positions.push_back("r1bq1rk1/1p2ppbp/p2p1np1/6B1/2P1P3/2N5/PP1QBPPP/R3K2R w KQ - bm 0-0; id \"sbd.046\";");
        positions.push_back("r1bq1rk1/1p3ppp/p1np4/3Np1b1/2B1P3/P7/1PP2PPP/RN1QK2R w KQ - bm 0-0; id \"sbd.047\";");
        positions.push_back("r1bq1rk1/4bppp/ppnppn2/8/2P1P3/2N5/PPN1BPPP/R1BQK2R w KQ - bm 0-0; id \"sbd.048\";");
        positions.push_back("r1bq1rk1/pp1n1pbp/2n1p1p1/2ppP3/8/2PP1NP1/PP1N1PBP/R1BQ1RK1 w - - bm d4; id \"sbd.049\";");
        positions.push_back("r1bq1rk1/pp1pppbp/2n2np1/8/4P3/1NN5/PPP1BPPP/R1BQK2R w KQ - bm 0-0; id \"sbd.050\";");
        positions.push_back("r1bq1rk1/pp2ppbp/2n2np1/2p3B1/4P3/2P2N2/PP1NBPPP/R2QK2R w KQ - bm 0-0; id \"sbd.051\";");
        positions.push_back("r1bq1rk1/pp2ppbp/2n3p1/2p5/2BPP3/2P1B3/P3NPPP/R2QK2R w KQ - bm 0-0; id \"sbd.052\";");
        positions.push_back("r1bq1rk1/pp3ppp/2n1pn2/2p5/1bBP4/2N1PN2/PP3PPP/R1BQ1RK1 w - - bm a3; id \"sbd.053\";");
        positions.push_back("r1bq1rk1/pp3ppp/2n2n2/3p4/8/P1NB4/1PP2PPP/R1BQK2R w KQ - bm 0-0; id \"sbd.054\";");
        positions.push_back("r1bq1rk1/ppp1npb1/3p2pp/3Pp2n/1PP1P3/2N5/P2NBPPP/R1BQR1K1 b - - bm Nf4; id \"sbd.055\";");
        positions.push_back("r1bq1rk1/ppp2ppp/2n1pn2/3p4/1bPP4/2NBPN2/PP3PPP/R1BQK2R w KQ - bm 0-0; id \"sbd.056\";");
        positions.push_back("r1bq1rk1/pppp1pbp/2n2np1/4p3/2P5/P1N2NP1/1P1PPPBP/R1BQK2R w KQ - bm 0-0; id \"sbd.057\";");
        positions.push_back("r1bqk2r/2ppbppp/p1n2n2/1p2p3/4P3/1B3N2/PPPPQPPP/RNB2RK1 b kq - bm 0-0; id \"sbd.058\";");
        positions.push_back("r1bqk2r/5ppp/p1np4/1p1Np1b1/4P3/2P5/PPN2PPP/R2QKB1R b KQkq - bm 0-0; id \"sbd.059\";");
        positions.push_back("r1bqk2r/bp3ppp/p1n1pn2/3p4/1PP5/P1N1PN2/1B3PPP/R2QKB1R b KQkq - bm 0-0; id \"sbd.060\";");
        positions.push_back("r1bqk2r/p2pppbp/2p3pn/2p5/4P3/2P2N2/PP1P1PPP/RNBQR1K1 b kq - bm 0-0; id \"sbd.061\";");
        positions.push_back("r1bqk2r/pp2bppp/2n1p3/1B1n4/3P4/2N2N2/PP3PPP/R1BQ1RK1 b kq - bm 0-0; id \"sbd.062\";");
        positions.push_back("r1bqk2r/pp2bppp/2n1p3/3n4/3P4/2NB1N2/PP3PPP/R1BQ1RK1 b kq - bm 0-0; id \"sbd.063\";");
        positions.push_back("r1bqk2r/pp2ppbp/2np1np1/2p5/4P3/1B1P1N1P/PPP2PP1/RNBQK2R w KQkq - bm 0-0; id \"sbd.064\";");
        positions.push_back("r1bqk2r/ppn1bppp/2n5/2p1p3/8/2NP1NP1/PP1BPPBP/R2Q1RK1 b kq - bm 0-0; id \"sbd.065\";");
        positions.push_back("r1bqk2r/ppp1bppp/2n5/3p4/3P4/2PB1N2/P1P2PPP/R1BQ1RK1 b kq - bm 0-0; id \"sbd.066\";");
        positions.push_back("r1bqk2r/ppp2ppp/2nb4/3np3/8/PP2P3/1BQP1PPP/RN2KBNR b KQkq - bm 0-0; id \"sbd.067\";");
        positions.push_back("r1bqk2r/ppp2ppp/3b4/4p3/8/1PPP1N2/2PB1PPP/R2Q1RK1 b kq - bm 0-0; id \"sbd.068\";");
        positions.push_back("r1bqk2r/pppp1ppp/5n2/4p3/Bb2P3/5Q2/PPPPNPPP/R1B1K2R b KQkq - bm 0-0; id \"sbd.069\";");
        positions.push_back("r1bqkb1r/pp3ppp/2n5/2pp4/3Pn3/2N2N2/PPP1BPPP/R1BQK2R w KQkq - bm 0-0; id \"sbd.070\";");
        positions.push_back("r1bqkb1r/pp3ppp/2npp3/3nP3/2BP4/5N2/PP3PPP/RNBQK2R w KQkq - bm 0-0; id \"sbd.071\";");
        positions.push_back("r1bqkbnr/3p1ppp/p1p1p3/8/4P3/3B4/PPP2PPP/RNBQK2R w KQkq - bm 0-0; id \"sbd.072\";");
        positions.push_back("r1bqkbnr/ppp2ppp/2n5/8/2BpP3/5N2/PP3PPP/RNBQK2R w KQkq - bm 0-0; id \"sbd.073\";");
        positions.push_back("r1bqrbk1/1pp3pp/2n2p2/p2np3/8/PP1PPN2/1BQNBPPP/R3K2R w KQ - bm 0-0; id \"sbd.074\";");
        positions.push_back("r1br2k1/1p2qppp/pN2pn2/P7/2pn4/4N1P1/1P2PPBP/R3QRK1 b - - bm Rb8; id \"sbd.075\";");
        positions.push_back("r1q1k2r/1b1nbppp/pp1ppn2/8/2PQP3/1PN2NP1/PB3PBP/R2R2K1 b kq - bm 0-0; id \"sbd.076\";");
        positions.push_back("r1q1k2r/pb1nbppp/1p2pn2/8/P1PNP3/2B3P1/2QN1PBP/R4RK1 b kq - bm 0-0; id \"sbd.077\";");
        positions.push_back("r1r3k1/1bq2pbp/pp1pp1p1/2n5/P3PP2/R2B4/1PPBQ1PP/3N1R1K w - - bm Bc3; id \"sbd.078\";");
        positions.push_back("r1rn2k1/pp1qppbp/6p1/3pP3/3P4/1P3N1P/PB1Q1PP1/R3R1K1 w - - bm Rac1; id \"sbd.079\";");
        positions.push_back("r2q1rk1/1b1nbpp1/pp2pn1p/8/2BN3B/2N1P3/PP2QPPP/2R2RK1 w - - bm Rfd1; id \"sbd.080\";");
        positions.push_back("r2q1rk1/1b3ppp/4pn2/1pP5/1b6/2NBPN2/1PQ2PPP/R3K2R w KQ - bm 0-0; id \"sbd.081\";");
        positions.push_back("r2q1rk1/pb1nppbp/6p1/1p6/3PP3/3QBN1P/P3BPP1/R3K2R w KQ - bm 0-0; id \"sbd.082\";");
        positions.push_back("r2q1rk1/pb2bppp/npp1pn2/3pN3/2PP4/1PB3P1/P2NPPBP/R2Q1RK1 w - - bm e4; id \"sbd.083\";");
        positions.push_back("r2q1rk1/pppb1pbp/2np1np1/4p3/2P5/P1NPPNP1/1P3PBP/R1BQK2R w KQ - bm 0-0; id \"sbd.084\";");
        positions.push_back("r2qk2r/1b1n1ppp/4pn2/p7/1pPP4/3BPN2/1B3PPP/R2QK2R w KQkq - bm 0-0; id \"sbd.085\";");
        positions.push_back("r2qk2r/1b2bppp/p1n1pn2/1p6/1P6/P2BPN2/1B2QPPP/RN3RK1 b kq - bm 0-0; id \"sbd.086\";");
        positions.push_back("r2qk2r/2p2ppp/p1n1b3/1pbpP3/4n3/1BP2N2/PP1N1PPP/R1BQ1RK1 b kq - bm 0-0; id \"sbd.087\";");
        positions.push_back("r2qk2r/3n1ppp/p3p3/3nP3/3R4/5N2/1P1N1PPP/3QR1K1 b kq - bm 0-0; id \"sbd.088\";");
        positions.push_back("r2qk2r/p1pn1ppp/b3pn2/3p4/Nb1P4/1P3NP1/P3PPBP/1RBQ1RK1 b kq - bm 0-0 Qe7; id \"sbd.089\";");
        positions.push_back("r2qk2r/ppp1bppp/2n2n2/8/2BP2b1/2N2N2/PP3PPP/R1BQR1K1 b kq - bm 0-0; id \"sbd.090\";");
        positions.push_back("r2qkb1r/pb1n1p2/2p1p2p/4P1pn/PppP4/2N2NB1/1P2BPPP/R2Q1RK1 w kq - bm Ne4; id \"sbd.091\";");
        positions.push_back("r2qkb1r/pp2nppp/1np1p3/4Pb2/3P4/PB3N2/1P3PPP/RNBQ1RK1 b kq - bm Ned5; id \"sbd.092\";");
        positions.push_back("r2qkb1r/pp3ppp/2bppn2/8/2PQP3/2N2N2/PP3PPP/R1B1K2R w KQkq - bm 0-0; id \"sbd.093\";");
        positions.push_back("r2qr1k1/p3bppp/1p2n3/3Q1N2/5P2/4B1P1/PP3R1P/R5K1 w - - bm Rd1; id \"sbd.094\";");
        positions.push_back("r2r2k1/p1pnqpp1/1p2p2p/3b4/3P4/3BPN2/PP3PPP/2RQR1K1 b - - bm c5; id \"sbd.095\";");
        positions.push_back("r2r2k1/pp1b1ppp/8/3p2P1/3N4/P3P3/1P3P1P/3RK2R b K - bm Rac8; id \"sbd.096\";");
        positions.push_back("r3k2r/1b1nb1p1/p1q1pn1p/1pp3N1/4PP2/2N5/PPB3PP/R1BQ1RK1 w kq - bm Nf3; id \"sbd.097\";");
        positions.push_back("r3k2r/1pqnnppp/p5b1/1PPp1p2/3P4/2N5/P2NB1PP/2RQ1RK1 b kq - bm 0-0; id \"sbd.098\";");
        positions.push_back("r3k2r/p1q1nppp/1pn5/2P1p3/4P1Q1/P1P2P2/4N1PP/R1B2K1R b kq - bm 0-0; id \"sbd.099\";");
        positions.push_back("r3k2r/pp2pp1p/6p1/2nP4/1R2PB2/4PK2/P5PP/5bNR w kq - bm Ne2; id \"sbd.100\";");
        positions.push_back("r3k2r/ppp1bppp/2n5/3n4/3PB3/8/PP3PPP/RNB1R1K1 b kq - bm 0-0-O; id \"sbd.101\";");
        positions.push_back("r3kb1r/pp3ppp/4bn2/3p4/P7/4N1P1/1P2PPBP/R1B1K2R w KQkq - bm 0-0; id \"sbd.102\";");
        positions.push_back("r3kbnr/1pp3pp/p1p2p2/8/3qP3/5Q1P/PP3PP1/RNB2RK1 w kq - bm Rd1; id \"sbd.103\";");
        positions.push_back("r3kr2/pppb1p2/2n3p1/3Bp2p/4P2N/2P5/PP3PPP/2KR3R b q - bm 0-0-O; id \"sbd.104\";");
        positions.push_back("r3nrk1/pp2qpb1/3p1npp/2pPp3/2P1P2N/2N3Pb/PP1BBP1P/R2Q1RK1 w - - bm Re1; id \"sbd.105\";");
        positions.push_back("r3r1k1/1pqn1pbp/p2p2p1/2nP2B1/P1P1P3/2NB3P/5PP1/R2QR1K1 w - - bm Rc1; id \"sbd.106\";");
        positions.push_back("r3r1k1/pp1q1ppp/2p5/P2n1p2/1b1P4/1B2PP2/1PQ3PP/R1B2RK1 w - - bm e4; id \"sbd.107\";");
        positions.push_back("r3r1k1/pp3ppp/2ppqn2/5R2/2P5/2PQP1P1/P2P2BP/5RK1 w - - bm Qd4; id \"sbd.108\";");
        positions.push_back("r3rbk1/p2b1p2/5p1p/1q1p4/N7/6P1/PP1BPPBP/3Q1RK1 w - - bm Nc3; id \"sbd.109\";");
        positions.push_back("r4r1k/pp1bq1b1/n2p2p1/2pPp1Np/2P4P/P1N1BP2/1P1Q2P1/2KR3R w - - bm Ne6; id \"sbd.110\";");
        positions.push_back("r4rk1/1bqp1ppp/pp2pn2/4b3/P1P1P3/2N2BP1/1PQB1P1P/2R2RK1 w - - bm b3; id \"sbd.111\";");
        positions.push_back("r4rk1/1q2bppp/p1bppn2/8/3BPP2/3B2Q1/1PP1N1PP/4RR1K w - - bm e5; id \"sbd.112\";");
        positions.push_back("r4rk1/pp2qpp1/2pRb2p/4P3/2p5/2Q1PN2/PP3PPP/4K2R w K - bm 0-0; id \"sbd.113\";");
        positions.push_back("r7/3rq1kp/2p1bpp1/p1Pnp3/2B4P/PP4P1/1B1RQP2/2R3K1 b - - bm Rad8; id \"sbd.114\";");
        positions.push_back("r7/pp1bpp2/1n1p2pk/1B3P2/4P1P1/2N5/PPP5/1K5R b - - bm Kg5; id \"sbd.115\";");
        positions.push_back("rn1q1rk1/p4pbp/bp1p1np1/2pP4/8/P1N2NP1/1PQ1PPBP/R1B1K2R w KQ - bm 0-0; id \"sbd.116\";");
        positions.push_back("rn1q1rk1/pb3p2/1p5p/3n2P1/3p4/P4P2/1P1Q1BP1/R3KBNR b KQ - bm Re8+; id \"sbd.117\";");
        positions.push_back("rn1q1rk1/pp2bppp/1n2p1b1/8/2pPP3/1BN1BP2/PP2N1PP/R2Q1RK1 w - - bm Bc2; id \"sbd.118\";");
        positions.push_back("rn1q1rk1/pp3ppp/4bn2/2bp4/5B2/2NBP1N1/PP3PPP/R2QK2R w KQ - bm 0-0; id \"sbd.119\";");
        positions.push_back("rn1qkbnr/pp1b1ppp/8/1Bpp4/3P4/8/PPPNQPPP/R1B1K1NR b KQkq - bm Qe7; id \"sbd.120\";");
        positions.push_back("rn1qr1k1/pb3p2/1p5p/3n2P1/3p4/P4P2/1P1QNBP1/R3KB1R b KQ - bm d3; id \"sbd.121\";");
        positions.push_back("rn2kb1r/pp2nppp/1q2p3/3pP3/3P4/5N2/PP2NPPP/R1BQK2R w KQkq - bm 0-0; id \"sbd.122\";");
        positions.push_back("rn3rk1/1bqp1ppp/p3pn2/8/Nb1NP3/4B3/PP2BPPP/R2Q1RK1 w - - bm Rc1; id \"sbd.123\";");
        positions.push_back("rn3rk1/pbp1qppp/1p1ppn2/8/2PP4/P1Q2NP1/1P2PPBP/R1B1K2R w KQ - bm 0-0; id \"sbd.124\";");
        positions.push_back("rnb1k2r/1pq2ppp/p2ppn2/2b5/3NPP2/2P2B2/PP4PP/RNBQ1R1K b kq - bm 0-0; id \"sbd.125\";");
        positions.push_back("rnb2rk1/ppq1ppbp/6p1/2p5/3PP3/2P2N2/P3BPPP/1RBQK2R w K - bm 0-0; id \"sbd.126\";");
        positions.push_back("rnbq1rk1/5ppp/p3pn2/1p6/2BP4/P1P2N2/5PPP/R1BQ1RK1 w - - bm Bd3; id \"sbd.127\";");
        positions.push_back("rnbq1rk1/pp2ppbp/2pp1np1/8/P2PP3/2N2N2/1PP1BPPP/R1BQK2R w KQ - bm 0-0; id \"sbd.128\";");
        positions.push_back("rnbq1rk1/ppp1ppbp/6p1/8/8/2P2NP1/P2PPPBP/R1BQK2R w KQ - bm 0-0; id \"sbd.129\";");
        positions.push_back("rnbqk1nr/pp3pbp/2ppp1p1/8/2BPP3/2N2Q2/PPP2PPP/R1B1K1NR w KQkq - bm Nge2; id \"sbd.130\";");
        positions.push_back("rnbqk2r/ppp2ppp/1b1p1n2/4p3/2B1P3/2PP1N2/PP1N1PPP/R1BQK2R b KQkq - bm 0-0; id \"sbd.131\";");
        positions.push_back("rnbqk2r/pppp2pp/4pn2/5p2/1b1P4/2P2NP1/PP2PPBP/RNBQK2R b KQkq - bm Be7; id \"sbd.132\";");
        positions.push_back("rnbqr1k1/pp1p1ppp/5n2/3Pb3/1P6/P1N3P1/4NPBP/R1BQK2R w KQ - bm 0-0; id \"sbd.133\";");
        positions.push_back("rnq1nrk1/pp3pbp/6p1/3p4/3P4/5N2/PP2BPPP/R1BQK2R w KQ - bm 0-0; id \"sbd.134\";");

        std::vector<int> timeControls = { 100,300,600,1200 }; //mseconds
        std::vector<int> scores = { 1,1,1,1 };

        ExtendedPosition::test(positions,timeControls,false,scores,[](int){return 0;},false);
        return true;
    }

    if (option == "STS") {
        std::vector<std::string> positions;
        bool b = true;
        for(int k = 1 ; k < 16 ; ++k){
            std::stringstream str;
            str << "TestSuite/STS" << k << ".epd";
            b &= ExtendedPosition::readEPDFile(str.str(),positions);
        }
        if ( !b ){
            return 1;
        }

        std::vector<int> timeControls = { 10000 }; //mseconds
        std::vector<int> scores = { 1};

        ExtendedPosition::test(positions,timeControls,false,scores,[=](int score){return int(25.212 * 100.f*float(score)/positions.size() + 1439.4);},false);
        return true;
    }

    return false;
}
