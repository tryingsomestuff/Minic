#include "dynamicConfig.hpp"

namespace DynamicConfig{
    bool mateFinder        = false;
    bool disableTT         = false;
    unsigned int ttSizeMb  = 128; // here in Mb, will be converted to real size next
    bool fullXboardOutput  = false;
    bool debugMode         = false;
    bool quiet             = true;
    std::string debugFile  = "minic.debug";
    unsigned int level     = 100;
    bool book              = false;
    std::string bookFile   = "book.bin";
    unsigned int threads   = 1;
    std::string syzygyPath = "";
    bool FRC               = false;
    bool UCIPonder         = false;
    unsigned int multiPV   = 1;
    ScoreType contempt     = 12;
    ScoreType contemptMG   = 12;
    bool limitStrength     = false;
    int strength           = 1500;
    bool useNNUE           = false;
    std::string NNUEFile   = "";

    int styleComplexity   = 50;
    int styleMaterial     = 50;
    int stylePositional   = 50;
    int styleDevelopment  = 50;
    int styleMobility     = 50;
    int styleAttack       = 50;
    int stylePawnStruct   = 50;
    int styleForwardness  = 50;
}
