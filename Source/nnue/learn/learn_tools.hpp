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
namespace Algo {
// Fisher-Yates
template<typename Rng, typename T> inline void shuffle(std::vector<T>& buf, Rng&& prng) {
   const auto size = buf.size();
   for (uint64_t i = 0; i < size; ++i) std::swap(buf[i], buf[prng.rand(size - i) + i]);
}
} // namespace Algo

inline uint64_t string_hash(const std::string& str) {
   uint64_t h = 525201411107845655ull;

   for (auto c : str) {
      h ^= static_cast<uint64_t>(c);
      h *= 0x5bd1e9955bd1e995ull;
      h ^= h >> 47;
   }

   return h;
}

/// xorshift64star Pseudo-Random Number Generator
/// This class is based on original code written and dedicated
/// to the public domain by Sebastiano Vigna (2014).
/// It has the following characteristics:
///
///  -  Outputs 64-bit numbers
///  -  Passes Dieharder and SmallCrush test batteries
///  -  Does not require warm-up, no zeroland to escape
///  -  Internal state is a single 64-bit integer
///  -  Period is 2^64 - 1
///  -  Speed: 1.60 ns/call (Core i7 @3.40GHz)
///
/// For further analysis see
///   <http://vigna.di.unimi.it/ftp/papers/xorshift.pdf>
class PRNG {
   uint64_t s;

   uint64_t rand64() {
      s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
      return s * 2685821657736338717LL;
   }

  public:
   PRNG() { set_seed_from_time(); }
   PRNG(uint64_t seed): s(seed) { assert(seed); }
   PRNG(const std::string& seed) { set_seed(seed); }

   template<typename T> T rand() { return T(rand64()); }

   /// Special generator used to fast init magic numbers.
   /// Output values only have 1/8th of their bits set on average.
   template<typename T> T sparse_rand() { return T(rand64() & rand64() & rand64()); }
   // Returns a random number from 0 to n-1. (Not uniform distribution, but this is enough in reality)
   uint64_t rand(uint64_t n) { return rand<uint64_t>() % n; }

   // Return the random seed used internally.
   uint64_t get_seed() const { return s; }

   void set_seed(uint64_t seed) { s = seed; }

   void set_seed_from_time() { set_seed(std::chrono::system_clock::now().time_since_epoch().count()); }

   void set_seed(const std::string& str) {
      if (str.empty()) { set_seed_from_time(); }
      else if (std::all_of(str.begin(), str.end(), [](char c) { return std::isdigit(c); })) {
         set_seed(std::stoull(str));
      }
      else {
         set_seed(string_hash(str));
      }
   }
};

enum MoveType { NORMAL, PROMOTION = 1 << 14, ENPASSANT = 2 << 14, CASTLING = 3 << 14 };

template<MoveType T> constexpr MiniMove MakeMove(Square from, Square to, Piece prom = P_wn) {
   return MiniMove(T + ((prom - P_wn) << 12) + (from << 6) + to);
}

constexpr MiniMove MakeMoveStd(Square from, Square to) { return MiniMove((from << 6) + to); }

constexpr Square from_sq(Move m) { return Square((m >> 6) & 0x3F); }

constexpr Square to_sq(Move m) { return Square(m & 0x3F); }

constexpr MoveType type_of(Move m) { return MoveType(m & (3 << 14)); }

constexpr Piece promotion_type(Move m) { return Piece(((m >> 12) & 3) + P_wn); }

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

MiniMove ToSFMove(const Position& p, Square from, Square to, MType type);

MiniMove FromSFMove(const Position& p, MiniMove sfmove);

#endif // WITH_DATA2BIN