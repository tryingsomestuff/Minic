#pragma once

#include "definition.hpp"

#if defined(WITH_DATA2BIN)

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct Position;

// For Minic data generation and use to stay compatible with SF one,
// I need to handle SF move encoding in the binary format ...
// So here is some SF extracted Move usage things
namespace FromSF {

enum MoveType { NORMAL, PROMOTION = 1 << 14, ENPASSANT = 2 << 14, CASTLING = 3 << 14 };

template<MoveType T> constexpr MiniMove MakeMove(const Square from, const Square to, const Piece prom = P_wn) {
   return static_cast<MiniMove>(T + ((prom - P_wn) << 12) + (from << 6) + to);
}

constexpr MiniMove MakeMoveStd(const Square from, const Square to) { return static_cast<MiniMove>((from << 6) + to); }

constexpr Square from_sq(const Move m) { return static_cast<Square>((m >> 6) & 0x3F); }

constexpr Square to_sq(const Move m) { return static_cast<Square>(m & 0x3F); }

constexpr MoveType type_of(const Move m) { return static_cast<MoveType>(m & (3 << 14)); }

constexpr Piece promotion_type(const Move m) { return static_cast<Piece>(((m >> 12) & 3) + P_wn); }

} // namespace FromSF

struct PackedSfen {
   uint8_t data[32];
};

struct PackedSfenValue {
   // phase
   PackedSfen sfen;

   // Evaluation value returned from Learner::search()
   int16_t score;

   // PV first move
   // Used when finding the match rate with the teacher
   uint16_t move;

   // Trouble of the phase from the initial phase.
   uint16_t gamePly;

   // 1 if the player on this side ultimately wins the game. -1 if you are losing.
   // 0 if a draw is reached.
   // The draw is in the teacher position generation command gensfen,
   // Only write if LEARN_GENSFEN_DRAW_RESULT is enabled.
   int8_t game_result;

   // When exchanging the file that wrote the teacher aspect with other people
   //Because this structure size is not fixed, pad it so that it is 40 bytes in any environment.
   uint8_t padding = 0;

   // 32 + 2 + 2 + 2 + 1 + 1 = 40bytes
};

void sfen_pack(const Position& p, PackedSfen& sfen);

int set_from_packed_sfen(Position& p, PackedSfen& sfen);

MiniMove ToSFMove(const Position& p, const Square from, const Square to, const MType type);

MiniMove FromSFMove(const Position& p, const MiniMove sfmove);

#endif // WITH_DATA2BIN