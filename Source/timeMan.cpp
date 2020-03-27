#include "timeMan.hpp"

#include "logging.hpp"
#include "position.hpp"

namespace TimeMan{

TimeType msecPerMove, msecInTC, nbMoveInTC, msecInc, msecUntilNextTC, overHead;
DepthType moveToGo;
unsigned long long maxKNodes;
bool isDynamic;
bool isUCIPondering;
std::chrono::time_point<Clock> startTime;

void init(){
    Logging::LogIt(Logging::logInfo) << "Init timeman" ;
    msecPerMove = 777;
    msecInTC    = nbMoveInTC = msecInc = msecUntilNextTC = -1;
    moveToGo    = -1;
    maxKNodes   = 0;
    isDynamic   = false;
    isUCIPondering  = false;
    overHead = 0;
    ///@todo a bool for possible emergency functionality
}

TimeType GetNextMSecPerMove(const Position & p){
    static const TimeType msecMarginMin = 100; // this is HUGE at short TC !
    static const TimeType msecMarginMax = 1000;
    static const float msecMarginCoef   = 0.01f;
    TimeType ms = -1;
    Logging::LogIt(Logging::logInfo) << "msecPerMove     " << msecPerMove;
    Logging::LogIt(Logging::logInfo) << "msecInTC        " << msecInTC   ;
    Logging::LogIt(Logging::logInfo) << "msecInc         " << msecInc    ;
    Logging::LogIt(Logging::logInfo) << "nbMoveInTC      " << nbMoveInTC ;
    Logging::LogIt(Logging::logInfo) << "msecUntilNextTC " << msecUntilNextTC;
    Logging::LogIt(Logging::logInfo) << "currentNbMoves  " << int(p.moves);
    Logging::LogIt(Logging::logInfo) << "moveToGo        " << int(moveToGo);
    Logging::LogIt(Logging::logInfo) << "maxKNodes       " << maxKNodes;
    TimeType msecIncLoc = (msecInc > 0) ? msecInc : 0;
    if ( maxKNodes > 0 ){
        Logging::LogIt(Logging::logInfo) << "Fixed nodes per move";
        ms =  INFINITETIME;
    }
    else if ( msecPerMove > 0 ) {
        Logging::LogIt(Logging::logInfo) << "Fixed time per move";
        ms =  msecPerMove;
    }
    else if ( nbMoveInTC > 0){ // mps is given (xboard style)
        Logging::LogIt(Logging::logInfo) << "Xboard style TC";
        assert(msecInTC > 0); assert(nbMoveInTC > 0);
        Logging::LogIt(Logging::logInfo) << "TC mode, xboard";
        const TimeType msecMargin = std::max(std::min(msecMarginMax, TimeType(msecMarginCoef*msecInTC)), msecMarginMin);
        if (!isDynamic) ms = int((msecInTC - msecMarginMin) / (float)nbMoveInTC) + msecIncLoc ;
        else { ms = std::min(msecUntilNextTC - msecMargin, int((msecUntilNextTC - msecMargin) /float(nbMoveInTC - ((p.moves - 1) % nbMoveInTC))) + msecIncLoc); }
    }
    else if (moveToGo > 0) { // moveToGo is given (uci style)
        assert(msecUntilNextTC > 0);
        Logging::LogIt(Logging::logInfo) << "UCI style TC";
        const TimeType msecMargin = std::max(std::min(msecMarginMax, TimeType(msecMarginCoef*msecUntilNextTC)), msecMarginMin);
        if (!isDynamic) Logging::LogIt(Logging::logFatal) << "bad timing configuration ...";
        else { ms = std::min(msecUntilNextTC - msecMargin, TimeType((msecUntilNextTC - msecMargin) / float(moveToGo) + msecIncLoc)*(isUCIPondering?3:2)/2); }
    }
    else{ // mps is not given
        Logging::LogIt(Logging::logInfo) << "Suddendeath style";
        const int nmoves = 17; // always be able to play this more moves !
        Logging::LogIt(Logging::logInfo) << "nmoves    " << nmoves;
        Logging::LogIt(Logging::logInfo) << "p.moves   " << int(p.moves);
        assert(nmoves > 0); assert(msecInTC >= 0);
        const TimeType msecMargin = std::max(std::min(msecMarginMax, TimeType(msecMarginCoef*msecInTC)), msecMarginMin);
        if (!isDynamic) ms = int((msecInTC+msecIncLoc-msecMarginMin) / (float)(nmoves)) ;
        else ms = std::min(msecUntilNextTC - msecMargin, TimeType((msecUntilNextTC - msecMargin) / (float)nmoves + msecIncLoc )*(isUCIPondering?3:2)/2);
    }
    return std::max(ms-overHead, TimeType(20));// if not much time left, let's try that hoping for a friendly GUI...
}
} // TimeMan
