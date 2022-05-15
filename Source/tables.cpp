#include "tables.hpp"

#include "logging.hpp"
#include "searcher.hpp"

void KillerT::initKillers() {
   Logging::LogIt(Logging::logInfo) << "Init killers";
   Logging::LogIt(Logging::logInfo) << "Size of killer table " << sizeof(killers) << "b";
   for (int k = 0; k < MAX_DEPTH; ++k)
      for (int i = 0; i < 2; ++i) killers[k][i] = INVALIDMOVE;
}

bool KillerT::isKiller(const Move m, const DepthType height) { return sameMove(m, killers[height][0]) || sameMove(m, killers[height][1]); }

void KillerT::update(Move m, DepthType height) {
   if (!sameMove(killers[height][0], m)) {
      killers[height][1] = killers[height][0];
      killers[height][0] = m;
   }
}

void HistoryT::initHistory() {
   Logging::LogIt(Logging::logInfo) << "Init history";
   Logging::LogIt(Logging::logInfo) << "Size of history table " << sizeof(history) / 1024 << "Kb";
   Logging::LogIt(Logging::logInfo) << "Size of history piece table " << sizeof(historyP) << "b";
   Logging::LogIt(Logging::logInfo) << "Size of history captude table " << sizeof(historyCap) / 1024 << "Kb";
   Logging::LogIt(Logging::logInfo) << "Size of history counter table " << sizeof(counter_history) / 1024 << "Kb";
   for (int i = 0; i < NbSquare; ++i)
      for (int k = 0; k < NbSquare; ++k)
         history[0][i][k] = history[1][i][k] = 0;

   for (int i = 0; i < NbPiece; ++i)
      for (int k = 0; k < NbSquare; ++k)
         historyP[i][k] = 0;

   for (int i = 0; i < NbPiece; ++i)
      for (int j = 0; j < NbSquare; ++j)
         for (int k = 0; k < 6; ++k)
            historyCap[i][j][k] = 0;

   for (int i = 0; i < NbPiece; ++i)
      for (int j = 0; j < NbSquare; ++j)
         for (int k = 0; k < NbPiece * NbSquare; ++k)
            counter_history[i][j][k] = -1;
}

void CounterT::initCounter() {
   Logging::LogIt(Logging::logInfo) << "Init counter";
   Logging::LogIt(Logging::logInfo) << "Size of counter table " << sizeof(counter) / 1024 << "Kb";
   for (int i = 0; i < NbSquare; ++i)
      for (int k = 0; k < NbSquare; ++k) counter[i][k] = 0;
}

void CounterT::update(Move m, const Position& p) {
   if (isValidMove(p.lastMove)) counter[Move2From(p.lastMove)][correctedMove2ToKingDest(p.lastMove)] = Move2MiniMove(m);
}

void updateTables(Searcher& context, const Position& p, DepthType depth, DepthType height, const Move m, TT::Bound bound, CMHPtrArray& cmhPtr) {
   if (bound == TT::B_beta) {
      context.killerT.update(m, height);
      context.counterT.update(m, p);
      context.historyT.update<1>(depth, m, p, cmhPtr);
   }
   else if (bound == TT::B_alpha)
      context.historyT.update<-1>(depth, m, p, cmhPtr);
}
