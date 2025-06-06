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
#include <cstdint>
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
#include <optional>
#include <random>
#include <regex>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
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

#include "config.hpp"

#ifdef WITH_FMTLIB
#include <fmt/color.h>
#endif

#ifdef WITH_EVAL_TUNING
#define CONST_EVAL_TUNING
#undef WITH_EVALSCORE_AS_INT
#else
#define CONST_EVAL_TUNING const
#endif

#ifdef WITH_SEARCH_TUNING
#define CONST_SEARCH_TUNING
#define CONSTEXPR_SEARCH_TUNING
#else
#define CONST_SEARCH_TUNING const
#define CONSTEXPR_SEARCH_TUNING constexpr
#endif

#ifdef WITH_PIECE_TUNING
#define CONST_PIECE_TUNING
#else
#define CONST_PIECE_TUNING const
#endif

inline constexpr size_t NNUEALIGNMENT = 64; // AVX512 compatible ...
inline constexpr auto NNUEALIGNMENT_STD = std::align_val_t{ NNUEALIGNMENT };

#ifdef __clang__
#define CONSTEXPR
#else
#define CONSTEXPR constexpr
#endif

#if _WIN32 || _WIN64
   #if _WIN64
     #define ENV64BIT
  #else
    #define ENV32BIT
  #endif
#endif

#if __GNUC__
  #if __x86_64__ || __ppc64__
    #define ENV64BIT
  #else
    #define ENV32BIT
  #endif
#endif

#ifdef _MSC_VER
#pragma warning(disable:4996)
#pragma warning(disable:4068)
#endif

#if !defined(__PRETTY_FUNCTION__) && !defined(__GNUC__)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#if defined(WITH_SMALL_MEMORY)
constexpr size_t SIZE_MULTIPLIER = 1024ull; // Kb
#else
constexpr size_t SIZE_MULTIPLIER = 1024ull * 1024ull; // Mb
#endif

#if defined(_MSC_VER)
#define FORCE_INLINE __inline
#elif defined(__GNUC__)
#if defined(__STRICT_ANSI__)
#define FORCE_INLINE __inline__
#else
#define FORCE_INLINE inline
#endif
#else
#define FORCE_INLINE
#endif

#ifdef _MSC_VER
#define FORCE_FINLINE __forceinline
#define RESTRICT __restrict
#elif defined(__GNUC__)
#define FORCE_FINLINE FORCE_INLINE __attribute__((always_inline))
#define RESTRICT __restrict__
#else
#define FORCE_FINLINE 
#endif

template<typename T> [[nodiscard]] constexpr T Abs(const T& s) { 
   static_assert(T(-1) < T(0));
   if constexpr (std::is_enum_v<T>){
      assert(s != std::numeric_limits<std::underlying_type_t<T>>::min());
   }
   else{
      assert(s != std::numeric_limits<T>::min());
   }
   const T ret = s >= T(0) ? s : T(-s);
   assert(ret == std::abs(s));
   return ret;
}

#define ENABLE_INCR_OPERATORS_ON(T) \
constexpr T& operator++(T& d) { return d = static_cast<T>(static_cast<std::underlying_type_t<T>>(d) + 1); } \
constexpr T& operator--(T& d) { return d = static_cast<T>(static_cast<std::underlying_type_t<T>>(d) - 1); }

using Clock          = std::chrono::system_clock;
using DepthType      = int8_t;
using Move           = int32_t;  // invalid if < 0
using MiniMove       = int16_t;  // invalid if < 0
using Square         = int8_t;   // invalid if < 0, MUST be signed because also used as negative delta sometimes (like in computeAttacks)
using Hash           = uint64_t; // invalid if == nullHash
using MiniHash       = uint32_t; // invalid if == 0
using Counter        = uint64_t;
using BitBoard       = uint64_t;
using ScoreType      = int16_t;
using TimeType       = int64_t;
using GenerationType = uint8_t;

enum File : uint8_t { File_a = 0, File_b, File_c, File_d, File_e, File_f, File_g, File_h };
ENABLE_INCR_OPERATORS_ON(File)

enum Rank : uint8_t { Rank_1 = 0, Rank_2, Rank_3, Rank_4, Rank_5, Rank_6, Rank_7, Rank_8 };
ENABLE_INCR_OPERATORS_ON(Rank)

