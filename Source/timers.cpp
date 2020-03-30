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

uint64_t rdtscCounterSee       = 0ull;
uint64_t rdtscCounterApply     = 0ull;
uint64_t rdtscCounterEval      = 0ull;
uint64_t rdtscCounterAttack    = 0ull;
uint64_t rdtscCounterMovePiece = 0ull;
uint64_t rdtscCounterGenerate  = 0ull;
uint64_t rdtscCounterTotal     = 0ull;

uint64_t callCounterSee       = 0ull;
uint64_t callCounterApply     = 0ull;
uint64_t callCounterEval      = 0ull;
uint64_t callCounterAttack    = 0ull;
uint64_t callCounterMovePiece = 0ull;
uint64_t callCounterGenerate  = 0ull;
uint64_t callCounterTotal     = 0ull;

void Display(){
    if ( callCounterSee)      std::cout << "See          " << rdtscCounterSee       << "  " << 100.f*rdtscCounterSee       / rdtscCounterTotal << "%  " << callCounterSee      << " " << rdtscCounterSee      / callCounterSee      << std::endl;
    if ( callCounterApply)    std::cout << "Apply        " << rdtscCounterApply     << "  " << 100.f*rdtscCounterApply     / rdtscCounterTotal << "%  " << callCounterApply    << " " << rdtscCounterApply    / callCounterApply    << std::endl;
    if ( callCounterMovePiece)std::cout << "Move Piece   " << rdtscCounterMovePiece << "  " << 100.f*rdtscCounterMovePiece / rdtscCounterTotal << "%  " << callCounterMovePiece<< " " << rdtscCounterMovePiece/ callCounterMovePiece<< std::endl;
    if ( callCounterEval)     std::cout << "Eval         " << rdtscCounterEval      << "  " << 100.f*rdtscCounterEval      / rdtscCounterTotal << "%  " << callCounterEval     << " " << rdtscCounterEval     / callCounterEval     << std::endl;
    if ( callCounterAttack)   std::cout << "Attack       " << rdtscCounterAttack    << "  " << 100.f*rdtscCounterAttack    / rdtscCounterTotal << "%  " << callCounterAttack   << " " << rdtscCounterAttack   / callCounterAttack   << std::endl;
    if ( callCounterGenerate) std::cout << "Generate     " << rdtscCounterGenerate  << "  " << 100.f*rdtscCounterGenerate  / rdtscCounterTotal << "%  " << callCounterGenerate << " " << rdtscCounterGenerate / callCounterGenerate << std::endl;
    std::cout                           << "Total        " << rdtscCounterTotal << std::endl;
}
}

#endif
