#include "correctionHistory.hpp"

#ifdef WITH_CORRECTION_HISTORY

#include "hash.hpp"
#include "position.hpp"
#include "searcher.hpp"

namespace {
#ifdef WITH_HORIZON_CORRHIST
constexpr DepthType horizonCorrMinDepth = 8; // somehow NNUE data mean depth
constexpr int horizonCorrRampDepth = 8; // plies past horizonCorrMinDepth to reach full correction weight

[[nodiscard]] FORCE_FINLINE int corrBucketAbs(const int v, const int step, const int maxBucket) {
   return std::min(maxBucket, Abs(v) / step);
}

[[nodiscard]] FORCE_FINLINE Hash horizonKey(ScoreType baselineEval, ScoreType ttRefScore, bool hasTTHint, bool pvnode, bool cutNode) {
   const int ttDisagreementBucket = hasTTHint ? corrBucketAbs(static_cast<int>(ttRefScore) - static_cast<int>(baselineEval), 24, 7) : 0;
   const int nodeTypeBucket = pvnode ? 0 : (cutNode ? 1 : 2);
   return static_cast<Hash>(ttDisagreementBucket | (nodeTypeBucket << 3));
}
#endif
} // namespace

ScoreType Searcher::correctionScore(const Position& p) const {
   int correction = 0;
#ifdef WITH_PAWN_CORRHIST
   correction += pawnCorrHist.score(p.c, p.ph);
#endif
#ifdef WITH_NONPAWN_CORRHIST
   correction += nonPawnCorrHist.score(p.c, nonPawnKey(p, Co_White));
   correction += nonPawnCorrHist.score(p.c, nonPawnKey(p, Co_Black));
#endif
   return static_cast<ScoreType>(correction);
}

ScoreType Searcher::correctedEval(const Position& p, ScoreType rawScore) const {
   // never distort a mate/mated score (this also covers the isInCheck case, since a mated eval already is one)
   if (isMateScore(rawScore)) return rawScore;
   return clampScore(static_cast<int>(rawScore) + static_cast<int>(correctionScore(p)));
}

ScoreType Searcher::horizonCorrection([[maybe_unused]] Color c, [[maybe_unused]] DepthType depth, [[maybe_unused]] ScoreType baselineEval,
                                       [[maybe_unused]] ScoreType ttRefScore, [[maybe_unused]] bool hasTTHint,
                                       [[maybe_unused]] bool pvnode, [[maybe_unused]] bool cutNode) const {
#ifdef WITH_HORIZON_CORRHIST
   if (depth <= horizonCorrMinDepth) return 0;
   // ramp up linearly instead of snapping to full weight right past the threshold
   const int ramp = std::min<int>(horizonCorrRampDepth, depth - horizonCorrMinDepth);
   const ScoreType raw = horizonCorrHist.score(c, horizonKey(baselineEval, ttRefScore, hasTTHint, pvnode, cutNode));
   return static_cast<ScoreType>(static_cast<int>(raw) * ramp / horizonCorrRampDepth);
#else
   return 0;
#endif
}

void Searcher::updateCorrectionHistory(const Position& p, DepthType depth, ScoreType bestScore, ScoreType baselineEval,
                                        [[maybe_unused]] ScoreType ttRefScore, [[maybe_unused]] bool hasTTHint,
                                        [[maybe_unused]] bool pvnode, [[maybe_unused]] bool cutNode) {
   const int residual = bestScore - baselineEval;

#ifdef WITH_PAWN_CORRHIST
   pawnCorrHist.update(p.c, p.ph, depth, residual, SearchConfig::correctionHistoryMaxPawn);
#endif
#ifdef WITH_NONPAWN_CORRHIST
   nonPawnCorrHist.update(p.c, nonPawnKey(p, Co_White), depth, residual, SearchConfig::correctionHistoryMaxNonPawn);
   nonPawnCorrHist.update(p.c, nonPawnKey(p, Co_Black), depth, residual, SearchConfig::correctionHistoryMaxNonPawn);
#endif
#ifdef WITH_HORIZON_CORRHIST
   if (depth > horizonCorrMinDepth) {
      horizonCorrHist.update(p.c, horizonKey(baselineEval, ttRefScore, hasTTHint, pvnode, cutNode),
                             depth, residual, SearchConfig::correctionHistoryMaxHorizon);
   }
#endif
}

#endif // WITH_CORRECTION_HISTORY
