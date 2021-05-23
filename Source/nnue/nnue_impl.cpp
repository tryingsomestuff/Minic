#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_CAMEL
#include "incbin.h"

namespace nnue {

#ifdef EMBEDEDNNUEPATH
namespace embeded {
INCBIN(weightsFile, INCBIN_STRINGIZE(EMBEDEDNNUEPATH));
// We now have these three symbols
// const unsigned char weightsFileData[];
// const unsigned int weightsFileSize;
} // namespace embeded
#endif

} // namespace nnue