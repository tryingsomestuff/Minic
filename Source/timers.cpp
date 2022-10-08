#include "timers.hpp"

#ifdef WITH_TIMER

#include "logging.hpp"

#ifdef _WIN32
#include <intrin.h>
FORCE_FINLINE uint64_t rdtsc() { return __rdtsc(); }
#else
uint64_t rdtsc() {
   unsigned int lo, hi;
   __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
   return ((uint64_t)hi << 32) | lo;
}
#endif

namespace Timers {

uint64_t rdtscCounter[TM_Max] = {0ull};
uint64_t callCounter[TM_Max]  = {0ull};

constexpr array1d<std::string_view, TM_Max> TimerNames = {  "Total",      "See",         "Apply",       "Eval1",     "Eval2",      "Eval3",    "Eval4",
                                                            "Eval5",      "Eval",        "NNUE",        "Attack",    "MovePiece",  "Generate", "PseudoLegal",
                                                            "IsAttacked", "MoveScoring", "MoveSorting", "ResetNNUE", "UpdateNNUE", "Sum"};

void Display() {
   std::cout << std::left  << std::setw(15) << "Counter"
             << std::right << std::setw(15) << "total tick"
             << std::right << std::setw(15) << "% of total"
             << std::right << std::setw(15) << "calls"
             << std::right << std::setw(15) << "tick per call"
             << std::endl;
   for (TimerType tm = TM_Total; tm < TM_Max; ++tm){
      if (callCounter[tm]){
         const float percent =  100.f * rdtscCounter[tm] / rdtscCounter[TM_Total];
         if( tm != TM_Total && tm != TM_Sum ){
            rdtscCounter[TM_Sum] += rdtscCounter[tm];
            callCounter[TM_Sum] += callCounter[tm];
         }
         std::cout << std::left  << std::setw(15) << TimerNames[tm]
                   << std::right << std::setw(15) << rdtscCounter[tm]
                   << std::right << std::setw(15) << percent
                   << std::right << std::setw(15) << callCounter[tm]
                   << std::right << std::setw(15) << rdtscCounter[tm] / callCounter[tm]
                   << std::endl;
      }
   }
}
} // namespace Timers

#endif
