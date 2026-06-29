#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "stackVector.hpp"
#include "inputLayer.hpp"

template<typename NT, bool Q> 
struct FeatureTransformer {

   const InputLayer<NT, inputLayerSize, firstInnerLayerSize, Q>* weights_;

   using BIT = typename Quantization<Q>::BIT;

   // active_ is always for input layer, so BIT shall be used
   StackVector<BIT, firstInnerLayerSize, Q> active_;

   const StackVector<BIT, firstInnerLayerSize, Q> & active() const { return active_; }

   FORCE_FINLINE void clear() {
      assert(weights_);
      active_.from(weights_->b);
   }

   // Prefetch the weight row for feature index `idx` into L1 cache before
   // the upcoming insert/erase call on the same index.  The feature-transformer
   // weight matrix W is large and randomly accessed (indexed by king-sq x
   // piece-sq x piece-type), so rows are almost never hot in L1/L2.  Issuing
   // the prefetch early (before all insert/erase ops for a move are dispatched)
   // hides the L3/DRAM load latency (~100-200 cycles) and avoids a stall when
   // insertIdx/eraseIdx actually reads the row.
   // Hint (0,3): prefetch for read, high temporal locality (keep in L1).
   FORCE_FINLINE void prefetch(const size_t idx) const {
      assert(weights_);
#if defined(__GNUC__) || defined(__clang__)
      __builtin_prefetch(weights_->W + idx * firstInnerLayerSize, 0, 3);
#else
      (void)idx;
#endif
   }

   FORCE_FINLINE void insert(const size_t idx) {
      assert(weights_);
      weights_->insertIdx(idx, active_);
   }

   FORCE_FINLINE void erase(const size_t idx) {
      assert(weights_);
      weights_->eraseIdx(idx, active_);
   }

   FeatureTransformer(const InputLayer<NT, inputLayerSize, firstInnerLayerSize, Q>* src): weights_ {src} { clear(); }

   FeatureTransformer() = delete;

#ifdef DEBUG_NNUE_UPDATE
   bool operator==(const FeatureTransformer<NT, Q>& other) { return active_ == other.active_; }

   bool operator!=(const FeatureTransformer<NT, Q>& other) { return active_ != other.active_; }
#endif
};

#endif // WITH_NNUE