#include "dynamicConfig.hpp"

namespace DynamicConfig{
    bool mateFinder          = false;
    bool disableTT           = false;
    unsigned int ttSizeMb    = 128; // here in Mb, will be converted to real size next
    bool fullXboardOutput    = false;
    bool debugMode           = false;
    bool quiet               = true;
    bool silent              = false;
    std::string debugFile    = "minic.debug";
    unsigned int level       = 100;
    unsigned int randomOpen  = 0;
    unsigned int threads     = 1;
    std::string syzygyPath   = "";
    bool FRC                 = false;
    bool UCIPonder           = false;
    unsigned int multiPV     = 1;
    ScoreType contempt       = 12;
    ScoreType contemptMG     = 12;
    bool limitStrength       = false;
    int strength             = 1500;
    bool nodesBasedLevel     = false;
    bool useNNUE             = false;
    bool forceNNUE           = false;

    // this is used to scale NNUE score to classic eval score. 
    // This way search params can remain the same ... more or less ...
    // see compute_scaling
    // from 32 to 256      x_scaled = x * NNUEscaling / 64
    int NNUEscaling = 64; 

#ifndef FORCEEMBEDEDNNUE    
    std::string NNUEFile;
#else
    std::string NNUEFile     = "embeded";
#endif
    bool genFen              = false;
    unsigned int genFenDepth = 8;
    unsigned int genFenDepthEG = 16;
    unsigned int randomPly   = 0;
    unsigned int moveOverHead= 50;
    bool armageddon          = false;
    bool withWDL             = false;
    
    std::string opponent     = "";
    int ratingAdv            = 0; 
    bool ratingAdvReveived   = false;
    double ratingFactor      = 1.;

    int styleComplexity      = 50;
    int styleMaterial        = 50;
    int stylePositional      = 50;
    int styleDevelopment     = 50;
    int styleMobility        = 50;
    int styleAttack          = 50;
    int stylePawnStruct      = 50;
    int styleForwardness     = 50;

    bool stylized            = false;
}
