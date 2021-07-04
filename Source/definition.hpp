#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <cctype>
#include <chrono>
#include <climits>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <intrin.h>
#include <stdlib.h>
typedef uint64_t u_int64_t;
#else
#include <unistd.h>
#endif

const std::string MinicVersion = "3.09";

// *** options
#define WITH_UCI    // include or not UCI protocol support
#define WITH_XBOARD // include or not XBOARB protocol support
#define WITH_MAGIC  // use magic bitboard or HB ones
#define WITH_SYZYGY // include or not syzgy ETG support
#define WITH_NNUE   // include or not NNUE support
#define WITH_STATS  // produce or not search statitics
//#define WITH_MPI    // support "distributed" version or not

//#define WITHOUT_FILESYSTEM              // some compiler don't support whole std:filesystem 
//#define LIMIT_THREADS_TO_PHYSICAL_CORES // in order to restrict thread to the number of physical core
//#define REPRODUCTIBLE_RESULTS           // clear state table betwwen all new search (not all new games)
//#define WITH_NNUE_CLIPPED_RELU          // use clipped relu instead of relu for NNUE
#ifndef __ANDROID__
#define USE_AVX_INTRIN                    // on AVX2 architecture, use a hand written dot product (a little faster)
//#define WITH_BLAS                       // link on a given BLAS library for cblas_sgemv operations
#endif

// *** Optim (?)
#define USE_PARTIAL_SORT        // do not sort every move in move list
//#define WITH_EVALSCORE_AS_INT // in fact just as slow as my basic impl ...

// *** Add-ons
#ifndef DEBUG_TOOL
#define DEBUG_TOOL // forced
#endif
#define WITH_TEST_SUITE
//#define WITH_PGN_PARSER

// *** NNUE learning things
#ifndef WITHOUT_FILESYSTEM
#define WITH_GENFILE
#endif

#ifdef WITH_NNUE
#ifndef __ANDROID__
#ifndef WITHOUT_FILESYSTEM
#define WITH_DATA2BIN
#endif
#endif
#endif

// *** Tuning
//#define WITH_TIMER
//#define WITH_SEARCH_TUNING
//#define WITH_TEXEL_TUNING
//#define WITH_PIECE_TUNING

// *** Debug
//#define VERBOSE_EVAL
//#define DEBUG_NNUE_UPDATE
//#define DEBUG_BACKTRACE
//#define DEBUG_HASH
//#define DEBUG_PHASH
//#define DEBUG_MATERIAL
//#define DEBUG_APPLY
//#define DEBUG_GENERATION 
//#define DEBUG_GENERATION_LEGAL // not compatible with DEBUG_PSEUDO_LEGAL
//#define DEBUG_BITBOARD
//#define DEBUG_PSEUDO_LEGAL // not compatible with DEBUG_GENERATION_LEGAL
//#define DEBUG_HASH_ENTRY
//#define DEBUG_KING_CAP
//#define DEBUG_PERFT
//#define DEBUG_STATICEVAL
//#define DEBUG_EVALSYM

#ifdef WITH_TEXEL_TUNING
#define CONST_TEXEL_TUNING
#undef WITH_EVALSCORE_AS_INT
#else
#define CONST_TEXEL_TUNING const
#endif

#ifdef WITH_SEARCH_TUNING
#define CONST_CLOP_TUNING
#else
#define CONST_CLOP_TUNING const
#endif

#ifdef WITH_PIECE_TUNING
#define CONST_PIECE_TUNING
#else
#define CONST_PIECE_TUNING const
#endif

#define INFINITETIME    TimeType(60ull * 60ull * 1000ull * 24ull * 30ull) // 1 month ...
#define STOPSCORE       ScoreType(-20000)
#define INFSCORE        ScoreType(15000)
#define MATE            ScoreType(10000)
#define WIN             ScoreType(3000)
#define INVALIDMOVE     int32_t(0xFFFF0002)
#define INVALIDMINIMOVE int16_t(0x0002)
#define NULLMOVE        int16_t(0x1112)
#define INVALIDSQUARE   -1
#define MAX_PLY         1024
#define MAX_MOVE        256 // 256 is enough I guess/hope ...
#define MAX_DEPTH       127 // if DepthType is a char, !!!do not go above 127!!!
#define HISTORY_POWER   10
#define HISTORY_MAX     (1 << HISTORY_POWER)
#define HISTORY_DIV(x)  ((x) >> HISTORY_POWER)
#define SQR(x)          ((x) * (x))
#define HSCORE(depth)   ScoreType(SQR(std::min((int)depth, 32)) * 4)
#define MAX_THREADS     256

