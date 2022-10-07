#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "quantization.hpp"

#ifdef WITH_NNUE_CLIPPED_RELU
template<typename T, bool Q> 
inline constexpr T activationInput(const T& x) {
   return std::min(std::max(T{x}, T{0}), T{Quantization<Q>::scale});
}

template<typename T, bool Q> 
inline constexpr T activation(const T& x) {
   return std::min(std::max(T{x}, T{0}), T{1});
}

#else // standard relu (not clipped)

template<typename T, bool Q> 
inline constexpr typename std::enable_if<!Q, T>::type activation(const T& x) { 
   return std::max(T(x), T {0}); 
}

#define activationInput        activation

#endif // WITH_NNUE_CLIPPED_RELU

#endif // WITH_NNUE