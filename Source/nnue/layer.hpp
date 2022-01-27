#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "quantization.hpp"
#include "stackVector.hpp"
#include "weightReader.hpp"

template<typename NT, size_t dim0, size_t dim1, bool Q> struct Layer {
   static constexpr size_t nbW = dim0 * dim1;
   static constexpr size_t nbB = dim1;

   typedef typename Quantization<Q>::BT BT;
   typedef typename Quantization<Q>::WT WT;

   // Layer is always for inner layer, so we can safely use WT and BT
   alignas(NNUEALIGNMENT) WT W[nbW];
   alignas(NNUEALIGNMENT) BT b[nbB];

   CONSTEXPR StackVector<BT, dim1> forward(const StackVector<BT, dim0>& x) const {
      alignas(NNUEALIGNMENT) auto result = StackVector<BT, dim1>::from(b);
#ifdef USE_SIMD_INTRIN
      for (size_t i = 0; i < dim1; ++i) { result.data[i] += x.dot_(W + i * dim0); }
#else
#pragma omp simd
      for (size_t i = 0; i < dim0; ++i) { result.fma_(x.data[i], W + i * dim1); }
#endif // USE_SIMD_INTRIN
      return result;
   }

   template<typename T> CONSTEXPR StackVector<BT, dim1> forward(const StackVector<T, dim0>& x) const {
      alignas(NNUEALIGNMENT) auto result = StackVector<BT, dim1>::from(b);
#ifdef USE_SIMD_INTRIN
      for (size_t i = 0; i < dim1; ++i) { result.data[i] += x.dot_(W + i * dim0); }
#else
#pragma omp simd
      for (size_t i = 0; i < dim0; ++i) { result.fma_(x.data[i], W + i * dim1); }
#endif // USE_SIMD_INTRIN
      return result; // RVO
   }

   Layer<NT, dim0, dim1, Q>& load_(WeightsReader<NT>& ws) {
      ws.template streamW<WT, Q>(W, nbW, dim0, dim1).template streamB<BT, Q>(b, nbB);
      return *this;
   }
};

#endif // WITH_NNUE