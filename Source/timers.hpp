#pragma once

#include "definition.hpp"

/*!
 * A little instrumentation facility to study part of Minic speed
 */

#ifdef WITH_TIMER
uint64_t rdtsc();
#define START_TIMER uint64_t rdtscBegin = rdtsc();
#define STOP_AND_SUM_TIMER(name) Timers::rdtscCounter[TM_##name] += rdtsc() - rdtscBegin; ++Timers::callCounter[TM_##name];

enum TimerType : unsigned char { TM_Total=0, TM_See, TM_Apply, TM_Eval1, TM_Eval2, TM_Eval3, TM_Eval4, TM_Eval5, TM_Eval, 
                                 TM_Attack, TM_MovePiece, TM_Generate, TM_PseudoLegal, TM_IsAttacked, TM_MoveScoring, TM_MoveSorting, TM_Max };

namespace Timers{
extern uint64_t rdtscCounter[TM_Max];
extern uint64_t callCounter[TM_Max];
void Display();
}
#else
#define START_TIMER
#define STOP_AND_SUM_TIMER(name)
#endif


