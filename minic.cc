#include <algorithm>
#include <cmath>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <iterator>
#include <vector>
#include <sstream>
#include <chrono>
#include <random>
#ifdef _WIN32
#include <stdlib.h>
#else
#include <unistd.h>
#endif

typedef std::chrono::high_resolution_clock Clock;
typedef char DepthType;
typedef int Move; // invalid if < 0
typedef char Square; // invalid if < 0
typedef unsigned long long int Hash; // invalid if == 0
typedef unsigned long long int Counter;
typedef short int ScoreType;

bool mateFinder = false;

#define STOPSCORE   ScoreType(20000)
#define INFSCORE    ScoreType(10000)
#define MATE        ScoreType(9000)
#define INVALIDMOVE    -1
#define INVALIDSQUARE  -1
#define MAX_PLY       512

#define SQFILE(s) s%8
#define SQRANK(s) s/8

Hash hashStack[MAX_PLY] = { 0 };

//#define DEBUG_ZHASH

std::string startPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
std::string fine70 = "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - -";

struct Stats{
   Counter nodes;
   Counter qnodes;
   Counter tthits;
   void init(){
      nodes  = 0;
      qnodes = 0;
      tthits = 0;       
   }
};

Stats stats;

enum Piece : char{
   P_bk = -6,
   P_bq = -5,
   P_br = -4,
   P_bb = -3,
   P_bn = -2,
   P_bp = -1,
   P_none = 0,
   P_wp = 1,
   P_wn = 2,
   P_wb = 3,
   P_wr = 4,
   P_wq = 5,
   P_wk = 6
};

const int PieceShift = 6;

ScoreType  Values[13]  = { -8000, -950, -500, -335, -325, -100, 0, 100, 325, 335, 500, 950, 8000 };
char Names[13]   = { 'k', 'q', 'r', 'b', 'n', 'p', ' ', 'P', 'N', 'B', 'R', 'Q', 'K' };

std::string Squares[64] = { "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
                            "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
                            "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
                            "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
                            "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
                            "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
                            "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
                            "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8" };

enum Sq : char {
    Sq_a1 =  0,Sq_b1,Sq_c1,Sq_d1,Sq_e1,Sq_f1,Sq_g1,Sq_h1,
    Sq_a2 =  8,Sq_b2,Sq_c2,Sq_d2,Sq_e2,Sq_f2,Sq_g2,Sq_h2,
    Sq_a3 = 16,Sq_b3,Sq_c3,Sq_d3,Sq_e3,Sq_f3,Sq_g3,Sq_h3,
    Sq_a4 = 24,Sq_b4,Sq_c4,Sq_d4,Sq_e4,Sq_f4,Sq_g4,Sq_h4,
    Sq_a5 = 32,Sq_b5,Sq_c5,Sq_d5,Sq_e5,Sq_f5,Sq_g5,Sq_h5,
    Sq_a6 = 40,Sq_b6,Sq_c6,Sq_d6,Sq_e6,Sq_f6,Sq_g6,Sq_h6,
    Sq_a7 = 48,Sq_b7,Sq_c7,Sq_d7,Sq_e7,Sq_f7,Sq_g7,Sq_h7,
    Sq_a8 = 56,Sq_b8,Sq_c8,Sq_d8,Sq_e8,Sq_f8,Sq_g8,Sq_h8
};
                            
enum Castling : char{
   C_none= 0,
   C_wks = 1,
   C_wqs = 2,
   C_bks = 4,
   C_bqs = 8
};

const int MvvLvaScores[13][13] = {
        //      K, Q, R, B, N, P, 0, P, N, B, R, Q, K
        /*K*/ { 0, 9, 9, 9, 9, 9, 0, 9, 9, 9, 9, 9, 0},
        /*Q*/ { 3, 4, 5, 6, 7, 8, 0, 8, 7, 6, 5, 4, 3},
        /*R*/ { 1,-5, 2, 3, 4, 5, 0, 5, 4, 3, 2,-5, 1},
        /*B*/ { 1,-6,-2, 1, 1, 3, 0, 3, 1, 1,-2,-6, 1},
        /*N*/ { 1,-6,-2, 0, 1, 2, 0, 2, 1, 0,-2,-6, 1},
        /*P*/ { 1,-9,-5,-3,-2, 0, 0, 0,-2,-3,-5,-9, 1},
        /*0*/ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        /*P*/ { 1,-9,-5,-3,-2, 0, 0, 0,-2,-3,-5,-9, 1},
        /*N*/ { 1,-6,-2, 0, 1, 2, 0, 2, 1, 0,-2,-6, 1},
        /*B*/ { 1,-6,-2, 1, 1, 3, 0, 3, 1, 1,-2,-6, 1},
        /*R*/ { 1,-5, 2, 3, 4, 5, 0, 5, 4, 3, 2,-5, 1},
        /*Q*/ { 3, 4, 5, 6, 7, 8, 0, 8, 7, 6, 5, 4, 3},
        /*K*/ { 0, 9, 9, 9, 9, 9, 0, 9, 9, 9, 9, 9, 0}
   };

enum MType : char{
   T_std        = 0,
   T_capture    = 1,
   T_ep         = 2,
   T_check      = 3,
   T_promq      = 4,
   T_promr      = 5,
   T_promb      = 6,
   T_promn      = 7,
   T_cappromq   = 8,
   T_cappromr   = 9,
   T_cappromb   = 10,
   T_cappromn   = 11,
   T_wks        = 12,
   T_wqs        = 13,
   T_bks        = 14,
   T_bqs        = 15
};

enum Color : char{
   Co_None  = -1,
   Co_White = 0,
   Co_Black = 1
};

ScoreType MoveScoring[16] = { 0, 1000, 1100, 300, 950, 500, 350, 300, 1950, 1500, 1350, 1300, 100, 100, 100, 100 };

Color Colors[13] = { Co_Black, Co_Black, Co_Black, Co_Black, Co_Black, Co_Black, Co_None, Co_White, Co_White, Co_White, Co_White, Co_White, Co_White};

int Signs[13] = { -1, -1, -1, -1, -1, -1, 0, 1, 1, 1, 1, 1, 1 };

struct Position{
  Piece b[64];
  unsigned char fifty;
  unsigned char moves;
  unsigned int castling;
  Square ep;
  Square wk;
  Square bk;
  mutable Hash h;
  Color c;
};

ScoreType Move2Score(Move h) { return (h >> 16) & 0xFFFF; }
Square    Move2From (Move h) { return (h >> 10) & 0x3F  ; }
Square    Move2To   (Move h) { return (h >>  4) & 0x3F  ; }
MType     Move2Type (Move h) { return MType(h & 0xF)    ; }
Move      ToMove(Square from, Square to, MType type){
   return (from << 10) | (to << 4) | type;
}
Move   ToMove(Square from, Square to, MType type, ScoreType score) {
    return (score << 16) | (from << 10) | (to << 4) | type;
}

Hash randomInt(){
    static std::mt19937 mt(42);
    static std::uniform_int_distribution<unsigned long long int> dist(0, UINT64_MAX);
    return dist(mt);
} 

Hash ZT[64][14]; // should be 13 but last ray is for castling[0 7 56 63][13] and ep [k][13] and color [3 4][13]

void initHash(){
   for(int k = 0 ; k < 64 ; ++k){
      for(int j = 0 ; j < 14 ; ++j){
         ZT[k][j] = randomInt();
      }
   }
}

Hash computeHash(const Position &p){

    //std::cout << "hash is " << p.h << std::endl;

   if (p.h != 0) return p.h;

   Hash h = 0;
   for (int k = 0; k < 64; ++k){
      Piece pp = p.b[k];
      if ( pp != P_none){
        h ^= ZT[k][pp+PieceShift];
      }
   }

   if ( p.ep != INVALIDSQUARE ) h ^= ZT[p.ep][13];
   if ( p.castling & C_wks) h ^= ZT[7][13];
   if ( p.castling & C_wqs) h ^= ZT[0][13];
   if ( p.castling & C_bks) h ^= ZT[63][13];
   if ( p.castling & C_bqs) h ^= ZT[56][13];
   if ( p.c == Co_White) h ^= ZT[3][13];
   if ( p.c == Co_Black) h ^= ZT[4][13];

   p.h = h;

   return h;
} 

struct TT{
   enum Bound{
      B_exact = 0,
      B_alpha = 1,
      B_beta  = 2
   };
   struct Entry{
      Entry():m(INVALIDMOVE),score(0),b(B_alpha),d(-1),h(0){}
#ifdef DEBUG_ZHASH
      Entry(Move m, int s,Bound b, DepthType d, Hash h, const std::string & fen ):m(m),score(m),b(b),d(d),h(h),fen(fen){}
#else
      Entry(Move m, int s, Bound b, DepthType d, Hash h) : m(m), score(m), b(b), d(d), h(h){}
#endif
      Move m;
      int score;
      Bound b;
      DepthType d;
      Hash h;
#ifdef DEBUG_ZHASH
      std::string fen;
#endif
   };
   static Entry * table;
   static int ttSize;
   
   static void initTable(int n){
      ttSize = n;
      table = new Entry[ttSize];
      std::cout << "Size of TT " << ttSize * sizeof(Entry) / 1024 / 1024 << "Mb" << std::endl;
   }

#ifdef DEBUG_ZHASH
   static bool getEntry(Hash h, DepthType d, Entry & e, const std::string & fen){
#else
   static bool getEntry(Hash h, DepthType d, Entry & e) {
#endif
      const Entry & _e = table[h%ttSize];
      if ( _e.h != h ){
          e.h = 0;
          return false;
      }
#ifdef DEBUG_ZHASH
      if (_e.fen != fen) {
          std::cout << "bad fen \n" << _e.fen << "\n" << fen << std::endl;
          return false;
      }
#endif
      if ( _e.d >= d ){
         e=_e;
         ++stats.tthits;
         return true;
      }
      return false;
   }
   
   static void setEntry(const Entry & e){
      Entry & _e = table[e.h%ttSize];
      _e = e; // always replace
   }
};

TT::Entry * TT::table = 0;
int TT::ttSize = 0;

namespace KillerT{
   Move killers[2][MAX_PLY];
   