enum Sq : uint8_t { Sq_a1  = 0,Sq_b1,Sq_c1,Sq_d1,Sq_e1,Sq_f1,Sq_g1,Sq_h1,
                    Sq_a2,Sq_b2,Sq_c2,Sq_d2,Sq_e2,Sq_f2,Sq_g2,Sq_h2,
                    Sq_a3,Sq_b3,Sq_c3,Sq_d3,Sq_e3,Sq_f3,Sq_g3,Sq_h3,
                    Sq_a4,Sq_b4,Sq_c4,Sq_d4,Sq_e4,Sq_f4,Sq_g4,Sq_h4,
                    Sq_a5,Sq_b5,Sq_c5,Sq_d5,Sq_e5,Sq_f5,Sq_g5,Sq_h5,
                    Sq_a6,Sq_b6,Sq_c6,Sq_d6,Sq_e6,Sq_f6,Sq_g6,Sq_h6,
                    Sq_a7,Sq_b7,Sq_c7,Sq_d7,Sq_e7,Sq_f7,Sq_g7,Sq_h7,
                    Sq_a8,Sq_b8,Sq_c8,Sq_d8,Sq_e8,Sq_f8,Sq_g8,Sq_h8};

template<typename T, int N>
using array1d = std::array<T,N>;

template<typename T, int N, int M>
using array2d = array1d<array1d<T,M>,N>;

template<typename T, int N, int M, int L>
using array3d = array1d<array1d<array1d<T,L>,M>,N>;

template<typename T>
using colored = array1d<T,2>;

inline constexpr array1d<Rank,2> PromRank = {Rank_8, Rank_1};
inline constexpr array1d<Rank,2> EPRank   = {Rank_6, Rank_3};

inline constexpr Hash nullHash          = 0ull; //std::numeric_limits<MiniHash>::max(); // use MiniHash to allow same "null" value for Hash(64) and MiniHash(32)
inline constexpr BitBoard emptyBitBoard = 0ull;
inline constexpr auto INFINITETIME      = static_cast<TimeType>(60ull * 60ull * 1000ull * 24ull * 30ull); // 1 month ...
inline constexpr auto STOPSCORE         = static_cast<ScoreType>(-20000);
inline constexpr auto INFSCORE          = static_cast<ScoreType>(15000);
inline constexpr auto MATE              = static_cast<ScoreType>(10000);
inline constexpr auto WIN               = static_cast<ScoreType>(3000);
inline constexpr auto INVALIDMOVE       = static_cast<int32_t>(0xFFFF0002);
inline constexpr auto INVALIDMINIMOVE   = static_cast<int16_t>(0x0002);
inline constexpr auto NULLMOVE          = static_cast<int16_t>(0x1112);

inline constexpr Square INVALIDSQUARE   = -1;
inline constexpr uint16_t MAX_PLY       = 2048; // same type as Position::halfmoves
inline constexpr int MAX_MOVE           = 256; // 256 is enough I guess/hope ...
inline constexpr auto MAX_DEPTH         = static_cast<DepthType>(127); // if DepthType is a char, !!!do not go above 127!!!
inline constexpr auto HISTORY_POWER     = 10;
inline constexpr ScoreType HISTORY_MAX  = (1 << HISTORY_POWER);
inline constexpr size_t MAX_THREADS     = 1024; // same type as Searcher::id()

constexpr ScoreType HISTORY_DIV(const int x) {
   const int ret = x >> HISTORY_POWER;
   assert (Abs(ret) < std::numeric_limits<ScoreType>::max());
   return ret;
}
constexpr ScoreType SQR(const ScoreType x)        { return x * x;}
constexpr ScoreType HSCORE(const DepthType depth) {
   const int ret = SQR(std::min(static_cast<int>(depth), 32)) * 4;
   assert (ret < std::numeric_limits<ScoreType>::max());
   return static_cast<ScoreType>(ret);}

enum Color : uint8_t { Co_White = 0, Co_Black = 1, Co_None = 2, Co_End = Co_None };
[[nodiscard]] constexpr Color operator~(Color c) { return static_cast<Color>(c ^ Co_Black); } // switch Color
ENABLE_INCR_OPERATORS_ON(Color)

