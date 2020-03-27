#pragma once

#include "definition.hpp"

/* A little instrumentation facility to study part of Minic speed
 */

#ifdef WITH_TIMER
uint64_t rdtsc();
#define START_TIMER uint64_t rdtscBegin = rdtsc();
#define STOP_AND_SUM_TIMER(name) Timers::rdtscCounter##name += rdtsc() - rdtscBegin; ++Timers::callCounter##name;

namespace Timers{
extern uint64_t rdtscCounterSee      ;
extern uint64_t rdtscCounterApply    ;
extern uint64_t rdtscCounterEval     ;
extern uint64_t rdtscCounterAttack   ;
extern uint64_t rdtscCounterMovePiece;
extern uint64_t rdtscCounterGenerate ;
extern uint64_t rdtscCounterTotal    ;

extern uint64_t callCounterSee       ;
extern uint64_t callCounterApply     ;
extern uint64_t callCounterEval      ;
extern uint64_t callCounterAttack    ;
extern uint64_t callCounterMovePiece ;
extern uint64_t callCounterGenerate  ;
extern uint64_t callCounterTotal     ;

void Display();
}
#else
#define START_TIMER
#define STOP_AND_SUM_TIMER(name)
#endif


