#pragma once

#include "definition.hpp"

/* A simple, and partial, XBOARD implementation
 */

namespace XBoard{
    extern bool display; ///@todo use it !
    void init();
    void setFeature();
    bool receiveMove(const std::string & command);
    bool replay(size_t nbmoves);
    void xboard();
} // XBoard