constexpr auto SQFILE(const Square s)                          { return static_cast<File>(s & 7);}
constexpr auto SQRANK(const Square s)                          { return static_cast<Rank>(s >> 3);}
constexpr auto ISOUTERFILE(const Square x)                     { return (SQFILE(x) == 0 || SQFILE(x) == 7);}
constexpr auto ISNEIGHBOUR(const Square x, const Square y)     { return (x >= 0 && x < 64 && y >= 0 && y < 64 && Abs(SQRANK(x) - SQRANK(y)) <= 1 && Abs(SQFILE(x) - SQFILE(y)) <= 1);}
constexpr auto PROMOTION_RANK(const Square x)                  { return (SQRANK(x) == 0 || SQRANK(x) == 7);}
constexpr auto PROMOTION_RANK_C(const Square x, const Square c){ return ((c == Co_Black && SQRANK(x) == 0) || (c == Co_White && SQRANK(x) == 7));}
constexpr auto MakeSquare(const File f, const Rank r)          { return static_cast<Square>((r << 3) + f);}
constexpr auto VFlip(const Square s)                           { return static_cast<Square>(s ^ Sq_a8);}
constexpr auto HFlip(const Square s)                           { return static_cast<Square>(s ^ Sq_h1);}
constexpr auto MFlip(const Square s)                           { return static_cast<Square>(s ^ Sq_h8);}

#define TO_STR2(x)                 #x
#define TO_STR(x)                  TO_STR2(x)
#define LINE_NAME(prefix, suffix)  JOIN(JOIN(prefix, __LINE__), suffix)
#define JOIN(symbol1, symbol2)     _DO_JOIN(symbol1, symbol2)
#define _DO_JOIN(symbol1, symbol2) symbol1##symbol2

#define DISCARD [[maybe_unused]] const auto LINE_NAME(_tmp,_) =

#ifdef _WIN32
#define GETPID _getpid
#else
#define GETPID ::getpid
#endif

template<typename T>
[[nodiscard]] FORCE_FINLINE
T asLeastOne(const T &t){
   return std::max(static_cast<std::remove_const_t<decltype(t)>>(1), t);
}

[[nodiscard]] FORCE_FINLINE
TimeType getTimeDiff(const Clock::time_point & reference){
   const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - reference).count();
   return static_cast<TimeType>(asLeastOne(diff));
}

[[nodiscard]] FORCE_FINLINE
TimeType getTimeDiff(const Clock::time_point & current, const Clock::time_point & reference){
   const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(current - reference).count();
   return static_cast<TimeType>(asLeastOne(diff));
}

[[nodiscard]] FORCE_FINLINE double msec2sec(TimeType periodInMsec){
   using fpMilliseconds = std::chrono::duration<float, std::chrono::milliseconds::period>;
   using fpSeconds = std::chrono::duration<float, std::chrono::seconds::period>;
   return fpSeconds(fpMilliseconds(periodInMsec)).count();
}

template<typename OT, typename IT>
[[nodiscard]] FORCE_FINLINE
OT clampInt(IT t){
   return static_cast<OT>(std::min(std::numeric_limits<IT>::max(), std::max(std::numeric_limits<IT>::min(), t)));
}

template<typename OT, typename IT>
[[nodiscard]] FORCE_FINLINE
OT clampIntU(IT t){
   return static_cast<OT>(std::min(std::numeric_limits<IT>::max(), std::max(0, t)));
}

template<typename T>
[[nodiscard]] FORCE_FINLINE
DepthType clampDepth(const T & d){
   return static_cast<DepthType>(std::max(static_cast<T>(0), std::min(static_cast<T>(MAX_DEPTH),d)));
}

template<typename T>
[[nodiscard]] constexpr ScoreType clampScore(T s) {
   return static_cast<ScoreType>(std::clamp(s, (T)(-MATE + 2 * MAX_DEPTH), (T)(MATE - 2 * MAX_DEPTH)));
}

enum GamePhase { MG = 0, EG = 1, GP_MAX = 2 };
ENABLE_INCR_OPERATORS_ON(GamePhase)

