#pragma once

#include "definition.hpp"

#ifdef WITH_CORRECTION_HISTORY

#include "searchConfig.hpp"

struct CorrectionHistoryT {
   static constexpr size_t size = 16384; // power of 2

   array2d<ScoreType, 2, size> table {}; // [Color][key & (size-1)]

   void clear() {
      table = {};
   }

   // current correction (in centipawns) to apply to the static eval for that bucket
   [[nodiscard]] FORCE_FINLINE ScoreType score(Color c, Hash key) const {
      return static_cast<ScoreType>(table[c][key & (size - 1)]);
   }

   FORCE_FINLINE void update(Color c, Hash key, DepthType depth, int evalDiff, int correctionMax) {
      ScoreType & item = table[c][key & (size - 1)];
      const int depthWeight = std::min(static_cast<int>(depth), SearchConfig::correctionHistoryDepthCap);
      const int change = std::clamp(evalDiff * depthWeight / SearchConfig::correctionHistoryDepthScale,
                                     -correctionMax, correctionMax);
      item = static_cast<ScoreType>(item + change - (item * Abs(change)) / correctionMax);
   }

};

#endif // WITH_CORRECTION_HISTORY