#define SQFILE(s)              ((s)&7)
#define SQRANK(s)              ((s) >> 3)
#define ISOUTERFILE(x)         (SQFILE(x) == 0 || SQFILE(x) == 7)
#define ISNEIGHBOUR(x, y)      ((x) >= 0 && (x) < 64 && (y) >= 0 && (y) < 64 && Abs(SQRANK(x) - SQRANK(y)) <= 1 && Abs(SQFILE(x) - SQFILE(y)) <= 1)
#define PROMOTION_RANK(x)      (SQRANK(x) == 0 || SQRANK(x) == 7)
#define PROMOTION_RANK_C(x, c) ((c == Co_Black && SQRANK(x) == 0) || (c == Co_White && SQRANK(x) == 7))
#define MakeSquare(f, r)       Square(((r) << 3) + (f))
#define VFlip(s)               ((s) ^ Sq_a8)
#define HFlip(s)               ((s) ^ 7)
#define MFlip(s)               ((s) ^ Sq_h8)

#define TO_STR2(x)                 #x
#define TO_STR(x)                  TO_STR2(x)
#define LINE_NAME(prefix, suffix)  JOIN(JOIN(prefix, __LINE__), suffix)
#define JOIN(symbol1, symbol2)     _DO_JOIN(symbol1, symbol2)
#define _DO_JOIN(symbol1, symbol2) symbol1##symbol2

#define EXTENDMORE(extension) (!extension)

typedef std::chrono::system_clock Clock;
typedef int8_t   DepthType;
typedef int32_t  Move;         // invalid if < 0
typedef int16_t  MiniMove;     // invalid if < 0
typedef int8_t   Square;       // invalid if < 0
typedef uint64_t Hash;         // invalid if == nullHash
typedef uint32_t MiniHash;     // invalid if == 0
typedef uint64_t Counter;
typedef uint64_t BitBoard;
typedef int16_t  ScoreType;
typedef int64_t  TimeType;
typedef uint8_t  GenerationType;

const Hash     nullHash      = 0ull; //std::numeric_limits<MiniHash>::max(); // use MiniHash to allow same "null" value for Hash(64) and MiniHash(32)
const BitBoard emptyBitBoard = 0ull;

template<typename T> constexpr ScoreType clampScore(T s) { return (ScoreType)std::clamp(s, (T)(-MATE + 2 * MAX_DEPTH), (T)(MATE - 2 * MAX_DEPTH)); }

enum GamePhase { MG = 0, EG = 1, GP_MAX = 2 };
inline constexpr GamePhase operator++(GamePhase& g) {
   g = GamePhase(g + 1);
   return g;
}

template<typename T> [[nodiscard]] inline constexpr T Abs(const T& s) { return s > 0 ? s : -s; }

template<typename T, int SIZE> struct OptList : public std::vector<T> {
   OptList(): std::vector<T>() { std::vector<T>::reserve(SIZE); }
};
typedef OptList<Move, MAX_MOVE / 4> MoveList;
typedef std::vector<Move>           PVList;

[[nodiscard]] inline constexpr MiniHash Hash64to32(Hash h) { return (h >> 32) & 0xFFFFFFFF; }
[[nodiscard]] inline constexpr MiniMove Move2MiniMove(Move m) { return m & 0xFFFF; } // skip score

// sameMove is not comparing score part of the Move, only the MiniMove part !
[[nodiscard]] inline constexpr bool sameMove(const Move& a, const Move& b) { return Move2MiniMove(a) == Move2MiniMove(b); }
[[nodiscard]] inline constexpr bool sameMove(const Move& a, const MiniMove& b) { return Move2MiniMove(a) == b; }

[[nodiscard]] inline constexpr bool VALIDMOVE(const Move& m) { return !sameMove(m, NULLMOVE) && !sameMove(m, INVALIDMOVE); }
[[nodiscard]] inline constexpr bool VALIDMOVE(const MiniMove& m) { return m != INVALIDMINIMOVE && m != NULLMOVE; }