template<typename T, int SIZE> struct OptList : public std::vector<T> {
   OptList(): std::vector<T>() { std::vector<T>::reserve(SIZE); }
};
using MoveList = OptList<Move, MAX_MOVE / 4>; ///@todo tune this ?
using PVList = std::vector<Move>;

[[nodiscard]] constexpr MiniHash Hash64to32(Hash h) { return static_cast<MiniHash>((h >> 32) & 0xFFFFFFFF); }
[[nodiscard]] constexpr MiniMove Move2MiniMove(Move m) { return static_cast<MiniMove>(m & 0xFFFF); } // skip score

// sameMove is not comparing score part of the Move, only the MiniMove part !
[[nodiscard]] constexpr bool sameMove(const Move& a, const Move& b) { return Move2MiniMove(a) == Move2MiniMove(b); }
[[nodiscard]] constexpr bool sameMove(const Move& a, const MiniMove& b) { return Move2MiniMove(a) == b; }

[[nodiscard]] constexpr bool isValidMove(const Move& m) { return !sameMove(m, NULLMOVE) && !sameMove(m, INVALIDMOVE); }
[[nodiscard]] constexpr bool isValidMove(const MiniMove& m) { return m != INVALIDMINIMOVE && m != NULLMOVE; }

inline constexpr std::string_view startPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
inline constexpr std::string_view fine70        = "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1";
inline constexpr std::string_view shirov        = "6r1/2rp1kpp/2qQp3/p3Pp1P/1pP2P2/1P2KP2/P5R1/6R1 w - - 0 1";
inline constexpr std::string_view shirov2       = "8/2rp1krp/2qQp3/p3Pp1P/1pP2P2/1P2KP2/P7/6R1 w - - 0 2";
inline constexpr std::string_view mate4         = "r1bnk2r/pppp1ppp/1b4q1/4P3/2B1N3/Q1Pp1N2/P4PPP/R3R1K1 w - - 1 1";

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
   P_wk   = 6,
   P_end
};
ENABLE_INCR_OPERATORS_ON(Piece)

constexpr Piece operator~(Piece pp) { return static_cast<Piece>(-pp); } // switch piece Color
constexpr int PieceShift = 6;
constexpr int NbPiece    = 2 * PieceShift + 1;

[[nodiscard]] constexpr int PieceIdx(Piece p) { return p + PieceShift; } ///@todo use it everywhere !

enum Mat : uint8_t { M_t = 0, M_p, M_n, M_b, M_r, M_q, M_k, M_bl, M_bd, M_M, M_m };
ENABLE_INCR_OPERATORS_ON(Mat)

extern CONST_PIECE_TUNING array1d<ScoreType,NbPiece> Values;
extern CONST_PIECE_TUNING array1d<ScoreType,NbPiece> ValuesEG;
extern CONST_PIECE_TUNING array1d<ScoreType,NbPiece> ValuesGP;
extern CONST_PIECE_TUNING array1d<ScoreType,NbPiece> ValuesSEE;

[[nodiscard]] FORCE_FINLINE ScoreType value(Piece pp)      { return Values[pp + PieceShift]; }
[[nodiscard]] FORCE_FINLINE ScoreType valueEG(Piece pp)    { return ValuesEG[pp + PieceShift]; }
[[nodiscard]] FORCE_FINLINE ScoreType valueGP(Piece pp)    { return ValuesGP[pp + PieceShift]; }
[[nodiscard]] FORCE_FINLINE ScoreType valueSEE(Piece pp)   { return ValuesSEE[pp + PieceShift]; }

#ifdef WITH_PIECE_TUNING
FORCE_FINLINE void SymetrizeValue() {
   for (Piece pp = P_wp; pp <= P_wk; ++pp) {
      Values[-pp + PieceShift]   = Values[pp + PieceShift];
      ValuesEG[-pp + PieceShift] = ValuesEG[pp + PieceShift];
      ValuesGP[-pp + PieceShift] = ValuesGP[pp + PieceShift];
      ValuesSEE[-pp + PieceShift] = ValuesSEE[pp + PieceShift];
    }
}
#endif

const ScoreType dummyScore = 0;

