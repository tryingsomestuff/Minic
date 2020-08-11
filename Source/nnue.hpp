#pragma once

// THIS IS MAINLY COPY/PASTE FROM STOCKFISH, as well as the full nnue sub-directory

#include "definition.hpp"

#ifdef WITH_NNUE

struct Position; // forward decl

#define WHITE Co_White
#define BLACK Co_Black
#define SQUARE_NB NbSquare

// NNUE eval scaling factor
extern int NNUEscaling;

// An ID used to track the pieces. Max. 32 pieces on board.
enum PieceId {
  PIECE_ID_ZERO   = 0,
  PIECE_ID_KING   = 30,
  PIECE_ID_WKING  = 30,
  PIECE_ID_BKING  = 31,
  PIECE_ID_NONE   = 32
};

inline PieceId operator++(PieceId & d){d=PieceId(d+1); return d;}

inline PieceId operator++(PieceId& d, int) {
  const PieceId x = d;
  d = PieceId(int(d) + 1);
  return x;
}

constexpr bool PieceIdOK(PieceId pid){ return pid < PIECE_ID_NONE;}

// unique number for each piece type on each square
enum PieceSquare : uint32_t {
  PS_NONE     =  0,
  PS_W_PAWN   =  1,
  PS_B_PAWN   =  1 * SQUARE_NB + 1,
  PS_W_KNIGHT =  2 * SQUARE_NB + 1,
  PS_B_KNIGHT =  3 * SQUARE_NB + 1,
  PS_W_BISHOP =  4 * SQUARE_NB + 1,
  PS_B_BISHOP =  5 * SQUARE_NB + 1,
  PS_W_ROOK   =  6 * SQUARE_NB + 1,
  PS_B_ROOK   =  7 * SQUARE_NB + 1,
  PS_W_QUEEN  =  8 * SQUARE_NB + 1,
  PS_B_QUEEN  =  9 * SQUARE_NB + 1,
  PS_W_KING   = 10 * SQUARE_NB + 1,
  PS_END      = PS_W_KING, // pieces without kings (pawns included)
  PS_B_KING   = 11 * SQUARE_NB + 1,
  PS_END2     = 12 * SQUARE_NB + 1
};

struct ExtPieceSquare {
  PieceSquare from[Co_End];
};

// Array for finding the PieceSquare corresponding to the piece on the board
extern ExtPieceSquare kpp_board_index[NbPiece];

// Structure holding which tracked piece (PieceId) is where (PieceSquare)
class EvalList {

public:

  // Max. number of pieces without kings is 30 but must be a multiple of 4 in AVX2
  static const int MAX_LENGTH = 32;

  // Array that holds the piece id for the pieces on the board
  PieceId piece_id_list[NbSquare];

  // List of pieces, separate from White and Black POV
  PieceSquare* piece_list_fw() const { return const_cast<PieceSquare*>(pieceListFw); }
  PieceSquare* piece_list_fb() const { return const_cast<PieceSquare*>(pieceListFb); }

  // Place the piece pc with piece_id on the square sq on the board
  void put_piece(PieceId piece_id, Square sq, int pc);

  // Convert the specified piece_id piece to ExtPieceSquare type and return it
  ExtPieceSquare piece_with_id(PieceId piece_id) const;

private:
  PieceSquare pieceListFw[MAX_LENGTH];
  PieceSquare pieceListFb[MAX_LENGTH];
};

// For differential evaluation of pieces that changed since last turn
struct DirtyPiece {
  // Number of changed pieces
  int dirty_num;

  // The ids of changed pieces, max. 2 pieces can change in one move
  PieceId pieceId[2];

  // What changed from the piece with that piece number
  ExtPieceSquare old_piece[2];
  ExtPieceSquare new_piece[2];
};

namespace nnue{
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

#endif