const std::string startPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const std::string fine70        = "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1";
const std::string shirov        = "6r1/2rp1kpp/2qQp3/p3Pp1P/1pP2P2/1P2KP2/P5R1/6R1 w - - 0 1";

enum Piece : signed char {
   P_bk   = -6,
   P_bq   = -5,
   P_br   = -4,
   P_bb   = -3,
   P_bn   = -2,
   P_bp   = -1,
   P_none = 0,
   P_wp   = 1,
   P_wn   = 2,
   P_wb   = 3,
   P_wr   = 4,
   P_wq   = 5,
   P_wk   = 6
};
inline constexpr Piece operator++(Piece& pp) {
   pp = Piece(pp + 1);
   return pp;
}
constexpr Piece operator~(Piece pp) { return Piece(-pp); } // switch piece Color
const int       PieceShift = 6;
const int       NbPiece    = 2 * PieceShift + 1;

[[nodiscard]] constexpr int PieceIdx(Piece p) { return p + PieceShift; } ///@todo use it everywhere !

enum Mat : uint8_t { M_t = 0, M_p, M_n, M_b, M_r, M_q, M_k, M_bl, M_bd, M_M, M_m };
inline constexpr Mat operator++(Mat& m) {
   m = Mat(m + 1);
   return m;
}

extern CONST_PIECE_TUNING ScoreType Values[NbPiece];
extern CONST_PIECE_TUNING ScoreType ValuesEG[NbPiece];

#ifdef WITH_PIECE_TUNING
inline void SymetrizeValue() {
   for (Piece pp = P_wp; pp <= P_wk; ++pp) {
      Values[-pp + PieceShift]   = Values[pp + PieceShift];
      ValuesEG[-pp + PieceShift] = ValuesEG[pp + PieceShift];
    }
}
#endif

const ScoreType        dummyScore     = 0;
const ScoreType* const absValues_[7]   = {&dummyScore,
                                          &Values[P_wp + PieceShift],
                                          &Values[P_wn + PieceShift],
                                          &Values[P_wb + PieceShift],
                                          &Values[P_wr + PieceShift],
                                          &Values[P_wq + PieceShift],
                                          &Values[P_wk + PieceShift]};
const ScoreType* const absValuesEG_[7] = {&dummyScore,
                                          &ValuesEG[P_wp + PieceShift],
                                          &ValuesEG[P_wn + PieceShift],
                                          &ValuesEG[P_wb + PieceShift],
                                          &ValuesEG[P_wr + PieceShift],
                                          &ValuesEG[P_wq + PieceShift],
                                          &ValuesEG[P_wk + PieceShift]};

inline ScoreType value(Piece pp)      { return Values[pp + PieceShift]; }
inline ScoreType absValue(Piece pp)   { return *absValues_[pp]; }
inline ScoreType absValueEG(Piece pp) { return *absValuesEG_[pp]; }

template<typename T> [[nodiscard]] inline constexpr int sgn(T val) { return (T(0) < val) - (val < T(0)); }

const std::string PieceNames[NbPiece] = {"k", "q", "r", "b", "n", "p", " ", "P", "N", "B", "R", "Q", "K"};

constexpr Square NbSquare = 64;

const std::string SquareNames[NbSquare]   = { "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
                                              "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
                                              "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
                                              "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
                                              "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
                                              "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
                                              "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
                                              "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8" };

const std::string FileNames[8] = {"a", "b", "c", "d", "e", "f", "g", "h"};
const std::string RankNames[8] = {"1", "2", "3", "4", "5", "6", "7", "8"};

