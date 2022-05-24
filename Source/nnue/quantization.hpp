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
   static constexpr WT scale {1};
   static constexpr float outFactor {1.f / 600.f};
   static float round(const float& x) { return x; }
};

// When quantization is activated (on read) we try to store weights and
// do computations using only integers, smaller being best for speed
// but there is some constrains : see doc/
template<> struct Quantization<true> {
   typedef float WT;
   typedef int16_t WIT;
   typedef float BT;
   typedef int16_t BIT;
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

template<bool Q> inline void quantize(){

}

#endif // WITH_NNUE