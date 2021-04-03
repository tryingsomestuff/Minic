#include "timeMan.hpp"

#include "logging.hpp"
#include "position.hpp"
#include "searcher.hpp"
#include "threading.hpp"

namespace TimeMan{

TimeType msecPerMove, msecInTC, nbMoveInTC, msecInc, msecUntilNextTC, overHead;
TimeType targetTime, maxTime;
DepthType moveToGo;
uint64_t maxNodes;
bool isDynamic;

void init(){
    Logging::LogIt(Logging::logInfo) << "Init timeman" ;
    msecPerMove = 777;
    msecInTC    = nbMoveInTC = msecInc = msecUntilNextTC = -1;
    moveToGo    = -1;
    maxNodes    = 0;
    isDynamic   = false;
    overHead    = 0;
    targetTime  = 0;
    maxTime     = 0;
}

TimeType GetNextMSecPerMove(const Position & p){
    static const TimeType msecMarginMin = DynamicConfig::moveOverHead; // this can be HUGE at short TC !
    static const TimeType msecMarginMax = 1000;
    static const float msecMarginCoef   = 0.01f; // 1% of remaining time (or time in TC)
    TimeType ms = -1;

    Logging::LogIt(Logging::logInfo) << "msecPerMove     " << msecPerMove;
    Logging::LogIt(Logging::logInfo) << "msecInTC        " << msecInTC   ;
    Logging::LogIt(Logging::logInfo) << "msecInc         " << msecInc    ;
    Logging::LogIt(Logging::logInfo) << "nbMoveInTC      " << nbMoveInTC ;
    Logging::LogIt(Logging::logInfo) << "msecUntilNextTC " << msecUntilNextTC;
    Logging::LogIt(Logging::logInfo) << "currentNbMoves  " << int(p.moves);
    Logging::LogIt(Logging::logInfo) << "moveToGo        " << int(moveToGo);
    Logging::LogIt(Logging::logInfo) << "maxNodes        " << maxNodes;

    const TimeType msecIncLoc = (msecInc > 0) ? msecInc : 0;

    if ( maxNodes > 0 ){
        Logging::LogIt(Logging::logInfo) << "Fixed nodes per move";
        targetTime =  INFINITETIME;
        maxTime = INFINITETIME;
        return targetTime;
    }

    else if ( msecPerMove > 0 ) {
        Logging::LogIt(Logging::logInfo) << "Fixed time per move";
        targetTime =  msecPerMove;
        maxTime = msecPerMove;
        return targetTime;
    }

    else if ( nbMoveInTC > 0){ // mps is given (xboard style)
        assert(msecInTC > 0); 
        assert(nbMoveInTC > 0);
        Logging::LogIt(Logging::logInfo) << "Xboard style TC";
        const TimeType msecMargin = std::max(std::min(msecMarginMax, TimeType(msecMarginCoef*msecInTC)), msecMarginMin);
        if (!isDynamic) ms = int((msecInTC - msecMarginMin) / (float)nbMoveInTC) + msecIncLoc ;
        else { ms = std::min(msecUntilNextTC - msecMargin, int((msecUntilNextTC - msecMargin) /float(nbMoveInTC - ((p.moves - 1) % nbMoveInTC))) + msecIncLoc); }
    }

    else if (moveToGo > 0) { // moveToGo is given (uci style)
        assert(msecUntilNextTC > 0);
        Logging::LogIt(Logging::logInfo) << "UCI style TC";
        const TimeType msecMargin = std::max(std::min(msecMarginMax, TimeType(msecMarginCoef*msecUntilNextTC)), msecMarginMin);
        if (!isDynamic) Logging::LogIt(Logging::logFatal) << "bad timing configuration ... (missing dynamic UCI time info)";
        else { ms = std::min(msecUntilNextTC - msecMargin, TimeType((msecUntilNextTC - msecMargin) / float(moveToGo) + msecIncLoc)*(ThreadPool::instance().main().getData().isPondering?3:2)/2); }
    }

    else{ // sudden death style
        Logging::LogIt(Logging::logInfo) << "Suddendeath style";
        const int nmoves = 17 - std::min(12,p.halfmoves/15); // always be able to play this more moves !
        Logging::LogIt(Logging::logInfo) << "nmoves    " << nmoves;
        Logging::LogIt(Logging::logInfo) << "p.moves   " << int(p.moves);
        assert(nmoves > 0); 
        assert(msecUntilNextTC >= 0);
        const TimeType msecMargin = std::max(std::min(msecMarginMax, TimeType(msecMarginCoef*msecUntilNextTC)), msecMarginMin);
        if (!isDynamic) Logging::LogIt(Logging::logFatal) << "bad timing configuration ... (missing dynamic time info for sudden death style TC)";
        else ms = std::min(msecUntilNextTC - msecMargin, TimeType((msecUntilNextTC - msecMargin) / (float)nmoves + msecIncLoc )*(ThreadPool::instance().main().getData().isPondering?3:2)/2);
    }

    targetTime = std::max(ms-overHead, TimeType(20)); // if not much time left, let's try that hoping for a friendly GUI...
    maxTime = std::min(msecUntilNextTC - msecMarginMin,targetTime*7);
    return targetTime;
}
} // TimeMan
