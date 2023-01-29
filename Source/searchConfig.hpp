#pragma once

#include "definition.hpp"
#include "logging.hpp"

/*!
 All parameters for search are defined here
 * There are const when no CLOP tuning is use and of course need to be mutable when begin tuned
 */
namespace SearchConfig {
inline const bool doWindow            = true;
inline const bool doPVS               = true;
inline const bool doNullMove          = true;
inline const bool doFutility          = true;
inline const bool doLMR               = true;
inline const bool doLMP               = true;
inline const bool doStaticNullMove    = true;
inline const bool doThreatsPruning    = true;
inline const bool doRazoring          = true;
inline const bool doQFutility         = false;
inline const bool doQDeltaPruning     = true;
inline const bool doProbcut           = true;
inline const bool doHistoryPruning    = true;
inline const bool doCapHistoryPruning = true;
inline const bool doCMHPruning        = true;
inline const bool doIID               = false;
inline const bool doIIR               = true;

enum CoeffNameType { CNT_init = 0, CNT_bonus, CNT_slopeD, CNT_slopeGP, CNT_minDepth, CNT_maxdepth };

// Most often, N will be =2 for "evalScoreIsHashScore"
//             M will be =2 for an "improving" bonus
template<size_t N_, size_t M_> struct Coeff {
   static constexpr size_t N {N_};
   static constexpr size_t M {M_};
   CONST_SEARCH_TUNING array1d<ScoreType, N> init;
   CONST_SEARCH_TUNING array1d<ScoreType, M> bonus;
   CONST_SEARCH_TUNING array1d<ScoreType, N> slopeDepth;
   CONST_SEARCH_TUNING array1d<ScoreType, N> slopeGamePhase;
   CONST_SEARCH_TUNING array1d<DepthType, N> minDepth;
   CONST_SEARCH_TUNING array1d<DepthType, N> maxDepth;
   const std::string name;
   [[nodiscard]] CONSTEXPR_SEARCH_TUNING ScoreType threshold(const DepthType d, const float gp, const size_t idx1 = 0, const size_t idx2 = 0) const {
      assert(idx1 < N);
      assert(idx2 < M);
      const auto value = init[idx1] + d * slopeDepth[idx1] + bonus[idx2] + static_cast<double>(gp) * slopeGamePhase[idx1];
      assert(value > std::numeric_limits<ScoreType>::min());
      assert(value < std::numeric_limits<ScoreType>::max());
      return static_cast<ScoreType>(value);
   }
   [[nodiscard]] CONSTEXPR_SEARCH_TUNING bool isActive(const DepthType d, const size_t idx1 = 0) const {
      assert(idx1 < N);
      return d >= minDepth[idx1] && d <= maxDepth[idx1];
   }
   [[nodiscard]] FORCE_FINLINE std::string getName(CoeffNameType t, size_t idx)const{
      assert(t >= 0);
      assert(t <= CNT_maxdepth);
      static const array1d<std::string, 6> suffixes { "Init", "Bonus", "SlopeDepth", "SlopeGP", "MinDepth", "MaxDepth" };
      return name + suffixes[t] + std::to_string(idx);
   }
};

extern CONST_SEARCH_TUNING Coeff<2,2> staticNullMoveCoeff;
extern CONST_SEARCH_TUNING Coeff<2,2> razoringCoeff;
extern CONST_SEARCH_TUNING Coeff<2,2> threatCoeff;
//extern CONST_SEARCH_TUNING Coeff<2,2> ownThreatCoeff;
extern CONST_SEARCH_TUNING Coeff<2,2> historyPruningCoeff;
extern CONST_SEARCH_TUNING Coeff<2,2> captureHistoryPruningCoeff;
extern CONST_SEARCH_TUNING Coeff<2,2> futilityPruningCoeff;
extern CONST_SEARCH_TUNING Coeff<2,2> failHighReductionCoeff;

// first value if eval score is used, second if hash score is used
extern CONST_SEARCH_TUNING colored<ScoreType> qfutilityMargin;
extern CONST_SEARCH_TUNING DepthType nullMoveMinDepth;
extern CONST_SEARCH_TUNING DepthType nullMoveVerifDepth;
extern CONST_SEARCH_TUNING ScoreType nullMoveMargin;
extern CONST_SEARCH_TUNING ScoreType nullMoveMargin2;
extern CONST_SEARCH_TUNING ScoreType nullMoveReductionDepthDivisor;
extern CONST_SEARCH_TUNING ScoreType nullMoveReductionInit;
extern CONST_SEARCH_TUNING ScoreType nullMoveDynamicDivisor;
extern CONST_SEARCH_TUNING ScoreType historyExtensionThreshold;
extern CONST_SEARCH_TUNING colored<DepthType> CMHMaxDepth;
//extern CONST_SEARCH_TUNING ScoreType randomAggressiveReductionFactor;
extern CONST_SEARCH_TUNING DepthType iidMinDepth;
extern CONST_SEARCH_TUNING DepthType iidMinDepth2;
extern CONST_SEARCH_TUNING DepthType iidMinDepth3;
extern CONST_SEARCH_TUNING DepthType probCutMinDepth;
extern CONST_SEARCH_TUNING DepthType probCutSearchDepthFactor;
extern CONST_SEARCH_TUNING int       probCutMaxMoves;
extern CONST_SEARCH_TUNING ScoreType probCutMargin;
extern CONST_SEARCH_TUNING ScoreType seeCaptureFactor;
extern CONST_SEARCH_TUNING ScoreType seeCaptureInit;
extern CONST_SEARCH_TUNING ScoreType seeCapDangerDivisor;
extern CONST_SEARCH_TUNING ScoreType seeQuietFactor;
extern CONST_SEARCH_TUNING ScoreType seeQuietInit;
extern CONST_SEARCH_TUNING ScoreType seeQuietDangerDivisor;
extern CONST_SEARCH_TUNING ScoreType seeQThreshold;
extern CONST_SEARCH_TUNING ScoreType betaMarginDynamicHistory;
//extern CONST_SEARCH_TUNING ScoreType probCutThreshold;
extern CONST_SEARCH_TUNING DepthType lmrMinDepth;
extern CONST_SEARCH_TUNING int       lmrCapHistoryFactor;
extern CONST_SEARCH_TUNING ScoreType lmrLateExtensionMargin;
extern CONST_SEARCH_TUNING DepthType singularExtensionDepth;
extern CONST_SEARCH_TUNING DepthType singularExtensionDepthMinus;
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

extern CONST_SEARCH_TUNING int       iirMinDepth;
extern CONST_SEARCH_TUNING int       iirReduction;

extern CONST_SEARCH_TUNING DepthType ttAlphaCutDepth;
extern CONST_SEARCH_TUNING ScoreType ttAlphaCutMargin;
extern CONST_SEARCH_TUNING DepthType ttBetaCutDepth;
extern CONST_SEARCH_TUNING ScoreType ttBetaCutMargin;

extern CONST_SEARCH_TUNING ScoreType lazySortThreshold;
extern CONST_SEARCH_TUNING ScoreType lazySortThresholdQS;


inline const DepthType lmpMaxDepth = 10;
inline const array2d<int,2,SearchConfig::lmpMaxDepth+1> lmpLimit = {{{0, 2, 3, 5, 9, 13, 18, 25, 34, 45, 55}, {0, 5, 6, 9, 14, 21, 30, 41, 55, 69, 84}}};

constexpr
double my_log2(double x){
   int N = 1;
   while(x>=2){
      ++N;
      x/=2;
   }
   const double ln2 = 0.6931471805599453094172321214581765680755001343602552541206800094;
   const double y = (x-1)/(x+1);
   double v = 2*y;
   for (int k = 1 ; k < 10; ++k){
      double p = y;
      const int n = 2*k+1;
      for (int i = 1 ; i < n ; ++i){
         p *= y;
      }
      v += 2*p/n;
   }
   return v + (N-1)*ln2;
}

/*
void test_log(){
   for (int d = 1; d < MAX_DEPTH; d++)
      for (int m = 1; m < MAX_MOVE; m++){
         std::cout << (my_log2(d * 1.6) * my_log2(m) * 0.4) << std::endl;
         std::cout << (std::log(d * 1.6) * std::log(m) * 0.4) << std::endl;
      }
}
*/

///@todo try lmr based on game phase
constexpr array2d<DepthType, MAX_DEPTH, MAX_MOVE> lmrReduction = [] {
   auto ret = decltype(lmrReduction){0};
   //Logging::LogIt(Logging::logInfo) << "Init lmr";
   for (int d = 1; d < MAX_DEPTH; d++)
      for (int m = 1; m < MAX_MOVE; m++)
         ret[d][m] = DepthType(my_log2(d * 1.6) * my_log2(m * 1) * 0.4);
   return ret;
}();

constexpr array2d<ScoreType, 6, 6> MvvLvaScores = [] {
   auto ret = decltype(MvvLvaScores){};
   //Logging::LogIt(Logging::logInfo) << "Init mvv-lva";
   constexpr array1d<ScoreType, 6> IValues = {1, 2, 3, 5, 9, 20}; ///@todo try N=B=3 ??
   for (int v = 0; v < 6; ++v)
      for (int a = 0; a < 6; ++a) ret[v][a] = static_cast<ScoreType>(IValues[v] * 20 - IValues[a]);
   return ret;
}();

} // namespace SearchConfig