enum Sq : uint8_t { Sq_a1  = 0,Sq_b1,Sq_c1,Sq_d1,Sq_e1,Sq_f1,Sq_g1,Sq_h1,
                    Sq_a2,Sq_b2,Sq_c2,Sq_d2,Sq_e2,Sq_f2,Sq_g2,Sq_h2,
                    Sq_a3,Sq_b3,Sq_c3,Sq_d3,Sq_e3,Sq_f3,Sq_g3,Sq_h3,
                    Sq_a4,Sq_b4,Sq_c4,Sq_d4,Sq_e4,Sq_f4,Sq_g4,Sq_h4,
                    Sq_a5,Sq_b5,Sq_c5,Sq_d5,Sq_e5,Sq_f5,Sq_g5,Sq_h5,
                    Sq_a6,Sq_b6,Sq_c6,Sq_d6,Sq_e6,Sq_f6,Sq_g6,Sq_h6,
                    Sq_a7,Sq_b7,Sq_c7,Sq_d7,Sq_e7,Sq_f7,Sq_g7,Sq_h7,
                    Sq_a8,Sq_b8,Sq_c8,Sq_d8,Sq_e8,Sq_f8,Sq_g8,Sq_h8};

enum File : uint8_t { File_a = 0, File_b, File_c, File_d, File_e, File_f, File_g, File_h };
inline constexpr File operator++(File& f) {
   f = File(f + 1);
   return f;
}
inline constexpr File operator--(File& f) {
   f = File(f - 1);
   return f;
}

enum Rank : uint8_t { Rank_1 = 0, Rank_2, Rank_3, Rank_4, Rank_5, Rank_6, Rank_7, Rank_8 };
inline constexpr Rank operator++(Rank& r) {
   r = Rank(r + 1);
   return r;
}
inline constexpr Rank operator--(Rank& r) {
   r = Rank(r - 1);
   return r;
}

constexpr Rank PromRank[2] = {Rank_8, Rank_1};
constexpr Rank EPRank[2]   = {Rank_6, Rank_3};

enum CastlingTypes : uint8_t { CT_OOO = 0, CT_OO = 1 };
enum CastlingRights : uint8_t {
   C_none        = 0,
   C_wks         = 1,
   C_wqs         = 2,
   C_w_all       = 3,
   C_all_but_b   = 3,
   C_bks         = 4,
   C_all_but_bqs = 7, 
   C_bqs         = 8,
   C_all_but_bks = 11,
   C_b_all       = 12,
   C_all_but_w   = 12,
   C_all_but_wqs = 13,
   C_all_but_wks = 14,
   C_all         = 15
};
inline constexpr CastlingRights operator&(const CastlingRights& a, const CastlingRights& b) { return CastlingRights(char(a) & char(b)); }
inline constexpr CastlingRights operator|(const CastlingRights& a, const CastlingRights& b) { return CastlingRights(char(a) | char(b)); }
inline constexpr CastlingRights operator~(const CastlingRights& a) { return CastlingRights(~char(a)); }
inline constexpr void           operator&=(CastlingRights& a, const CastlingRights& b) { a = a & b; }
inline constexpr void           operator|=(CastlingRights& a, const CastlingRights& b) { a = a | b; }

[[nodiscard]] inline Square stringToSquare(const std::string& str) { return (str.at(1) - 49) * 8 + (str.at(0) - 97); }

enum MType : uint8_t {
   T_std      = 0,
   T_capture  = 1,
   T_reserved = 2,
   T_ep       = 3, // binary    0 to   11
   T_promq    = 4,
   T_promr    = 5,
   T_promb    = 6,
   T_promn    = 7, // binary  100 to  111
   T_cappromq = 8,
   T_cappromr = 9,
   T_cappromb = 10,
   T_cappromn = 11, // binary 1000 to 1011
   T_wks      = 12,
   T_wqs      = 13,
   T_bks      = 14,
   T_bqs      = 15 // binary 1100 to 1111
};

[[nodiscard]] inline constexpr bool moveTypeOK(MType m) { return m <= T_bqs; }
[[nodiscard]] inline constexpr bool squareOK(Square s) { return s >= 0 && s < NbSquare; }
[[nodiscard]] inline constexpr bool pieceOK(Piece pp) { return pp >= P_bk && pp <= P_wk; }
[[nodiscard]] inline constexpr bool pieceValid(Piece pp) { return pieceOK(pp) && pp != P_none; }

[[nodiscard]] inline constexpr Piece promShift(MType mt) {
   assert(mt >= T_promq);
   assert(mt <= T_cappromn);
   return Piece(P_wq - (mt % 4));
} // awfull hack

