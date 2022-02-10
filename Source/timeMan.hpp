#pragma once

#include "definition.hpp"

struct Position;

/*!
 * Time managament in Minic
 * GUI protocol is setting internal TimeMan variable as possible.
 * Then Timeman is responsible to compute msec for next move, using getNextMSecPerMove(), based on GUI available information.
 * Then Searcher::currentMoveMs is set to getNextMSecPerMove at the begining of a search.
 * Then, during a search Searcher::getCurrentMoveMs() is used to check the available time.
 */

namespace TimeMan {
extern TimeType msecPerMove, msecInTC, msecInc, msecUntilNextTC, overHead;
extern TimeType targetTime, maxTime;
extern int      moveToGo, nbMoveInTC;
extern uint64_t maxNodes;
extern bool     isDynamic;

const TimeType msecMinimal = 20;

void init();

[[nodiscard]] TimeType getNextMSecPerMove(const Position& p);

enum TCType { TC_suddendeath, TC_repeating, TC_fixed };

void simulate(TCType tcType, TimeType initialTime, TimeType increment = -1, int movesInTC = -1, TimeType guiLag = 0);

} // namespace TimeMan
