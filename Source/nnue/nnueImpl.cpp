#include "definition.hpp"

#ifdef WITH_NNUE

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_CAMEL
#include "incbin.h"

namespace nnue {

#ifdef EMBEDDEDNNUEPATH
namespace embedded {
INCBIN(weightsFile, INCBIN_STRINGIZE(EMBEDDEDNNUEPATH));
// We now have these three symbols
// const unsigned char weightsFileData[];
// const unsigned int weightsFileSize;
} // namespace embedded
#endif // EMBEDDEDNNUEPATH

} // namespace nnue

#endif // WITH_NNUE