#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "quantization.hpp"

template<typename T, bool Q> 
constexpr T activationInput(const T& x) {
   return std::min(std::max(x, T{0}), T{Quantization<Q>::scale});
}

template<typename T, bool Q> 
constexpr T activation(const T& x) {
   return std::min(std::max(x, T{0}), T{1});
}

#endif // WITH_NNUE