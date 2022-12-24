#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

// quantization constantes

// By default, quantization is disabled, values are float and we scale only output layer
template<bool Q> struct Quantization {
   using WT = float;
   using WIT = float;
   using BT = float;
   using BIT = float;
   static constexpr WT scale {1};
   static constexpr float outFactor {1.f / 600.f};
   static float round(const float& x) { return x; }
};

// When quantization is activated (on read) we try to store weights and
// do computations using only integers, smaller being best for speed
// but there is some constrains : see doc/
template<> struct Quantization<true> {
   using WT = float;
   using WIT = int16_t;
   using BT = float;
   using BIT = int16_t;
   static constexpr WT scale {512}; // 32*512 = 16384 and |weight| & |bias| < 0.6
   static constexpr float outFactor {1.f / 600.f};
   static float round(const float& x) { return std::round(x); }
};

template<bool Q> inline void quantizationInfo() {
   if constexpr (Q) {
      Logging::LogIt(Logging::logInfo) << "Quantization info :";
      Logging::LogIt(Logging::logInfo) << "scale " << Quantization<true>::scale;
   }
   else {
      Logging::LogIt(Logging::logInfo) << "No quantization, using float net";
   }
}

#endif // WITH_NNUE