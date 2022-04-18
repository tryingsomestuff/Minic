#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "stackVector.hpp"
#include "inputLayer.hpp"

template<typename NT, bool Q> struct FeatureTransformer {

   const InputLayer<NT, inputLayerSize, firstInnerLayerSize, Q>* weights_;

   typedef typename Quantization<Q>::BIT BIT;
   typedef typename Quantization<Q>::WIT WIT;

   // active_ is always for input layer, so BIT shall be used
   StackVector<BIT, firstInnerLayerSize> active_;

   constexpr StackVector<BIT, firstInnerLayerSize> active() const { return active_; }

   void clear() {
      assert(weights_);
      active_ = StackVector<BIT, firstInnerLayerSize>::from(weights_->b);
   }

   void insert(const size_t idx) {
      assert(weights_);
      weights_->insertIdx(idx, active_);
   }

   void erase(const size_t idx) {
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