inline array1d<const ScoreType* const,7> absValues_    = {&dummyScore,
                                                          &Values[P_wp + PieceShift],
                                                          &Values[P_wn + PieceShift],
                                                          &Values[P_wb + PieceShift],
                                                          &Values[P_wr + PieceShift],
                                                          &Values[P_wq + PieceShift],
                                                          &Values[P_wk + PieceShift]};

inline array1d<const ScoreType* const,7> absValuesEG_    = {&dummyScore,
                                                            &ValuesEG[P_wp + PieceShift],
                                                            &ValuesEG[P_wn + PieceShift],
                                                            &ValuesEG[P_wb + PieceShift],
                                                            &ValuesEG[P_wr + PieceShift],
                                                            &ValuesEG[P_wq + PieceShift],
                                                            &ValuesEG[P_wk + PieceShift]};

inline array1d<const ScoreType* const,7> absValuesGP_    = {&dummyScore,
                                                            &ValuesGP[P_wp + PieceShift],
                                                            &ValuesGP[P_wn + PieceShift],
                                                            &ValuesGP[P_wb + PieceShift],
                                                            &ValuesGP[P_wr + PieceShift],
                                                            &ValuesGP[P_wq + PieceShift],
                                                            &ValuesGP[P_wk + PieceShift]};

inline array1d<const ScoreType* const,7> absValuesSEE_    = {&dummyScore,
                                                             &ValuesSEE[P_wp + PieceShift],
                                                             &ValuesSEE[P_wn + PieceShift],
                                                             &ValuesSEE[P_wb + PieceShift],
                                                             &ValuesSEE[P_wr + PieceShift],
                                                             &ValuesSEE[P_wq + PieceShift],
                                                             &ValuesSEE[P_wk + PieceShift]};

[[nodiscard]] FORCE_FINLINE ScoreType absValue(Piece pp)   { return *absValues_[pp]; }
[[nodiscard]] FORCE_FINLINE ScoreType absValueEG(Piece pp) { return *absValuesEG_[pp]; }
[[nodiscard]] FORCE_FINLINE ScoreType absValueGP(Piece pp) { return *absValuesGP_[pp]; }
[[nodiscard]] FORCE_FINLINE ScoreType absValueSEE(Piece pp){ return *absValuesSEE_[pp]; }

template<typename T> [[nodiscard]] constexpr int sgn(T val) { return (T(0) < val) - (val < T(0)); }

inline constexpr array1d<std::string_view,NbPiece> PieceNames = {"k", "q", "r", "b", "n", "p", " ", "P", "N", "B", "R", "Q", "K"};
inline constexpr array1d<std::string_view,NbPiece> PieceNamesUTF = {"♔", "♕", "♖", "♗", "♘", "♙", " ", "♟︎", "♞", "♝", "♜", "♛", "♚" };

inline constexpr Square NbSquare = 64;

inline constexpr array1d<std::string_view,NbSquare> SquareNames = { "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
                                                                    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
                                                                    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
                                                                    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
                                                                    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
                                                                    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
                                                                    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
                                                                    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8" };

inline constexpr array1d<std::string_view,8> FileNames = {"a", "b", "c", "d", "e", "f", "g", "h"};
inline constexpr array1d<std::string_view,8> RankNames = {"1", "2", "3", "4", "5", "6", "7", "8"};

template<Color C> [[nodiscard]] constexpr ScoreType ColorSignHelper() { return C == Co_White ? +1 : -1; }
template<Color C> [[nodiscard]] constexpr Square    PromotionSquare(const Square k) { return C == Co_White ? (SQFILE(k) + 56) : SQFILE(k); }
template<Color C> [[nodiscard]] constexpr Rank      ColorRank(const Square k) { return Rank(C == Co_White ? SQRANK(k) : (7 - SQRANK(k))); }

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
constexpr CastlingRights operator&(const CastlingRights& a, const CastlingRights& b) { return CastlingRights(char(a) & char(b)); }
constexpr CastlingRights operator|(const CastlingRights& a, const CastlingRights& b) { return CastlingRights(char(a) | char(b)); }
constexpr CastlingRights operator~(const CastlingRights& a) { return CastlingRights(~char(a)); }
constexpr void           operator&=(CastlingRights& a, const CastlingRights& b) { a = a & b; }
constexpr void           operator|=(CastlingRights& a, const CastlingRights& b) { a = a | b; }

[[nodiscard]] FORCE_FINLINE Square stringToSquare(const std::string& str) { return static_cast<Square>((str.at(1) - 49) * 8 + (str.at(0) - 97)); }

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
   T_bqs      = 15, // binary 1100 to 1111
   T_max      = 16
};

