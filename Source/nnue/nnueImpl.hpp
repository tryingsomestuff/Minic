#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

enum ActivationType{
   eReLU = 0,
   eClippedReLU,
   ePReLU,
   eClippedPReLU,
   eApproxSigmoid
};

constexpr auto activationType = eClippedReLU;

#ifdef USE_SIMD_INTRIN
#include "simd.hpp" // manual simd implementations
#endif

// NNUE implementation was initially taken from Seer implementation in October 2020.
// see https://github.com/connormcmonigle/seer-nnue

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_CAMEL
#include "incbin.h"

namespace nnue {

#ifdef EMBEDDEDNNUEPATH
namespace embedded {
INCBIN_EXTERN(weightsFile);
}
#endif

namespace FeatureIdx {

   inline constexpr size_t major = 64 * 12;
   inline constexpr size_t minor = 64;
   
   inline constexpr size_t usPawnOffset   = 0;
   inline constexpr size_t usKnightOffset = usPawnOffset + minor;
   inline constexpr size_t usBishopOffset = usKnightOffset + minor;
   inline constexpr size_t usRookOffset   = usBishopOffset + minor;
   inline constexpr size_t usQueenOffset  = usRookOffset + minor;
   inline constexpr size_t usKingOffset   = usQueenOffset + minor;
   
   inline constexpr size_t themPawnOffset   = usKingOffset + minor;
   inline constexpr size_t themKnightOffset = themPawnOffset + minor;
   inline constexpr size_t themBishopOffset = themKnightOffset + minor;
   inline constexpr size_t themRookOffset   = themBishopOffset + minor;
   inline constexpr size_t themQueenOffset  = themRookOffset + minor;
   inline constexpr size_t themKingOffset   = themQueenOffset + minor;
   
   [[nodiscard]] constexpr size_t usOffset(const Piece pt) {
      switch (pt) {
         case P_wp: return usPawnOffset;
         case P_wn: return usKnightOffset;
         case P_wb: return usBishopOffset;
         case P_wr: return usRookOffset;
         case P_wq: return usQueenOffset;
         case P_wk: return usKingOffset;
         default: return usPawnOffset;
      }
   }
   
   [[nodiscard]] constexpr size_t themOffset(const Piece pt) {
      switch (pt) {
         case P_wp: return themPawnOffset;
         case P_wn: return themKnightOffset;
         case P_wb: return themBishopOffset;
         case P_wr: return themRookOffset;
         case P_wq: return themQueenOffset;
         case P_wk: return themKingOffset;
         default: return themPawnOffset;
      }
   }
   
} // namespace FeatureIdx

#include "evaluator.hpp"

} // namespace nnue

#endif // WITH_NNUE
