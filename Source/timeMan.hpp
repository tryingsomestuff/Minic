#pragma once

#include "definition.hpp"

struct Position;

/* Time managament in Minic
 * GUI protocol is setting  internal TimeMan variable as possible.
 * Then Timeman is responsible to compute msec for next move, using GetNextMSecPerMove(), based on GUI available information.
 * Then Searcher::currentMoveMs is set to GetNextMSecPerMove at the begining of a search.
 * Then, during a search Searcher::getCurrentMoveMs() is used to check the available time.
 */

namespace TimeMan{
extern TimeType msecPerMove, msecInTC, nbMoveInTC, msecInc, msecUntilNextTC, overHead;
extern TimeType targetTime, maxTime;
extern DepthType moveToGo;
extern unsigned long long maxKNodes; ///@todo use this !
extern bool isDynamic;
extern bool isUCIPondering;
extern bool isUCIAnalysis;

void init();

TimeType GetNextMSecPerMove(const Position & p);

} // TimeMan
