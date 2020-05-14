#include "timers.hpp"

#ifdef WITH_TIMER

#include "logging.hpp"

#ifdef _WIN32
#include <intrin.h>
uint64_t rdtsc(){return __rdtsc();}
#else
uint64_t rdtsc(){
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}
#endif

namespace Timers{

uint64_t rdtscCounter[TM_Max] = {0ull};
uint64_t callCounter [TM_Max] = {0ull};

const std::string TimerNames[TM_Max] = { "Total", "See", "Apply", "Eval1", "Eval2", "Eval3", "Eval4", "Eval5", "Eval", "Attack", "MovePiece", "Generate", "PseudoLegal", "IsAttacked", "MoveScoring", "MoveSorting"};

#define DisplayHelper(name) \
    if ( callCounter[name] ) std::cout << std::left << std::setw(15) << TimerNames[tm] \
                                       << std::right << std::setw(15) << rdtscCounter[name] << "  " \
                                       << std::right << std::setw(15) << 100.f*rdtscCounter[name] / rdtscCounter[TM_Total] << "%  " \
                                       << std::right << std::setw(15) << callCounter[name] << " " \
                                       << std::right << std::setw(15) << rdtscCounter[name]/callCounter[name] << std::endl;

void Display(){
    for ( TimerType tm = TM_Total ; tm < TM_Max ; tm = TimerType(tm+1)) DisplayHelper(tm);
}
}

#endif
