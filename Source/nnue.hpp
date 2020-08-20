#pragma once

#include "definition.hpp"

#ifdef WITH_NNUE

struct Position; // forward decl

// convert Minic things to NNUE ones
#define WHITE Co_White
#define BLACK Co_Black
#define SQUARE_NB NbSquare
#define PIECE_NB NbPiece
#define NO_PIECE PieceIdx(P_none)

// Internal wrapper to the NNUE things

namespace nnue{

  // NNUE eval scaling factor
  extern int NNUEscaling;

  ScoreType ComputeScore(const Position& pos, bool refresh);
  void Initialize();
  bool ReadParameters(std::istream& stream);
  void UpdateAccumulatorIfPossible(const Position& pos);
  // curently loaded network
  extern std::string eval_file_loaded;

  void init_NNUE();
  void verify_NNUE();
  void compute_scaling(int count = 50000);

  ScoreType evaluate(const Position& pos);
  bool  load_eval_file(const std::string& evalFile);

} // nnue

#include "learn/learn_tools.hpp"

#endif