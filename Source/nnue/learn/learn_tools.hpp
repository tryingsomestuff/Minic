#pragma once

#include "../../definition.hpp"

#if defined(WITH_DATA2BIN) or defined(WITH_LEARNER)

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// in order for Minic data generation and use to stay compatible with SF one, 
// I need to handle SF move encoding in the binary format ...
// So here is some SF extracted Move usage things

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

enum MoveType {
	NORMAL,
	PROMOTION = 1 << 14,
	ENPASSANT = 2 << 14,
	CASTLING  = 3 << 14
};

template<MoveType T>
constexpr MiniMove MakeMove(Square from, Square to, Piece prom = P_wn) {
	return MiniMove(T + ((prom - P_wn) << 12) + (from << 6) + to);
}

constexpr MiniMove MakeMoveStd(Square from, Square to) {
	return MiniMove((from << 6) + to);
}

constexpr Square from_sq(Move m) {
  return Square((m >> 6) & 0x3F);
}

constexpr Square to_sq(Move m) {
  return Square(m & 0x3F);
}

constexpr MoveType type_of(Move m) {
  return MoveType(m & (3 << 14));
}

constexpr Piece promotion_type(Move m) {
  return Piece(((m >> 12) & 3) + P_wn);
}

constexpr Square flip_rank(Square s) { return Square(s ^ Sq_a8); }

constexpr Square flip_file(Square s) { return Square(s ^ Sq_h1); }

} // FromSF

namespace Path{
	// Combine the path name and file name and return it.
	// If the folder name is not an empty string, append it if there is no'/' or'\\' at the end.
	inline std::string Combine(const std::string& folder, const std::string& filename)
	{
		if (folder.length() >= 1 && *folder.rbegin() != '/' && *folder.rbegin() != '\\')
			return folder + "/" + filename;

		return folder + filename;
	}

	// Get the file name part (excluding the folder name) from the full path expression.
	inline std::string GetFileName(const std::string& path)
	{
		// I don't know which "\" or "/" is used.
		auto path_index1 = path.find_last_of("\\") + 1;
		auto path_index2 = path.find_last_of("/") + 1;
		auto path_index = std::max(path_index1, path_index2);

		return path.substr(path_index);
	}
}

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

void sfen_pack(const Position & p, PackedSfen& sfen);

int set_from_packed_sfen(Position &p, PackedSfen& sfen , bool mirror);

MiniMove ToSFMove(const Position & p, Square from, Square to, MType type);

MiniMove FromSFMove(const Position & p, MiniMove sfmove);

namespace EvalLearningTools
{
	// -------------------------------------------------
	//   Array for learning that stores gradients etc.
	// -------------------------------------------------

#pragma pack(2)
	struct Weight
	{
		// cumulative value of one mini-batch gradient
		LearnFloatType g = LearnFloatType(0);

		// When ADA_GRAD_UPDATE. LearnFloatType == float,
		// total 4*2 + 4*2 + 1*2 = 18 bytes
		// It suffices to secure a Weight array that is 4.5 times the size of the evaluation function parameter of 1GB.
		// However, sizeof(Weight)==20 code is generated if the structure alignment is in 4-byte units, so
		// Specify pragma pack(2).

		// For SGD_UPDATE, this structure is reduced by 10 bytes to 8 bytes.

		// Learning rate η(eta) such as AdaGrad.
		// It is assumed that eta1,2,3,eta1_epoch,eta2_epoch have been set by the time updateFV() is called.
		// The epoch of update_weights() gradually changes from eta1 to eta2 until eta1_epoch.
		// After eta2_epoch, gradually change from eta2 to eta3.
		static double eta;
		static double eta1;
		static double eta2;
		static double eta3;
		static uint64_t eta1_epoch;
		static uint64_t eta2_epoch;

		// Batch initialization of eta. If 0 is passed, the default value will be set.
		static void init_eta(double eta1, double eta2, double eta3, uint64_t eta1_epoch, uint64_t eta2_epoch)
		{
			Weight::eta1 = (eta1 != 0) ? eta1 : 30.0;
			Weight::eta2 = (eta2 != 0) ? eta2 : 30.0;
			Weight::eta3 = (eta3 != 0) ? eta3 : 30.0;
			Weight::eta1_epoch = (eta1_epoch != 0) ? eta1_epoch : 0;
			Weight::eta2_epoch = (eta2_epoch != 0) ? eta2_epoch : 0;
		}

