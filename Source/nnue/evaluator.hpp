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
   // if dirty, then an update/reset is necessary
   bool dirty = true;

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
   
   float propagate(Color c, const int bucket) const {
      assert(!dirty);
      assert(bucket >= 0);
      assert(bucket < (NNUEWeights<NT, Q>::nbuckets));
      
      constexpr float deqScale = 1.f / Quantization<Q>::scale;
      const auto& layer = weights.innerLayer[bucket];

#ifdef USE_SIMD_INTRIN
      StackVector<BT, 2 * firstInnerLayerSize, Q> x0;
      {
         const auto& first  = (c == Co_White) ? white : black;
         const auto& second = (c == Co_White) ? black : white;
         simdDequantizeActivate_i16_f32<firstInnerLayerSize, Q>(
            x0.data, first.active().data, deqScale);
         simdDequantizeActivate_i16_f32<firstInnerLayerSize, Q>(
            x0.data + firstInnerLayerSize, second.active().data, deqScale);
      }

      auto x1 = layer.fc0.forward(x0).activation_();

      StackVector<BT, 16, Q> x2;
      simdCopy_f32<8>(x2.data, x1.data);
      layer.fc1.forwardTo(x1, x2.data + 8);
      simdActivation<8, Q>(x2.data + 8);

      StackVector<BT, 24, Q> x3;
      simdCopy_f32<16>(x3.data, x2.data);
      layer.fc2.forwardTo(x2, x3.data + 16);
      simdActivation<8, Q>(x3.data + 16);

      const float val = layer.fc3.forward(x3).data[0];
#if defined(__AVX2__)
      _mm256_zeroupper();
#endif

#else
      // Non-SIMD fallback
      const auto w_x {white.active().dequantize(deqScale)
                               .apply_(activationInput<BT, Q>)
                  };
      const auto b_x {black.active().dequantize(deqScale)
                               .apply_(activationInput<BT, Q>)
                  };
      
      const auto x0 = c == Co_White ? splice(w_x, b_x) : splice(b_x, w_x);
      const auto x1 = layer.fc0.forward(x0)
                            .apply_(activation<BT, Q>);
      const auto x2 = splice(x1, layer.fc1.forward(x1)
                                      .apply_(activation<BT, Q>));
      const auto x3 = splice(x2, layer.fc2.forward(x2)
                                      .apply_(activation<BT, Q>));
      const float val = layer.fc3.forward(x3).data[0];
#endif
      return val * Quantization<Q>::outFactor;
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