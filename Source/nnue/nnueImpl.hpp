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

#ifdef USE_SIMD_INTRIN
#include "dot.hpp" // dot product simd implementation
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

#include "evaluator.hpp"

} // namespace nnue

#endif // WITH_NNUE
