#include "searcher.hpp"

#include "book.hpp"
#include "logging.hpp"
#include "skill.hpp"

namespace{
// Sizes and phases of the skip-blocks, used for distributing search depths across the threads, from stockfish
const unsigned int threadSkipSize = 20;
const int skipSize[threadSkipSize]  = { 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4 };
const int skipPhase[threadSkipSize] = { 0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7 };
}

void Searcher::displayGUI(DepthType depth, DepthType seldepth, ScoreType bestScore, const PVList & pv, int multipv, const std::string & mark){
    const auto now = Clock::now();
    const TimeType ms = std::max((TimeType)1,(TimeType)std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count());
    getData().datas.times[depth] = ms;
    std::stringstream str;
    const Counter nodeCount = ThreadPool::instance().counter(Stats::sid_nodes) + ThreadPool::instance().counter(Stats::sid_qnodes);
    if (Logging::ct == Logging::CT_xboard) {
        str << int(depth) << " " << bestScore << " " << ms / 10 << " " << nodeCount << " ";
        if (DynamicConfig::fullXboardOutput) str << (int)seldepth << " " << Counter(nodeCount / (ms / 1000.f) / 1000.) << " " << ThreadPool::instance().counter(Stats::sid_tbHit1) + ThreadPool::instance().counter(Stats::sid_tbHit2);
        str << "\t" << ToString(pv);
        if ( !mark.empty() ) str << mark;
    }
    else if (Logging::ct == Logging::CT_uci) {
        const std::string multiPVstr = DynamicConfig::multiPV > 1 ? (" multipv " + std::to_string(multipv)) : "";
        str << "info" << multiPVstr << " depth " << int(depth) << " score cp " << bestScore << " time " << ms << " nodes " << nodeCount << " nps " << Counter(nodeCount / (ms / 1000.f)) << " seldepth " << (int)seldepth << " pv " << ToString(pv) << " tbhits " << ThreadPool::instance().counter(Stats::sid_tbHit1) + ThreadPool::instance().counter(Stats::sid_tbHit2);
        static auto lastHashFull = Clock::now();
        if ( (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHashFull).count() > 500
              && (TimeType)std::max(1, int(std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count()*2)) < ThreadPool::instance().main().getCurrentMoveMs() ){
            lastHashFull = now;
            str << " hashfull " << TT::hashFull();
        }
    }
    if (!subSearch) Logging::LogIt(Logging::logGUI) << str.str();
}

