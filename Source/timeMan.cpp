#include "timeMan.hpp"

#include "logging.hpp"
#include "position.hpp"
#include "searcher.hpp"
#include "threading.hpp"

namespace TimeMan {

TimeType msecPerMove, msecInTC, msecInc, msecUntilNextTC, overHead;
TimeType targetTime, maxTime;
int      moveToGo, nbMoveInTC;
uint64_t maxNodes;
bool     isDynamic;

void init() {
   Logging::LogIt(Logging::logInfo) << "Init timeman";
   msecPerMove = 777;
   msecInTC = msecInc = msecUntilNextTC = -1;
   nbMoveInTC = -1;
   moveToGo   = -1;
   maxNodes   = 0;
   isDynamic  = false;
   overHead   = 0;
   targetTime = 0;
   maxTime    = 0;
}

TimeType getNextMSecPerMove(const Position& p) {

   const TimeType msecMarginMin  = DynamicConfig::moveOverHead; // minimum margin
   const TimeType msecMarginMax  = 3000; // maximum margin
   const float    msecMarginCoef = 0.01f; // part of the remaining time use as margin
   auto getMargin = [&](TimeType remainingTime){
      return std::max(std::min(msecMarginMax, static_cast<TimeType>(msecMarginCoef * static_cast<float>(remainingTime))), msecMarginMin);
   };

   TimeType ms = -1; // this is what we will compute here

   Logging::LogIt(Logging::logInfo) << "msecPerMove     " << msecPerMove;
   Logging::LogIt(Logging::logInfo) << "msecInTC        " << msecInTC;
   Logging::LogIt(Logging::logInfo) << "msecInc         " << msecInc;
   Logging::LogIt(Logging::logInfo) << "nbMoveInTC      " << nbMoveInTC;
   Logging::LogIt(Logging::logInfo) << "msecUntilNextTC " << msecUntilNextTC;
   Logging::LogIt(Logging::logInfo) << "currentNbMoves  " << int(p.moves);
   Logging::LogIt(Logging::logInfo) << "moveToGo        " << int(moveToGo);
   Logging::LogIt(Logging::logInfo) << "maxNodes        " << maxNodes;

   const TimeType msecIncLoc = (msecInc > 0) ? msecInc : 0;

   TimeType msecMargin = msecMarginMin;

   if (maxNodes > 0) {
      Logging::LogIt(Logging::logInfo) << "Fixed nodes per move";
      targetTime = INFINITETIME;
      maxTime    = INFINITETIME;
      return targetTime;
   }

   else if (msecPerMove > 0) {
      Logging::LogIt(Logging::logInfo) << "Fixed time per move";
      targetTime = msecPerMove;
      maxTime    = msecPerMove;
      return targetTime;
   }

   else if (nbMoveInTC > 0) { // mps is given (xboard style)
      assert(nbMoveInTC > 0);
      Logging::LogIt(Logging::logInfo) << "Xboard style TC";
      if (!isDynamic){
         assert(msecInTC > 0);
         // we have all static information here : nbMoveInTC in a nbMoveInTC TC with msecIncLoc of increment at each move.
         // so we simply split time in equal parts
         // we use the minimal margin
         // WARNING note that this might be a bit stupid when used with a book
         const TimeType totalIncr = nbMoveInTC * msecIncLoc;
         ms = static_cast<TimeType>(static_cast<double>(msecInTC + totalIncr - msecMargin) / static_cast<double>(nbMoveInTC));
      }
      else {
         assert(msecUntilNextTC > 0);
         // we have some dynamic information here : 
         // - the next TC will be in : (nbMoveInTC - ((p.moves - 1) % nbMoveInTC)) moves
         // - current remaining time is msecUntilNextTC
         // remainings increment sum is computed
         // a margin will be used
         // WARNING using this late moves in current TC can be a little longer depending on the margin used
         const int moveUntilNextTC = nbMoveInTC - ((p.moves - 1) % nbMoveInTC);
         const TimeType remainingIncr = moveUntilNextTC * msecIncLoc;
         msecMargin = getMargin(msecUntilNextTC);
         ms = static_cast<TimeType>(static_cast<double>(msecUntilNextTC + remainingIncr - msecMargin) / static_cast<double>(moveUntilNextTC));
      }
   }

   else if (moveToGo > 0) { // moveToGo is given (uci style)
      assert(msecUntilNextTC > 0);
      Logging::LogIt(Logging::logInfo) << "UCI style TC";
      if (!isDynamic){
         Logging::LogIt(Logging::logFatal) << "bad timing configuration ... (missing dynamic UCI time info)";
      }
      else {
         // we have dynamic information here :
         // - move to go before next TC
         // - current remaining time
         // remainings increment sum is computed
         // a margin will be used
         // a correction factor is applied for UCI pondering in order to search longer ///@todo why only 3/2 ...
         const TimeType remainingIncr = moveToGo * msecIncLoc;
         const float ponderingCorrection = (ThreadPool::instance().main().getData().isPondering ? 3 : 2) / 2;
         msecMargin = getMargin(msecUntilNextTC);
         ms = static_cast<TimeType>((static_cast<double>(msecUntilNextTC + remainingIncr - msecMargin) / static_cast<double>(moveToGo) ) * ponderingCorrection);
      }
   }

   else { // sudden death style
      assert(msecUntilNextTC >= 0);
      Logging::LogIt(Logging::logInfo) << "Suddendeath style";
      if (!isDynamic) Logging::LogIt(Logging::logFatal) << "bad timing configuration ... (missing dynamic time info for sudden death style TC)";
      // we have only remaining time available 
      // we will do "has if" nmoves still need to be played
      // we will take into account the time needed to parse input and get current position (overhead)
      // in situation where increment is smaller than minimum margin we will be a lot more conservative
      // a margin will be used
      // a correction factor is applied for UCI pondering in order to search longer ///@todo why only 3/2 ...
      // be carefull here !, using xboard (for instance under cutechess), we won't receive an increment
      const bool riskySituation = msecInc < msecMinimal;
      const int nmoves = (riskySituation ? 28 : 15 ) 
                       - (riskySituation ? std::min(8, p.halfmoves / 20) : std::min(10, p.halfmoves / 12));
      Logging::LogIt(Logging::logInfo) << "nmoves      " << nmoves;
      Logging::LogIt(Logging::logInfo) << "p.moves     " << int(p.moves);
      Logging::LogIt(Logging::logInfo) << "p.halfmoves " << int(p.halfmoves);
      if (nmoves * msecMinimal > msecUntilNextTC){
         Logging::LogIt(Logging::logGUI) << Logging::_protocolComment[Logging::ct] << "Minic is in time trouble ...";
      }
      const float ponderingCorrection = (ThreadPool::instance().main().getData().isPondering ? 3 : 2) / 2;
      msecMargin = getMargin(msecUntilNextTC);
      ms = static_cast<TimeType>( (static_cast<double>(msecUntilNextTC - msecMargin) / static_cast<double>(nmoves)) * ponderingCorrection);
   }

   // take overhead into account
   // and if not much time left, let's try to return msecMinimal and hope for a friendly GUI...
   targetTime = std::max(ms - overHead, msecMinimal); 
   // maxTime will be our limit even in case of trouble
   maxTime = std::min(msecUntilNextTC - msecMargin, targetTime * 7);
   return targetTime;
}

void simulate(TCType tcType, TimeType initialTime, TimeType increment, TimeType movesInTC, TimeType guiLag){

   Logging::LogIt(Logging::logInfo) << "Start of TC simulation";

   // init TC parameter
   isDynamic       = tcType == TC_suddendeath;
   nbMoveInTC      = tcType == TC_repeating ? movesInTC : static_cast<TimeType>(-1);
   msecPerMove     = -1;
   msecInTC        = tcType == TC_repeating ? initialTime : static_cast<TimeType>(-1);
   msecInc         = increment;
   msecUntilNextTC = tcType == TC_suddendeath ? initialTime : static_cast<TimeType>(-1);
   moveToGo        = -1;
   
   // and check configuration
   switch(tcType){
      case TC_suddendeath:
         assert(msecUntilNextTC >= 0);      
         break;
      case TC_repeating:
         assert(msecInTC > 0);
         assert(nbMoveInTC > 0);      
         break;
      case TC_fixed:
         assert(false); // not used here
         break;
   }

   RootPosition p(startPosition);
   Position p2 = p;
#ifdef WITH_NNUE
   NNUEEvaluator evaluator;
   p2.associateEvaluator(evaluator);
   p2.resetNNUEEvaluator(p2.Evaluator());
#endif

   TimeType remaining = msecUntilNextTC;
   TimeType currentMoveMs = 0;

   while( remaining > 0 && p2.moves < 300){
      currentMoveMs = getNextMSecPerMove(p2);
      Logging::LogIt(Logging::logInfo) << "currentMoveMs " << currentMoveMs;
      remaining -= currentMoveMs;
      remaining += msecInc;
      remaining -= guiLag;
      Logging::LogIt(Logging::logInfo) << "remaining " << remaining;
      switch(tcType){
         case TC_suddendeath:
            msecUntilNextTC = remaining;
         break;
         case TC_repeating:
         break;
         case TC_fixed:
         break;
      }
      const bool isInCheck = isAttacked(p2, kingSquare(p2));
      ThreadData _data;
      _data.score = randomMover(p2, _data.pv, isInCheck);
      if ( _data.pv.empty() || ! applyMove(p2,_data.pv[0])) break;
      std::cout << GetFEN(p2) << std::endl;
   }
   Logging::LogIt(Logging::logInfo) << "End of TC simulation : " << (remaining > 0 ? "success" : "failed");
}

} // namespace TimeMan
