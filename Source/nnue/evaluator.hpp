#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include "activation.hpp"
#include "featureTransformer.hpp"
#include "sided.hpp"
#include "weight.hpp"

template<typename NT, bool Q> struct NNUEEval : Sided<NNUEEval<NT, Q>, FeatureTransformer<NT, Q>> {
   // common data (weights and bias)
   static NNUEWeights<NT, Q> weights;
   // instance data (active index)
   FeatureTransformer<NT, Q> white;
   FeatureTransformer<NT, Q> black;

   void clear() {
      white.clear();
      black.clear();
   }

   typedef typename Quantization<Q>::BT BT;

   constexpr float propagate(Color c) const {
      const auto w_x = white.active();
      const auto b_x = black.active();
      const auto x0  = c == Co_White ? splice(w_x, b_x).apply_(activationInput<BT, Q>) : splice(b_x, w_x).apply_(activationInput<BT, Q>);
      const auto x1 = (weights.fc0).forward(x0).apply_(activation<BT, Q>);
      const auto x2 = splice(x1, (weights.fc1).forward(x1).apply_(activation<BT, Q>));
      const auto x3 = splice(x2, (weights.fc2).forward(x2).apply_(activation<BT, Q>));
      const float val = (weights.fc3).forward(x3).item();
      return val / Quantization<Q>::outFactor;
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

   // default CTOR always use loaded weights
   NNUEEval() {
      white = &weights.w;
      black = &weights.b;
   }
};

template<typename NT, bool Q> NNUEWeights<NT, Q> NNUEEval<NT, Q>::weights;

#endif // WITH_NNUE 