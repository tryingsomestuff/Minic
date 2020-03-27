#include "searcher.hpp"

#include "book.hpp"
#include "logging.hpp"

namespace{
// Sizes and phases of the skip-blocks, used for distributing search depths across the threads, from stockfish
const unsigned int threadSkipSize = 20;
const int skipSize[threadSkipSize]  = { 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4 };
const int skipPhase[threadSkipSize] = { 0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7 };
}

void Searcher::displayGUI(DepthType depth, DepthType seldepth, ScoreType bestScore, const PVList & pv, int multipv, const std::string & mark){
    static unsigned char count = 0;
    count++; // overflow is ok
    const auto now = Clock::now();
    const TimeType ms = std::max(1,(int)std::chrono::duration_cast<std::chrono::milliseconds>(now - TimeMan::startTime).count());
    std::stringstream str;
    Counter nodeCount = ThreadPool::instance().counter(Stats::sid_nodes) + ThreadPool::instance().counter(Stats::sid_qnodes);
    if (Logging::ct == Logging::CT_xboard) {
        str << int(depth) << " " << bestScore << " " << ms / 10 << " " << nodeCount << " ";
        if (DynamicConfig::fullXboardOutput) str << (int)seldepth << " " << int(nodeCount / (ms / 1000.f) / 1000.) << " " << ThreadPool::instance().counter(Stats::sid_tbHit1) + ThreadPool::instance().counter(Stats::sid_tbHit2);
        str << "\t" << ToString(pv);
        if ( !mark.empty() ) str << mark;
    }
    else if (Logging::ct == Logging::CT_uci) {
        str << "info " << "multipv " << multipv << " depth " << int(depth) << " score cp " << bestScore << " time " << ms << " nodes " << nodeCount << " nps " << int(nodeCount / (ms / 1000.f)) << " seldepth " << (int)seldepth << " pv " << ToString(pv) << " tbhits " << ThreadPool::instance().counter(Stats::sid_tbHit1) + ThreadPool::instance().counter(Stats::sid_tbHit2);
        static auto lastHashFull = Clock::now();
        if (  (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHashFull).count() > 500
              && (TimeType)std::max(1, int(std::chrono::duration_cast<std::chrono::milliseconds>(now - TimeMan::startTime).count()*2)) < getCurrentMoveMs()
              && !stopFlag){
            lastHashFull = now;
            str << " hashfull " << TT::hashFull();
        }
    }
    Logging::LogIt(Logging::logGUI) << str.str();
}

