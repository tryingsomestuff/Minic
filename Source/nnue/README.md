This directory is a copy/paste from SF NNUE 
initially based on
https://github.com/official-stockfish/Stockfish/pull/2912/commits/d65d0d04809b40334f5ea67f00cb9d3aedf513f3
and merged a few times since then

Only small change are needed to make it Minic compatible and it feels to me that this thing will benefit from being a separate library.

Wrapping in Minic is futher done in nnue.hpp/cpp. Please follow WITH_NNUE elsewhere in the code.