// castling are encoded with to Square being initial rook position
// this allows to change it to king destination square
const array1d<Square,T_bqs+1> correctedKingDestSq = { INVALIDSQUARE, INVALIDSQUARE, INVALIDSQUARE, INVALIDSQUARE,
                                                      INVALIDSQUARE, INVALIDSQUARE, INVALIDSQUARE, INVALIDSQUARE,
                                                      INVALIDSQUARE, INVALIDSQUARE, INVALIDSQUARE, INVALIDSQUARE,
                                                      Sq_g1, Sq_c1, Sq_g8, Sq_c8};
// this allows to change it to rook destination square
const array1d<Square,T_bqs+1> correctedRookDestSq = { INVALIDSQUARE, INVALIDSQUARE, INVALIDSQUARE, INVALIDSQUARE,
                                                      INVALIDSQUARE, INVALIDSQUARE, INVALIDSQUARE, INVALIDSQUARE,
                                                      INVALIDSQUARE, INVALIDSQUARE, INVALIDSQUARE, INVALIDSQUARE,
                                                      Sq_f1, Sq_d1, Sq_f8, Sq_d8};

[[nodiscard]] constexpr bool isValidMoveType(const MType m)      { return m <= T_bqs; }
[[nodiscard]] constexpr bool isValidSquare(const Square s)       { return s >= 0 && s < NbSquare; }
[[nodiscard]] constexpr bool isValidPiece(const Piece pp)        { return pp >= P_bk && pp <= P_wk; }
[[nodiscard]] constexpr bool isValidPieceNotNone(const Piece pp) { return isValidPiece(pp) && pp != P_none; }
[[nodiscard]] constexpr bool isNotEmpty(const BitBoard bb)       { return bb != emptyBitBoard; }

[[nodiscard]] constexpr Piece promShift(const MType mt) {
   assert(mt >= T_promq);
   assert(mt <= T_cappromn);
   return static_cast<Piece>(P_wq - (mt % 4));
} // awfull hack

// previous best root 20000
// ttmove 15000
// ~~king evasion 10000~~
// good cap +7000 +SEE (or MVA and cap history)
// killers 1900-1700-1500
// counter 1300
// standard move use -1024 < history < 1024
// leaving threat +512
// MVV-LVA [0 400]
// recapture +512
inline constexpr array1d<ScoreType,16> MoveScoring    = {   0,                   // standard
                                                         7000,                   // cap (bad cap will be *=-1)
                                                            0,                   // reserved
                                                         7000,                   // ep
                                                         3950, 3500, 3350, 3300, // prom
                                                         7950, 7500, 7350, 7300, // prom+cap
                                                          256,  256,  256,  256  // castling bonus
                                                        };

[[nodiscard]] FORCE_FINLINE bool isSkipMove(const Move& a, const std::vector<MiniMove>* skipMoves) {
   return skipMoves && std::ranges::find(*skipMoves, Move2MiniMove(a)) != skipMoves->end();
}

[[nodiscard]] constexpr ScoreType Move2Score(const Move m) {
   assert(isValidMove(m));
   return static_cast<ScoreType>((m >> 16) & 0xFFFF);
}
[[nodiscard]] constexpr Square Move2From(const Move m) {
   assert(isValidMove(m));
   return static_cast<Square>((m >> 10) & 0x3F);
}
[[nodiscard]] constexpr Square Move2To(const Move m) {
   assert(isValidMove(m));
   return static_cast<Square>((m >> 4) & 0x3F);
}
[[nodiscard]] constexpr MType Move2Type(const Move m) {
   assert(isValidMove(m));
   return static_cast<MType>(m & 0xF);
}
[[nodiscard]] constexpr MiniMove ToMove(const Square from, const Square to, const MType type) {
   assert(isValidSquare(from));
   assert(isValidSquare(to));
   return static_cast<MiniMove>((from << 10) | (to << 4) | type);
}
[[nodiscard]] constexpr Move ToMove(const Square from, const Square to, const MType type, const ScoreType score) {
   assert(isValidSquare(from));
   assert(isValidSquare(to));
   return static_cast<Move>((score << 16) | (from << 10) | (to << 4) | type);
}

