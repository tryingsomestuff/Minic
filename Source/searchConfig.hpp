#pragma once

#include "definition.hpp"
#include "logging.hpp"

/*!
 All parameters for search are defined here
 * There are const when no CLOP tuning is use and of course need to be mutable when begin tuned
 */
namespace SearchConfig {
const bool doWindow            = true;
const bool doPVS               = true;
const bool doNullMove          = true;
const bool doFutility          = true;
const bool doLMR               = true;
const bool doLMP               = true;
const bool doStaticNullMove    = true;
const bool doThreatsPruning    = true;
const bool doRazoring          = true;
const bool doQFutility         = false;
const bool doQDeltaPruning     = true;
const bool doProbcut           = true;
const bool doHistoryPruning    = true;
const bool doCapHistoryPruning = true;
const bool doCMHPruning        = true;
const bool doIID               = false;
const bool doIIR               = true;

enum CoeffNameType { CNT_init = 0, CNT_bonus, CNT_slopeD, CNT_slopeGP, CNT_minDepth, CNT_maxdepth };

// Most often, N will be =2 for "evalScoreIsHashScore"
//             M will be =2 for an "improving" bonus
template<size_t _N, size_t _M> struct Coeff {
   static const size_t N {_N};
   static const size_t M {_M};
   CONST_SEARCH_TUNING std::array<ScoreType, N> init;
   CONST_SEARCH_TUNING std::array<ScoreType, M> bonus;
   CONST_SEARCH_TUNING std::array<ScoreType, N> slopeDepth;
   CONST_SEARCH_TUNING std::array<ScoreType, N> slopeGamePhase;
   CONST_SEARCH_TUNING std::array<DepthType, N> minDepth;
   CONST_SEARCH_TUNING std::array<DepthType, N> maxDepth;
   const std::string name;
   [[nodiscard]] inline CONSTEXPR_SEARCH_TUNING ScoreType threshold(DepthType d, float gp, size_t idx1 = 0, size_t idx2 = 0) const {
      assert(idx1 >= 0);
      assert(idx1 < N);
      assert(idx2 >= 0);
      assert(idx2 < M);
      const auto value = init[idx1] + d * slopeDepth[idx1] + bonus[idx2] + static_cast<double>(gp) * slopeGamePhase[idx1];
      assert(value > std::numeric_limits<ScoreType>::min());
      assert(value < std::numeric_limits<ScoreType>::max());
      return static_cast<ScoreType>(value); 
   }
   [[nodiscard]] inline CONSTEXPR_SEARCH_TUNING bool isActive(DepthType d, size_t idx1 = 0) const {
      assert(idx1 >= 0);
      assert(idx1 < N);
      return d >= minDepth[idx1] && d <= maxDepth[idx1]; 
   }
   [[nodiscard]] inline std::string getName(CoeffNameType t, size_t idx)const{
      assert(idx >= 0);
      assert(t >= 0);
      assert(t <= CNT_maxdepth);
      static const std::string suffixes[] = { "Init", "Bonus", "SlopeDepth", "SlopeGP", "MinDepth", "MaxDepth" };
      return name + suffixes[t] + std::to_string(idx);
   }
};

extern CONST_SEARCH_TUNING Coeff<2,2> staticNullMoveCoeff;
extern CONST_SEARCH_TUNING Coeff<2,2> razoringCoeff;
extern CONST_SEARCH_TUNING Coeff<2,2> threatCoeff;
extern CONST_SEARCH_TUNING Coeff<2,2> historyPruningCoeff;
extern CONST_SEARCH_TUNING Coeff<2,2> captureHistoryPruningCoeff;
extern CONST_SEARCH_TUNING Coeff<2,2> futilityPruningCoeff;
extern CONST_SEARCH_TUNING Coeff<2,2> failHighReductionCoeff;

// first value if eval score is used, second if hash score is used
extern CONST_SEARCH_TUNING ScoreType qfutilityMargin[2];
extern CONST_SEARCH_TUNING DepthType nullMoveMinDepth;
extern CONST_SEARCH_TUNING DepthType nullMoveVerifDepth;
extern CONST_SEARCH_TUNING ScoreType nullMoveMargin;
extern CONST_SEARCH_TUNING ScoreType nullMoveMargin2;
extern CONST_SEARCH_TUNING ScoreType nullMoveDynamicDivisor;
extern CONST_SEARCH_TUNING ScoreType historyExtensionThreshold;
extern CONST_SEARCH_TUNING DepthType CMHMaxDepth[2];
//extern CONST_SEARCH_TUNING ScoreType randomAggressiveReductionFactor;
extern CONST_SEARCH_TUNING DepthType iidMinDepth;
extern CONST_SEARCH_TUNING DepthType iidMinDepth2;
extern CONST_SEARCH_TUNING DepthType iidMinDepth3;
extern CONST_SEARCH_TUNING DepthType probCutMinDepth;
extern CONST_SEARCH_TUNING int       probCutMaxMoves;
extern CONST_SEARCH_TUNING ScoreType probCutMargin;
extern CONST_SEARCH_TUNING ScoreType seeCaptureFactor;
extern CONST_SEARCH_TUNING ScoreType seeCapDangerDivisor;
extern CONST_SEARCH_TUNING ScoreType seeQuietFactor;
extern CONST_SEARCH_TUNING ScoreType seeQuietDangerDivisor;
extern CONST_SEARCH_TUNING ScoreType seeQThreshold;
extern CONST_SEARCH_TUNING ScoreType betaMarginDynamicHistory;
//extern CONST_SEARCH_TUNING ScoreType probCutThreshold;
extern CONST_SEARCH_TUNING DepthType lmrMinDepth;
extern CONST_SEARCH_TUNING int       lmrCapHistoryFactor;
extern CONST_SEARCH_TUNING DepthType singularExtensionDepth;
///@todo on move / opponent
extern CONST_SEARCH_TUNING ScoreType dangerLimitPruning;
extern CONST_SEARCH_TUNING ScoreType dangerLimitForwardPruning;
extern CONST_SEARCH_TUNING ScoreType dangerLimitReduction;
extern CONST_SEARCH_TUNING ScoreType dangerDivisor;
extern CONST_SEARCH_TUNING ScoreType failLowRootMargin;

extern CONST_SEARCH_TUNING ScoreType deltaBadMargin;
extern CONST_SEARCH_TUNING ScoreType deltaBadSEEThreshold;
extern CONST_SEARCH_TUNING ScoreType deltaGoodMargin;
extern CONST_SEARCH_TUNING ScoreType deltaGoodSEEThreshold;

extern CONST_SEARCH_TUNING DepthType aspirationMinDepth;
extern CONST_SEARCH_TUNING ScoreType aspirationInit;
extern CONST_SEARCH_TUNING ScoreType aspirationDepthCoef;
extern CONST_SEARCH_TUNING ScoreType aspirationDepthInit;

extern CONST_SEARCH_TUNING DepthType ttMaxFiftyValideDepth;

const DepthType lmpMaxDepth = 10;
const int       lmpLimit[][SearchConfig::lmpMaxDepth + 1] = {{0, 2, 3, 5, 9, 13, 18, 25, 34, 45, 55}, {0, 5, 6, 9, 14, 21, 30, 41, 55, 69, 84}};

///@todo try lmr based on game phase
constexpr Matrix<DepthType, MAX_DEPTH, MAX_MOVE> lmrReduction = [] {
   auto ret = decltype(lmrReduction){0};
   //Logging::LogIt(Logging::logInfo) << "Init lmr";
   for (int d = 1; d < MAX_DEPTH; d++)
      for (int m = 1; m < MAX_MOVE; m++) ret[d][m] = DepthType(std::log(d * 1.6) * std::log(m) * 0.4);
   return ret;
}();

constexpr Matrix<ScoreType, 6, 6> MvvLvaScores = [] {
   auto ret = decltype(MvvLvaScores){};
   //Logging::LogIt(Logging::logInfo) << "Init mvv-lva";
   constexpr ScoreType IValues[6] = {1, 2, 3, 5, 9, 20}; ///@todo try N=B=3 ??
   for (int v = 0; v < 6; ++v)
      for (int a = 0; a < 6; ++a) ret[v][a] = static_cast<ScoreType>(IValues[v] * 20 - IValues[a]);
   return ret;
}();

} // namespace SearchConfig