enum Color : int8_t { Co_None = -1, Co_White = 0, Co_Black = 1, Co_End };
[[nodiscard]] constexpr Color operator~(Color c) { return Color(c ^ Co_Black); } // switch Color
inline constexpr Color        operator++(Color& c) {
   c = Color(c + 1);
   return c;
}

// previous best root 20000, ttmove 15000, king evasion 10000
// promcap >7000, cap 7000, (checks 6000,)
// killers 1900-1700-1500, counter 1300, castling 200, other by -1000 < history < 1000, bad cap <-7000,
// leaving threat 1000
// MVV-LVA [0 400]
// recapture 400
constexpr ScoreType MoveScoring[16] = {0, 7000, 7100, 6000, 3950, 3500, 3350, 3300, 7950, 7500, 7350, 7300, 200, 200, 200, 200};

[[nodiscard]] inline bool isSkipMove(const Move& a, const std::vector<MiniMove>* skipMoves) {
   return skipMoves && std::find(skipMoves->begin(), skipMoves->end(), Move2MiniMove(a)) != skipMoves->end();
}

[[nodiscard]] inline constexpr ScoreType Move2Score(Move m) {
   assert(VALIDMOVE(m));
   return (m >> 16) & 0xFFFF;
}
[[nodiscard]] inline constexpr Square Move2From(Move m) {
   assert(VALIDMOVE(m));
   return (m >> 10) & 0x3F;
}
[[nodiscard]] inline constexpr Square Move2To(Move m) {
   assert(VALIDMOVE(m));
   return (m >> 4) & 0x3F;
}
[[nodiscard]] inline constexpr MType Move2Type(Move m) {
   assert(VALIDMOVE(m));
   return MType(m & 0xF);
}
[[nodiscard]] inline constexpr MiniMove ToMove(Square from, Square to, MType type) {
   assert(from >= 0 && from < 64);
   assert(to >= 0 && to < 64);
   return (from << 10) | (to << 4) | type;
}
[[nodiscard]] inline constexpr Move ToMove(Square from, Square to, MType type, ScoreType score) {
   assert(from >= 0 && from < 64);
   assert(to >= 0 && to < 64);
   return (score << 16) | (from << 10) | (to << 4) | type;
}

[[nodiscard]] inline constexpr bool isMatingScore(ScoreType s) { return (s >= MATE - MAX_DEPTH); }
[[nodiscard]] inline constexpr bool isMatedScore(ScoreType s) { return (s <= -MATE + MAX_DEPTH); }
[[nodiscard]] inline constexpr bool isMateScore(ScoreType s) { return (Abs(s) >= MATE - MAX_DEPTH); }

[[nodiscard]] inline constexpr bool isPromotionStd(const MType mt) {
   assert(moveTypeOK(mt));
   return (mt >> 2 == 0x1);
}
[[nodiscard]] inline constexpr bool isPromotionStd(const Move m) { return isPromotionStd(Move2Type(m)); }
[[nodiscard]] inline constexpr bool isPromotionCap(const MType mt) {
   assert(moveTypeOK(mt));
   return (mt >> 2 == 0x2);
}
[[nodiscard]] inline constexpr bool isPromotionCap(const Move m) { return isPromotionCap(Move2Type(m)); }
[[nodiscard]] inline constexpr bool isPromotion(const MType mt) {
   assert(moveTypeOK(mt));
   return isPromotionStd(mt) || isPromotionCap(mt);
}
[[nodiscard]] inline constexpr bool isPromotion(const Move m) { return isPromotion(Move2Type(m)); }
[[nodiscard]] inline constexpr bool isCastling(const MType mt) {
   assert(moveTypeOK(mt));
   return (mt >> 2) == 0x3;
}
[[nodiscard]] inline constexpr bool isCastling(const Move m) { return isCastling(Move2Type(m)); }
[[nodiscard]] inline constexpr bool isCapture(const MType mt) {
   assert(moveTypeOK(mt));
   return mt == T_capture || mt == T_ep || isPromotionCap(mt);
}
[[nodiscard]] inline constexpr bool isCapture(const Move m) { return isCapture(Move2Type(m)); }

[[nodiscard]] inline constexpr bool isCaptureOrProm(const MType mt) {
   assert(moveTypeOK(mt));
   return mt == T_capture || mt == T_ep || isPromotion(mt);
}
[[nodiscard]] inline constexpr bool isCaptureOrProm(const Move m) { return isCaptureOrProm(Move2Type(m)); }

