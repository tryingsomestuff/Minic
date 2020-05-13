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
    ScoreType contempt     = 15;
    ScoreType contemptMG   = 15;
    bool limitStrength     = false;
    int strength           = 1500;
}
