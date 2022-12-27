#include "timeMan.hpp"

#include "dynamicConfig.hpp"
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
   Logging::LogIt(Logging::logInfo) << "currentNbMoves  " << static_cast<int>(p.moves);
   Logging::LogIt(Logging::logInfo) << "moveToGo        " << static_cast<int>(moveToGo);
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
         ms = static_cast<TimeType>(static_cast<float>(msecInTC + totalIncr - msecMargin) / static_cast<float>(nbMoveInTC));
      }
      else {
         assert(msecUntilNextTC > 0);
         // we have some dynamic information here :
         // - the next TC will be in : (nbMoveInTC - ((p.moves - 1) % nbMoveInTC)) moves
         // - current remaining time is msecUntilNextTC
         // remainings increment sum is computed
         // a margin will be used
         // WARNING using this late moves in current TC can be a little longer depending on the margin used
         const int moveUntilNextTC = nbMoveInTC - ((p.moves - 1) % nbMoveInTC); // this modulus is a bit slow probably
         const TimeType remainingIncr = moveUntilNextTC * msecIncLoc;
         msecMargin = getMargin(msecUntilNextTC);
         ms = static_cast<TimeType>(static_cast<float>(msecUntilNextTC + remainingIncr - msecMargin) / static_cast<float>(moveUntilNextTC));
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
         const float ponderingCorrection = (ThreadPool::instance().main().getData().isPondering ? 3 : 2) / 2.f;
         msecMargin = getMargin(msecUntilNextTC);
         ms = static_cast<TimeType>((static_cast<float>(msecUntilNextTC + remainingIncr - msecMargin) / static_cast<float>(moveToGo) ) * ponderingCorrection);
      }
   }

   else { // sudden death style
      assert(msecUntilNextTC >= 0);
      Logging::LogIt(Logging::logInfo) << "Suddendeath style";
      if (!isDynamic) Logging::LogIt(Logging::logFatal) << "bad timing configuration ... (missing dynamic time info for sudden death style TC)";
      // we have only remaining time available
      // we will do "as if" nmoves still need to be played
      // we will take into account the time needed to parse input and get current position (overhead)
      // in situation where increment is smaller than minimum margin we will be a lot more conservative
      // a margin will be used
      // a correction factor is applied for UCI pondering in order to search longer ///@todo why only 3/2 ...
      // be carefull here !, using xboard (for instance under cutechess), we won't receive an increment
      // moreover, we won't assume that receiving an increment once is enough to be sure we will receive it next time
      // so we won't try to live on increment too soon. This will only happens when the game goes one and "nmoves" get smaller...

      // if increment is smaller than the minimal lost by move, this is a "risky TC situation"
      const bool riskySituation = msecInc < msecMinimal;
      // in case of a non risky situation, we can make some correction
      // if game is long we will hope the end is near (between 0 and 15)
      const int gameLengthContrib = riskySituation ? static_cast<int>(std::min(15.f, p.halfmoves / 20.f))
                                                   : static_cast<int>(std::min(15.f, p.halfmoves / 10.f));
      // if material is low we will hope the end is near (between 0 and 15)
      ScoreType sw = 0;
      ScoreType sb = 0;
      const float gp = gamePhase(p.mat, sw, sb);
      const int gamePhaseContrib  = riskySituation ? 0
                                                   : static_cast<int>((1.f-gp)*15);
      // the applied correction will be the bigger one
      const int nmovesCorrection = std::max(gameLengthContrib, gamePhaseContrib);                                                    
      const int nmoves = riskySituation ? 28 : (16 - nmovesCorrection); 

      Logging::LogIt(Logging::logInfo) << "risky ?           " << riskySituation;
      Logging::LogIt(Logging::logInfo) << "msecIncLoc        " << msecIncLoc;
      Logging::LogIt(Logging::logInfo) << "msecUntilNextTC   " << msecUntilNextTC;
      Logging::LogIt(Logging::logInfo) << "gameLengthContrib " << gameLengthContrib;
      Logging::LogIt(Logging::logInfo) << "gamePhaseContrib  " << gamePhaseContrib;
      Logging::LogIt(Logging::logInfo) << "nmoves            " << nmoves;
      Logging::LogIt(Logging::logInfo) << "p.moves           " << static_cast<int>(p.moves);
      Logging::LogIt(Logging::logInfo) << "p.halfmoves       " << static_cast<int>(p.halfmoves);

      if (nmoves * msecMinimal > msecUntilNextTC){
         Logging::LogIt(Logging::logGUI) << Logging::_protocolComment[Logging::ct] << "Minic is in time trouble ...";
      }

      msecMargin = getMargin(msecUntilNextTC);
      const float frac = (static_cast<float>(msecUntilNextTC - msecMargin) / static_cast<float>(nmoves));

      const float ponderingCorrection = (ThreadPool::instance().main().getData().isPondering ? 3 : 2) / 2.f;
      ms = static_cast<TimeType>( frac * ponderingCorrection);
   }

   // take overhead into account
   // and if not much time left, let's try to return msecMinimal and hope for a friendly GUI...
   targetTime = std::max(ms - overHead, msecMinimal);
   // maxTime will be our limit even in case of trouble
   maxTime = std::min(msecUntilNextTC - msecMargin, targetTime * 7);
   return targetTime;
}

void simulate(const TCType tcType, const TimeType initialTime, const TimeType increment, const int movesInTC, const TimeType guiLag){

   Logging::LogIt(Logging::logInfo) << "Start of TC simulation";

   // init TC parameter
   isDynamic       = tcType == TC_suddendeath;
   nbMoveInTC      = tcType == TC_repeating ? movesInTC : -1;
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
   p2.resetNNUEEvaluator(p2.evaluator());
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
      const bool isInCheck = isPosInCheck(p2);
      ThreadData _data;
      _data.score = randomMover(p2, _data.pv, isInCheck);
      if ( _data.pv.empty() ) break;
      if (const Position::MoveInfo moveInfo(p2,_data.pv[0]); ! applyMove(p2, moveInfo)) break;
      std::cout << GetFEN(p2) << std::endl;
   }
   Logging::LogIt(Logging::logInfo) << "End of TC simulation : " << (remaining > 0 ? "success" : "failed");
}

} // namespace TimeMan
