#include "searcher.hpp"

#include "distributed.h"
#include "logging.hpp"
#include "skill.hpp"
#include "uci.hpp"

namespace{
// Sizes and phases of the skip-blocks, used for distributing search depths across the threads, from stockfish
const unsigned int threadSkipSize = 20;
const int skipSize[threadSkipSize]  = { 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4 };
const int skipPhase[threadSkipSize] = { 0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7 };
}

// Output following chosen protocol
void Searcher::displayGUI(DepthType depth, DepthType seldepth, ScoreType bestScore, const PVList & pv, int multipv, const std::string & mark){
    const auto now = Clock::now();
    const TimeType ms = std::max((TimeType)1,(TimeType)std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count());
    getData().datas.times[depth] = ms;
    if (subSearch) return; // no need to display stuff for subsearch
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
        str << "info" << multiPVstr << " depth " << int(depth) << " score " << UCI::uciScore(bestScore) << " time " << ms << " nodes " << nodeCount << " nps " << Counter(nodeCount / (ms / 1000.f)) << " seldepth " << (int)seldepth << " tbhits " << ThreadPool::instance().counter(Stats::sid_tbHit1) + ThreadPool::instance().counter(Stats::sid_tbHit2);
        static auto lastHashFull = Clock::now();
        if ( (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHashFull).count() > 500
              && (TimeType)std::max(1, int(std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count()*2)) < ThreadPool::instance().main().getCurrentMoveMs() ){
            lastHashFull = now;
            str << " hashfull " << TT::hashFull();
        }
        str << " pv " << ToString(pv); //only add pv at the end (c-chess-cli doesn't like to read something after pv...)
    }
    Logging::LogIt(Logging::logGUI) << str.str();
}

