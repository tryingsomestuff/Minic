#include "tables.hpp"

#include "logging.hpp"

void KillerT::initKillers(){
    Logging::LogIt(Logging::logInfo) << "Init killers" ;
    for(int k = 0 ; k < MAX_DEPTH; ++k) for(int i = 0; i < 2; ++i) killers[k][i] = INVALIDMOVE;
}

bool KillerT::isKiller(const Move m, const DepthType ply){
    return sameMove(m, killers[ply][0]) || sameMove(m, killers[ply][1]);
}

void KillerT::update(Move m, DepthType ply){
   if (!sameMove(killers[0][ply], m)) {
      killers[ply][1] = killers[ply][0];
      killers[ply][0] = m;
   }
}

void HistoryT::initHistory(){
    Logging::LogIt(Logging::logInfo) << "Init history" ;
    for(int i = 0; i < 64; ++i) for(int k = 0 ; k < 64; ++k) history[0][i][k] = history[1][i][k] = 0;
    for(int i = 0; i < 13; ++i) for(int k = 0 ; k < 64; ++k) historyP[i][k] = 0;
    for(int i = 0; i < 13; ++i) for(int j = 0 ; j < 64; ++j) for(int k = 0 ; k < 64; ++k)  counter_history[i][j][k] = -1;
}

void CounterT::initCounter(){
    Logging::LogIt(Logging::logInfo) << "Init counter" ;
    for(int i = 0; i < 64; ++i) for(int k = 0 ; k < 64; ++k) counter[i][k] = 0;
}

void CounterT::update(Move m, const Position & p){
    if ( VALIDMOVE(p.lastMove) ) counter[Move2From(p.lastMove)][Move2To(p.lastMove)] = m;
}
