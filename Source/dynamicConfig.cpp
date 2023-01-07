#include "dynamicConfig.hpp"

#include "logging.hpp"

namespace DynamicConfig {
bool         mateFinder       = false;
bool         disableTT        = false;
#if defined(WITH_SMALL_MEMORY)
unsigned int ttSizeMb         = 2; // here in Kb in fact (SIZE_MULTIPLIER), will be converted to real size next
unsigned int ttPawnSizeMb     = 1;  // here in Kb in fact (SIZE_MULTIPLIER), will be converted to real size next
#else
unsigned int ttSizeMb         = 128; // here in Mb, will be converted to real size next
unsigned int ttPawnSizeMb     = 16;  // here in Mb, will be converted to real size next
#endif
bool         fullXboardOutput = false;
bool         debugMode        = false;
int          minOutputLevel   = Logging::logGUI;
std::string  debugFile        = "minic.debug";
unsigned int level            = 100;
unsigned int randomOpen       = 0;
unsigned int threads          = 1;
std::string  syzygyPath       = "";
bool         FRC              = false;
bool         DFRC             = false;
bool         UCIPonder        = false;
unsigned int multiPV          = 1;
ScoreType    contempt         = 12;
ScoreType    contemptMG       = 12;
bool         limitStrength    = false;
int          strength         = 1500;
bool         nodesBasedLevel  = false;
bool         useNNUE          = false;
bool         forceNNUE        = false;
bool         bongCloud        = false;
bool         anarchy          = false;

std::string  chessvariant     = "chess";

// this is used to scale NNUE score to classic eval score.
// This way search params can remain the same ... more or less ...
// from 32 to 256      x_scaled = x * NNUEscaling / 64
int NNUEScaling   = 80; // hand tuned
int NNUEThreshold = 600;
int NNUEThreshold2 = 300;

#ifndef FORCEEMBEDDEDNNUE
std::string NNUEFile;
#else
std::string NNUEFile = "embedded";
#endif
bool         genFen        = false;
bool         pgnOut        = false;
unsigned int genFenDepth   = 8;
unsigned int genFenDepthEG = 16;
unsigned int randomPly     = 0;
unsigned int moveOverHead  = 50;
bool         armageddon    = false;
bool         antichess     = false;
bool         withWDL       = false;

std::string opponent          = "";
int         ratingAdv         = 0;
bool        ratingAdvReceived = false;
double      ratingFactor      = 1.;

int styleComplexity  = 50;
int styleMaterial    = 50;
int stylePositional  = 50;
int styleDevelopment = 50;
int styleMobility    = 50;
int styleAttack      = 50;
int stylePawnStruct  = 50;
int styleForwardness = 50;

bool stylized = false;
} // namespace DynamicConfig
