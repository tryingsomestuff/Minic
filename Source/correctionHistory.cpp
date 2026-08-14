#include "correctionHistory.hpp"

#ifdef WITH_CORRECTION_HISTORY

#include "hash.hpp"
#include "position.hpp"
#include "searcher.hpp"

ScoreType Searcher::correctionScore(const Position& p) const {
   int correction = 0;
#ifdef WITH_PAWN_CORRHIST
   correction += pawnCorrHist.score(p.c, p.ph);
#endif
#ifdef WITH_NONPAWN_CORRHIST
   correction += nonPawnCorrHist.score(p.c, nonPawnKey(p, Co_White));
   correction += nonPawnCorrHist.score(p.c, nonPawnKey(p, Co_Black));
#endif
#ifdef WITH_MINOR_CORRHIST
   correction += minorCorrHist.score(p.c, minorKey(p));
#endif
#ifdef WITH_MAJOR_CORRHIST
   correction += majorCorrHist.score(p.c, majorKey(p));
#endif
   return static_cast<ScoreType>(correction);
}

ScoreType Searcher::correctedEval(const Position& p, ScoreType rawScore) const {
   // never distort a mate/mated score (this also covers the isInCheck case, since a mated eval already is one)
   if (isMateScore(rawScore)) return rawScore;
   return clampScore(static_cast<int>(rawScore) + static_cast<int>(correctionScore(p)));
}

void Searcher::updateCorrectionHistory(const Position& p, DepthType depth, ScoreType bestScore, ScoreType baselineEval) {
   const int diff = bestScore - baselineEval;
#ifdef WITH_PAWN_CORRHIST
   pawnCorrHist.update(p.c, p.ph, depth, diff, SearchConfig::correctionHistoryMaxPawn);
#endif
#ifdef WITH_NONPAWN_CORRHIST
   nonPawnCorrHist.update(p.c, nonPawnKey(p, Co_White), depth, diff, SearchConfig::correctionHistoryMaxNonPawn);
   nonPawnCorrHist.update(p.c, nonPawnKey(p, Co_Black), depth, diff, SearchConfig::correctionHistoryMaxNonPawn);
#endif
#ifdef WITH_MINOR_CORRHIST
   minorCorrHist.update(p.c, minorKey(p), depth, diff, SearchConfig::correctionHistoryMaxMinor);
#endif
#ifdef WITH_MAJOR_CORRHIST
   majorCorrHist.update(p.c, majorKey(p), depth, diff, SearchConfig::correctionHistoryMaxMajor);
#endif
}

#endif // WITH_CORRECTION_HISTORY
