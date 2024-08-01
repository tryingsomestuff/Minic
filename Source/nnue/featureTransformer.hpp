#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "stackVector.hpp"
#include "inputLayer.hpp"

template<typename NT, bool Q> 
struct FeatureTransformer {

   const InputLayer<NT, inputLayerSize, firstInnerLayerSize, Q>* weights_;

   FORCE_FINLINE const NT * slopes() const {
      assert(weights_);
      return weights_->slopes;
   }

   using BIT = typename Quantization<Q>::BIT;

   // active_ is always for input layer, so BIT shall be used
   StackVector<BIT, firstInnerLayerSize, Q> active_;

   const StackVector<BIT, firstInnerLayerSize, Q> & active() const { return active_; }

   FORCE_FINLINE void clear() {
      assert(weights_);
      active_.from(weights_->b);
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