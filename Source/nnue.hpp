#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "nnueImpl.hpp"

// Internal wrapper to the NNUE things
namespace NNUEWrapper {

using nnueNType = float; // type of data inside the binary net (currently floats)
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

#ifdef WITH_DATA2BIN
#include "learn/convert.hpp"
#endif

#endif // WITH_NNUE