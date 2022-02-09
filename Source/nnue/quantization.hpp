#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

// quantization constantes

// By default, quantization is disabled, values are float and we scale only output layer
template<bool Q> struct Quantization {
   typedef float WT;
   typedef float WIT;
   typedef float BT;
   typedef float BIT;
   static constexpr float weightMax {100}; // dummy value
   static constexpr int   weightScale {1};
   static constexpr int   weightFactor {1};
   static constexpr int   biasFactor {1};
   static constexpr float outFactor {1.f / 600.f};
   static float           round(const float& x) { return x; }
};

// When quantization is activated (on read) we try to store weights and
// do computations using only integers, smaller being best for speed
// but there is some constrains : see doc/
template<> struct Quantization<true> {
   typedef int16_t WIT;
   typedef int8_t  WT;
   typedef int32_t BIT;
   typedef int32_t BT;
   static constexpr float weightMax {2.0f}; // supposed min/max of weights values
   static constexpr int   weightScale {std::numeric_limits<WT>::max()};
   static constexpr int   weightFactor {static_cast<int>(weightScale / weightMax)};
   static constexpr int   biasFactor {weightScale * weightFactor};
   static constexpr float outFactor {(weightFactor * weightFactor * weightMax) / 600.f};
   static float           round(const float& x) { return std::round(x); }
};

template<bool Q> inline void quantizationInfo() {
   if constexpr (Q) {
      Logging::LogIt(Logging::logInfo) << "Quantization info :";
      Logging::LogIt(Logging::logInfo) << "weightMax    " << Quantization<true>::weightMax;
      Logging::LogIt(Logging::logInfo) << "weightScale  " << Quantization<true>::weightScale;
      Logging::LogIt(Logging::logInfo) << "weightFactor " << Quantization<true>::weightFactor;
      Logging::LogIt(Logging::logInfo) << "biasFactor   " << Quantization<true>::biasFactor;
      Logging::LogIt(Logging::logInfo) << "outFactor    " << Quantization<true>::outFactor;
   }
   else {
      Logging::LogIt(Logging::logInfo) << "No quantization, using float net";
   }
}

#endif // WITH_NNUE