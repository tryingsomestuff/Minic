#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "quantization.hpp"
#include "stackVector.hpp"
#include "weightReader.hpp"

constexpr size_t inputLayerSize      = 12 * 64 * 64;
constexpr size_t firstInnerLayerSize = 128;

template<typename NT, size_t dim0, size_t dim1, bool Q> struct InputLayer {
   static constexpr size_t nbW = dim0 * dim1;
   static constexpr size_t nbB = dim1;

   typedef typename Quantization<Q>::BIT BIT;
   typedef typename Quantization<Q>::WIT WIT;

   // InputLayer is alway for input layer, so we use WIT and BIT
   typename Quantization<Q>::WIT* W {nullptr};
   alignas(NNUEALIGNMENT) typename Quantization<Q>::BIT b[nbB];

   void insertIdx(const size_t idx, StackVector<BIT, nbB>& x) const {
      const WIT* wPtr = W + idx * dim1;
      x.add_(wPtr);
   }

   void eraseIdx(const size_t idx, StackVector<BIT, nbB>& x) const {
      const WIT* wPtr = W + idx * dim1;
      x.sub_(wPtr);
   }

   InputLayer<NT, dim0, dim1, Q>& load_(WeightsReader<NT>& ws) {
      ws.template streamWI<WIT, Q>(W, nbW).template streamBI<BIT, Q>(b, nbB);
      return *this;
   }

   InputLayer<NT, dim0, dim1, Q>& operator=(const InputLayer<NT, dim0, dim1, Q>& other) {
#pragma omp simd
      for (size_t i = 0; i < nbW; ++i) { W[i] = other.W[i]; }
#pragma omp simd
      for (size_t i = 0; i < nbB; ++i) { b[i] = other.b[i]; }
      return *this;
   }

   InputLayer<NT, dim0, dim1, Q>& operator=(InputLayer<NT, dim0, dim1, Q>&& other) {
      std::swap(W, other.W);
      std::swap(b, other.b);
      return *this;
   }

   InputLayer(const InputLayer<NT, dim0, dim1, Q>& other) {
      W = new (NNUEALIGNMENT_STD) WIT[nbW];
#pragma omp simd
      for (size_t i = 0; i < nbW; ++i) { W[i] = other.W[i]; }
#pragma omp simd
      for (size_t i = 0; i < nbB; ++i) { b[i] = other.b[i]; }
   }

   InputLayer(InputLayer<NT, dim0, dim1, Q>&& other) {
      std::swap(W, other.W);
      std::swap(b, other.b);
   }

   InputLayer() { W = new (NNUEALIGNMENT_STD) WIT[nbW]; }

   ~InputLayer() {
      if (W != nullptr) { ::operator delete(W, NNUEALIGNMENT_STD); }
   }
};

#endif // WITH_NNUE