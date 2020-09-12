#pragma once

// THIS IS MAINLY COPY/PASTE FROM STOCKFISH, as well as the full nnue sub-directory

#include "nnue.hpp"

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

// Array for finding the PieceSquare corresponding to the piece on the board
// Must be user defined in order to respect Piece engine piece order
extern uint32_t kpp_board_index[PIECE_NB][COLOR_NB];

constexpr Square rotate180(Square sq){return (Square)(sq ^ 0x3F);}

// For differential evaluation of pieces that changed since last turn
struct DirtyPiece {

  // Number of changed pieces
  int dirty_num;

  // Max 3 pieces can change in one move. A promotion with capture moves
  // both the pawn and the captured piece to SQ_NONE and the piece promoted
  // to from SQ_NONE to the capture square.
  Piece piece[3];

  // From and to squares, which may be SQ_NONE
  Square from[3];
  Square to[3];

  bool operator==(const DirtyPiece & dp){
     return dirty_num == dp.dirty_num
     && piece[0] == dp.piece[0]
     && piece[1] == dp.piece[1]
     && piece[2] == dp.piece[2]
     && from[0] == dp.from[0]
     && from[1] == dp.from[1]
     && from[2] == dp.from[2]
     && to[0] == dp.to[0]
     && to[1] == dp.to[1]
     && to[2] == dp.to[2];
  }
};