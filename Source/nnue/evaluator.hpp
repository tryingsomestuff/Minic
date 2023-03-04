#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "activation.hpp"
#include "featureTransformer.hpp"
#include "sided.hpp"
#include "weight.hpp"

template<typename NT, bool Q> 
struct NNUEEval : Sided<NNUEEval<NT, Q>, FeatureTransformer<NT, Q>> {
   // common data (weights and bias)
   static NNUEWeights<NT, Q> weights;

   // instance data (active index)
   FeatureTransformer<NT, Q> white;
   FeatureTransformer<NT, Q> black;

   // status of the FeatureTransformers
   // if dirty, then un update/reset is necessary
   bool dirty = true;

   static constexpr int nbuckets {NNUEWeights<NT, Q>::nbuckets};
   static constexpr int bucketDivisor {NNUEWeights<NT, Q>::bucketDivisor};

   FORCE_FINLINE void clear() {
      dirty = true;
      white.clear();
      black.clear();
   }

   FORCE_FINLINE void clear(Color color) {
      dirty = true;
      if (color == Co_White)
         white.clear();
      else
         black.clear();
   }

   using BT = typename Quantization<Q>::BT;

   constexpr float propagate(Color c, const int npiece) const {
      assert(!dirty);
      const auto & w_x {white.active()};
      const auto & b_x {black.active()};
      const auto x0 = (c == Co_White ? splice(w_x, b_x) : splice(b_x, w_x));//.apply_(activationInput<BT, Q>);
      const int phase = std::max(0, std::min(nbuckets-1, (npiece-1) / bucketDivisor));
      if constexpr (Q){
         const auto x0d = x0.dequantize(1.f/Quantization<Q>::scale);
         const auto x1 = weights.innerLayer[phase].fc0.forward(x0d);//.apply_(activation<BT, Q>);
         const auto x2 = splice(x1, (weights.innerLayer[phase].fc1).forward(x1));//.apply_(activation<BT, Q>));
         const auto x3 = splice(x2, (weights.innerLayer[phase].fc2).forward(x2));//.apply_(activation<BT, Q>));
         const float val = (weights.innerLayer[phase].fc3).forward(x3).data[0];
         return val / Quantization<Q>::outFactor;
      }
      else {
         const auto x1 = weights.innerLayer[phase].fc0.forward(x0);//.apply_(activation<BT, Q>);
         const auto x2 = splice(x1, (weights.innerLayer[phase].fc1).forward(x1));//.apply_(activation<BT, Q>));
         const auto x3 = splice(x2, (weights.innerLayer[phase].fc2).forward(x2));//.apply_(activation<BT, Q>));
         const float val = (weights.innerLayer[phase].fc3).forward(x3).data[0];
         return val / Quantization<Q>::outFactor;
      }
   }

#ifdef DEBUG_NNUE_UPDATE
   bool operator==(const NNUEEval<NT,Q>& other) {
      if (white != other.white || black != other.black) return false;
      return true;
   }

   bool operator!=(const NNUEEval<NT,Q>& other) {
      if (white != other.white || black != other.black) return true;
      return false;
   }
#endif

   // default CTOR always use loaded weights in FeatureTransformer 
   // (as pointer of course, not a copy)
   NNUEEval():
      white(&weights.w),
      black(&weights.b){}
};

template<typename NT, bool Q> 
NNUEWeights<NT, Q> NNUEEval<NT, Q>::weights;

#endif // WITH_NNUE 