[[nodiscard]] constexpr ScoreType matingScore  (const DepthType in) { return static_cast<ScoreType>(MATE - in); }
[[nodiscard]] constexpr ScoreType matedScore   (const DepthType in) { return static_cast<ScoreType>(-MATE + in); }

[[nodiscard]] constexpr bool isMatingScore(const ScoreType s) { return (s >= MATE - MAX_DEPTH); }
[[nodiscard]] constexpr bool isMatedScore (const ScoreType s) { return (s <= matedScore(MAX_DEPTH)); }
[[nodiscard]] constexpr bool isMateScore  (const ScoreType s) { return (Abs(s) >= MATE - MAX_DEPTH); }

[[nodiscard]] constexpr bool isPromotionStd(const MType mt) {
   assert(isValidMoveType(mt));
   return (mt >> 2 == 0x1);
}
[[nodiscard]] constexpr bool isPromotionStd(const Move m) { return isPromotionStd(Move2Type(m)); }
[[nodiscard]] constexpr bool isPromotionCap(const MType mt) {
   assert(isValidMoveType(mt));
   return (mt >> 2 == 0x2);
}
[[nodiscard]] constexpr bool isPromotionCap(const Move m) { return isPromotionCap(Move2Type(m)); }

[[nodiscard]] constexpr bool isPromotion(const MType mt) {
   assert(isValidMoveType(mt));
   return isPromotionStd(mt) || isPromotionCap(mt);
}
[[nodiscard]] constexpr bool isPromotion(const Move m) { return isPromotion(Move2Type(m)); }

[[nodiscard]] constexpr bool isCastling(const MType mt) {
   assert(isValidMoveType(mt));
   return (mt >> 2) == 0x3;
}
[[nodiscard]] constexpr bool isCastling(const Move m) { return isCastling(Move2Type(m)); }

[[nodiscard]] constexpr bool isCapture(const MType mt) {
   assert(isValidMoveType(mt));
   return mt == T_capture || mt == T_ep || isPromotionCap(mt);
}
[[nodiscard]] constexpr bool isCapture(const Move m) { return isCapture(Move2Type(m)); }

[[nodiscard]] constexpr bool isCaptureNoEP(const MType mt) {
   assert(isValidMoveType(mt));
   return mt == T_capture || isPromotionCap(mt);
}
[[nodiscard]] constexpr bool isCaptureNoEP(const Move m) { return isCaptureNoEP(Move2Type(m)); }


[[nodiscard]] constexpr bool isCaptureOrProm(const MType mt) {
   assert(isValidMoveType(mt));
   return mt == T_capture || mt == T_ep || isPromotion(mt);
}
[[nodiscard]] constexpr bool isCaptureOrProm(const Move m) { return isCaptureOrProm(Move2Type(m)); }

[[nodiscard]] constexpr bool isEnPassant(const MType mt) {
   assert(isValidMoveType(mt));
   return mt == T_ep;
}
[[nodiscard]] constexpr bool isEnPassant(const Move m) { return isEnPassant(Move2Type(m)); }

[[nodiscard]] constexpr ScoreType badCapScore(const Move m) { return Move2Score(m) + MoveScoring[T_capture]; }

[[nodiscard]] constexpr Square chebyshevDistance(const Square sq1, const Square sq2) {
   return std::max(Abs(static_cast<Square>(SQRANK(sq2) - SQRANK(sq1))), Abs(static_cast<Square>(SQFILE(sq2) - SQFILE(sq1))));
}
[[nodiscard]] constexpr Square manatthanDistance(const Square sq1, const Square sq2) {
   return Abs(static_cast<Square>(SQRANK(sq2) - SQRANK(sq1))) + Abs(static_cast<Square>(SQFILE(sq2) - SQFILE(sq1)));
}
[[nodiscard]] constexpr Square minDistance(const Square sq1, const Square sq2) {
   return std::min(Abs(static_cast<Square>(SQRANK(sq2) - SQRANK(sq1))), Abs(static_cast<Square>(SQFILE(sq2) - SQFILE(sq1))));
}