PVList Searcher::search(const Position & p, Move & m, DepthType & d, ScoreType & sc, DepthType & seldepth){
    d=std::max((DepthType)1,DynamicConfig::level==SearchConfig::nlevel?d:std::min(d,SearchConfig::levelDepthMax[DynamicConfig::level/10]));
    if ( isMainThread() ){
        TimeMan::startTime = Clock::now();
        Logging::LogIt(Logging::logInfo) << "Search params :" ;
        Logging::LogIt(Logging::logInfo) << "requested time  " << getCurrentMoveMs() ;
        Logging::LogIt(Logging::logInfo) << "requested depth " << (int) d ;
        stopFlag = false;
        moveDifficulty = MoveDifficultyUtil::MD_std;
        //TT::clearTT(); // to be used for reproductible results
        TT::age();
    }
    else{
        Logging::LogIt(Logging::logInfo) << "helper thread waiting ... " << id() ;
        while(startLock.load()){;}
        Logging::LogIt(Logging::logInfo) << "... go for id " << id() ;
    }
    stats.init();
    //clearPawnTT(); ///@todo loop context
    killerT.initKillers();
    historyT.initHistory();
    counterT.initCounter();

    stack[p.halfmoves].h = p.h;

    DepthType reachedDepth = 0;
    PVList pv;
    ScoreType bestScore = 0;
    m = INVALIDMOVE;

    if ( isMainThread() ){
       const Move bookMove = SanitizeCastling(p,Book::Get(computeHash(p)));
       if ( bookMove != INVALIDMOVE){
           if ( isMainThread() ) startLock.store(false);
           pv.push_back(bookMove);
           m = pv[0];
           d = 0;
           sc = 0;
           seldepth = 0;
           displayGUI(d,d,sc,pv,1);
           return pv;
       }
    }

    ScoreType depthScores[MAX_DEPTH] = { 0 };
    const bool isInCheck = isAttacked(p, kingSquare(p));
    const DepthType easyMoveDetectionDepth = 5;

    DepthType startDepth = 1;//std::min(d,easyMoveDetectionDepth);
    std::vector<RootScores> multiPVMoves(DynamicConfig::multiPV);

    if ( isMainThread() && d > easyMoveDetectionDepth+5 && Searcher::currentMoveMs < INFINITETIME && Searcher::currentMoveMs > 800 && TimeMan::msecUntilNextTC > 0){
       // easy move detection (small open window search)
       rootScores.clear();
       ScoreType easyScore = pvs<true,false>(-MATE, MATE, p, easyMoveDetectionDepth, 0, pv, seldepth, isInCheck,false);
       std::sort(rootScores.begin(), rootScores.end(), [](const RootScores& r1, const RootScores & r2) {return r1.s > r2.s; });
       if (stopFlag) { bestScore = easyScore; goto pvsout; }
       if (rootScores.size() == 1) moveDifficulty = MoveDifficultyUtil::MD_forced; // only one : check evasion or zugzwang
       else if (rootScores.size() > 1 && rootScores[0].s > rootScores[1].s + MoveDifficultyUtil::easyMoveMargin) moveDifficulty = MoveDifficultyUtil::MD_easy;
    }

    if ( DynamicConfig::level == 0 ){ // random mover
       bestScore = randomMover(p,pv,isInCheck);
       goto pvsout;
    }

    // ID loop
    for(DepthType depth = startDepth ; depth <= std::min(d,DepthType(MAX_DEPTH-6)) && !stopFlag ; ++depth ){ // -6 so that draw can be found for sure ///@todo I don't understand this -6 anymore ..
        // MultiPV loop
        std::vector<MiniMove> skipMoves;
        for (unsigned int multi = 0 ; multi < (Logging::ct == Logging::CT_uci?DynamicConfig::multiPV:1) ; ++multi){
            if ( !skipMoves.empty() && isMatedScore(bestScore) ) break;
            if (!isMainThread()){ // stockfish like thread management
                const int i = (id()-1)%threadSkipSize;
                if (((depth + skipPhase[i]) / skipSize[i]) % 2) continue;
            }
            else{ if ( depth > 1) startLock.store(false);} // delayed other thread start
            Logging::LogIt(Logging::logInfo) << "Thread " << id() << " searching depth " << (int)depth;
            PVList pvLoc;
            ScoreType delta = (SearchConfig::doWindow && depth>4)?6+std::max(0,(20-depth)*2):MATE; // MATE not INFSCORE in order to enter the loop below once ///@todo try delta function of depth
            ScoreType alpha = std::max(ScoreType(bestScore - delta), ScoreType (-MATE));
            ScoreType beta  = std::min(ScoreType(bestScore + delta), MATE);
            ScoreType score = 0;
            DepthType windowDepth = depth;
            // Aspiration loop
            while( true && !stopFlag ){
                pvLoc.clear();
                stack[p.halfmoves].h = p.h;
                score = pvs<true,false>(alpha,beta,p,windowDepth,0,pvLoc,seldepth,isInCheck,false,skipMoves.empty()?nullptr:&skipMoves);
                if ( stopFlag ) break;
                delta += 2 + delta/2; // from xiphos ...
                if (alpha > -MATE && score <= alpha) {
                    beta  = std::min(MATE,ScoreType((alpha + beta)/2));
                    alpha = std::max(ScoreType(score - delta), ScoreType(-MATE) );
                    Logging::LogIt(Logging::logInfo) << "Increase window alpha " << alpha << ".." << beta;
                    if ( isMainThread() ){
                        PVList pv2;
                        TT::getPV(p, *this, pv2);
                        displayGUI(depth,seldepth,score,pv2,multi+1,"!");
                        windowDepth = depth;
                    }
                }
                else if (beta < MATE && score >= beta ) {
                    //alpha = std::max(ScoreType(-MATE),ScoreType((alpha + beta)/2));
                    beta  = std::min(ScoreType(score + delta), ScoreType( MATE) );
                    Logging::LogIt(Logging::logInfo) << "Increase window beta "  << alpha << ".." << beta;
                    if ( isMainThread() ){
                        PVList pv2;
                        TT::getPV(p, *this, pv2);
                        displayGUI(depth,seldepth,score,pv2,multi+1,"?");
                        --windowDepth; // from Ethereal
                    }
                }
                else break;
            }
            if (stopFlag){ // handle multiPV display
                if ( multi == 0 ) break;
                if ( multiPVMoves.size() == DynamicConfig::multiPV){
                    PVList pvMulti;
                    pvMulti.push_back(multiPVMoves[multi].m);
                    displayGUI(depth-1,0,multiPVMoves[multi].s,pvMulti,multi+1);
                }
            }
            else{
                pv = pvLoc;
                reachedDepth = depth;
                bestScore    = score;
                if ( isMainThread() ){
                    displayGUI(depth,seldepth,bestScore,pv,multi+1);
                    if (TimeMan::isDynamic && depth > MoveDifficultyUtil::emergencyMinDepth && bestScore < depthScores[depth - 1] - MoveDifficultyUtil::emergencyMargin) { moveDifficulty = MoveDifficultyUtil::MD_hardDefense; Logging::LogIt(Logging::logInfo) << "Emergency mode activated : " << bestScore << " < " << depthScores[depth - 1] - MoveDifficultyUtil::emergencyMargin; }
                    if (TimeMan::isDynamic && (TimeType)std::max(1, int(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count()*1.8)) > getCurrentMoveMs()) { stopFlag = true; Logging::LogIt(Logging::logInfo) << "stopflag triggered, not enough time for next depth"; break; } // not enought time
                    depthScores[depth] = bestScore;
                }
                if ( !pv.empty() ){
                    skipMoves.push_back(Move2MiniMove(pv[0]));
                    multiPVMoves[multi].m = pv[0];
                    multiPVMoves[multi].s = bestScore;
                }
            }
        }
    }
pvsout:
    if ( isMainThread() ) startLock.store(false);
    if (pv.empty()){
        m = INVALIDMOVE;
        Logging::LogIt(Logging::logWarn) << "Empty pv" ;
    } else m = pv[0];
    d = reachedDepth;
    sc = bestScore;
    if (isMainThread()) ThreadPool::instance().DisplayStats();
    return pv;
}
