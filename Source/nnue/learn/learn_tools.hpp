#pragma once

#ifdef WITH_DATA2BIN

#include <cstdint>
#include <string>
#include <vector>

namespace FromSF{

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
  PRNG(uint64_t seed) : s(seed) { assert(seed); }

  template<typename T> T rand() { return T(rand64()); }

  /// Special generator used to fast init magic numbers.
  /// Output values only have 1/8th of their bits set on average.
  template<typename T> T sparse_rand()
  { return T(rand64() & rand64() & rand64()); }
  // Returns a random number from 0 to n-1. (Not uniform distribution, but this is enough in reality)
  uint64_t rand(uint64_t n) { return rand<uint64_t>() % n; }

  // Return the random seed used internally.
  uint64_t get_seed() const { return s; }
};

// async version of PRNG
struct AsyncPRNG
{
  AsyncPRNG(uint64_t seed) : prng(seed) { assert(seed); }
  // [ASYNC] Extract one random number.
  template<typename T> T rand() {
    std::unique_lock<std::mutex> lk(mutex);
    return prng.rand<T>();
  }

  // [ASYNC] Returns a random number from 0 to n-1. (Not uniform distribution, but this is enough in reality)
  uint64_t rand(uint64_t n) {
    std::unique_lock<std::mutex> lk(mutex);
    return prng.rand(n);
  }

  // Return the random seed used internally.
  uint64_t get_seed() const { return prng.get_seed(); }

protected:
  std::mutex mutex;
  PRNG prng;
};

} // FromSF

struct PackedSfen { 
	uint8_t data[32]; 
}; 

struct PackedSfenValue{
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
	uint8_t padding;

	// 32 + 2 + 2 + 2 + 1 + 1 = 40bytes
};

bool convert_bin(const std::vector<std::string>& filenames, const std::string& output_file_name, 
                 const int ply_minimum, const int ply_maximum, const int interpolate_eval);

bool convert_bin_from_pgn_extract(const std::vector<std::string>& filenames, const std::string& output_file_name, 
                                  const bool pgn_eval_side_to_move);

bool convert_plain(const std::vector<std::string>& filenames, const std::string& output_file_name);

int set_from_packed_sfen(Position &p, PackedSfen& sfen , bool mirror);

#endif // WITH_DATA2BIN