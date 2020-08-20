#pragma once

// THIS IS MAINLY COPY/PASTE FROM STOCKFISH, as well as the full nnue sub-directory

#include "../../nnue.hpp"

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
// Must be user defined in order to respect Piece engine piece order
extern ExtPieceSquare kpp_board_index[PIECE_NB];

constexpr Square rotate180(Square sq){return (Square)(sq ^ 0x3F);}

// Structure holding which tracked piece (PieceId) is where (PieceSquare)
class EvalList {

public:

  // Max. number of pieces without kings is 30 but must be a multiple of 4 in AVX2
  static const int MAX_LENGTH = 32;

  // Array that holds the piece id for the pieces on the board
  PieceId piece_id_list[SQUARE_NB];

  // List of pieces, separate from White and Black POV
  inline PieceSquare* piece_list_fw() const { return const_cast<PieceSquare*>(pieceListFw); }
  inline PieceSquare* piece_list_fb() const { return const_cast<PieceSquare*>(pieceListFb); }

  // Place the piece pc with piece_id on the square sq on the board
  inline void put_piece(PieceId piece_id, Square sq, int pc){
    assert(PieceIdOK(piece_id));
    if (pc != NO_PIECE){
        pieceListFw[piece_id] = PieceSquare(kpp_board_index[pc].from[WHITE] + sq);
        pieceListFb[piece_id] = PieceSquare(kpp_board_index[pc].from[BLACK] + rotate180(sq));
        piece_id_list[sq] = piece_id;
    }
    else{
        pieceListFw[piece_id] = PS_NONE;
        pieceListFb[piece_id] = PS_NONE;
        piece_id_list[sq] = piece_id;
    }
  }

  // Convert the specified piece_id piece to ExtPieceSquare type and return it
  inline ExtPieceSquare piece_with_id(PieceId piece_id) const{
    ExtPieceSquare eps;
    eps.from[WHITE] = pieceListFw[piece_id];
    eps.from[BLACK] = pieceListFb[piece_id];
    return eps;
  }

  // Initialize the pieceList.
  // Set the value of unused pieces to PieceSquare::PS_NONE in case you want to deal with dropped pieces.
  // A normal evaluation function can be used as an evaluation function for missing frames.
  // piece_no_list is initialized with PieceId::PIECE_ID_NONE to facilitate debugging.
  inline void clear(){
      for (auto& p : pieceListFw) p = PieceSquare::PS_NONE;
      for (auto& p : pieceListFb) p = PieceSquare::PS_NONE;
      for (auto& v : piece_id_list) v = PieceId::PIECE_ID_NONE;
  }

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