   void initKillers(){
      for(int i = 0; i < 2; ++i){
          for(int k = 0 ; k < MAX_PLY; ++k){
              killers[i][k] = INVALIDMOVE;
          }
      }
   }
};

namespace HistoryT{
    ScoreType history[13][64];
   void initHistory(){
      for(int i = 0; i < 13; ++i){
          for(int k = 0 ; k < 64; ++k){
              history[i][k] = 0;
          }
      }
   }
   void update(DepthType depth, Move m, const Position & p, bool plus){
       if ( Move2Type(m) == T_std ){
          history[p.b[Move2From(m)] + PieceShift ][Move2To(m)] += (plus?+1:-1) * (depth*depth*4 - history[p.b[Move2From(m)] + PieceShift ][Move2To(m)] * depth*depth/2 / 200);
       }
   }
};

bool isCapture(const MType & mt){
   return mt == T_capture
       || mt == T_ep
       || mt == T_cappromq
       || mt == T_cappromr
       || mt == T_cappromb
       || mt == T_cappromn;
}

bool isCapture(const Move & m){
   return isCapture(Move2Type(m));
}


std::string GetFENShort(const Position &p ){

    // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR"

    std::stringstream ss;
    int count = 0;
    for (int i = 7; i >= 0; --i) {
        for (int j = 0; j < 8; j++) {
            int k = 8 * i + j;
            if (p.b[k] == P_none) {
                ++count;
            }
            else {
                if (count != 0) {
                    ss << count;
                    count = 0;
                }
                ss << Names[p.b[k]+PieceShift];
            }
            if (j == 7) {
                if (count != 0) {
                    ss << count;
                    count = 0;
                }
                if (i != 0) {
                    ss << "/";
                }
            }
        }
    }

    return ss.str();
}

std::string GetFENShort2(const Position &p) {

    // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d5"

    std::stringstream ss;

    ss << GetFENShort(p);

    ss << " " << (p.c == Co_White ? "w" : "b") << " ";

    bool withCastling = false;
    if (p.castling & C_wks) {
        ss << "K";
        withCastling = true;
    }
    if (p.castling & C_wqs) {
        ss << "Q";
        withCastling = true;
    }
    if (p.castling & C_bks) {
        ss << "k";
        withCastling = true;
    }
    if (p.castling & C_bqs) {
        ss << "q";
        withCastling = true;
    }

    if (!withCastling) ss << "-";

    if (p.ep != INVALIDSQUARE) {
        ss << " " << Squares[p.ep];
    }
    else {
        ss << " -";
    }

    return ss.str();
}


std::string GetFEN(const Position &p) {

    // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d5 0 2"

    std::stringstream ss;

    ss << GetFENShort2(p);

    ss << " " << (int)p.fifty << " " << (int)p.moves;

    return ss.str();
}

std::string ToString(const Move & m){
   if ( m == INVALIDMOVE ) return "invalid move";
   std::stringstream ss;
   std::string prom;
   switch (Move2Type(m)) {
   case T_bks:
   case T_wks:
       return "0-0";
       break;
   case T_bqs:
   case T_wqs:
       return "0-0-0";
       break;
   case T_promq:
       prom = "q";
       break;
   case T_promr:
       prom = "r";
       break;
   case T_promb:
       prom = "b";
       break;
   case T_promn:
       prom = "n";
       break;
   case T_cappromq:
       prom = "q";
       break;
   case T_cappromr:
       prom = "r";
       break;
   case T_cappromb:
       prom = "b";
       break;
   case T_cappromn:
       prom = "n";
       break;
   default:
       break;
   }
   ss << Squares[Move2From(m)] << Squares[Move2To(m)];
   return ss.str() + prom;
}

std::string ToString(const std::vector<Move> & moves){
   std::stringstream ss;
   for(size_t k = 0 ; k < moves.size(); ++k){
      ss << ToString(moves[k]) << " ";
   }
   return ss.str();
}

std::string ToString(const Position & p){
    std::stringstream ss;
    ss << std::endl;
    for (Square j = 7; j >= 0; --j) {
        ss << "+-+-+-+-+-+-+-+-+" << std::endl;
        ss << "|";
        for (Square i = 0; i < 8; ++i) {
            ss << Names[p.b[i+j*8]+PieceShift] << '|';
        }
        ss << std::endl; ;
    }
    ss << "+-+-+-+-+-+-+-+-+" ;
    ss << std::endl;
    if ( p.ep >=0 ) ss << "ep " << Squares[p.ep]<< std::endl;
    ss << "wk " << Squares[p.wk] << std::endl;
    ss << "bk " << Squares[p.bk] << std::endl;
    return ss.str();
}

bool stopFlag;

const int mailbox[120] = {
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1,  0,  1,  2,  3,  4,  5,  6,  7, -1,
     -1,  8,  9, 10, 11, 12, 13, 14, 15, -1,
     -1, 16, 17, 18, 19, 20, 21, 22, 23, -1,
     -1, 24, 25, 26, 27, 28, 29, 30, 31, -1,
     -1, 32, 33, 34, 35, 36, 37, 38, 39, -1,
     -1, 40, 41, 42, 43, 44, 45, 46, 47, -1,
     -1, 48, 49, 50, 51, 52, 53, 54, 55, -1,
     -1, 56, 57, 58, 59, 60, 61, 62, 63, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

const int mailbox64[64] = {
    21, 22, 23, 24, 25, 26, 27, 28,
    31, 32, 33, 34, 35, 36, 37, 38,
    41, 42, 43, 44, 45, 46, 47, 48,
    51, 52, 53, 54, 55, 56, 57, 58,
    61, 62, 63, 64, 65, 66, 67, 68,
    71, 72, 73, 74, 75, 76, 77, 78,
    81, 82, 83, 84, 85, 86, 87, 88,
    91, 92, 93, 94, 95, 96, 97, 98
};

bool Slide[6] = {false, false, true, true, true, false};
int Offsets[6] = {8, 8, 4, 4, 8, 8};
int Offset[6][8] = {
   { -20, -10, -11, -9, 9, 11, 10, 20 },
   { -21, -19, -12, -8, 8, 12, 19, 21 }, /* knight */
   { -11,  -9,   9, 11, 0,  0,  0,  0 }, /* bishop */
   { -10,  -1,   1, 10, 0,  0,  0,  0 }, /* rook */
   { -11, -10,  -9, -1, 1,  9, 10, 11 }, /* queen */
   { -11, -10,  -9, -1, 1,  9, 10, 11 }  /* king */
};

const ScoreType PST[6][64] = {
    {
    0,    0,    0,    0,    0,    0,    0,    0,
   -2,   11,   -4,   -2,   -2,   -4,   11,   -2,
   -3,   -4,   -3,   -1,   -1,   -3,   -4,   -3,
   -3,    2,    1,   12,   12,    1,    2,   -3,
   -9,   -5,   11,   20,   20,   11,   -5,   -9,
  -10,   -1,   11,   14,   14,   11,   -1,  -10,
   -6,    3,    4,    1,    1,    4,    3,   -6,
    0,    0,    0,    0,    0,    0,    0,    0  },
    {
    -114,  -39,  -24,  -16,  -16,  -24,  -39, -114,
     -36,  -11,    2,    8,    8,    2,  -11,  -36,
      -6,   21,   32,   38,   38,   32,   21,   -6,
     -15,    9,   22,   29,   29,   22,    9,  -15,
     -14,   10,   25,   27,   27,   25,   10,  -14,
     -41,  -12,    0,    5,    5,    0,  -12,  -41,
     -48,  -25,  -12,   -5,   -5,  -12,  -25,  -48,
     -94,  -56,  -46,  -42,  -42,  -46,  -56,  -94  },
     {
    -20,   -6,  -11,  -16,  -16,  -11,   -6,  -20,
    -13,    9,    3,   -1,   -1,    3,    9,  -13,
     -9,    9,    7,    1,    1,    7,    9,   -9,
     -6,   15,    9,    5,    5,    9,   15,   -6,
     -6,   16,   12,    5,    5,   12,   16,   -6,
     -5,   15,   12,    6,    6,   12,   15,   -5,
    -11,   11,    7,    0,    0,    7,   11,  -11,
    -25,   -7,  -14,  -19,  -19,  -14,   -7,  -25 },
    {
    -13,   -8,   -6,   -2,   -2,   -6,   -8,  -13,
     -7,    2,    4,    7,    7,    4,    2,   -7,
    -12,   -4,    0,    1,    1,    0,   -4,  -12,
    -12,   -4,    0,    0,    0,    0,   -4,  -12,
    -12,   -3,    0,    1,    1,    0,   -3,  -12,
    -12,   -5,   -2,    1,    1,   -2,   -5,  -12,
    -12,   -4,   -1,    0,    0,   -1,   -4,  -12,
    -14,   -9,   -9,   -5,   -5,   -9,   -9,  -14  }, 
    {
    0,   -2,    0,    0,    0,    0,   -2,    0,
   -1,    4,    4,    3,    3,    4,    4,   -1,
   -1,    3,    4,    5,    5,    4,    3,   -1,
   -1,    5,    4,    4,    4,    4,    5,   -1,
    0,    4,    5,    4,    4,    5,    4,    0,
   -1,    3,    5,    5,    5,    5,    3,   -1,
   -2,    3,    5,    4,    4,    5,    3,   -2,
    0,   -2,   -1,    0,    0,   -1,   -2,    0 },
    {
    36,   51,   27,    0,    0,   27,   51,   36,
    50,   74,   36,   11,   11,   36,   74,   50,
    69,   92,   49,   23,   23,   49,   92,   69,
    87,  103,   67,   38,   38,   67,  103,   87,
   103,  108,   86,   64,   64,   86,  108,  103,
   116,  143,  102,   64,   64,  102,  143,  116,
   154,  177,  139,  105,  105,  139,  177,  154,
   156,  187,  157,  114,  114,  157,  187,  156 }
};

const ScoreType PSTEG[6][64] = {
    {
    0,    0,    0,    0,    0,    0,    0,    0,
    7,    3,    4,    7,    7,    4,    3,    7,
    3,    3,    2,    1,    1,    3,    3,    3,
    3,    3,    2,   -2,   -2,    2,    3,    3,
    1,    1,   -3,   -1,   -1,   -3,    1,    1,
   -1,   -2,    2,    1,    1,    2,   -2,   -1,
    2,   -1,    3,    0,    0,    3,   -1,    2,
    0,    0,    0,    0,    0,    0,    0,    0 },
    {
    -45,  -37,  -20,   -5,   -5,  -20,  -37,  -45,
    -27,  -20,  -10,    5,    5,  -10,  -20,  -27,
    -22,  -15,   -2,   11,   11,   -2,  -15,  -22,
    -19,  -10,    1,   16,   16,    1,  -10,  -19,
    -17,  -10,    2,   15,   15,    2,  -10,  -17,
    -20,  -16,   -2,   11,   11,   -2,  -16,  -20,
    -28,  -22,   -7,    3,    3,   -7,  -22,  -28,
    -43,  -34,  -19,   -5,   -5,  -19,  -34,  -43 },
     {
    -22,  -13,  -15,   -7,   -7,  -15,  -13,  -22,
    -14,   -4,   -5,    2,    2,   -5,   -4,  -14,
    -10,    0,    0,    5,    5,    0,    0,  -10,
    -10,   -1,   -2,    5,    5,   -2,   -1,  -10,
    -10,   -1,   -2,    6,    6,   -2,   -1,  -10,
     -9,    0,   -1,    6,    6,   -1,    0,   -9,
    -14,   -3,   -5,    1,    1,   -5,   -3,  -14,
    -24,  -12,  -15,   -7,   -7,  -15,  -12,  -24 },
    {
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0 }, 
    {
    -30,  -22,  -17,  -12,  -12,  -17,  -22,  -30,
    -22,  -12,   -8,   -2,   -2,   -8,  -12,  -22,
    -16,   -6,   -4,    1,    1,   -4,   -6,  -16,
    -11,   -2,    4,    8,    8,    4,   -2,  -11,
    -12,   -2,    3,    7,    7,    3,   -2,  -12,
    -16,   -7,   -3,    2,    2,   -3,   -7,  -16,
    -23,  -12,   -8,   -2,   -2,   -8,  -12,  -23,
    -29,  -23,  -17,  -12,  -12,  -17,  -23,  -29 },
    {
    3,   22,   33,   37,   37,   33,   22,    3,
   20,   41,   50,   57,   57,   50,   41,   20,
   39,   64,   73,   72,   72,   73,   64,   39,
   45,   67,   83,   84,   84,   83,   67,   45,
   44,   70,   70,   74,   74,   70,   70,   44,
   34,   57,   69,   68,   68,   69,   57,   34,
   17,   38,   59,   55,   55,   59,   38,   17,
    0,   20,   31,   35,   35,   31,   20,    0 }
};

namespace TimeMan{
   int msecPerMove;
   int msecWholeGame;
   int nbMoveInTC;
   int msecInc;
   bool isDynamic;

   std::chrono::time_point<Clock> startTime;

   void init(){
      msecPerMove   = 777;
      msecWholeGame = -1;
      nbMoveInTC    = -1;
      msecInc       = -1;
      isDynamic     = false;
   }

   int GetNextMSecPerMove(){
      if ( msecPerMove > 0 ){ // fixed msecPerMove already given (some forced mode depth or time)
         return msecPerMove;
      }
      if ( nbMoveInTC > 0){ // mps is given
         return int(0.95 * (msecWholeGame+(msecInc>0)?nbMoveInTC*msecInc:0) / (float)nbMoveInTC);
      }
      else{ // mps is not given
         return int(0.95 * (msecWholeGame+(msecInc>0)?60*msecInc:0) / 60.f); ///@todo better
      }
   }
}

void addMove(Square from, Square to, MType type, std::vector<Move> & moves){
   moves.push_back(ToMove(from,to,type,0));
}

bool isAttacked(const Position & p, const Square k){
   Color side = p.c;
   Color opponent = Color((p.c+1)%2);
   for (Square i = 0; i < 64; ++i){
      if (Colors[p.b[i]+PieceShift] == opponent) {
         if (abs(p.b[i]) == P_wp) {
            if (side == Co_White) {
               if ( SQFILE(i) != 0 && i - 9 == k){
                  return true;
               }
               if ( SQFILE(i) != 7 && i - 7 == k){
                  return true;
               }
            }
            else {
               if ( SQFILE(i) != 0 && i + 7 == k){
                  return true;
               }
               if ( SQFILE(i) != 7 && i + 9 == k){
                  return true;
               }
            }
         }
         else{
            for (int j = 0; j < Offsets[abs(p.b[i])-1]; ++j){
               for (Square n = i;;) {
                  n = mailbox[mailbox64[n] + Offset[abs(p.b[i])-1][j]];
                  if (n == -1) break;
                  if (n == k) return true;
                  if (p.b[n] != P_none) break;
                  if ( !Slide[abs(p.b[i])-1]) break;
               }
            }
         }
      }
   }
   return false;
}

void generate(const Position & p, std::vector<Move> & moves, bool onlyCap = false){
   moves.clear();
   Color side = p.c;
   Color opponent = Color((p.c+1)%2);
   for(Square from = 0 ; from < 64 ; ++from){
     Piece piece = p.b[from];
     if (Colors[piece+PieceShift] == side) {
       if (abs(piece) != P_wp) {
         for (Square j = 0; j < Offsets[abs(piece)-1]; ++j) {
           for (Square to = from;;) {
             to = mailbox[mailbox64[to] + Offset[abs(piece)-1][j]];
             if (to < 0) break;
             Color targetC = Colors[p.b[to]+PieceShift];
             if (targetC != Co_None) {
               if (targetC == opponent) {
                 addMove(from, to, T_capture, moves); // capture
               }
               break; // as soon as a capture or an own piece is found
             }
             if ( !onlyCap) addMove(from, to, T_std, moves); // not a capture
             if (!Slide[abs(piece)-1]) break; // next direction
           }
         }
         if ( abs(piece) == P_wk && !onlyCap ){ // castling
           if ( side == Co_White) {
              if ( (p.castling & C_wqs) && p.b[Sq_b1] == P_none && p.b[Sq_c1] == P_none && p.b[Sq_d1] == P_none
                && !isAttacked(p,Sq_c1) && !isAttacked(p,Sq_d1) && !isAttacked(p,Sq_e1)){
                addMove(from, Sq_c1, T_wqs, moves); // wqs
              }
              if ( (p.castling & C_wks) && p.b[Sq_f1] == P_none && p.b[Sq_g1] == P_none
                && !isAttacked(p,Sq_e1) && !isAttacked(p,Sq_f1) && !isAttacked(p,Sq_g1)){
                addMove(from, Sq_g1, T_wks, moves); // wks
              }
           }
           else{
              if ( (p.castling & C_bqs) && p.b[Sq_b8] == P_none && p.b[Sq_c8] == P_none && p.b[Sq_d8] == P_none
                && !isAttacked(p,Sq_c8) && !isAttacked(p,Sq_d8) && !isAttacked(p,Sq_e8)){
                addMove(from, Sq_c8, T_bqs, moves); // bqs
              }
              if ( (p.castling & C_bks) && p.b[Sq_f8] == P_none && p.b[Sq_g8] == P_none
                && !isAttacked(p,Sq_e8) && !isAttacked(p,Sq_f8) && !isAttacked(p,Sq_g8)){
                addMove(from, Sq_g8, T_bks, moves); // bks
              }
           }
         }
       }
       else {
          int pawnOffsetTrick = side==Co_White?4:0;
          for (Square j = 0; j < 4; ++j) {
             int offset = Offset[abs(piece)-1][j+pawnOffsetTrick];
             Square to = mailbox[mailbox64[from] + offset];
             if (to < 0) continue;
             Color targetC = Colors[p.b[to]+PieceShift];
             if (targetC != Co_None) {
               if (targetC == opponent
                   && ( abs(offset) == 11 || abs(offset) == 9 ) ) {
                 if ( SQRANK(to) == 0 || SQRANK(to) == 7) {
                    addMove(from, to, T_cappromq, moves); // pawn capture with promotion
                    addMove(from, to, T_cappromr, moves); // pawn capture with promotion
                    addMove(from, to, T_cappromb, moves); // pawn capture with promotion
                    addMove(from, to, T_cappromn, moves); // pawn capture with promotion
                 }
                 else{
                    addMove(from, to, T_capture, moves); // simple pawn capture
                 }
               }
             }
             else{
               // ep pawn capture
               if ( abs(offset) == 11 || abs(offset) == 9 ) {
                  if ( p.ep == to ){
                      addMove(from, to, T_ep, moves); // en passant
                  } 
               }
               else if ( abs(offset) == 20 && !onlyCap
                    && ( SQRANK(from) == 1 || SQRANK(from) == 6 )
                    && p.b[(from + to)/2] == P_none){
                  addMove(from, to, T_std, moves); // double push
               }
               else if ( abs(offset) == 10 && !onlyCap){
                   if ( SQRANK(to) == 0 || SQRANK(to) == 7 ){
                      addMove(from, to, T_promq, moves); // promotion Q
                      addMove(from, to, T_promr, moves); // promotion R
                      addMove(from, to, T_promb, moves); // promotion B 
                      addMove(from, to, T_promn, moves); // promotion N
                   }
                   else{
                      addMove(from, to, T_std, moves); // simple pawn push
                   }
               }
            }
          }
       }
     }
   }
}

struct MoveSorter{
   MoveSorter(const Position & p, DepthType ply, const TT::Entry * e = NULL):p(p),ply(ply),e(e){
   }
   bool operator()(Move & a, Move & b){
      const MType t1 = Move2Type(a);
      const MType t2 = Move2Type(b);
      ScoreType s1 = Move2Score(a);
      ScoreType s2 = Move2Score(b);
      if (s1 == 0) {
          s1 = MoveScoring[t1];
          if (isCapture(t1)) {
              s1 += MvvLvaScores[p.b[Move2To(a)] + PieceShift][p.b[Move2From(a)] + PieceShift];
          }
          if (e && e->m == a) s1 += 3000;
          if (a == KillerT::killers[0][ply]) s1 += 150;
          if (a == KillerT::killers[1][ply]) s1 += 130;
          if (t1 == T_std) s1 += HistoryT::history[p.b[Move2From(a)] + PieceShift][Move2To(a)];
          a = ToMove(Move2From(a), Move2To(a), Move2Type(a), s1);
      }
      if (s2 == 0) {
          s2 = MoveScoring[t2];
          if (isCapture(t2)) {
              s2 += MvvLvaScores[p.b[Move2To(b)] + PieceShift][p.b[Move2From(b)] + PieceShift];
          }
          if (e && e->m == b) s2 += 3000;
          if (b == KillerT::killers[0][ply]) s2 += 150;
          if (b == KillerT::killers[1][ply]) s2 += 130;
          if (t2 == T_std) s2 += HistoryT::history[p.b[Move2From(b)] + PieceShift][Move2To(b)];
          b = ToMove(Move2From(b), Move2To(b), Move2Type(b), s2);
      }
      return s1 > s2;
   }
   const Position & p;
   const TT::Entry * e;
   const DepthType ply;
};

void sort(std::vector<Move> & moves, const Position & p, DepthType ply, const TT::Entry * e = NULL){
   std::sort(moves.begin(),moves.end(),MoveSorter(p,ply,e));
}

bool isDraw(const Position & p, unsigned int ply){
   ///@todo
   int count = 0;
   for (int k = ply-1; k >=0; --k) {
       if (hashStack[k] == 0) break;
       if (hashStack[k] == p.h) ++count;
       if (count > 2) return true;
   }
   if ( p.fifty >= 100 ) return true;
   return false;
}

Square kingSquare(const Position & p){
   return (p.c == Co_White) ? p.wk : p.bk;
}

bool apply(Position & p, const Move & m){
    
   if ( m == INVALIDMOVE ) return false;
    
   const Square from  = Move2From(m);
   const Square to    = Move2To(m);
   const MType  type  = Move2Type(m);
   const Piece  fromP = p.b[from];
   const Piece  toP   = p.b[to];
   
   switch(type){
      case T_std:
      case T_capture:
      case T_check:
      p.b[from] = P_none;
      p.b[to]   = fromP;

      p.h ^= ZT[from][fromP + PieceShift]; // remove fromP at from
      if (type == T_capture) p.h ^= ZT[to][toP + PieceShift]; // if capture remove toP at to
      p.h ^= ZT[to][fromP + PieceShift]; // add fromP at to

      // update castling rigths and king position
      if ( fromP == P_wk ){
         p.wk = to;
         if (p.castling & C_wks) p.h ^= ZT[7][13];
         if (p.castling & C_wqs) p.h ^= ZT[0][13];
         p.castling &= ~(C_wks | C_wqs);
      }
      else if ( fromP == P_bk ){
         p.bk = to;
         if (p.castling & C_bks) p.h ^= ZT[63][13];
         if (p.castling & C_bqs) p.h ^= ZT[56][13];
         p.castling &= ~(C_bks | C_bqs);
      }

      if ( fromP == P_wr && from == Sq_a1 && (p.castling & C_wqs)){
         p.castling &= ~C_wqs;
         p.h ^= ZT[0][13];
      }
      else if ( fromP == P_wr && from == Sq_h1 && (p.castling & C_wks) ){
         p.castling &= ~C_wks;
         p.h ^= ZT[7][13];
      }
      else if ( fromP == P_br && from == Sq_a8 && (p.castling & C_bqs)){
         p.castling &= ~C_bqs;
         p.h ^= ZT[56][13];
      }
      else if ( fromP == P_br && from == Sq_h8 && (p.castling & C_bks) ){
         p.castling &= ~C_bks;
         p.h ^= ZT[63][13];
      }
      break;

      case T_ep:
         p.b[from] = P_none;
         p.b[to] = fromP;
         p.b[p.ep + (p.c==Co_White?-8:+8)] = P_none;

         p.h ^= ZT[from][fromP + PieceShift]; // remove fromP at from
         p.h ^= ZT[p.ep + (p.c == Co_White ? -8 : +8)][(p.c == Co_White ? P_bp : P_wp) + PieceShift];
         p.h ^= ZT[to][fromP + PieceShift]; // add fromP at to
      break;

      case T_promq:
      case T_cappromq:
      if (type == T_capture) p.h ^= ZT[to][toP + PieceShift];
      p.b[to]   = (p.c==Co_White?P_wq:P_bq);
      p.b[from] = P_none;

      p.h ^= ZT[from][fromP + PieceShift];
      p.h ^= ZT[to][(p.c == Co_White ? P_wq : P_bq) + PieceShift];
      break;

      case T_promr:
      case T_cappromr:
      if (type == T_capture) p.h ^= ZT[to][toP + PieceShift];
      p.b[to]   = (p.c==Co_White?P_wr:P_br);
      p.b[from] = P_none;
      
      p.h ^= ZT[from][fromP + PieceShift];
      p.h ^= ZT[to][(p.c == Co_White ? P_wr : P_br) + PieceShift];
      break;

      case T_promb:
      case T_cappromb:
      if (type == T_capture) p.h ^= ZT[to][toP + PieceShift];
      p.b[to]   = (p.c==Co_White?P_wb:P_bb);
      p.b[from] = P_none;

      p.h ^= ZT[from][fromP + PieceShift];
      p.h ^= ZT[to][(p.c == Co_White ? P_wb : P_bb) + PieceShift];
      break;

      case T_promn:
      case T_cappromn:
      if (type == T_capture) p.h ^= ZT[to][toP + PieceShift];
      p.b[to]   = (p.c==Co_White?P_wn:P_bn);
      p.b[from] = P_none;

      p.h ^= ZT[from][fromP + PieceShift];
      p.h ^= ZT[to][(p.c == Co_White ? P_wn : P_bn) + PieceShift];
      break;

      case T_wks:
      p.b[Sq_e1] = P_none;
      p.b[Sq_f1] = P_wr;
      p.b[Sq_g1] = P_wk;
      p.wk = Sq_g1;
      p.b[Sq_h1] = P_none;
      if (p.castling & C_wqs) p.h ^= ZT[0][13];
      if (p.castling & C_wks) p.h ^= ZT[7][13];
      p.castling &= ~(C_wks | C_wqs);

      p.h ^= ZT[Sq_h1][P_wr + PieceShift]; // remove rook
      p.h ^= ZT[Sq_e1][P_wk + PieceShift]; // remove king
      p.h ^= ZT[Sq_f1][P_wr + PieceShift]; // add rook
      p.h ^= ZT[Sq_g1][P_wk + PieceShift]; // add king
      break;

      case T_wqs:
      p.b[Sq_a1] = P_none;
      p.b[Sq_b1] = P_none;
      p.b[Sq_c1] = P_wk;
      p.wk = Sq_c1;
      p.b[Sq_d1] = P_wr;
      p.b[Sq_e1] = P_none;
      if (p.castling & C_wqs) p.h ^= ZT[0][13];
      if (p.castling & C_wks) p.h ^= ZT[7][13];
      p.castling &= ~(C_wks | C_wqs);

      p.h ^= ZT[Sq_a1][P_wr + PieceShift]; // remove rook
      p.h ^= ZT[Sq_e1][P_wk + PieceShift]; // remove king
      p.h ^= ZT[Sq_d1][P_wr + PieceShift]; // add rook
      p.h ^= ZT[Sq_c1][P_wk + PieceShift]; // add king
      break;

      case T_bks:
      p.b[Sq_e8] = P_none;
      p.b[Sq_f8] = P_br;
      p.b[Sq_g8] = P_bk;
      p.bk = Sq_g8;
      p.b[Sq_h8] = P_none;
      if (p.castling & C_bqs) p.h ^= ZT[56][13];
      if (p.castling & C_bks) p.h ^= ZT[63][13];
      p.castling &= ~(C_bks | C_bqs);

      p.h ^= ZT[Sq_h8][P_br + PieceShift]; // remove rook
      p.h ^= ZT[Sq_e8][P_bk + PieceShift]; // remove king
      p.h ^= ZT[Sq_f8][P_br + PieceShift]; // add rook
      p.h ^= ZT[Sq_g8][P_bk + PieceShift]; // add king
      break;

      case T_bqs:
      p.b[Sq_a8] = P_none;
      p.b[Sq_b8] = P_none;
      p.b[Sq_c8] = P_bk;
      p.bk = Sq_c8;
      p.b[Sq_d8] = P_br;
      p.b[Sq_e8] = P_none;
      if (p.castling & C_bqs) p.h ^= ZT[56][13];
      if (p.castling & C_bks) p.h ^= ZT[63][13];
      p.castling &= ~(C_bks | C_bqs);

      p.h ^= ZT[Sq_a8][P_br + PieceShift]; // remove rook
      p.h ^= ZT[Sq_e8][P_bk + PieceShift]; // remove king
      p.h ^= ZT[Sq_d8][P_br + PieceShift]; // add rook
      p.h ^= ZT[Sq_c8][P_bk + PieceShift]; // add king
      break;

   }
   
   if ( isAttacked(p,kingSquare(p)) ) return false;

   // Update castling right if rook captured
   if ( toP == P_wr && to == Sq_a1 && (p.castling & C_wqs) ){
      p.castling &= ~C_wqs;
      p.h ^= ZT[0][13];
   }
   else if ( toP == P_wr && to == Sq_h1 && (p.castling & C_wks) ){
      p.castling &= ~C_wks;
      p.h ^= ZT[7][13];
   }
   else if ( toP == P_br && to == Sq_a8 && (p.castling & C_bqs)){
      p.castling &= ~C_bqs;
      p.h ^= ZT[56][13];
   }
   else if ( toP == P_br && to == Sq_h8 && (p.castling & C_bks)){
      p.castling &= ~C_bks;
      p.h ^= ZT[63][13];
   }

   // update EP
   if (p.ep != INVALIDSQUARE) p.h ^= ZT[p.ep][13];
   p.ep = INVALIDSQUARE;
   if ( abs(fromP) == P_wp && abs(to-from) == 16 ) p.ep = (from + to)/2;
   if (p.ep != INVALIDSQUARE) p.h ^= ZT[p.ep][13];
   
   p.c = Color((p.c+1)%2);
   if ( toP != P_none || abs(fromP) == P_wp ) p.fifty = 0;
   if ( p.c == Co_Black ) ++p.moves;

   p.h ^= ZT[3][13];
   p.h ^= ZT[4][13];

   return true;
}

int gamePhase(const Position & p){
   int absscore = 0;
   int nbPiece = 0;
   int nbPawn = 0;
   for( Square k = 0 ; k < 64 ; ++k){
      if ( p.b[k] != P_none ){
         absscore += std::abs(Values[p.b[k]+PieceShift]);
         if ( std::abs(p.b[k]) != P_wp ) ++nbPiece;
         else ++nbPawn;
      }
   }
   absscore = 100*(absscore-16000)    / (24140-16000);
   nbPawn   = 100*nbPawn      / 16;
   nbPiece  = 100*(nbPiece-2) / 14;
   //std::cout << absscore << " " << nbPawn << " " << nbPiece << std::endl;
   return int(absscore*0.4+nbPiece*0.3+nbPawn*0.3);
}

ScoreType eval(const Position & p, float & gp){
    ScoreType sc = 0;
   gp = gamePhase(p)/100.f;
   //std::cout << "phase: " << gp << std::endl;
   const bool white2Play = p.c == Co_White;
   for( Square k = 0 ; k < 64 ; ++k){
      if ( p.b[k] != P_none ){
         sc += Values[p.b[k]+PieceShift];
         const int s = Signs[p.b[k]+PieceShift];
         Square kk = k;
         if ( s > 0 ){
            kk = 63-k;
         }
         //std::cout << Names[p.b[k]+PieceShift] << " " << PST[abs(p.b[k])-1][kk] << " " << PSTEG[abs(p.b[k])-1][kk] << std::endl;
         sc += ScoreType(s * (gp*PST[abs(p.b[k])-1][kk] + (1.f-gp)*PSTEG[abs(p.b[k])-1][kk] ) );
      }
   }
   return (white2Play?+1:-1)*sc;
}

ScoreType qsearch(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply){

  alpha = std::max(alpha, (ScoreType)(-MATE + ply));
  beta  = std::min(beta , (ScoreType)(MATE - ply + 1));
  if (alpha >= beta) return alpha;
   
  float gp = 0;
  ScoreType val = eval(p,gp);
  if ( val >= beta ) return val;
  if ( val > alpha) alpha = val;

  // delta pruning
  if ( val-1000 > beta ) return val;
  if ( val+1000 < alpha) return val;

  ++stats.qnodes;

  std::vector<Move> moves;
  generate(p,moves,true);
  sort(moves,p,ply);

  for(auto it = moves.begin() ; it != moves.end() ; ++it){
     Position p2 = p;
     if ( ! apply(p2,*it) ) continue;
     if (p.c == Co_White && Move2To(*it) == p.bk) return MATE - ply;
     if (p.c == Co_Black && Move2To(*it) == p.wk) return MATE - ply;
     val = -qsearch(-beta,-alpha,p2,ply+1);
     if ( val > alpha ){
        if ( val >= beta ){
           return val;
        }
        alpha = val;
     }
  }
  return val;
}

ScoreType pvs(ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, bool pvnode, unsigned int ply, std::vector<Move> & pv){

  if ( std::max(1,(int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count()) > TimeMan::GetNextMSecPerMove() ){
    stopFlag = true;
    return STOPSCORE;
  }

  if ( depth <= 0 ){
     return qsearch(alpha,beta,p,ply);
  }

  if ( isDraw(p,ply) ) return 0;

  ++stats.nodes;

  alpha = std::max(alpha, (ScoreType)(-MATE + ply));
  beta  = std::min(beta,  (ScoreType)(MATE  - ply + 1));
  if (alpha >= beta) return alpha;

  bool rootnode = ply == 1;
  
  TT::Entry e;
#ifdef DEBUG_ZHASH
  if (TT::getEntry(computeHash(p), depth, e, GetFENShort2(p))) {
#else
  if (TT::getEntry(computeHash(p), depth, e)) {
#endif
      if (e.h != 0 && !rootnode && !pvnode && std::abs(e.score) < MATE - MAX_PLY &&
          ((e.b == TT::B_alpha && e.score <= alpha)
              || (e.b == TT::B_beta  && e.score >= beta)
              || (e.b == TT::B_exact))) {
          KillerT::killers[1][ply] = KillerT::killers[0][ply];
          KillerT::killers[0][ply] = e.m;
          pv.push_back(e.m);
          return e.score;
      }
  }

  float gp = 0;
  ScoreType val = eval(p, gp);
  bool futility = false;
  bool isInCheck = isAttacked(p,kingSquare(p));
  
  // prunings
  if ( !mateFinder 
      && !rootnode 
      && gp > 0.2 
      && !pvnode 
      && std::abs(alpha) < MATE-MAX_PLY 
      && std::abs(beta) < MATE-MAX_PLY 
      && ! isInCheck ){

     // static null move
     if (depth <= 3 && val >= beta + 80*depth ) return val;
  
     // razoring
     int rAlpha = alpha - 200;
     if ( depth <= 3 && val <= rAlpha ){
         val = qsearch(rAlpha,rAlpha+1,p,ply);
         if ( ! stopFlag && val <= alpha ) return val;
     }
  
     // null move
     if ( pv.size() > 1 && depth >=4 ){
       Position pN = p;
       pN.h = 0;
       pN.c = Color((pN.c+1)%2);
       p.h ^= ZT[3][13];
       p.h ^= ZT[4][13];
       int R = depth/4 + 3;
       std::vector<Move> nullPV;
       ScoreType nullscore = -pvs(-beta,-beta+1,pN,depth-R-1,true,ply+1,nullPV);
       if ( !stopFlag && nullscore >= beta ) return nullscore;
     }
     
     // LMP
     if ( val <= alpha - 130*depth ) futility = true;
  }

  std::vector<Move> moves;
  generate(p,moves);
  if ( moves.empty() ) return isInCheck?-MATE + ply : 0;
  sort(moves,p,ply,&e);
  int validMoveCount = 0;
  bool alphaUpdated = false;
  Move bestMove = e.m;
  for(auto it = moves.begin() ; it != moves.end() && !stopFlag ; ++it){
     Position p2 = p;
     if ( ! apply(p2,*it) ) continue;
     if (p.c == Co_White && Move2To(*it) == p.bk) return MATE - ply;
     if (p.c == Co_Black && Move2To(*it) == p.wk) return MATE - ply;
     validMoveCount++;
     hashStack[ply] = p.h;
     bool isAdvancedPawnPush = std::abs(p.b[Move2From(*it)]) == P_wp && (SQRANK(Move2To(*it)) > 5 || SQRANK(Move2To(*it)) < 2);
     std::vector<Move> childPV;
     if ( validMoveCount == 1 ){
        val = -pvs(-beta,-alpha,p2,depth-1,true,ply+1,childPV);
     }
     else{
        int reduction = 0;
        // LMP
        if ( !mateFinder 
            && futility 
            && !isAdvancedPawnPush
            && Move2Type(*it) == T_std
            && depth <= 10
            && validMoveCount >= 3*depth ) continue;
        // LMR
        if ( !mateFinder 
            && depth >= 3
            && !isInCheck
            && !isAdvancedPawnPush
            && Move2Type(*it) == T_std
            && validMoveCount >= 2 
            && std::abs(alpha) < MATE-MAX_PLY 
            && std::abs(beta) < MATE-MAX_PLY 
            && ! isInCheck ) reduction = int(1+sqrt(depth*validMoveCount/8));
        if (pvnode && reduction > 0) --reduction;
        // PVS
        val = -pvs(-alpha-1,-alpha,p2,depth-1-reduction,false,ply+1,childPV);
        if ( reduction > 0 && val > alpha ){
           childPV.clear();
           val = -pvs(-alpha-1,-alpha,p2,depth-1,false,ply+1,childPV);
        }
        if ( val > alpha && val < beta ){
           childPV.clear();
           val = -pvs(-beta,-alpha,p2,depth-1,true,ply+1,childPV); // potential new pv node
        }
     }
     
     if ( !stopFlag && val > alpha ){
        alphaUpdated = true;
        pv.clear();
        pv.push_back(*it);
        std::copy(childPV.begin(),childPV.end(),std::back_inserter(pv));
        if ( val >= beta ){
           if ( Move2Type(*it) == T_std ){
              KillerT::killers[1][ply] = KillerT::killers[0][ply];
              KillerT::killers[0][ply] = *it;
              HistoryT::update(depth,*it,p,true);
              for(auto it2 = moves.begin() ; *it2!=*it; ++it2){
                  if ( Move2Type(*it2) == T_std ){
                     HistoryT::update(depth,*it2,p,false);
                  }
              }
           }
#ifdef DEBUG_ZHASH
           TT::setEntry({*it,val,TT::B_beta,DepthType(depth-1),computeHash(p),GetFENShort2(p)});
#else
           TT::setEntry({ *it,val,TT::B_beta,DepthType(depth - 1),computeHash(p) });
#endif
           return val;
        }
        alpha = val;
        bestMove = *it;
     }
  }

  if ( validMoveCount == 0 ){
     return isInCheck?-MATE + ply : 0;
  }

  if ( bestMove != INVALIDMOVE && !stopFlag){
#ifdef DEBUG_ZHASH
     TT::setEntry({bestMove,alpha,alphaUpdated?TT::B_exact:TT::B_alpha,DepthType(depth-1),computeHash(p),GetFENShort2(p)});
#else
     TT::setEntry({ bestMove,alpha,alphaUpdated ? TT::B_exact : TT::B_alpha,DepthType(depth - 1),computeHash(p) });
#endif
  }

  return stopFlag?STOPSCORE:alpha;
}

std::vector<Move> search(const Position & p, Move & m, DepthType & d, ScoreType & sc){
  std::cout << "# Search called" << std::endl;
  stopFlag = false;

  stats.init();
  
  KillerT::initKillers();
  HistoryT::initHistory();
  
  TimeMan::startTime = Clock::now();
  
  DepthType reachedDepth = 0;
  std::vector<Move> pv;
  ScoreType bestScore = 0;
  m = INVALIDMOVE;
  
  hashStack[0] = p.h;

  for(DepthType depth = 1 ; depth <= d && !stopFlag ; ++depth ){
    std::cout << "# Iterative deepening " << (int)depth << std::endl;
    std::vector<Move> pvLoc;
    ScoreType delta = (depth>4)?25:MATE;
    ScoreType alpha = std::max(ScoreType(bestScore - delta), ScoreType (-INFSCORE));
    ScoreType beta  = std::min(ScoreType(bestScore + delta), INFSCORE);
    ScoreType score = 0;
    while( delta <= MATE ){
       pvLoc.clear();
       score = pvs(alpha,beta,p,depth,true,1,pvLoc);
       if ( stopFlag ) break;
       delta *=2;
       if (score <= alpha){
           std::cout << "# Windows research alpha " << alpha << ", score : " << score << ", delta " << delta << std::endl;
           alpha = std::max(ScoreType(score - delta), ScoreType(-MATE) );
       }
       else if (score >= beta ){
           std::cout << "# Windows research beta" << ", score : " << score << ", delta " << delta << std::endl;
           beta  = std::min(ScoreType(score + delta),  MATE );
       }
       else break;
    }

    if ( !stopFlag ){
       pv = pvLoc;
       reachedDepth = depth;
       bestScore    = score;
       
       int ms = std::max(1,(int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count());
       std::cout << int(depth) << " " << bestScore << " " << ms/10 << " " << stats.nodes + stats.qnodes << " " << ToString(pv) << " " << int((stats.nodes + stats.qnodes)/(ms/1000.f)/1000.) << "knps " << stats.tthits/1000 << "ktbhits" << std::endl;
    }
    
    if (bestScore <= -MATE+1) {
        break;
    }
    
    if ( mateFinder && bestScore >= MATE - MAX_PLY ){
        break;
    }
  }
  
  if (bestScore <= -MATE+1) {
      std::cout << "# Mated ..." << std::endl;
      pv.clear();
      pv.push_back(INVALIDMOVE);
  }

  if (bestScore >= MATE-MAX_PLY) {
      std::cout << "# Forced mate found ..." << std::endl;
  }  
  
  if (pv.empty()) {
      m = INVALIDMOVE;
  }
  else {
      m = pv[0];
  }
  d = reachedDepth;
  sc = bestScore;
  return pv;
}

bool ReadFEN(const std::string & fen, Position & p){
    std::vector<std::string> strList;
    std::stringstream iss(fen);
    std::copy(std::istream_iterator<std::string>(iss),
              std::istream_iterator<std::string>(),
              back_inserter(strList));

    std::cout << "# Reading fen " << fen << std::endl;
    
    for(Square k = 0 ; k < 64 ; ++k){
       p.b[k] = P_none;
    }

    Square j = 1;
    Square i = 0;
    while ((j <= 64) && (i <= (char)strList[0].length())){
        char letter = strList[0].at(i);
        ++i;
        Square r = 7-(j-1)/8;
        Square f = (j-1)%8;
        Square k = r*8+f;
        switch (letter)
        {
        case 'p': p.b[k]= P_bp; break;
        case 'r': p.b[k]= P_br; break;
        case 'n': p.b[k]= P_bn; break;
        case 'b': p.b[k]= P_bb; break;
        case 'q': p.b[k]= P_bq; break;
        case 'k': p.b[k]= P_bk; p.bk = k; break;
        case 'P': p.b[k]= P_wp; break;
        case 'R': p.b[k]= P_wr; break;
        case 'N': p.b[k]= P_wn; break;
        case 'B': p.b[k]= P_wb; break;
        case 'Q': p.b[k]= P_wq; break;
        case 'K': p.b[k]= P_wk; p.wk = k; break;
        case '/': j--; break;
        case '1': break;
        case '2': j++; break;
        case '3': j += 2; break;
        case '4': j += 3; break;
        case '5': j += 4; break;
        case '6': j += 5; break;
        case '7': j += 6; break;
        case '8': j += 7; break;
        default: std::cout << "#FEN ERROR 0 : " << letter << std::endl;
        }
        j++;
    }

    // set the turn; default is white
    p.c = Co_White;
    if (strList.size() >= 2){
        if (strList[1] == "w") {
            p.c = Co_White;
        }
        else if (strList[1] == "b") {
            p.c = Co_Black;
        }
        else {
            std::cout << "#FEN ERROR 1" << std::endl;
            return false;
        }
    }

    // Initialize all castle possibilities (default is none)
    p.castling = C_none;
    if (strList.size() >= 3){
        bool found = false;
        if (strList[2].find('K') != std::string::npos){
            p.castling |= C_wks;
            found = true;
        }
        if (strList[2].find('Q') != std::string::npos){
            p.castling |= C_wqs;
            found = true;
        }
        if (strList[2].find('k') != std::string::npos){
            p.castling |= C_bks;
            found = true;
        }
        if (strList[2].find('q') != std::string::npos){
            p.castling |= C_bqs;
            found = true;
        }
        if ( ! found ){
            std::cout << "#No castling right given" << std::endl;
        }
    }
    else{
        std::cout << "#No castling right given" << std::endl;
    }

    // read en passant and save it (default is invalid)
    p.ep = -1;
    if ((strList.size() >= 4) && strList[3] != "-" ){
        if (strList[3].length() >= 2){
            if ((strList[3].at(0) >= 'a') && (strList[3].at(0) <= 'h') &&
                    ((strList[3].at(1) == '3') || (strList[3].at(1) == '6'))){
                int f = strList[3].at(0)-97;
                int r = strList[3].at(1);
                p.ep = f + 8*r;
            }
            else {
                std::cout << "#FEN ERROR 3-1 : bad en passant square : " << strList[3] << std::endl;
                return false;
            }
        }
        else{
            std::cout << "#FEN ERROR 3-2 : bad en passant square : " << strList[3] << std::endl;
            return false;
        }
    }
    else{
        std::cout << "#No en passant square given" << std::endl;
    }

    // read 50 moves rules
    if (strList.size() >= 5){
        std::stringstream ss(strList[4]);
        int tmp;
        ss >> tmp;
        p.fifty = tmp;
    }
    else{
        p.fifty = 0;
    }

    // read number of move
    if (strList.size() >= 6){
        std::stringstream ss(strList[5]);
        int tmp;
        ss >> tmp;
        p.moves = tmp;
    }
    else{
        p.moves = 1;
    }

    p.h = 0;
    p.h = computeHash(p);

    return true;
}

struct PerftAccumulator{
   PerftAccumulator():
      pseudoNodes(0),
      validNodes(0),
      captureNodes(0),
      epNodes(0),
      checkNode(0),
      checkMateNode(0){}
      
   Counter pseudoNodes;
   Counter validNodes;
   Counter captureNodes;
   Counter epNodes;
   Counter checkNode;
   Counter checkMateNode;

   void Display(){
      std::cout << "#pseudoNodes   " << pseudoNodes   << std::endl;
      std::cout << "#validNodes    " << validNodes    << std::endl;
      std::cout << "#captureNodes  " << captureNodes  << std::endl;
      std::cout << "#epNodes       " << epNodes       << std::endl;
      std::cout << "#checkNode     " << checkNode     << std::endl;
      std::cout << "#checkMateNode " << checkMateNode << std::endl;
   }
};

Counter perft(const Position & p, DepthType depth, PerftAccumulator & acc, bool divide = false){
   if ( depth == 0) return 0;
   std::vector<Move> moves;
   generate(p,moves);
   if ( depth == 1 ){
      acc.pseudoNodes += moves.size();
   }
   int validMoves = 0;
   for(auto it = moves.begin() ; it != moves.end() ; ++it){
     Position p2 = p;
     if ( ! apply(p2,*it) ) continue;
     ++validMoves;
     if ( divide && depth == 2 ){
        std::cout << "#" << ToString(p2) << std::endl;
     } 
     Counter nNodes = perft(p2,depth-1,acc,divide);
     if ( divide && depth == 2 ){
        std::cout << "#=> after " << ToString(*it) << " " << nNodes << std::endl;
     }
     if ( divide && depth == 1 ){
        std::cout << "#" << (int)depth << " " <<  ToString(*it) << std::endl;
     }     
   }
   if ( depth == 1 ){
      acc.validNodes += validMoves;
   }
   if ( divide && depth == 2 ){
      std::cout << "#********************" << std::endl;
   }
   return acc.validNodes;
}

namespace XBoard{
   bool display;
   bool pondering;

   enum Mode : unsigned char {
        m_play_white = 0,
        m_play_black = 1,
        m_force = 2,
        m_analyze = 3
   };
   Mode mode;

   enum SideToMove : unsigned char {
        stm_white = 0,
        stm_black = 1
   };
   SideToMove stm;

   enum Ponder : unsigned char {
        p_off = 0,
        p_on = 1
   };
   Ponder ponder;
   
   std::string command;
   Position position;
   Move move;
   DepthType depth;

   void init(){
      ponder    = p_off;
      mode      = m_analyze;
      stm       = stm_white;
      depth     = -1;
      display   = false;
      pondering = false;
      ReadFEN(startPosition,position);
   }
   
   SideToMove Opponent(SideToMove & s) {
     return s == stm_white ? stm_black : stm_white;
   }

   void ReadLine() {
   #define CPP_STYLE_READ_LINE
   #ifdef CPP_STYLE_READ_LINE
       command.clear();
       std::getline(std::cin, command);
   #else
       static char com[4096];
       char *np;
       if ((fgets(com, sizeof com, stdin)) != NULL) {
           if (np = strchr(com, '\n')) { // replace \n with \0 ...
               *np = '\0';
           }
       }
       command = com;
   #endif
       std::cout << "#Receive command : " << command << std::endl;
   }

   bool SideToMoveFromFEN(const std::string & fen) {
     bool b = ReadFEN(fen,position);
     stm = position.c == Co_White ? stm_white : stm_black;
     return b;
   }

   Move ThinkUntilTimeUp(){
      std::cout << "#Thinking... " << std::endl;
      ScoreType score = 0;
      Move m;
      if ( depth < 0 ) depth = 64;
      std::cout << "#depth " << (int)depth << std::endl;
      std::cout << "#time  " << TimeMan::GetNextMSecPerMove() << std::endl;
      std::cout << ToString(position) << std::endl;
      DepthType reachedDepth = depth;
      std::vector<Move> pv = search(position,m,reachedDepth,score);
      std::cout << "#...done returning move " << ToString(m) << std::endl;
      return m;
   }

   bool MakeMove(Move m,bool disp){
      if ( disp && m != INVALIDMOVE) std::cout << "move " << ToString(m) << std::endl;
      
      std::cout << ToString(position) << std::endl;
      return apply(position,m);
   }

   void PonderUntilInput(){
      std::cout << "#Pondering... " << std::endl;
      ScoreType score = 0;
      Move m;
      DepthType d = 1;
      std::vector<Move> pv = search(position,m,d,score);
   }

   void Stop(){
      stopFlag = true;
   }

   void StopPonder(){
      if ((int)mode == (int)Opponent(stm) && ponder == p_on && pondering) {
        pondering = false;
        Stop();
      }
   }

   void xboard();
   
};

void Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters = " " ){
  std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
  std::string::size_type pos = str.find_first_of(delimiters, lastPos);
  while (std::string::npos != pos || std::string::npos != lastPos) {
    tokens.push_back(str.substr(lastPos, pos - lastPos));
    lastPos = str.find_first_not_of(delimiters, pos);
    pos = str.find_first_of(delimiters, lastPos);
  }
}

Square StringToSquare(const std::string & str){
   Square file = str.at(0) - 97; // ASCII 'a' = 97
   Square rank = str.at(1) - 49; // ASCII '1' = 49
   return rank * 8 + file;
}

bool ReadMove(const Color c, const std::string & ss, Square & from, Square & to, MType & moveType ) {

    if ( ss.empty()){
        std::cout << "#Trying to read empty move ! " << std::endl;
        moveType = T_std;
        return false;
    }

    std::string str(ss);

    // add space to go to own internal notation
    if ( str != "0-0" && str != "0-0-0" && str != "O-O" && str != "O-O-O" ){
        str.insert(2," ");
    }

    std::vector<std::string> strList;
    std::stringstream iss(str);
    std::copy(std::istream_iterator<std::string>(iss),
              std::istream_iterator<std::string>(),
              back_inserter(strList));

    moveType = T_std;
    if ( strList.empty()){
        std::cout << "#Trying to read bad move, seems empty " << str << std::endl;
        return false;
    }

    // detect special move
    if (strList[0] == "0-0" || strList[0] == "O-O"){
        if ( c == Co_White ){
            moveType = T_wks;
        }
        else{
            moveType = T_bks;
        }
    }
    else if (strList[0] == "0-0-0" || strList[0] == "O-O-O"){
        if ( c == Co_White){
            moveType = T_wqs;
        }
        else{
            moveType = T_bqs;
        }
    }
    else{
        if ( strList.size() == 1 ){
            std::cout << "#Trying to read bad move, malformed (=1) " << str << std::endl;
            return false;
        }
        if ( strList.size() > 2 && strList[2] != "ep"){
            std::cout << "#Trying to read bad move, malformed (>2)" << str << std::endl;
            return false;
        }

        if (strList[0].size() == 2 && (strList[0].at(0) >= 'a') && (strList[0].at(0) <= 'h') &&
                ((strList[0].at(1) >= 1) && (strList[0].at(1) <= '8'))) {
            from = StringToSquare(strList[0]);
        }
        else {
            std::cout << "#Trying to read bad move, invalid from square " << str << std::endl;
            return false;
        }

        // be carefull, promotion possible !
        if (strList[1].size() >= 2 && (strList[1].at(0) >= 'a') && (strList[1].at(0) <= 'h') &&
                ((strList[1].at(1) >= '1') && (strList[1].at(1) <= '8'))) {
            if ( strList[1].size() > 2 ){ // promotion
                std::string prom;
                if ( strList[1].size() == 3 ){ // probably e7 e8q notation
                   prom = strList[1][2];
                   to = StringToSquare(strList[1].substr(0,2));
                }
                else{ // probably e7 e8=q notation
                   std::vector<std::string> strListTo;
                   Tokenize(strList[1],strListTo,"=");
                   to = StringToSquare(strListTo[0]);
                   prom = strListTo[1];
                }

                if ( prom == "Q" || prom == "q"){
                    moveType = T_promq;
                }
                else if ( prom == "R" || prom == "r"){
                    moveType = T_promr;
                }
                else if ( prom == "B" || prom == "b"){
                    moveType = T_promb;
                }
                else if ( prom == "N" || prom == "n"){
                    moveType = T_promn;
                }
                else{
                    std::cout << "#Trying to read bad move, invalid to square " << str << std::endl;
                    return false;
                }
            }
            else{
               to = StringToSquare(strList[1]);
            }
        }
        else {
            std::cout << "#Trying to read bad move, invalid to square " << str << std::endl;
            return false;
        }

    }

    return true;
}

std::string Trim(const std::string& str, const std::string& whitespace = " \t"){
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

void XBoard::xboard(){

    std::cout << "#Starting XBoard main loop" << std::endl;

    ///@todo more feature disable !!
    std::cout << "feature ping=1 setboard=1 colors=0 usermove=1 memory=0 sigint=0 sigterm=0 otime=0 time=0 nps=0 myname=\"Minic 0.1\"" << std::endl;
    std::cout << "feature done=1" << std::endl;

    while(true) {
        std::cout << "#XBoard: mode " << mode << std::endl;
        std::cout << "#XBoard: stm  " << stm << std::endl;
        if(mode == m_analyze){
            //AnalyzeUntilInput(); ///@todo
        }
        // move as computer if mode is equal to stm
        if((int)mode == (int)stm) { // mouarfff
            move = ThinkUntilTimeUp();
            if(move == INVALIDMOVE){ // game ends
                mode = m_force;
            }
            else{
                if ( ! MakeMove(move,true) ){
                    std::cout << "#Bad computer move !" << std::endl;
                    std::cout << ToString(position) << std::endl;
                }
                stm = Opponent(stm);
            }
        }

        // if not our turn, and ponder on, let's think ...
        if(move != INVALIDMOVE && (int)mode == (int)Opponent(stm) && ponder == p_on && !pondering){
           PonderUntilInput();
        }

        bool commandOK = true;
        int once = 0;

        while(once++ == 0 || !commandOK){ // loop until a good command is found

            commandOK = true;

            // read next command !
            ReadLine();

            if ( command == "force"){
                mode = m_force;
            }
            else if ( command == "xboard"){
                std::cout << "#This is minic!" << std::endl;
            }
            else if ( command == "post"){
                display = true;
            }
            else if ( command == "nopost"){
                display = false;
            }
            else if ( command == "computer"){
                // nothing !
            }
            else if ( strncmp(command.c_str(),"protover",8) == 0){
                // nothing !
            }
            else if ( strncmp(command.c_str(),"accepted",8) == 0){
                // nothing !
            }
            else if ( strncmp(command.c_str(),"rejected",8) == 0){
                // nothing !
            }
            else if ( strncmp(command.c_str(),"ping",4) == 0){
                std::string str(command);
                size_t p = command.find("ping");
                str = str.substr(p+4);
                str = Trim(str);
                std::cout << "pong " << str << std::endl;
            }
            else if ( command == "new"){
                StopPonder();
                Stop();

                mode = (Mode)((int)stm); ///@todo should not be here !!! I thought start mode shall be analysis ...
                ReadFEN(startPosition,position);
                move = INVALIDMOVE;
                if(mode != m_analyze){
                    mode = m_play_black;
                    stm = stm_white;
                }
            }
            else if ( command == "white"){ // deprecated
                StopPonder();
                Stop();

                mode = m_play_black;
                stm = stm_white;
            }
            else if ( command == "black"){ // deprecated
                StopPonder();
                Stop();

                mode = m_play_white;
                stm = stm_black;
            }
            else if ( command == "go"){
                StopPonder();
                Stop();

                mode = (Mode)((int)stm);
            }
            else if ( command == "playother"){
                StopPonder();
                Stop();

                mode = (Mode)((int)Opponent(stm));
            }
            else if ( strncmp(command.c_str(),"usermove",8) == 0){
                StopPonder();
                Stop();

                std::string mstr(command);
                size_t p = command.find("usermove");
                mstr = mstr.substr(p + 8);
                mstr = Trim(mstr);
                Square from = INVALIDSQUARE;
                Square to   = INVALIDSQUARE;
                MType mtype = T_std;
                ReadMove(position.c,mstr,from,to,mtype);
                Move m = ToMove(from,to,mtype);
                bool whiteToMove = position.c==Co_White;
                // convert castling Xboard notation to internal castling style if needed
                if ( mtype == T_std &&
                  from == (whiteToMove?Sq_e1:Sq_e8) &&
                  position.b[from] == (whiteToMove?P_wk:P_bk) ){
                  if ( to == (whiteToMove?Sq_c1:Sq_c8)){
                    m = ToMove(from,to,whiteToMove?T_wqs:T_bqs);
                  }
                  else if ( to == (whiteToMove?Sq_g1:Sq_g8)){
                    m = ToMove(from,to,whiteToMove?T_wks:T_bks);
                  }
                }    
                if(!MakeMove(m,false)){ // make move
                    commandOK = false;
                    std::cout << "#Bad opponent move !" << std::endl;
                    std::cout << ToString(position) << std::endl;
                }
                else{
                    stm = Opponent(stm);
                }
            }
            else if (  strncmp(command.c_str(),"setboard",8) == 0){
                StopPonder();
                Stop();

                std::string fen(command);
                size_t p = command.find("setboard");
                fen = fen.substr(p+8);
                if (!SideToMoveFromFEN(fen)){
                    std::cout << "#Illegal FEN" << std::endl;
                    commandOK = false;
                }
            }
            else if ( strncmp(command.c_str(),"result",6) == 0){
                StopPonder();
                Stop();
                mode = m_force;
            }
            else if ( command == "analyze"){
                StopPonder();
                Stop();
                mode = m_analyze;
            }
            else if ( command == "exit"){
                StopPonder();
                Stop();
                mode = m_force;
            }
            else if ( command == "easy"){
                StopPonder();
                ponder = p_off;
            }
            else if ( command == "hard"){
                ponder = p_off; ///@todo
            }
            else if ( command == "quit"){
                _exit(0);
            }
            else if ( command == "pause"){
                StopPonder();
                ReadLine();
                while( command != "resume"){
                    std::cout << "#Error (paused): " << command << std::endl;
                    ReadLine();
                }
            }
            else if ( strncmp(command.c_str(), "st", 2) == 0) {
                int msecPerMove = 0;
                sscanf(command.c_str(), "st %d", &msecPerMove);
                msecPerMove *= 1000;
                // forced move time
                TimeMan::isDynamic                = false;
                TimeMan::nbMoveInTC               = -1;
                TimeMan::msecPerMove              = msecPerMove;
                TimeMan::msecWholeGame            = -1;
                TimeMan::msecInc                  = -1;
                depth                             = 64; // infinity
                std::cout << "msecPerMove   " << TimeMan::msecPerMove << std::endl;
                std::cout << "msecWholeGame " << TimeMan::msecWholeGame << std::endl;
                std::cout << "msecInc       " << TimeMan::msecInc << std::endl;
                std::cout << "nbMoveInTC    " << TimeMan::nbMoveInTC << std::endl;
                std::cout << "depth         " << depth << std::endl;
            }
            else if ( strncmp(command.c_str(), "sd", 2) == 0) {
                int d = 0;
                sscanf(command.c_str(), "sd %d", &d);
                depth = d;
                if(depth<0) depth = 8;
                // forced move depth
                TimeMan::isDynamic                = false;
                TimeMan::nbMoveInTC               = -1;
                TimeMan::msecPerMove              = 60*60*1000*24; // 1 day == infinity ...
                TimeMan::msecWholeGame            = -1;
                TimeMan::msecInc                  = -1;
                std::cout << "msecPerMove   " << TimeMan::msecPerMove << std::endl;
                std::cout << "msecWholeGame " << TimeMan::msecWholeGame << std::endl;
                std::cout << "msecInc       " << TimeMan::msecInc << std::endl;
                std::cout << "nbMoveInTC    " << TimeMan::nbMoveInTC << std::endl;
                std::cout << "depth         " << depth << std::endl;
            }
            else if(strncmp(command.c_str(), "level",5) == 0) {
                int timeLeft = 0;
                int sec = 0;
                int inc = 0;
                int mps = 0;
                if( sscanf(command.c_str(), "level %d %d %d", &mps, &timeLeft, &inc) != 3) {
                  sscanf(command.c_str(), "level %d %d:%d %d", &mps, &timeLeft, &sec, &inc);
                }
                timeLeft *= 60000;
                timeLeft += sec * 1000;
                int msecinc = inc * 1000;
                TimeMan::isDynamic                = false;
                TimeMan::nbMoveInTC               = mps;
                TimeMan::msecPerMove              = -1;
                TimeMan::msecWholeGame            = timeLeft;
                TimeMan::msecInc                  = msecinc;
                depth                             = 64; // infinity
                std::cout << "msecPerMove   " << TimeMan::msecPerMove << std::endl;
                std::cout << "msecWholeGame " << TimeMan::msecWholeGame << std::endl;
                std::cout << "msecInc       " << TimeMan::msecInc << std::endl;
                std::cout << "nbMoveInTC    " << TimeMan::nbMoveInTC << std::endl;
                std::cout << "depth         " << depth << std::endl;
            }
            else if ( command == "edit"){
                ///@todo xboard edit
                ;
            }
            else if ( command == "?"){
                Stop();
            }
            else if ( command == "draw"){
                ///@todo xboard draw
                ;
            }
            else if ( command == "undo"){
                ///@todo xboard undo
                ;
            }
            else if ( command == "remove"){
                ///@todo xboard remove
                ;
            }
            //************ end of Xboard command ********//
            else{
                // old move syntax
                std::cout << "#Xboard does not know this command " << command << std::endl;
            }

        } // readline

    } // while true

}

int main(int argc, char ** argv){
   if ( argc < 2 ) return 1;

   initHash();
   TT::initTable(1024*1024);
   stats.init();

   std::string cli = argv[1];

   if ( cli == "-xboard" ){
      XBoard::init();
      TimeMan::init();
      XBoard::xboard();
      return 0;
   }

   if ( cli == "-perft_test" ){
      {
         Position p;
         int expected = 4865609;
         std::string fen = startPosition;
         ReadFEN(fen,p);
         std::cout << ToString(p) << std::endl;
         DepthType d = 5;
         PerftAccumulator acc;
         if ( perft(p,d,acc,false) != expected ){
            std::cout << "Error !! " << fen << " " << expected << std::endl;
         }
         acc.Display();
      }
      std::cout << "##########################" << std::endl;
      {
         Position p;
         int expected = 4085603;
         std::string fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ";
         ReadFEN(fen,p);
         std::cout << ToString(p) << std::endl;
         DepthType d = 4;
         PerftAccumulator acc;
         if ( perft(p,d,acc,false) != expected ){
            std::cout << "Error !! " << fen << " " << expected << std::endl;
         }
         acc.Display();
      }
      std::cout << "##########################" << std::endl;
      {
         Position p;
         int expected = 11030083;
         std::string fen = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ";
         ReadFEN(fen,p);
         std::cout << ToString(p) << std::endl;
         DepthType d = 6;
         PerftAccumulator acc;
         if ( perft(p,d,acc,false) != expected ){
            std::cout << "Error !! " << fen << " " << expected << std::endl;
         }
         acc.Display();
      }
      std::cout << "##########################" << std::endl;
      {
         Position p;
         int expected = 15833292;
         std::string fen = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
         ReadFEN(fen,p);
         std::cout << ToString(p) << std::endl;
         DepthType d = 5;
         PerftAccumulator acc;
         if ( perft(p,d,acc,false) != expected ){
            std::cout << "Error !! " << fen << " " << expected << std::endl;
         }
         acc.Display();
      }
   }   
   
   if ( argc < 3 ) return 1;

   std::string fen = argv[2];
   
   if ( fen == "start") fen = startPosition;
   if ( fen == "fine70") fen = fine70;
   
   Position p;
   if ( ! ReadFEN(fen,p) ){
      std::cout << "#Error reading fen" << std::endl;
      return 1;
   }
   std::cout << ToString(p) << std::endl;

   if (cli == "-test_hash") {
       ///@todo
   }

   if ( cli == "-eval" ){
      float gp = 0;
      int score = eval(p,gp);
      std::cout << "#eval " << score << " phase " << gp << std::endl;
      return 0;
   }
   
   if ( cli == "-gen" ){
      std::vector<Move> moves;
      generate(p,moves);
      sort(moves,p,0);
      std::cout << "#nb moves : " << moves.size() << std::endl;
      for(auto it = moves.begin(); it != moves.end(); ++it){
         std::cout << ToString(*it) << std::endl;
      }
      return 0;
   }

   if ( cli == "-testmove" ){
      Move m = ToMove(8,16,T_std);
      Position p2 = p;
      apply(p2,m);
      std::cout << ToString(p2) << std::endl;
      return 0;
   }

   if ( cli == "-perft" ){
      DepthType d = 5;
      if ( argc >= 3 ){
         d = atoi(argv[3]);
      };
      PerftAccumulator acc;
      perft(p,d,acc,false);
      acc.Display();
      return 0;
   }

   if ( cli == "-analyze" ){
      DepthType d = 5;
      if ( argc >= 3 ){
         d = atoi(argv[3]);
      };
      Move bestMove = INVALIDMOVE;
      ScoreType s;
      TimeMan::isDynamic                = false;
      TimeMan::nbMoveInTC               = -1;
      TimeMan::msecPerMove              = 60*60*1000*24; // 1 day == infinity ...
      TimeMan::msecWholeGame            = -1;
      TimeMan::msecInc                  = -1;      
      std::vector<Move> pv = search(p,bestMove,d,s);
      std::cout << "#best move is " << ToString(bestMove) << " " << (int)d << " " << s << " pv : " << ToString(pv) << std::endl;
      return 0;
   }

   if ( cli == "-mateFinder" ){
      mateFinder = true;
      DepthType d = 5;
      if ( argc >= 3 ){
         d = atoi(argv[3]);
      };
      Move bestMove = INVALIDMOVE;
      ScoreType s;
      TimeMan::isDynamic                = false;
      TimeMan::nbMoveInTC               = -1;
      TimeMan::msecPerMove              = 60*60*1000*24; // 1 day == infinity ...
      TimeMan::msecWholeGame            = -1;
      TimeMan::msecInc                  = -1;      
      std::vector<Move> pv = search(p,bestMove,d,s);
      std::cout << "#best move is " << ToString(bestMove) << " " << (int)d << " " << s << " pv : " << ToString(pv) << std::endl;
      return 0;
   }   
   
   std::cout << "Error : unknown command line" << std::endl;
   return 1;
   
}