[[nodiscard]] inline constexpr bool      isBadCap(const Move m) { return Move2Score(m) < -MoveScoring[T_capture] + 800; }
[[nodiscard]] inline constexpr ScoreType badCapScore(const Move m) { return Move2Score(m) + MoveScoring[T_capture]; }

[[nodiscard]] inline constexpr Square chebyshevDistance(Square sq1, Square sq2) {
   return std::max(Abs(SQRANK(sq2) - SQRANK(sq1)), Abs(SQFILE(sq2) - SQFILE(sq1)));
}
[[nodiscard]] inline constexpr Square manatthanDistance(Square sq1, Square sq2) {
   return Abs(SQRANK(sq2) - SQRANK(sq1)) + Abs(SQFILE(sq2) - SQFILE(sq1));
}
[[nodiscard]] inline constexpr Square minDistance(Square sq1, Square sq2) {
   return std::min(Abs(SQRANK(sq2) - SQRANK(sq1)), Abs(SQFILE(sq2) - SQFILE(sq1)));
}

namespace MoveDifficultyUtil {
    enum MoveDifficulty { MD_forced = 0, MD_easy, MD_std, MD_hardDefense, MD_hardAttack };
    const DepthType emergencyMinDepth         = 14;
    const ScoreType emergencyMargin           = 90;
    const ScoreType emergencyAttackThreashold = 150;
    const ScoreType easyMoveMargin            = 180;
    const int       emergencyFactor           = 5;
    const float     maxStealFraction          = 0.2f; // of remaining time

extern float variability;
///@todo optimize for speed
[[nodiscard]] inline float variabilityFactor() { return 2 / (1 + exp(1 - MoveDifficultyUtil::variability)); } // inside [0.5 .. 2]
} // namespace MoveDifficultyUtil

inline void updatePV(PVList& pv, const Move& m, const PVList& childPV) {
    pv.clear();
    pv.push_back(m);
    std::copy(childPV.begin(), childPV.end(), std::back_inserter(pv));
}

template<typename T, int seed> [[nodiscard]] inline T randomInt(T m, T M) {
   static std::mt19937 mt(seed); // fixed seed for ZHash for instance !!!
   static std::uniform_int_distribution<T> dist(m, M);
    return dist(mt);
}

template<typename T> [[nodiscard]] inline T randomInt(T m, T M) {
   static std::random_device r;
   static std::seed_seq seed {r(), r(), r(), r(), r(), r(), r(), r()};
   static std::mt19937 mt(seed);
   static std::uniform_int_distribution<T> dist(m, M);
    return dist(mt);
}

template<class T> [[nodiscard]] inline constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
   assert(!(hi < lo));
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

[[nodiscard]] constexpr Square relative_square(Color c, Square s) { return Square(s ^ (c * 56)); }

template<Color C> [[nodiscard]] inline constexpr Square ColorSquarePstHelper(Square k) { return relative_square(~C, k); }

[[nodiscard]] inline constexpr uint64_t powerFloor(uint64_t x) {
    uint64_t power = 1;
    while (power <= x) power *= 2;
   return power / 2;
}

inline void* std_aligned_alloc(size_t alignment, size_t size) {
#ifdef __ANDROID__
  return malloc(size);
#elif defined(__EMSCRIPTEN__)
  return malloc(size);
#elif defined(__APPLE__)
  return aligned_alloc(alignment, size);
#elif defined(_WIN32)
  return _mm_malloc(size, alignment);
#else
   void* ret = std::aligned_alloc(alignment, size);
#ifdef MADV_HUGEPAGE
  madvise(ret, size, MADV_HUGEPAGE);
#endif
  return ret;
#endif
}

inline void std_aligned_free(void* ptr) {
#ifdef __ANDROID__
  return free(ptr);
#elif defined(__EMSCRIPTEN__)
  return free(ptr);
#elif defined(__APPLE__)
  free(ptr);
#elif defined(_WIN32)
  _mm_free(ptr);
#else
  free(ptr);
#endif
}

inline std::string toHexString(uint32_t i) {
  std::stringstream s;
  s << "0x" << std::hex << i;
  return s.str();
}