[[nodiscard]] FORCE_FINLINE Square correctedMove2ToKingDest(const Move m) {
   assert(isValidMove(m));
   const MType mtype = Move2Type(m);
   return (!isCastling(mtype)) ? Move2To(m) : correctedKingDestSq[mtype];
}

[[nodiscard]] FORCE_FINLINE Square correctedMove2ToRookDest(const Move m) {
   assert(isValidMove(m));
   const MType mtype = Move2Type(m);
   return (!isCastling(mtype)) ? Move2To(m) : correctedRookDestSq[mtype];
}

namespace MoveDifficultyUtil {
    enum MoveDifficulty { 
      MD_forced = 0, 
      MD_easy, 
      MD_std, 
      MD_moobDefenceIID, 
      MD_moobAttackIID
    };

    enum PositionEvolution{
      PE_none = 0,
      PE_std,
      PE_moobingAttack,
      PE_moobingDefence,
      PE_boomingAttack,
      PE_boomingDefence,
    };

    const DepthType emergencyMinDepth          = 14;
    const ScoreType emergencyMargin            = 64;
    const ScoreType emergencyAttackThreshold   = 130;
    const ScoreType easyMoveMargin             = 180;
    const int       emergencyFactorIID         = 3;
    const int       emergencyFactorIIDGood     = 3;
    const int       emergencyFactorMoobHistory = 2;
    const int       emergencyFactorBoomHistory = 1;
    const int       maxStealDivisor            = 5; // 1/maxStealDivisor of remaining time

   extern float variability;
   [[nodiscard]] FORCE_FINLINE float variabilityFactor() { return 2.f / (1.f + std::exp(1.f - MoveDifficultyUtil::variability)); } // inside [0.5 .. 2]

} // namespace MoveDifficultyUtil

template<typename T, int seed> [[nodiscard]] FORCE_FINLINE T randomInt(T m, T M) {
   ///@todo this is slow because the static here implies a guard variable !!
   static std::mt19937 mt(seed); // fixed seed for ZHash for instance !!!
   static std::uniform_int_distribution<T> dist(m, M);
   return dist(mt);
}

template<typename T> [[nodiscard]] FORCE_FINLINE T randomInt(T m, T M) {
   ///@todo this is slow because the static here implies a guard variable !!
   static std::random_device r;
   static std::seed_seq seed {r(), r(), r(), r(), r(), r(), r(), r()};
   static std::mt19937 mt(seed);
   static std::uniform_int_distribution<T> dist(m, M);
   return dist(mt);
}

template<class T> [[nodiscard]] constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
    assert(!(hi < lo));
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

[[nodiscard]] constexpr Square relative_square(const Color c, const Square s) { return static_cast<Square>(s ^ (c * 56)); }

template<Color C> [[nodiscard]] constexpr Square ColorSquarePstHelper(const Square k) { return relative_square(~C, k); }

[[nodiscard]] constexpr uint64_t powerFloor(const uint64_t x) {
   uint64_t power = 1;
   while (power <= x) power *= 2;
   return power / 2;
}

FORCE_FINLINE void* std_aligned_alloc(const size_t alignment, const size_t size) {
#ifdef __ANDROID__
  return malloc(size);
#elif defined(__EMSCRIPTEN__)
  return malloc(size);
#elif defined(__APPLE__)
  return aligned_alloc(alignment, size);
#elif defined(_WIN32)
  return _mm_malloc(size, alignment);
#elif defined(ARDUINO) || defined(ESP32)
  return malloc(size);
#else
   void* ret = std::aligned_alloc(alignment, size);
#ifdef MADV_HUGEPAGE
  madvise(ret, size, MADV_HUGEPAGE);
#endif
  return ret;
#endif
}

FORCE_FINLINE void std_aligned_free(void* ptr) {
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

FORCE_FINLINE std::string toHexString(const uint32_t i) {
  std::stringstream s;
  s << "0x" << std::hex << i;
  return s.str();
}
