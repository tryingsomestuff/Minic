#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "nnueImpl.hpp"

#ifndef NDEBUG
#define SCALINGCOUNT 5000
#else
#define SCALINGCOUNT 50000
#endif

// Internal wrapper to the NNUE things
namespace NNUEWrapper {

typedef float nnueNType; // type of data inside the binary net (currently floats)
inline constexpr bool quantization = true; // only for input layer

inline void init() {
   bool       loadOk        = false;
   const bool shallLoadNNUE = !DynamicConfig::NNUEFile.empty() && DynamicConfig::NNUEFile != "none";
   if (shallLoadNNUE) {
      Logging::LogIt(Logging::logInfoPrio) << "Loading NNUE net " << DynamicConfig::NNUEFile;
      loadOk = nnue::NNUEWeights<nnueNType, quantization>::load(DynamicConfig::NNUEFile, nnue::NNUEEval<nnueNType, quantization>::weights);
   }
   else {
      Logging::LogIt(Logging::logInfoPrio) << "No NNUE net given";
   }

   if (loadOk) {
      DynamicConfig::useNNUE = true;
   }
   else {
      if (shallLoadNNUE) Logging::LogIt(Logging::logInfoPrio) << "Fail to load NNUE net(s), using standard evaluation instead";
      else
         Logging::LogIt(Logging::logInfoPrio) << "Using standard evaluation";
      DynamicConfig::useNNUE = false;
   }
}

} // namespace NNUEWrapper

using NNUEEvaluator = nnue::NNUEEval<NNUEWrapper::nnueNType, NNUEWrapper::quantization>;

namespace FeatureIdx {

inline constexpr size_t major = 64 * 12;
inline constexpr size_t minor = 64;

inline constexpr size_t usPawnOffset   = 0;
inline constexpr size_t usKnightOffset = usPawnOffset + minor;
inline constexpr size_t usBishopOffset = usKnightOffset + minor;
inline constexpr size_t usRookOffset   = usBishopOffset + minor;
inline constexpr size_t usQueenOffset  = usRookOffset + minor;
inline constexpr size_t usKingOffset   = usQueenOffset + minor;

inline constexpr size_t themPawnOffset   = usKingOffset + minor;
inline constexpr size_t themKnightOffset = themPawnOffset + minor;
inline constexpr size_t themBishopOffset = themKnightOffset + minor;
inline constexpr size_t themRookOffset   = themBishopOffset + minor;
inline constexpr size_t themQueenOffset  = themRookOffset + minor;
inline constexpr size_t themKingOffset   = themQueenOffset + minor;

[[nodiscard]] constexpr size_t usOffset(const Piece pt) {
   switch (pt) {
      case P_wp: return usPawnOffset;
      case P_wn: return usKnightOffset;
      case P_wb: return usBishopOffset;
      case P_wr: return usRookOffset;
      case P_wq: return usQueenOffset;
      case P_wk: return usKingOffset;
      default: return usPawnOffset;
   }
}

[[nodiscard]] constexpr size_t themOffset(const Piece pt) {
   switch (pt) {
      case P_wp: return themPawnOffset;
      case P_wn: return themKnightOffset;
      case P_wb: return themBishopOffset;
      case P_wr: return themRookOffset;
      case P_wq: return themQueenOffset;
      case P_wk: return themKingOffset;
      default: return themPawnOffset;
   }
}

} // namespace FeatureIdx

#ifdef WITH_DATA2BIN
#include "learn/convert.hpp"
#endif

#endif // WITH_NNUE