#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "quantization.hpp"
#include "stackVector.hpp"
#include "weightReader.hpp"

template<typename NT, size_t dim0, size_t dim1, bool Q> 
struct Layer {
   static constexpr size_t nbW = dim0 * dim1;
   static constexpr size_t nbB = dim1;

   using BT = typename Quantization<Q>::BT;
   using WT = typename Quantization<Q>::WT;
   using ST = typename Quantization<Q>::ST;

   // Layer is always for inner layer, so we can safely use WT and BT
   // Always small enough to be statically allocated
   alignas(NNUEALIGNMENT) WT W[nbW];
   alignas(NNUEALIGNMENT) BT b[nbB];
   alignas(NNUEALIGNMENT) ST slopes[nbB];

   template<typename T> 
   CONSTEXPR StackVector<BT, dim1, Q> forward(const StackVector<T, dim0, Q>& x) const {
      StackVector<BT, dim1, Q> result;
      result.from(b);
#ifdef USE_SIMD_INTRIN
#pragma omp simd
      for (size_t i = 0; i < dim1; ++i) { result.data[i] += x.dot_(W + i * dim0); }
#else
#pragma omp simd
      for (size_t i = 0; i < dim0; ++i) { result.fma_(x.data[i], W + i * dim1); }
#endif // USE_SIMD_INTRIN
      return result; // RVO
   }

   Layer<NT, dim0, dim1, Q>& load_(WeightsReader<NT>& ws) {
      ws.template streamW<WT>(W, nbW, dim0, dim1)
        .template streamB<BT>(b, nbB)
        .template streamS<ST>(slopes, nbB);
      return *this;
   }

   // non copyable
   Layer<NT, dim0, dim1, Q>& operator=(const Layer<NT, dim0, dim1, Q>& other) = delete;
   Layer<NT, dim0, dim1, Q>& operator=(Layer<NT, dim0, dim1, Q>&& other) = delete;
   Layer(const Layer<NT, dim0, dim1, Q>& other) = delete;
   Layer(Layer<NT, dim0, dim1, Q>&& other) = delete;

   Layer(){
      for (size_t i = 0; i < nbB; ++i){
         slopes[i] = 1;
       }
   };
   ~Layer() = default;

};

#endif // WITH_NNUE