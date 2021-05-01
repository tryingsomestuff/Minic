#pragma once

#include "dynamicConfig.hpp"

namespace Opponent{

void init();

inline void ratingReceived(){
   DynamicConfig::ratingAdvReveived = true;
}

}
