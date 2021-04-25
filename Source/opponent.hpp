#pragma once

 #include "definition.hpp"
 #include "dynamicConfig.hpp"

namespace Opponent{

void init(){
    if ( DynamicConfig::opponent.empty()){
       ///@todo restore default
    }
    else{
       ///@todo adjust contempt based on opponent ?
       ///@todo play with style ?
    }
}

}