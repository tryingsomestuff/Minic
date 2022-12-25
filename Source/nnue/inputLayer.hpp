#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "quantization.hpp"
#include "stackVector.hpp"
#include "weightReader.hpp"

constexpr size_t inputLayerSize      = 12 * 64 * 64;
constexpr size_t firstInnerLayerSize = 384;

template<typename NT, size_t dim0, size_t dim1, bool Q> 
struct InputLayer {
   static constexpr size_t nbW = dim0 * dim1;
   static constexpr size_t nbB = dim1;

   using BIT = typename Quantization<Q>::BIT;
   using WIT = typename Quantization<Q>::WIT;

   // often too big to be statically allocated (see CTOR/DTOR for dynamic alloc)
   typename Quantization<Q>::WIT* W {nullptr};

   // bias can be statically allocated 
   alignas(NNUEALIGNMENT) BIT b[nbB];

   FORCE_FINLINE void insertIdx(const size_t idx, StackVector<BIT, nbB, Q>& x) const {
      const WIT* wPtr = W + idx * dim1;
      x.add_(wPtr);
   }

   FORCE_FINLINE void eraseIdx(const size_t idx, StackVector<BIT, nbB, Q>& x) const {
      const WIT* wPtr = W + idx * dim1;
      x.sub_(wPtr);
   }

   InputLayer<NT, dim0, dim1, Q>& load_(WeightsReader<NT>& ws) {
      ws.template streamWI<WIT, Q>(W, nbW).template streamBI<BIT, Q>(b, nbB);
      return *this;
   }

   // non copyable
   InputLayer<NT, dim0, dim1, Q>& operator=(const InputLayer<NT, dim0, dim1, Q>& other) = delete;
   InputLayer<NT, dim0, dim1, Q>& operator=(InputLayer<NT, dim0, dim1, Q>&& other) = delete;
   InputLayer(const InputLayer<NT, dim0, dim1, Q>& other) = delete;
   InputLayer(InputLayer<NT, dim0, dim1, Q>&& other) = delete;

   InputLayer() { 
       W = new (NNUEALIGNMENT_STD) WIT[nbW]; 
    }

   ~InputLayer() {
      ::operator delete(W, NNUEALIGNMENT_STD);
   }
};

#endif // WITH_NNUE