PVList Searcher::search(const Position & pp, Move & m, DepthType & d, ScoreType & sc, DepthType & seldepth){

    Position p(pp);
#ifdef WITH_NNUE    
    NNUE::Accumulator acc;
    p.setAccumulator(acc);
#endif    

    if ( isMainThread() ) initCaslingPermHashTable(p); // let's be sure ... ///@todo clean up this crap !!!!

    DynamicConfig::level = DynamicConfig::limitStrength ? Skill::Elo2Level() : DynamicConfig::level;
    d=std::max((DepthType)1,Skill::enabled()?std::min(d,Skill::limitedDepth()):d);

    stopFlag = false;
    moveDifficulty = MoveDifficultyUtil::MD_std;
    startTime = Clock::now();

#ifdef WITH_GENFILE
    if ( DynamicConfig::genFen && id() < MAX_THREADS && ! genStream.is_open() ){
       genStream.open("genfen_" + std::to_string(::getpid()) + "_" + std::to_string(id()) + ".epd",std::ofstream::app);
    }
#endif

    if ( isMainThread() || id() >= MAX_THREADS ){
        Logging::LogIt(Logging::logInfo) << "Search params :" ;
        Logging::LogIt(Logging::logInfo) << "requested time  " << getCurrentMoveMs() ;
        Logging::LogIt(Logging::logInfo) << "requested depth " << (int) d ;
        TT::age();
        MoveDifficultyUtil::variability = 1.f;
        if ( isMainThread() ) ThreadPool::instance().clearSearch(); // reset for all other threads !!!
    }
    else{
        Logging::LogIt(Logging::logInfo) << "helper thread waiting ... " << id() ;
        while(startLock.load()){;}
        Logging::LogIt(Logging::logInfo) << "... go for id " << id() ;
    }
    
    {
        EvalData data;
        ScoreType e = eval(p,data,*this);
        assert(p.halfmoves < MAX_PLY && p.halfmoves >= 0);
        stack[p.halfmoves] = {p,computeHash(p),data,e,INVALIDMINIMOVE};
    }

    DepthType reachedDepth = 0;
    PVList pvOut;
    ScoreType bestScore = 0;
    m = INVALIDMOVE;

    if ( isMainThread() ){
       const MiniMove bookMove = SanitizeCastling(p,Book::Get(computeHash(p)));
       if ( VALIDMOVE(bookMove) ){
           Logging::LogIt(Logging::logInfo) << "Unlocking other threads (book move)";
           startLock.store(false);
           pvOut.push_back(bookMove);
           m = pvOut[0];
           d = 0;
           sc = 0;
           seldepth = 0;
           displayGUI(d,d,sc,pvOut,1);
           return pvOut;
       }
    }

    const bool isInCheck = isAttacked(p, kingSquare(p));
    const DepthType easyMoveDetectionDepth = 5;

    DepthType startDepth = 1;//std::min(d,easyMoveDetectionDepth);

    DynamicConfig::multiPV = (Logging::ct == Logging::CT_uci?DynamicConfig::multiPV:1);
    if ( Skill::enabled() ){
        DynamicConfig::multiPV = std::max(DynamicConfig::multiPV,4u);
    }    
    std::vector<RootScores> multiPVMoves(DynamicConfig::multiPV,{INVALIDMOVE,-MATE});

    bool fhBreak = false;

    const auto maxNodes = TimeMan::maxNodes;
    TimeMan::maxNodes = 0; // reset this for depth 1 to be sure to iterate at least once ...

    if ( DynamicConfig::level == 0 ){ // random mover
       bestScore = randomMover(p,pvOut,isInCheck,*this);
       goto pvsout;
    }

    // only main thread here, stopflag will be triggered anyway for other threads
    if ( isMainThread() && DynamicConfig::multiPV == 1 && d > easyMoveDetectionDepth+5 && maxNodes == 0 
         && currentMoveMs < INFINITETIME && currentMoveMs > 800 && TimeMan::msecUntilNextTC > 0){
       // easy move detection (small open window search)
       rootScores.clear();
       ScoreType easyScore = pvs<true>(-MATE, MATE, p, easyMoveDetectionDepth, 0, pvOut, seldepth, isInCheck,false,false);
       std::sort(rootScores.begin(), rootScores.end(), [](const RootScores& r1, const RootScores & r2) {return r1.s > r2.s; });
       if (stopFlag) { // no more time, this is strange ...
           bestScore = easyScore; 
           goto pvsout; 
       }
       if (rootScores.size() == 1){
           moveDifficulty = MoveDifficultyUtil::MD_forced; // only one : check evasion or zugzwang
       }
       else if (rootScores.size() > 1 && rootScores[0].s > rootScores[1].s + MoveDifficultyUtil::easyMoveMargin){
           moveDifficulty = MoveDifficultyUtil::MD_easy;
       }
    }

    // ID loop
    for(DepthType depth = startDepth ; depth <= std::min(d,DepthType(MAX_DEPTH-6)) && !stopFlag ; ++depth ){ // -6 so that draw can be found for sure ///@todo I don't understand this -6 anymore ..
        
        // MultiPV loop
        std::vector<MiniMove> skipMoves;
        for (unsigned int multi = 0 ; multi < DynamicConfig::multiPV && !stopFlag ; ++multi){

            if ( !skipMoves.empty() && isMatedScore(bestScore) ) break;
            if (!isMainThread()){ // stockfish like thread management
                const int i = (id()-1)%threadSkipSize;
                if (((depth + skipPhase[i]) / skipSize[i]) % 2) continue;
            }
            else{ 
                if ( depth > 1){
                    TimeMan::maxNodes = maxNodes; // restore real value
                    // delayed other thread start
                    if ( startLock.load() ){
                       Logging::LogIt(Logging::logInfo) << "Unlocking other threads";
                       startLock.store(false);
                    }
                }
            } 
            Logging::LogIt(Logging::logInfo) << "Thread " << id() << " searching depth " << (int)depth;
            PVList pvLoc;
            ScoreType delta = (SearchConfig::doWindow && depth>4)?6+std::max(0,(20-depth)*2):MATE; // MATE not INFSCORE in order to enter the loop below once
            ScoreType alpha = std::max(ScoreType(bestScore - delta), ScoreType (-MATE));
            ScoreType beta  = std::min(ScoreType(bestScore + delta), MATE);
            ScoreType score = 0;
#ifndef WITH_TEXEL_TUNING
            contempt = { ScoreType((p.c == Co_White ? +1 : -1) * (DynamicConfig::contempt + DynamicConfig::contemptMG)) , ScoreType((p.c == Co_White ? +1 : -1) * DynamicConfig::contempt) };
#else
            contempt = 0;
#endif            

            // dynamic contempt ///@todo tune this
            contempt += ScoreType(std::round(25*std::tanh(bestScore/400.f)));
            Logging::LogIt(Logging::logInfo) << "Dynamic contempt " << contempt;

            DepthType windowDepth = depth;

            // Aspiration loop
            while( !stopFlag ){

                pvLoc.clear();
                //stack[p.halfmoves].h = computeHash(p); ///@todo this seems useless ?
                score = pvs<true>(alpha,beta,p,windowDepth,0,pvLoc,seldepth,isInCheck,false,false,skipMoves.empty()?nullptr:&skipMoves);
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
                    if ( isMainThread() ){
                        PVList pv2;
                        TT::getPV(p, *this, pv2);
                        displayGUI(depth,seldepth,score,pv2,multi+1,"?");
                    }
                    --windowDepth; // from Ethereal
                    /*
                    // check other moves (if not multi-PV ...)
                    if ( DynamicConfig::multiPV == 1 && pvLoc.size() && beta > alpha + 50){
                        std::vector<MiniMove> skipMovesFailHigh = { Move2MiniMove(pvLoc[0]) };
                        DepthType seldepth2 = 0;
                        PVList pv2;
                        const ScoreType fhVerification = pvs<true,false>(beta-1,beta,p,windowDepth,0,pv2,seldepth2,isInCheck,false,&skipMovesFailHigh);
                        if ( !stopFlag && fhVerification <= beta ){
                            Logging::LogIt(Logging::logInfo) << "Breaking aspiration loop beta...";
                            fhBreak = true;
                            break;
                        }
                    }
                    */
                    //alpha = std::max(ScoreType(-MATE),ScoreType((alpha + beta)/2));
                    beta  = std::min(ScoreType(score + delta), ScoreType( MATE) );
                    Logging::LogIt(Logging::logInfo) << "Increase window beta "  << alpha << ".." << beta;
                }
                else break;

            } // Aspiration loop end

            if (stopFlag){ 
                if ( multi != 0 && isMainThread() ){
                   // handle multiPV display only based on previous ID iteration data
                   PVList pvMulti;
                   pvMulti.push_back(multiPVMoves[multi].m);
                   displayGUI(depth-1,0,multiPVMoves[multi].s,pvMulti,multi+1); ///@todo store pv and seldepth in multiPVMoves ?
                }
            }
            else{
                reachedDepth = depth;
                bestScore    = score;

                // if no good pv available (too short), let's build one for display purpose ...
                if ( fhBreak && pvLoc.size() ){ 
                    Position p2 = p;
                    applyMove(p2,pvLoc[0]);
                    PVList pv2;
                    TT::getPV(p2, *this, pv2);
                    pv2.insert(pv2.begin(),pvLoc[0]);
                    pvLoc = pv2;
                }

                // fill skipmove for next multiPV iteration, and backup multiPV info
                if ( !pvLoc.empty() && DynamicConfig::multiPV > 1){
                    skipMoves.push_back(Move2MiniMove(pvLoc[0]));
                    multiPVMoves[multi].m = pvLoc[0];
                    multiPVMoves[multi].s = bestScore;
                }

                // update the outputed pv only with the best move line
                if ( multi == 0 ){
                   pvOut = pvLoc;
                }
                
                if ( isMainThread() ){
                    // output to GUI
                    displayGUI(depth,seldepth,bestScore,pvLoc,multi+1);

                    // store current depth info 
                    getData().datas.scores[depth] = bestScore;
                    getData().datas.nodes[depth] = ThreadPool::instance().counter(Stats::sid_nodes) + ThreadPool::instance().counter(Stats::sid_qnodes);
                    if ( pvLoc.size() ) getData().datas.moves[depth] = Move2MiniMove(pvLoc[0]);

                    // check for an emergency
                    if (TimeMan::isDynamic && depth > MoveDifficultyUtil::emergencyMinDepth 
                    && bestScore < getData().datas.scores[depth - 1] - MoveDifficultyUtil::emergencyMargin) { 
                        moveDifficulty = bestScore > MoveDifficultyUtil::emergencyAttackThreashold ? MoveDifficultyUtil::MD_hardAttack : MoveDifficultyUtil::MD_hardDefense;
                        Logging::LogIt(Logging::logInfo) << "Emergency mode activated : " << bestScore << " < " << getData().datas.scores[depth - 1] - MoveDifficultyUtil::emergencyMargin; 
                    }

                    // update a "variability" measure to scale remaining time on it ///@todo tune this more
                    if ( depth > 12 && pvLoc.size() ){
                        if ( getData().datas.moves[depth] != getData().datas.moves[depth-1] ) MoveDifficultyUtil::variability *= (1.f + float(depth)/100);
                        else MoveDifficultyUtil::variability *= 0.97f;
                        Logging::LogIt(Logging::logInfo) << "Variability :" << MoveDifficultyUtil::variability;
                        Logging::LogIt(Logging::logInfo) << "Variability time factor :" << MoveDifficultyUtil::variabilityFactor();
                    }

                    // check for remaining time
                    if (TimeMan::isDynamic 
                    && (TimeType)std::max(1, int(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count()*1.8)) > getCurrentMoveMs()) { 
                        stopFlag = true; 
                        Logging::LogIt(Logging::logInfo) << "stopflag triggered, not enough time for next depth"; break; 
                    } 

                    // compute EBF
                    if ( depth > 1 ){
                        Logging::LogIt(Logging::logInfo) << "EBF  " << float(getData().datas.nodes[depth]) / (std::max(Counter(1),getData().datas.nodes[depth-1]));
                        Logging::LogIt(Logging::logInfo) << "EBF2 " << float(ThreadPool::instance().counter(Stats::sid_qnodes)) / std::max(Counter(1),ThreadPool::instance().counter(Stats::sid_nodes));
                    }
                }
            }
        } // multiPV loop end

    } // iterative deepening loop end

pvsout:
    if ( isMainThread() ){
        Logging::LogIt(Logging::logInfo) << "Unlocking other threads (end of search)";
        startLock.store(false);
    }
    if (pvOut.empty()){
        m = INVALIDMOVE;
        if (!subSearch) Logging::LogIt(Logging::logWarn) << "Empty pv" ;
    } 
    else{
        if ( Skill::enabled()) m = Skill::pick(multiPVMoves);
        else m = pvOut[0];
    }
    d = reachedDepth;
    sc = bestScore;
    if (isMainThread()) ThreadPool::instance().DisplayStats();
    return pvOut;
}