PVList Searcher::search(const Position & pp, Move & m, DepthType & requestedDepth, ScoreType & sc, DepthType & seldepth){

    if ( isMainThread() ) Distributed::sync(Distributed::_commStat2,__PRETTY_FUNCTION__);

    // we start by a copy, because position object must be mutable here.
    Position p(pp);
#ifdef WITH_NNUE
    // Create an evaluator and reset it with the current position
    NNUEEvaluator nnueEvaluator;
    p.associateEvaluator(nnueEvaluator);
    p.resetNNUEEvaluator(nnueEvaluator); 
#endif

    if ( isMainThread() ) initCaslingPermHashTable(p); // let's be sure ... ///@todo clean up this crap !!!!

    // requested depth can be changed according to level or skill parameter
    DynamicConfig::level = DynamicConfig::limitStrength ? Skill::convertElo2Level() : DynamicConfig::level;
    if ( Distributed::isMainProcess() ){
       requestedDepth=std::max((DepthType)1,(Skill::enabled()&&!DynamicConfig::nodesBasedLevel)?std::min(requestedDepth,Skill::limitedDepth()):requestedDepth);
    }
    else{
       requestedDepth = MAX_DEPTH; 
       currentMoveMs = INFINITETIME; // overrides currentMoveMs
    }

    if ( Distributed::isMainProcess() && DynamicConfig::nodesBasedLevel && Skill::enabled()){
        TimeMan::maxNodes = Skill::limitedNodes();
        Logging::LogIt(Logging::logDebug) << "Limited nodes to fit level: " << TimeMan::maxNodes;
    }

    // initialize basic search variable
    stopFlag = false;
    moveDifficulty = MoveDifficultyUtil::MD_std;
    startTime = Clock::now();

#ifdef WITH_GENFILE
    // open the genfen file for output if needed
    if ( DynamicConfig::genFen && id() < MAX_THREADS && ! genStream.is_open() ){
       genStream.open("genfen_" + std::to_string(::getpid()) + "_" + std::to_string(id()) + ".epd",std::ofstream::app);
    }
#endif

    // Main thread only will reset some table
    if ( isMainThread() || id() >= MAX_THREADS ){
        if ( isMainThread()){
           TT::age();
           MoveDifficultyUtil::variability = 1.f; // not usefull for co-searcher threads that won't depend on time
           ThreadPool::instance().clearSearch();  // reset data for all other threads !!!
        }
        Logging::LogIt(Logging::logInfo) << "Search params :" ;
        Logging::LogIt(Logging::logInfo) << "requested time  " << getCurrentMoveMs() << " (" << currentMoveMs << ")"; // won't exceed TimeMan::maxTime
        Logging::LogIt(Logging::logInfo) << "requested depth " << (int) requestedDepth;
    }
    // other threads will wait here for start signal
    else{
        Logging::LogIt(Logging::logInfo) << "helper thread waiting ... " << id() ;
        while(startLock.load()){;}
        Logging::LogIt(Logging::logInfo) << "... go for id " << id() ;
    }
    
    // fill "root" position stack data
    {
        EvalData data;
        ScoreType e = eval(p,data,*this,false);
        assert(p.halfmoves < MAX_PLY && p.halfmoves >= 0);
        stack[p.halfmoves] = {p,computeHash(p),data,e,INVALIDMINIMOVE};
    }

    // initialize search results
    DepthType reachedDepth = 0;
    PVList pvOut;
    ScoreType bestScore = 0;
    m = INVALIDMOVE;

    const bool isInCheck = isAttacked(p, kingSquare(p));
    const DepthType easyMoveDetectionDepth = 5;
    DepthType startDepth = 1;//std::min(d,easyMoveDetectionDepth);
    bool fhBreak = false;

    // initialize multiPV stuff
    DynamicConfig::multiPV = (Logging::ct == Logging::CT_uci?DynamicConfig::multiPV:1);
    if ( Skill::enabled() && !DynamicConfig::nodesBasedLevel ){
        DynamicConfig::multiPV = std::max(DynamicConfig::multiPV,4u);
    }    
    std::vector<RootScores> multiPVMoves(DynamicConfig::multiPV,{INVALIDMOVE,-MATE});

    // handle "maxNodes" style search (will always complete depth 1 search)
    const auto maxNodes = TimeMan::maxNodes;
    // reset this for depth 1 to be sure to iterate at least once ...
    // on main process, requested value will be restored, but not on other process
    TimeMan::maxNodes = 0; 

    // using MAX_DEPTH-6 so that draw can be found for sure ///@todo I don't understand this -6 anymore ..
    const DepthType targetMaxDepth = std::min(requestedDepth,DepthType(MAX_DEPTH-6));

    // random mover can be forced for the few first moves of a game or be setting level to 0
    if ( DynamicConfig::level == 0 || p.halfmoves < DynamicConfig::randomPly ){ 
       if ( p.halfmoves < DynamicConfig::randomPly ) Logging::LogIt(Logging::logInfo) << "Randomized ply";
       bestScore = randomMover(p,pvOut,isInCheck);
       goto pvsout;
    }

    // easy move detection (shallow open window search)
    // only main thread here (stopflag will be triggered anyway for other threads if needed)
    if ( isMainThread() && DynamicConfig::multiPV == 1 && requestedDepth > easyMoveDetectionDepth+5 && maxNodes == 0 
         && currentMoveMs < INFINITETIME && currentMoveMs > 1000 && TimeMan::msecUntilNextTC > 0){
       rootScores.clear();
       ScoreType easyScore = pvs<true>(-MATE, MATE, p, easyMoveDetectionDepth, 0, pvOut, seldepth, isInCheck, false, false);
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
    for(DepthType depth = startDepth ; depth <= targetMaxDepth && !stopFlag ; ++depth ){ 
        
        // MultiPV loop
        std::vector<MiniMove> skipMoves;
        for (unsigned int multi = 0 ; multi < DynamicConfig::multiPV && !stopFlag ; ++multi){

            // No need to continue multiPV loop if a mate is found
            if ( !skipMoves.empty() && isMatedScore(bestScore) ) break;

            if (isMainThread()){
                if ( depth > 1){
                    if ( Distributed::isMainProcess()) TimeMan::maxNodes = maxNodes; // restore real value (only on main processus!)
                    // delayed other thread start (can use a depth condition...)
                    if ( startLock.load() ){
                       Logging::LogIt(Logging::logInfo) << "Unlocking other threads";
                       startLock.store(false);
                    }
                }
            } 
            // stockfish like thread management (not for co-searcher)
            else if ( !subSearch){ 
                const int i = (id()-1)%threadSkipSize;
                if (((depth + skipPhase[i]) / skipSize[i]) % 2) continue;
            }

            Logging::LogIt(Logging::logInfo) << "Thread " << id() << " searching depth " << (int)depth;

            // dynamic contempt ///@todo tune this
#ifndef WITH_TEXEL_TUNING
            contempt = { ScoreType((p.c == Co_White ? +1 : -1) * (DynamicConfig::contempt + DynamicConfig::contemptMG)) , ScoreType((p.c == Co_White ? +1 : -1) * DynamicConfig::contempt) };
#else
            contempt = 0;
#endif            
            contempt += ScoreType(std::round(25*std::tanh(bestScore/400.f)));
            Logging::LogIt(Logging::logInfo) << "Dynamic contempt " << contempt;

            // initialize aspiration window loop variables
            PVList pvLoc;
            ScoreType delta = (SearchConfig::doWindow && depth>4)?6+std::max(0,(20-depth)*2):MATE; // MATE not INFSCORE in order to enter the loop below once
            ScoreType alpha = std::max(ScoreType(bestScore - delta), ScoreType (-MATE));
            ScoreType beta  = std::min(ScoreType(bestScore + delta), MATE);
            ScoreType score = 0;
            DepthType windowDepth = depth;

            // Aspiration loop
            while( !stopFlag ){

                pvLoc.clear();
                score = pvs<true>(alpha, beta, p, windowDepth, 0, pvLoc, seldepth, isInCheck, false, false, skipMoves.empty()?nullptr:&skipMoves);
                if ( stopFlag ) break;
                delta += 2 + delta/2; // formula from xiphos ...
                if (alpha > -MATE && score <= alpha) {
                    beta  = std::min(MATE,ScoreType((alpha + beta)/2));
                    alpha = std::max(ScoreType(score - delta), ScoreType(-MATE) );
                    Logging::LogIt(Logging::logInfo) << "Increase window alpha " << alpha << ".." << beta;
                    if ( isMainThread() && DynamicConfig::multiPV == 1){
                        PVList pv2;
                        TT::getPV(p, *this, pv2);
                        displayGUI(depth,seldepth,score,pv2,multi+1,"!");
                        windowDepth = depth;
                    }
                }
                else if (beta < MATE && score >= beta ) {
                    if ( isMainThread() && DynamicConfig::multiPV == 1){
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
#ifdef WITH_NNUE
                    NNUEEvaluator evaluator = p.Evaluator();
                    p2.associateEvaluator(evaluator);
#endif                    
                    applyMove(p2,pvLoc[0]);
                    PVList pv2;
                    TT::getPV(p2, *this, pv2);
                    pv2.insert(pv2.begin(),pvLoc[0]);
                    pvLoc = pv2;
                }

                // In multiPV mode, fill skipmove for next multiPV iteration, and backup multiPV info
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

                    // sync stopfloag in other process
                    if ( ! Distributed::isMainProcess() ){
                       Distributed::get(&ThreadPool::instance().main().stopFlag,1,Distributed::_winStop,0);
                    }
                }
            } 
        } // multiPV loop end
    } // iterative deepening loop end

    stopFlag = true; // here stopFlag must always be true ...

pvsout:

    if ( isMainThread() ){
        Distributed::winFence(Distributed::_winStop);
        Distributed::syncStat();
        Distributed::syncTT();
    }

    if ( isMainThread() ){
        Logging::LogIt(Logging::logInfo) << "Unlocking other threads (end of search)";
        startLock.store(false);
    }
    if (pvOut.empty()){
        m = INVALIDMOVE;
        if (!subSearch) Logging::LogIt(Logging::logWarn) << "Empty pv" ;
    } 
    else{
        if ( Skill::enabled() && !DynamicConfig::nodesBasedLevel) m = Skill::pick(multiPVMoves);
        else m = pvOut[0];
    }
    requestedDepth = reachedDepth;
    sc = bestScore;
    
    if (isMainThread()){
       if ( Distributed::worldSize > 1 ){
          Distributed::showStat();
       }
       else{
          ThreadPool::instance().DisplayStats(); 
       }
    }    

#ifdef WITH_GENFILE
    // calling writeToGenFile at each root node
    if ( isMainThread() && DynamicConfig::genFen && p.halfmoves >= DynamicConfig::randomPly && DynamicConfig::level != 0 ) writeToGenFile(p);
#endif

    return pvOut;
}
