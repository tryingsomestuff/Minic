#include "nnue.hpp"

#ifdef WITH_NNUE

#include "com.hpp"
#include "dynamicConfig.hpp"
#include "evalDef.hpp"
#include "logging.hpp"
#include "moveGen.hpp"
#include "score.hpp"
#include "smp.hpp"

#include "nnue/nnue_def.h"
#include "nnue/evaluate_nnue.h"

// this is used to scale NNUE score to classic eval score. 
// This way search params can remain the same ... more or less ...
// see compute_scaling
int NNUEWrapper::NNUEscaling = 64; // from 32 to 128      x_scaled = x * NNUEscaling / 64

// defined in nnue_def.h
// Must be user defined in order to respect Piece engine piece order
 uint32_t kpp_board_index[PIECE_NB][COLOR_NB] = {
 // convention: W - us, B - them
 // viewed from other side, W and B are reversed
    { PS_B_KING,   PS_W_KING   },
    { PS_B_QUEEN,  PS_W_QUEEN  },
    { PS_B_ROOK,   PS_W_ROOK   },
    { PS_B_BISHOP, PS_W_BISHOP },
    { PS_B_KNIGHT, PS_W_KNIGHT },
    { PS_B_PAWN,   PS_W_PAWN   },
    { PS_NONE,     PS_NONE     },
    { PS_W_PAWN,   PS_B_PAWN   },
    { PS_W_KNIGHT, PS_B_KNIGHT },
    { PS_W_BISHOP, PS_B_BISHOP },
    { PS_W_ROOK,   PS_B_ROOK   },
    { PS_W_QUEEN,  PS_B_QUEEN  },
    { PS_W_KING,   PS_B_KING   }
};

namespace NNUEWrapper{

std::string eval_file_loaded="None";

// Load the evaluation function file
bool load_eval_file(const std::string& evalFile) {
    NNUE::Initialize();
    NNUE::fileName = evalFile;
    std::ifstream stream(evalFile, std::ios::binary);
    const bool result = NNUE::ReadParameters(stream);
    return result;
}

// Evaluation function. Perform differential calculation.
ScoreType evaluate(const Position& pos) {
    ScoreType v = NNUE::ComputeScore(pos, false);
    v = std::min(std::max(v, ScoreType(-WIN + 1)), ScoreType(WIN - 1));
    return v;
}

void init_NNUE() {
    if (eval_file_loaded != DynamicConfig::NNUEFile)
        if (load_eval_file(DynamicConfig::NNUEFile)){
            eval_file_loaded = DynamicConfig::NNUEFile;
            DynamicConfig::useNNUE = true; // forced when nnue file is found and valid
            COM::init(); // reset start position
            compute_scaling();
        }
}

void verify_NNUE() {
    if (DynamicConfig::useNNUE && eval_file_loaded != DynamicConfig::NNUEFile){ // this won't happen
        Logging::LogIt(Logging::logFatal) << "Use of NNUE evaluation, but the file " << DynamicConfig::NNUEFile << " was not loaded successfully. "
                                          << "These network evaluation parameters must be available, compatible with this version of the code. "
                                          << "The UCI option NNUEFile might need to specify the full path, including the directory/folder name, to the file.";
    }

    if (DynamicConfig::useNNUE){
        Logging::LogIt(Logging::logInfo) << "NNUE evaluation using " << DynamicConfig::NNUEFile << " enabled.";
    }
    else{
        Logging::LogIt(Logging::logInfo) << "classical evaluation enabled.";
    }
}

void compute_scaling(int count){
    static std::random_device rd;
    static std::mt19937 g(63); // fixed seed !

    Logging::LogIt(Logging::logInfo) << "Automatic computation of NNUEscaling with " << count << " random positions ...";

    Position p;
    readFEN(startPosition,p,true);

    EvalData data;

    float factor = 0;
    int k = 0;

    bool bkTT = DynamicConfig::disableTT;
    DynamicConfig::disableTT = true;

    while(k < count){
        MoveList moves;
        //const bool isInCheck = isAttacked(p, kingSquare(p));
        MoveGen::generate<MoveGen::GP_all>(p, moves, false);
        if (moves.empty()){
            readFEN(startPosition,p,true);
            continue;
        }
        std::shuffle(moves.begin(), moves.end(),g);
        for (auto it = moves.begin(); it != moves.end(); ++it) {
            Position p2 = p;
            if (!applyMove(p2, *it)) continue;
            p = p2;
            const Square to = Move2To(*it);
            if (p.c == Co_White && to == p.king[Co_Black]){
               readFEN(startPosition,p,true);
               continue;
            }
            if (p.c == Co_Black && to == p.king[Co_White]){
               readFEN(startPosition,p,true);
               continue;
            }

            DynamicConfig::useNNUE = 0;
            const ScoreType eStd = eval(p,data,ThreadPool::instance().main());
            DynamicConfig::useNNUE = 1;
            const ScoreType eNNUE = eval(p,data,ThreadPool::instance().main());

            if ( std::abs(eStd) < 1000 && eStd*eNNUE > 0 ){
               ++k;
               factor += eStd/eNNUE;
            }
            break;
        }
        readFEN(startPosition,p,true);
        continue;
    }
    NNUEWrapper::NNUEscaling = int(factor*64/k);
    Logging::LogIt(Logging::logInfo) << "NNUEscaling " << NNUEWrapper::NNUEscaling << " (" << factor/k << ")";

    DynamicConfig::disableTT = bkTT;
}

} // nnue

#endif 