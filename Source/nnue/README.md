This directory is a copy/paste from Stockfish NNUE and NodChip Stockfish repo and easily merged a few times since then.
(initially based on https://github.com/official-stockfish/Stockfish/pull/2912/commits/d65d0d04809b40334f5ea67f00cb9d3aedf513f3)

Only small change are needed to make it Minic compatible and it feels to me that this thing will benefit from being a separate library.

Wrapping in Minic is futher done in nnue.hpp/cpp. Please follow WITH_NNUE elsewhere in the code.

Please note than NNUE code for eval is merged from Stockfish official repo while learning code is merged and adapted to Minic from NodChip repo.
