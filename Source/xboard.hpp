#pragma once

#include "definition.hpp"

/*! 
 * A simple, and partial, XBOARD implementation
 * Some helm here : https://www.gnu.org/software/xboard/engine-intf.html
 */
namespace XBoard {
extern bool display; ///@todo use it !
void        init();
void        xboard();

void moveApplied(bool success);
} // namespace XBoard
