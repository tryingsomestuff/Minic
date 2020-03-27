#pragma once

#include "definition.hpp"

/* Things that are widely used with values that can be varying due to GUI or CLI interaction
 */

namespace DynamicConfig{
    extern bool mateFinder       ;
    extern bool disableTT        ;
    extern unsigned int ttSizeMb ;
    extern bool fullXboardOutput ;
    extern bool debugMode        ;
    extern bool quiet            ;
    extern std::string debugFile ;
    extern unsigned int level    ;
    extern bool book             ;
    extern std::string bookFile  ;
    extern unsigned int threads  ;
    extern std::string syzygyPath;
    extern bool FRC              ;
    extern bool UCIPonder        ;
    extern unsigned int multiPV  ;
}
