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

   static constexpr int nbuckets {NNUEWeights<NT, Q>::nbuckets};
   static constexpr int bucketDivisor {NNUEWeights<NT, Q>::bucketDivisor};

   void clear() {
      white.clear();
      black.clear();
   }

   typedef typename Quantization<Q>::BT BT;

   constexpr float propagate(Color c, const int npiece) const {
      const auto w_x = white.active();
      const auto b_x = black.active();
      const auto x0  = c == Co_White ? splice(w_x, b_x).apply_(activationInput<BT, Q>) : splice(b_x, w_x).apply_(activationInput<BT, Q>);
      
      const int phase = std::min(nbuckets-1, (npiece-1) / bucketDivisor);
      /*
      int k1 = phase;
      int k2 = phase;
      if( npiece > phase * bucketDivisor + bucketDivisor/2 ){
         k2 = std::min(nbuckets - 1, phase + 1);
      }
      else{
         k1 = std::max(0, phase - 1);
      }
      if ( k1 == k2 ){
         assert(phase == 0 || phase == nbuckets-1);
      */
         const auto x1 = (weights.innerLayer[phase].fc0).forward(x0).apply_(activation<BT, Q>);
         const auto x2 = splice(x1, (weights.innerLayer[phase].fc1).forward(x1).apply_(activation<BT, Q>));
         const auto x3 = splice(x2, (weights.innerLayer[phase].fc2).forward(x2).apply_(activation<BT, Q>));
         const float val = (weights.innerLayer[phase].fc3).forward(x3).item();
         return val / Quantization<Q>::outFactor;
      /*
      }
      else{
         const int dsup = k2 * bucketDivisor + bucketDivisor/2 - npiece;          
         assert(dsup>=0 && dsup<=8);
         float val = 0;
         {
         const auto x1 = (weights.innerLayer[k1].fc0).forward(x0).apply_(activation<BT, Q>);
         const auto x2 = splice(x1, (weights.innerLayer[k1].fc1).forward(x1).apply_(activation<BT, Q>));
         const auto x3 = splice(x2, (weights.innerLayer[k1].fc2).forward(x2).apply_(activation<BT, Q>));
         val += (weights.innerLayer[k1].fc3).forward(x3).item() * (8-dsup) / 8.f;
         }
         {
         const auto x1 = (weights.innerLayer[k2].fc0).forward(x0).apply_(activation<BT, Q>);
         const auto x2 = splice(x1, (weights.innerLayer[k2].fc1).forward(x1).apply_(activation<BT, Q>));
         const auto x3 = splice(x2, (weights.innerLayer[k2].fc2).forward(x2).apply_(activation<BT, Q>));
         val += (weights.innerLayer[k2].fc3).forward(x3).item() * dsup / 8.f;
         }         
         return val / Quantization<Q>::outFactor;
      }
      */
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