		// Set eta according to epoch.
		static void calc_eta(uint64_t epoch)
		{
			if (Weight::eta1_epoch == 0) // Exclude eta2
				Weight::eta = Weight::eta1;
			else if (epoch < Weight::eta1_epoch)
				// apportion
				Weight::eta = Weight::eta1 + (Weight::eta2 - Weight::eta1) * epoch / Weight::eta1_epoch;
			else if (Weight::eta2_epoch == 0) // Exclude eta3
				Weight::eta = Weight::eta2;
			else if (epoch < Weight::eta2_epoch)
				Weight::eta = Weight::eta2 + (Weight::eta3 - Weight::eta2) * (epoch - Weight::eta1_epoch) / (Weight::eta2_epoch - Weight::eta1_epoch);
			else
				Weight::eta = Weight::eta3;
		}

		template <typename T> void updateFV(T& v) { updateFV(v, 1.0); }

		// Since the maximum value that can be accurately calculated with float is INT16_MAX*256-1
		// Keep the small value as a marker.
		const LearnFloatType V0_NOT_INIT = (INT16_MAX * 128);

		// What holds v internally. The previous implementation kept a fixed decimal with only a fractional part to save memory,
		// Since it is doubtful in accuracy and the visibility is bad, it was abolished.
		LearnFloatType v0 = LearnFloatType(V0_NOT_INIT);

		// AdaGrad g2
		LearnFloatType g2 = LearnFloatType(0);

		// update with AdaGrad
		// When executing this function, the value of g and the member do not change
		// Guaranteed by the caller. It does not have to be an atomic operation.
		// k is a coefficient for eta. 1.0 is usually sufficient. If you want to lower eta for your turn item, set this to 1/8.0 etc.
		template <typename T>
		void updateFV(T& v,double k)
		{
			// AdaGrad update formula
			// Gradient vector is g, vector to be updated is v, η(eta) is a constant,
			//     g2 = g2 + g^2
			//     v = v - ηg/sqrt(g2)

			constexpr double epsilon = 0.000001;

			if (g == LearnFloatType(0))
				return;

			g2 += g * g;

			// If v0 is V0_NOT_INIT, it means that the value is not initialized with the value of KK/KKP/KPP array,
			// In this case, read the value of v from the one passed in the argument.
			double V = (v0 == V0_NOT_INIT) ? v : v0;

			V -= k * eta * (double)g / sqrt((double)g2 + epsilon);

			// Limit the value of V to be within the range of types.
			// By the way, windows.h defines the min and max macros, so to avoid it,
			// Here, it is enclosed in parentheses so that it is not treated as a function-like macro.
			V = (std::min)((double)(std::numeric_limits<T>::max)() , V);
			V = (std::max)((double)(std::numeric_limits<T>::min)() , V);

			v0 = (LearnFloatType)V;
			v = (T)round(V);

			// Clear g because one update of mini-batch for this element is over
			// g[i] = 0;
			// → There is a problem of dimension reduction, so this will be done by the caller.
		}

		// grad setting
		template <typename T> void set_grad(const T& g_) { g = g_; }

		// Add grad
		template <typename T> void add_grad(const T& g_) { g += g_; }

		LearnFloatType get_grad() const { return g; }
	};
#pragma pack(0)

	// Turned weight array
	// In order to be able to handle it transparently, let's have the same member as Weight.
	struct Weight2
	{
		Weight w[2];

		//Evaluate your turn, eta 1/8.
		template <typename T> void updateFV(std::array<T, 2>& v) { w[0].updateFV(v[0] , 1.0); w[1].updateFV(v[1],1.0/8.0); }

		template <typename T> void set_grad(const std::array<T, 2>& g) { for (int i = 0; i<2; ++i) w[i].set_grad(g[i]); }
		template <typename T> void add_grad(const std::array<T, 2>& g) { for (int i = 0; i<2; ++i) w[i].add_grad(g[i]); }

		std::array<LearnFloatType, 2> get_grad() const { return std::array<LearnFloatType, 2>{w[0].get_grad(), w[1].get_grad()}; }
	};

} // EvalLearningTools

namespace Dependency {
    int mkdir(std::string dir_name)
    {
        return mkdir(dir_name.c_str());
    }
}

#endif // WITH_DATA2BIN or WITH_LEARNER