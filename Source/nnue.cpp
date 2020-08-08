#include "nnue.hpp"

#ifdef WITH_NNUE

#include "com.hpp"
#include "dynamicConfig.hpp"
#include "logging.hpp"

#include "nnue/evaluate_nnue.h"

// THIS IS MAINLY COPY/PASTE FROM STOCKFISH, as well as the full nnue sub-directory

ExtPieceSquare kpp_board_index[NbPiece] = {
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

static constexpr Square rotate180(Square sq){return (Square)(sq ^ 0x3F);}

// Place the piece pc with piece_id on the square sq on the board
void EvalList::put_piece(PieceId piece_id, Square sq, int pc){
    assert(PieceIdOK(piece_id));
    if (pc != PieceIdx(P_none)){
        pieceListFw[piece_id] = PieceSquare(kpp_board_index[pc].from[Co_White] + sq);
        pieceListFb[piece_id] = PieceSquare(kpp_board_index[pc].from[Co_Black] + rotate180(sq));
        piece_id_list[sq] = piece_id;
    }
    else{
        pieceListFw[piece_id] = PS_NONE;
        pieceListFb[piece_id] = PS_NONE;
        piece_id_list[sq] = piece_id;
    }
}

// Convert the specified piece_id piece to ExtPieceSquare type and return it
ExtPieceSquare EvalList::piece_with_id(PieceId piece_id) const{
    ExtPieceSquare eps;
    eps.from[Co_White] = pieceListFw[piece_id];
    eps.from[Co_Black] = pieceListFb[piece_id];
    return eps;
}


namespace nnue{

std::string eval_file_loaded="None";

// Load the evaluation function file
bool load_eval_file(const std::string& evalFile) {
    Eval::NNUE::Initialize();
    Eval::NNUE::fileName = evalFile;
    std::ifstream stream(evalFile, std::ios::binary);
    const bool result = Eval::NNUE::ReadParameters(stream);
    return result;
}

// Evaluation function. Perform differential calculation.
ScoreType evaluate(const Position& pos) {
    ScoreType v = Eval::NNUE::ComputeScore(pos, false);
    v = std::min(std::max(v, ScoreType(-WIN + 1)), ScoreType(WIN - 1));
    return v;
}

void init_NNUE() {
    if (eval_file_loaded != DynamicConfig::NNUEFile)
        if (load_eval_file(DynamicConfig::NNUEFile)){
            eval_file_loaded = DynamicConfig::NNUEFile;
            DynamicConfig::useNNUE = true; // forced when nnue file is found and valid
            COM::init(); // reset start position
        }
}

void verify_NNUE() {
    if (DynamicConfig::useNNUE && eval_file_loaded != DynamicConfig::NNUEFile){ // this won't happen
        Logging::LogIt(Logging::logFatal) << "Use of NNUE evaluation, but the file " << DynamicConfig::NNUEFile << " was not loaded successfully. "
                                          << "These network evaluation parameters must be available, compatible with this version of the code. "
                                          << "The UCI option NNUEFile might need to specify the full path, including the directory/folder name, to the file.";
    }

    if (DynamicConfig::useNNUE)
        Logging::LogIt(Logging::logInfo) << "NNUE evaluation using " << DynamicConfig::NNUEFile << " enabled.";
    else
        Logging::LogIt(Logging::logInfo) << "classical evaluation enabled.";
}

} // nnue

#include "nnue/features/half_kp.cpp"
#include "nnue/evaluate_nnue.cpp"

#endif