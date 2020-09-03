#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

struct Position; // forward decl

// convert Minic things to SF/NNUE ones
///@todo prefix everything with NNUE ...
///@todo put a namespace around all NNUE "lib"
#define WHITE Co_White
#define BLACK Co_Black
#define SQUARE_NB NbSquare
#define FILE_NB 8
#define RANK_NB 8
#define PIECE_NB NbPiece
#define NO_PIECE PieceIdx(P_none)
#define NNUEValue ScoreType

// Internal wrapper to the NNUE things
namespace NNUEWrapper{

  // NNUE eval scaling factor
  extern int NNUEscaling;

  ScoreType ComputeScore(const Position& pos, bool refresh);
  void Initialize();
  void UpdateAccumulatorIfPossible(const Position& pos);
  // curently loaded network
  extern std::string eval_file_loaded;

  void init_NNUE();
  void verify_NNUE();
  void compute_scaling(int count = 50000);

  ScoreType evaluate(const Position& pos);
  bool  load_eval_file(const std::string& evalFile);

} // nnue

// optionnal learning tools
// note that this next header is free from engine specific stuff !
#include "learn/convert.hpp"
#include "learn/learner.hpp"

#endif