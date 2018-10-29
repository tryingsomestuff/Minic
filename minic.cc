#include <algorithm>
#include <assert.h>
#include <cmath>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <sstream>
#include <set>
#include <chrono>
#include <random>
#include <unordered_map>
#ifdef _WIN32
#include <stdlib.h>
#else
#include <unistd.h>
#endif

//#define IMPORTBOOK

const std::string MinicVersion = "0.9";

typedef std::chrono::high_resolution_clock Clock;
typedef char DepthType;
typedef int Move;    // invalid if < 0
typedef char Square; // invalid if < 0
typedef unsigned long long int Hash; // invalid if == 0
typedef unsigned long long int Counter;
typedef short int ScoreType;

#define STOPSCORE   ScoreType(20000)
#define INFSCORE    ScoreType(10000)
#define MATE        ScoreType(9000)
#define INVALIDMOVE    -1
#define INVALIDSQUARE  -1
#define MAX_PLY       512
#define MAX_MOVE      512
#define MAX_DEPTH     64

#define SQFILE(s) (s%8)
#define SQRANK(s) (s/8)

const bool doWindow         = true;
const bool doPVS            = true;
const bool doNullMove       = true;
const bool doFutility       = true;
const bool doLMR            = true;
const bool doLMP            = true;
const bool doStaticNullMove = true;
const bool doRazoring       = true;
const bool doQFutility      = true;

const ScoreType qfutilityMargin          = 128;
const int       staticNullMoveMaxDepth   = 3;
const ScoreType staticNullMoveDepthCoeff = 160;
const ScoreType razoringMargin           = 200;
const int       razoringMaxDepth         = 3;
const int       nullMoveMinDepth         = 2;
const int       lmpMaxDepth              = 10;
const ScoreType futilityDepthCoeff       = 160;
const int       iidMinDepth              = 5;
const int       lmrMinDepth              = 3;

const int lmpLimit[][lmpMaxDepth + 1] = {
    { 0, 3, 4, 6, 10, 15, 21, 28, 36, 45, 55 } , // improved
    { 0, 5, 6, 9, 15, 23, 32, 42, 54, 68, 83 } };// not improving

int lmrReduction[MAX_DEPTH][MAX_MOVE];

void init_lmr(){
    for (int d = 0; d < MAX_DEPTH; d++)
        for (int m = 0; m < MAX_MOVE; m++)
            lmrReduction[d][m] = (int)sqrt(float(d) * m / 8.f);
}

const unsigned int ttSizeMb  = 128; // here in Mb, will be converted to real size next
const unsigned int ttESizeMb = 128; // here in Mb, will be converted to real size next

bool mateFinder = false;

Hash hashStack[MAX_PLY] = { 0 };
ScoreType scoreStack[MAX_PLY] = { 0 };

std::string startPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
std::string fine70 = "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - -";

int currentMoveMs = 777;

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
   P_bk = -6,   P_bq = -5,   P_br = -4,   P_bb = -3,   P_bn = -2,   P_bp = -1,
   P_none = 0,
   P_wp = 1,    P_wn = 2,    P_wb = 3,    P_wr = 4,    P_wq = 5,    P_wk = 6
};

const int PieceShift = 6;

ScoreType   Values[13]    = { -8000, -950, -500, -335, -325, -100, 0, 100, 325, 335, 500, 950, 8000 };
ScoreType   ValuesEG[13]  = { -8000, -1200, -550, -305, -275, -100, 0, 100, 275, 305, 550, 1200, 8000 };
std::string Names[13]     = { "k", "q", "r", "b", "n", "p", " ", "P", "N", "B", "R", "Q", "K" };

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

enum Castling : char{  C_none= 0,   C_wks = 1,   C_wqs = 2,   C_bks = 4,   C_bqs = 8 };

int MvvLvaScores[6][6];

void initMvvLva(){
    static ScoreType IValues[6] = { 1, 2, 3, 5, 9, 20 };
    for(int v = 0; v < 6 ; ++v)
        for(int a = 0; a < 6 ; ++a)
           MvvLvaScores[v][a] = IValues[v] * 20 - IValues[a];
}

enum MType : char{
   T_std        = 0,
   T_capture    = 1,
   T_ep         = 2,
   T_check      = 3, // not used yet
   T_promq      = 4,   T_promr      = 5,   T_promb      = 6,   T_promn      = 7,
   T_cappromq   = 8,   T_cappromr   = 9,   T_cappromb   = 10,  T_cappromn   = 11,
   T_wks        = 12,   T_wqs       = 13,  T_bks        = 14,  T_bqs        = 15
};

enum Color : char{ Co_None  = -1,   Co_White = 0,   Co_Black = 1 };

// ttmove 3000, promcap >1000, cap, checks, killers, castling, other by history < 200.
ScoreType MoveScoring[16] = { 0, 1000, 1100, 300, 950, 500, 350, 300, 1950, 1500, 1350, 1300, 250, 250, 250, 250 };

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

inline Piece getPieceIndex (const Position &p, Square k){ assert(k >= 0 && k < 64); return Piece(p.b[k] + PieceShift); }

inline Piece getPieceType  (const Position &p, Square k){ assert(k >= 0 && k < 64); return (Piece)std::abs(p.b[k]);}

inline Color getColor      (const Position &p, Square k){ assert(k >= 0 && k < 64); return Colors[getPieceIndex(p,k)];}

inline std::string getName (const Position &p, Square k){ assert(k >= 0 && k < 64); return Names[getPieceIndex(p,k)];}

inline ScoreType getValue  (const Position &p, Square k){ assert(k >= 0 && k < 64); return Values[getPieceIndex(p,k)];}

inline ScoreType getValueEG(const Position &p, Square k){ assert(k >= 0 && k < 64); return ValuesEG[getPieceIndex(p,k)];}

inline ScoreType getSign   (const Position &p, Square k){ assert(k >= 0 && k < 64); return Signs[getPieceIndex(p,k)];}

inline ScoreType Move2Score(Move h) { assert(h != INVALIDMOVE); return (h >> 16) & 0xFFFF; }
inline Square    Move2From (Move h) { assert(h != INVALIDMOVE); return (h >> 10) & 0x3F  ; }
inline Square    Move2To   (Move h) { assert(h != INVALIDMOVE); return (h >>  4) & 0x3F  ; }
inline MType     Move2Type (Move h) { assert(h != INVALIDMOVE); return MType(h & 0xF)    ; }
inline Move      ToMove(Square from, Square to, MType type) {
    assert(from >= 0 && from < 64);
    assert(to >= 0 && to < 64);
    return (from << 10) | (to << 4) | type; }
inline Move      ToMove(Square from, Square to, MType type, ScoreType score) {
    assert(from >= 0 && from < 64);
    assert(to >= 0 && to < 64);
    return (score << 16) | (from << 10) | (to << 4) | type; }

Hash randomInt(){
    static std::mt19937 mt(42); // fixed seed
    static std::uniform_int_distribution<unsigned long long int> dist(0, UINT64_MAX);
    return dist(mt);
}

Hash ZT[64][14]; // should be 13 but last ray is for castling[0 7 56 63][13] and ep [k][13] and color [3 4][13]

void initHash(){
   for(int k = 0 ; k < 64 ; ++k)
      for(int j = 0 ; j < 14 ; ++j)
         ZT[k][j] = randomInt();
}

Hash computeHash(const Position &p){
   if (p.h != 0) return p.h;
   Hash h = 0;
   for (int k = 0; k < 64; ++k){
      const Piece pp = p.b[k];
      if ( pp != P_none) h ^= ZT[k][pp+PieceShift];
   }
   if ( p.ep != INVALIDSQUARE ) h ^= ZT[p.ep][13];
   if ( p.castling & C_wks)     h ^= ZT[7][13];
   if ( p.castling & C_wqs)     h ^= ZT[0][13];
   if ( p.castling & C_bks)     h ^= ZT[63][13];
   if ( p.castling & C_bqs)     h ^= ZT[56][13];
   if ( p.c == Co_White)        h ^= ZT[3][13];
   if ( p.c == Co_Black)        h ^= ZT[4][13];
   p.h = h;
   return h;
}

struct TT{
   enum Bound{ B_exact = 0,      B_alpha = 1,      B_beta  = 2 };
   struct Entry{
      Entry():m(INVALIDMOVE),score(0),b(B_alpha),d(-1),h(0){}
      Entry(Move m, int s, Bound b, DepthType d, Hash h) : m(m), score(m), b(b), d(d), h(h){}
      Move m;
      int score;
      Bound b;
      DepthType d;
      Hash h;
   };

   struct Bucket {
       static const int nbBucket = 2;
       Entry e[nbBucket]; // first is replace always, second is replace by depth
   };

   static unsigned int powerFloor(unsigned int x) {
       unsigned int power = 1;
       while (power < x) power *= 2;
       return power/2;
   }

   static unsigned int ttSize;
   static Bucket * table;

   static void initTable(){
      ttSize = powerFloor(ttSizeMb * 1024 * 1024 / (unsigned int)sizeof(Bucket));
      table = new Bucket[ttSize];
      std::cout << "Size of TT " << ttSize * sizeof(Bucket) / 1024 / 1024 << "Mb" << std::endl;
   }

   static void clearTT() {
       for (unsigned int k = 0; k < ttSize; ++k) {
           table[k].e[0] = { INVALIDMOVE, 0, B_alpha, 0, 0 };
           table[k].e[1] = { INVALIDMOVE, 0, B_alpha, 0, 0 };
       }
   }

   static bool getEntry(Hash h, DepthType d, Entry & e, int nbuck = 0) {
      assert(h > 0);
      const Entry & _e = table[h%ttSize].e[nbuck];
      if ( _e.h != h ){
          if (nbuck >= Bucket::nbBucket - 1) return false;
          return getEntry(h,d,e,nbuck+1);
      }
      e = _e;
      if ( _e.d >= d ){
         ++stats.tthits;
         return true;
      }
      if (nbuck >= Bucket::nbBucket - 1) return false;
      return getEntry(h, d, e, nbuck + 1);
   }

   static void setEntry(const Entry & e){
      assert(e.h > 0);
      assert(e.m != INVALIDMOVE);
      table[e.h%ttSize].e[0] = e; // always replace
      Entry & _eDepth = table[e.h%ttSize].e[1];
      if ( e.d >= _eDepth.d ) _eDepth = e; // replace if better depth
   }

   struct EvalEntry {
       ScoreType score;
       float gp;
       Hash h;
   };

   static unsigned int ttESize;
   static EvalEntry * evalTable;

   static void initETable() {
       ttESize = powerFloor(ttESizeMb * 1024 * 1024 / (unsigned int)sizeof(EvalEntry));
       evalTable = new EvalEntry[ttESize];
       std::cout << "Size of ETT " << ttESize * sizeof(EvalEntry) / 1024 / 1024 << "Mb" << std::endl;
   }

   static void clearETT() {
       for (unsigned int k = 0; k < ttESize; ++k) {
           evalTable[k] = { 0, 0., 0 };
       }
   }

   static bool getEvalEntry(Hash h, ScoreType & score, float & gp) {
       assert(h > 0);
       const EvalEntry & _e = evalTable[h%ttESize];
       if (_e.h != h) return false;
       score = _e.score;
       gp = _e.gp;
       return true;
   }

   static void setEvalEntry(const EvalEntry & e) {
       assert(e.h > 0);
       evalTable[e.h%ttESize] = e; // always replace
   }

};

TT::Bucket *    TT::table     = 0;
TT::EvalEntry * TT::evalTable = 0;
unsigned int    TT::ttSize    = 0;
unsigned int    TT::ttESize   = 0;

namespace KillerT{
   Move killers[2][MAX_PLY];
   void initKillers(){
      for(int i = 0; i < 2; ++i)
          for(int k = 0 ; k < MAX_PLY; ++k)
              killers[i][k] = INVALIDMOVE;
   }
}

namespace HistoryT{
   ScoreType history[13][64];
   void initHistory(){
      for(int i = 0; i < 13; ++i)
          for(int k = 0 ; k < 64; ++k)
              history[i][k] = 0;
   }
   inline void update(DepthType depth, Move m, const Position & p, bool plus){
       if ( Move2Type(m) == T_std ) history[getPieceIndex(p,Move2From(m))][Move2To(m)] += ScoreType( (plus?+1:-1) * (depth*depth/6.f) - (history[getPieceIndex(p,Move2From(m))][Move2To(m)] * depth*depth/6.f / 200.f));
   }
}

inline bool isCapture(const MType & mt){ return mt == T_capture || mt == T_ep || mt == T_cappromq || mt == T_cappromr || mt == T_cappromb || mt == T_cappromn; }

inline bool isCapture(const Move & m){ return isCapture(Move2Type(m)); }

std::string trim(const std::string& str, const std::string& whitespace = " \t"){
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) return ""; // no content
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

std::string GetFENShort(const Position &p ){
    // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR"
    std::stringstream ss;
    int count = 0;
    for (int i = 7; i >= 0; --i) {
        for (int j = 0; j < 8; j++) {
            const Square k = 8 * i + j;
            if (p.b[k] == P_none)  ++count;
            else {
                if (count != 0) {
                    ss << count;
                    count = 0;
                }
                ss << getName(p,k);
            }
            if (j == 7) {
                if (count != 0) {
                    ss << count;
                    count = 0;
                }
                if (i != 0) ss << "/";
            }
        }
    }
    return ss.str();
}

std::string GetFENShort2(const Position &p) {
    // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d5
    std::stringstream ss;
    ss << GetFENShort(p) << " " << (p.c == Co_White ? "w" : "b") << " ";
    bool withCastling = false;
    if (p.castling & C_wks) {
        ss << "K"; withCastling = true;
    }
    if (p.castling & C_wqs) {
        ss << "Q"; withCastling = true;
    }
    if (p.castling & C_bks) {
        ss << "k"; withCastling = true;
    }
    if (p.castling & C_bqs) {
        ss << "q"; withCastling = true;
    }
    if (!withCastling) ss << "-";
    if (p.ep != INVALIDSQUARE) ss << " " << Squares[p.ep];
    else ss << " -";
    return ss.str();
}


std::string GetFEN(const Position &p) {
    // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d5 0 2"
    std::stringstream ss;
    ss << GetFENShort2(p) << " " << (int)p.fifty << " " << (int)p.moves;
    return ss.str();
}

std::string ToString(const Move & m, bool withScore = false){ ///@todo use less lines using a string array instead of the prom switch
   if ( m == INVALIDMOVE ) return "invalid move";
   std::stringstream ss;
   std::string prom;
   const std::string score = (withScore ? " (" + std::to_string(Move2Score(m)) + ")" : "");
   switch (Move2Type(m)) {
   case T_bks:
   case T_wks:
       return "O-O" + score;
       break;
   case T_bqs:
   case T_wqs:
       return "O-O-O" + score;
       break;
   default:
       static const std::string suffixe[] = { "","","","","q","r","b","n","q","r","b","n" };
       prom = suffixe[Move2Type(m)];
       break;
   }
   ss << Squares[Move2From(m)] << Squares[Move2To(m)];
   return ss.str() + prom + score;
}

int gamePhase(const Position & p){
   int absscore = 0;
   int nbPiece  = 0;
   int nbPawn   = 0;
   for( Square k = 0 ; k < 64 ; ++k){
      if ( p.b[k] != P_none ){
         absscore += std::abs(getValue(p,k));
         if ( std::abs(p.b[k]) != P_wp ) ++nbPiece;
         else ++nbPawn;
      }
   }
   absscore = 100*(absscore-16000)    / (24140-16000);
   nbPawn   = 100*nbPawn              / 16;
   nbPiece  = 100*(nbPiece-2)         / 14;
   return int(absscore*0.4+nbPiece*0.3+nbPawn*0.3);
}

std::string ToString(const std::vector<Move> & moves){
   std::stringstream ss;
   for(size_t k = 0 ; k < moves.size(); ++k) ss << ToString(moves[k]) << " ";
   return ss.str();
}

std::string ToString(const Position & p){
    std::stringstream ss;
    ss << "#" << std::endl;
    for (Square j = 7; j >= 0; --j) {
        ss << "# +-+-+-+-+-+-+-+-+" << std::endl;
        ss << "# |";
        for (Square i = 0; i < 8; ++i) ss << getName(p,i+j*8) << '|';
        ss << std::endl; ;
    }
    ss << "# +-+-+-+-+-+-+-+-+" ;
    ss << "#" << std::endl;
    if ( p.ep >=0 ) ss << "# ep " << Squares[p.ep]<< std::endl;
    ss << "# wk " << Squares[p.wk] << std::endl;
    ss << "# bk " << Squares[p.bk] << std::endl;
    ss << "# Turn " << (p.c == Co_White ? "white" : "black") << std::endl;
    ss << "# Phase " << gamePhase(p) << std::endl;
    ss << "# Hash " << computeHash(p) << std::endl;
    ss << "# FEN " << GetFEN(p) << std::endl;
    return ss.str();
}

inline Color opponentColor(const Color c){ return Color((c+1)%2);}

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
int Offset[6][8] = { { -20, -10, -11, -9, 9, 11, 10, 20 },    /* pawn */
                     { -21, -19, -12, -8, 8, 12, 19, 21 },    /* knight */
                     { -11,  -9,   9, 11, 0,  0,  0,  0 },    /* bishop */
                     { -10,  -1,   1, 10, 0,  0,  0,  0 },    /* rook */
                     { -11, -10,  -9, -1, 1,  9, 10, 11 },    /* queen */
                     { -11, -10,  -9, -1, 1,  9, 10, 11 } };  /* king */

const ScoreType PST[6][64] = {
 {
 //pawn
      0,   0,   0,   0,   0,   0,  0,   0,
     98, 134,  61,  95,  68, 126, 34, -11,
     -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
      0,   0,   0,   0,   0,   0,  0,   0
 },{
 //knight
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23
 },{
 //bishop
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21
 },{
 //rook
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26
 },{
 //queen
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50
 },{
 //king
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14
 }
};

const ScoreType PSTEG[6][64] = {
 {
 //pawn
     0,   0,   0,   0,   0,   0,   0,   0,
   178, 173, 158, 134, 147, 132, 165, 187,
    94, 100,  85,  67,  56,  53,  82,  84,
    32,  24,  13,   5,  -2,   4,  17,  17,
    13,   9,  -3,  -7,  -7,  -8,   3,  -1,
     4,   7,  -6,   1,   0,  -5,  -1,  -8,
    13,   8,   8,  10,  13,   0,   2,  -7,
     0,   0,   0,   0,   0,   0,   0,   0
 },{
 //knight
   -58, -38, -13, -28, -31, -27, -63, -99,
   -25,  -8, -25,  -2,  -9, -25, -24, -52,
   -24, -20,  10,   9,  -1,  -9, -19, -41,
   -17,   3,  22,  22,  22,  11,   8, -18,
   -18,  -6,  16,  25,  16,  17,   4, -18,
   -23,  -3,  -1,  15,  10,  -3, -20, -22,
   -42, -20, -10,  -5,  -2, -20, -23, -44,
   -29, -51, -23, -15, -22, -18, -50, -64
 },{
 //bishop
   -14, -21, -11,  -8, -7,  -9, -17, -24,
    -8,  -4,   7, -12, -3, -13,  -4, -14,
     2,  -8,   0,  -1, -2,   6,   0,   4,
    -3,   9,  12,   9, 14,  10,   3,   2,
    -6,   3,  13,  19,  7,  10,  -3,  -9,
   -12,  -3,   8,  10, 13,   3,  -7, -15,
   -14, -18,  -7,  -1,  4,  -9, -15, -27,
   -23,  -9, -23,  -5, -9, -16,  -5, -17
 },{
 //rook
   13, 10, 18, 15, 12,  12,   8,   5,
   11, 13, 13, 11, -3,   3,   8,   3,
    7,  7,  7,  5,  4,  -3,  -5,  -3,
    4,  3, 13,  1,  2,   1,  -1,   2,
    3,  5,  8,  4, -5,  -6,  -8, -11,
   -4,  0, -5, -1, -7, -12,  -8, -16,
   -6, -6,  0,  2, -9,  -9, -11,  -3,
   -9,  2,  3, -1, -5, -13,   4, -20
 },{
 //queen
    -9,  22,  22,  27,  27,  19,  10,  20,
   -17,  20,  32,  41,  58,  25,  30,   0,
   -20,   6,   9,  49,  47,  35,  19,   9,
     3,  22,  24,  45,  57,  40,  57,  36,
   -18,  28,  19,  47,  31,  34,  39,  23,
   -16, -27,  15,   6,   9,  17,  10,   5,
   -22, -23, -30, -16, -16, -23, -36, -32,
   -33, -28, -22, -43,  -5, -32, -20, -41
 },{
 //king
   -74, -35, -18, -18, -11,  15,   4, -17,
   -12,  17,  14,  17,  17,  38,  23,  11,
    10,  17,  23,  15,  20,  45,  44,  13,
    -8,  22,  24,  27,  26,  33,  26,   3,
   -18,  -4,  21,  24,  27,  23,   9, -11,
   -19,  -3,  11,  21,  23,  16,   7,  -9,
   -27, -11,   4,  13,  14,   4,  -5, -17,
   -53, -34, -21, -11, -28, -14, -24, -43
 }
};

bool readFEN(const std::string & fen, Position & p){
    std::vector<std::string> strList;
    std::stringstream iss(fen);
    std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));

    std::cout << "# Reading fen " << fen << std::endl;

    for(Square k = 0 ; k < 64 ; ++k) p.b[k] = P_none;

    Square j = 1, i = 0;
    while ((j <= 64) && (i <= (char)strList[0].length())){
        char letter = strList[0].at(i);
        ++i;
        Square r = 7-(j-1)/8;
        Square f = (j-1)%8;
        Square k = r*8+f;
        switch (letter) {
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
        if (strList[1] == "w")      p.c = Co_White;
        else if (strList[1] == "b") p.c = Co_Black;
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
            p.castling |= C_wks; found = true;
        }
        if (strList[2].find('Q') != std::string::npos){
            p.castling |= C_wqs; found = true;
        }
        if (strList[2].find('k') != std::string::npos){
            p.castling |= C_bks; found = true;
        }
        if (strList[2].find('q') != std::string::npos){
            p.castling |= C_bqs; found = true;
        }
        if ( ! found ){
            std::cout << "#No castling right given" << std::endl;
        }
    }
    else std::cout << "#No castling right given" << std::endl;

    // read en passant and save it (default is invalid)
    p.ep = -1;
    if ((strList.size() >= 4) && strList[3] != "-" ){
        if (strList[3].length() >= 2){
            if ((strList[3].at(0) >= 'a') && (strList[3].at(0) <= 'h') && ((strList[3].at(1) == '3') || (strList[3].at(1) == '6'))){
                p.ep = (strList[3].at(0)-97) + 8*(strList[3].at(1));
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
    else std::cout << "#No en passant square given" << std::endl;

    // read 50 moves rules
    if (strList.size() >= 5){
        std::stringstream ss(strList[4]);
        int tmp;
        ss >> tmp;
        p.fifty = tmp;
    }
    else p.fifty = 0;

    // read number of move
    if (strList.size() >= 6){
        std::stringstream ss(strList[5]);
        int tmp;
        ss >> tmp;
        p.moves = tmp;
    }
    else p.moves = 1;

    p.h = 0; p.h = computeHash(p);

    return true;
}

void tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters = " " ){
  std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
  std::string::size_type pos     = str.find_first_of(delimiters, lastPos);
  while (std::string::npos != pos || std::string::npos != lastPos) {
    tokens.push_back(str.substr(lastPos, pos - lastPos));
    lastPos = str.find_first_not_of(delimiters, pos);
    pos = str.find_first_of(delimiters, lastPos);
  }
}

Square stringToSquare(const std::string & str){ return (str.at(1) - 49) * 8 + (str.at(0) - 97); }

bool readMove(const Position & p, const std::string & ss, Square & from, Square & to, MType & moveType ) {

    if ( ss.empty()){
        std::cout << "#Trying to read empty move ! " << std::endl;
        moveType = T_std;
        return false;
    }

    std::string str(ss);

    // add space to go to own internal notation
    if ( str != "0-0" && str != "0-0-0" && str != "O-O" && str != "O-O-O" ) str.insert(2," ");

    std::vector<std::string> strList;
    std::stringstream iss(str);
    std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));

    moveType = T_std;
    if ( strList.empty()){
        std::cout << "#Trying to read bad move, seems empty " << str << std::endl;
        return false;
    }

    // detect special move
    if (strList[0] == "0-0" || strList[0] == "O-O"){
        if ( p.c == Co_White ) moveType = T_wks;
        else moveType = T_bks;
    }
    else if (strList[0] == "0-0-0" || strList[0] == "O-O-O"){
        if ( p.c == Co_White) moveType = T_wqs;
        else moveType = T_bqs;
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
            from = stringToSquare(strList[0]);
        }
        else {
            std::cout << "#Trying to read bad move, invalid from square " << str << std::endl;
            return false;
        }

        bool isCapture = false;

        // be carefull, promotion possible !
        if (strList[1].size() >= 2 && (strList[1].at(0) >= 'a') && (strList[1].at(0) <= 'h') &&  ((strList[1].at(1) >= '1') && (strList[1].at(1) <= '8'))) {
            if ( strList[1].size() > 2 ){ // promotion
                std::string prom;
                if ( strList[1].size() == 3 ){ // probably e7 e8q notation
                   prom = strList[1][2];
                   to = stringToSquare(strList[1].substr(0,2));
                }
                else{ // probably e7 e8=q notation
                   std::vector<std::string> strListTo;
                   tokenize(strList[1],strListTo,"=");
                   to = stringToSquare(strListTo[0]);
                   prom = strListTo[1];
                }

                isCapture = p.b[to] != P_none;

                if      ( prom == "Q" || prom == "q") moveType = isCapture ? T_cappromq : T_promq;
                else if ( prom == "R" || prom == "r") moveType = isCapture ? T_cappromr : T_promr;
                else if ( prom == "B" || prom == "b") moveType = isCapture ? T_cappromb : T_promb;
                else if ( prom == "N" || prom == "n") moveType = isCapture ? T_cappromn : T_promn;
                else{
                    std::cout << "#Trying to read bad move, invalid to square " << str << std::endl;
                    return false;
                }
            }
            else{
               to = stringToSquare(strList[1]);
               isCapture = p.b[to] != P_none;
            }
        }
        else {
            std::cout << "#Trying to read bad move, invalid to square " << str << std::endl;
            return false;
        }
    }

    if (getPieceType(p,from) == P_wp && to == p.ep) moveType = T_ep;

    return true;
}

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

   int GetNextMSecPerMove(const Position & p){
      int ms = -1;
      std::cout << "msecPerMove   " << msecPerMove << std::endl;
      std::cout << "msecWholeGame " << msecWholeGame << std::endl;
      std::cout << "msecInc       " << msecInc << std::endl;
      std::cout << "nbMoveInTC    " << nbMoveInTC << std::endl;
      if ( msecPerMove > 0 ) ms =  msecPerMove;
      else if ( nbMoveInTC > 0){ // mps is given
      assert(msecWholeGame > 0);
      assert(nbMoveInTC > 0);
         ms = int(0.85 * (msecWholeGame+((msecInc>0)?nbMoveInTC*msecInc:0)) / (float)(nbMoveInTC+0.5));
      }
      else{ // mps is not given
         ///@todo something better using the real time command
         int nmoves = 60; // let's start for a 60 ply game
         if (p.moves > 20) nmoves = 100; // if the game is long, let's go for a 100 ply game
         if (p.moves > 40) nmoves = 200; // if the game is very long, let's go for a 200 ply game
         ms = int(0.85 * (msecWholeGame+((msecInc>0)?p.moves*msecInc:0))/ (float)nmoves); ///@todo better
      }
      return ms;
   }
}

void addMove(Square from, Square to, MType type, std::vector<Move> & moves){
   assert( from >= 0 && from < 64);
   assert( to >=0 && to < 64);
   moves.push_back(ToMove(from,to,type,0));
}

bool getAttackers(const Position & p, const Square k, std::vector<Square> & attakers){
   assert( k >= 0 && k < 64);
   const Color side = p.c;
   const Color opponent = opponentColor(p.c);
   attakers.clear();
   for (Square i = 0; i < 64; ++i){
      if (getColor(p,i) == opponent) {
         if (getPieceType(p,i) == P_wp) {
            if (side == Co_White) {
               if ( SQFILE(i) != 0 && i - 9 == k) attakers.push_back(i);
               if ( SQFILE(i) != 7 && i - 7 == k) attakers.push_back(i);
            }
            else {
               if ( SQFILE(i) != 0 && i + 7 == k) attakers.push_back(i);
               if ( SQFILE(i) != 7 && i + 9 == k) attakers.push_back(i);
            }
         }
         else{
            const Piece ptype = getPieceType(p,i);
            for (int j = 0; j < Offsets[ptype-1]; ++j){
               for (Square n = i;;) {
                  n = mailbox[mailbox64[n] + Offset[ptype-1][j]];
                  if (n == -1) break;
                  if (n == k) attakers.push_back(i);
                  if (p.b[n] != P_none) break;
                  if ( !Slide[ptype-1]) break;
               }
            }
         }
      }
   }
   return !attakers.empty();
}

bool isAttacked(const Position & p, const Square k){
   assert( k >= 0 && k < 64);
   const Color side = p.c;
   const Color opponent = opponentColor(p.c);
   for (Square i = 0; i < 64; ++i){
      if (getColor(p,i) == opponent) {
         if (getPieceType(p,i) == P_wp) {
            if (side == Co_White) {
               if ( SQFILE(i) != 0 && i - 9 == k) return true;
               if ( SQFILE(i) != 7 && i - 7 == k) return true;
            }
            else {
               if ( SQFILE(i) != 0 && i + 7 == k) return true;
               if ( SQFILE(i) != 7 && i + 9 == k) return true;
            }
         }
         else{
            const Piece ptype = getPieceType(p,i);
            for (int j = 0; j < Offsets[ptype-1]; ++j){
               for (Square n = i;;) {
                  n = mailbox[mailbox64[n] + Offset[ptype-1][j]];
                  if (n == -1) break;
                  if (n == k) return true;
                  if (p.b[n] != P_none) break;
                  if ( !Slide[ptype-1]) break;
               }
            }
         }
      }
   }
   return false;
}

void generate(const Position & p, std::vector<Move> & moves, bool onlyCap = false){
   moves.clear();
   const Color side = p.c;
   const Color opponent = opponentColor(p.c);
   for(Square from = 0 ; from < 64 ; ++from){
     const Piece piece = p.b[from];
     const Piece ptype = (Piece)std::abs(piece);
     if (Colors[piece+PieceShift] == side) {
       if (ptype != P_wp) {
         for (Square j = 0; j < Offsets[ptype-1]; ++j) {
           for (Square to = from;;) {
             to = mailbox[mailbox64[to] + Offset[ptype-1][j]];
             if (to < 0) break;
             const Color targetC = getColor(p,to);
             if (targetC != Co_None) {
               if (targetC == opponent) addMove(from, to, T_capture, moves); // capture
               break; // as soon as a capture or an own piece is found
             }
             if ( !onlyCap) addMove(from, to, T_std, moves); // not a capture
             if (!Slide[ptype-1]) break; // next direction
           }
         }
         if ( ptype == P_wk && !onlyCap ){ // castling
           if ( side == Co_White) {
              if ( (p.castling & C_wqs) && p.b[Sq_b1] == P_none && p.b[Sq_c1] == P_none && p.b[Sq_d1] == P_none && !isAttacked(p,Sq_c1) && !isAttacked(p,Sq_d1) && !isAttacked(p,Sq_e1)) addMove(from, Sq_c1, T_wqs, moves); // wqs
              if ( (p.castling & C_wks) && p.b[Sq_f1] == P_none && p.b[Sq_g1] == P_none && !isAttacked(p,Sq_e1) && !isAttacked(p,Sq_f1) && !isAttacked(p,Sq_g1)) addMove(from, Sq_g1, T_wks, moves); // wks
           }
           else{
              if ( (p.castling & C_bqs) && p.b[Sq_b8] == P_none && p.b[Sq_c8] == P_none && p.b[Sq_d8] == P_none && !isAttacked(p,Sq_c8) && !isAttacked(p,Sq_d8) && !isAttacked(p,Sq_e8)) addMove(from, Sq_c8, T_bqs, moves); // bqs
              if ( (p.castling & C_bks) && p.b[Sq_f8] == P_none && p.b[Sq_g8] == P_none && !isAttacked(p,Sq_e8) && !isAttacked(p,Sq_f8) && !isAttacked(p,Sq_g8)) addMove(from, Sq_g8, T_bks, moves); // bks
           }
         }
       }
       else {
          int pawnOffsetTrick = side==Co_White?4:0;
          for (Square j = 0; j < 4; ++j) {
             int offset = Offset[ptype-1][j+pawnOffsetTrick];
             const Square to = mailbox[mailbox64[from] + offset];
             if (to < 0) continue;
             const Color targetC = getColor(p,to);
             if (targetC != Co_None) {
               if (targetC == opponent && ( abs(offset) == 11 || abs(offset) == 9 ) ) {
                 if ( SQRANK(to) == 0 || SQRANK(to) == 7) {
                    addMove(from, to, T_cappromq, moves); // pawn capture with promotion
                    addMove(from, to, T_cappromr, moves); // pawn capture with promotion
                    addMove(from, to, T_cappromb, moves); // pawn capture with promotion
                    addMove(from, to, T_cappromn, moves); // pawn capture with promotion
                 }
                 else addMove(from, to, T_capture, moves); // simple pawn capture
               }
             }
             else{
               // ep pawn capture
               if ( (abs(offset) == 11 || abs(offset) == 9 ) && (p.ep == to) ) addMove(from, to, T_ep, moves); // en passant
               else if ( abs(offset) == 20 && !onlyCap && ( SQRANK(from) == 1 || SQRANK(from) == 6 ) && p.b[(from + to)/2] == P_none) addMove(from, to, T_std, moves); // double push
               else if ( abs(offset) == 10 && !onlyCap){
                   if ( SQRANK(to) == 0 || SQRANK(to) == 7 ){
                      addMove(from, to, T_promq, moves); // promotion Q
                      addMove(from, to, T_promr, moves); // promotion R
                      addMove(from, to, T_promb, moves); // promotion B
                      addMove(from, to, T_promn, moves); // promotion N
                   }
                   else addMove(from, to, T_std, moves); // simple pawn push
               }
            }
          }
       }
     }
   }
}

Square kingSquare(const Position & p){ return (p.c == Co_White) ? p.wk : p.bk; }

bool apply(Position & p, const Move & m){

   assert(m != INVALIDMOVE);

   const Square from  = Move2From(m);
   const Square to    = Move2To(m);
   const MType  type  = Move2Type(m);
   const Piece  fromP = p.b[from];
   const Piece  toP   = p.b[to];

   const int fromId   = fromP + PieceShift;
   const int toId     = toP + PieceShift;

   switch(type){
      case T_std:
      case T_capture:
      case T_check:
      p.b[from] = P_none;
      p.b[to]   = fromP;

      p.h ^= ZT[from][fromId]; // remove fromP at from
      if (type == T_capture) p.h ^= ZT[to][toId]; // if capture remove toP at to
      p.h ^= ZT[to][fromId]; // add fromP at to

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
         p.h ^= ZT[from][fromId]; // remove fromP at from
         p.h ^= ZT[p.ep + (p.c == Co_White ? -8 : +8)][(p.c == Co_White ? P_bp : P_wp) + PieceShift];
         p.h ^= ZT[to][fromId]; // add fromP at to
      break;

      case T_promq:
      case T_cappromq:
      if (type == T_capture) p.h ^= ZT[to][toId];
      p.b[to]   = (p.c==Co_White?P_wq:P_bq);
      p.b[from] = P_none;
      p.h ^= ZT[from][fromId];
      p.h ^= ZT[to][(p.c == Co_White ? P_wq : P_bq) + PieceShift];
      break;

      case T_promr:
      case T_cappromr:
      if (type == T_capture) p.h ^= ZT[to][toId];
      p.b[to]   = (p.c==Co_White?P_wr:P_br);
      p.b[from] = P_none;
      p.h ^= ZT[from][fromId];
      p.h ^= ZT[to][(p.c == Co_White ? P_wr : P_br) + PieceShift];
      break;

      case T_promb:
      case T_cappromb:
      if (type == T_capture) p.h ^= ZT[to][toId];
      p.b[to]   = (p.c==Co_White?P_wb:P_bb);
      p.b[from] = P_none;
      p.h ^= ZT[from][fromId];
      p.h ^= ZT[to][(p.c == Co_White ? P_wb : P_bb) + PieceShift];
      break;

      case T_promn:
      case T_cappromn:
      if (type == T_capture) p.h ^= ZT[to][toId];
      p.b[to]   = (p.c==Co_White?P_wn:P_bn);
      p.b[from] = P_none;
      p.h ^= ZT[from][fromId];
      p.h ^= ZT[to][(p.c == Co_White ? P_wn : P_bn) + PieceShift];
      break;

      case T_wks:
      p.b[Sq_e1] = P_none; p.b[Sq_f1] = P_wr; p.b[Sq_g1] = P_wk; p.b[Sq_h1] = P_none;
      p.wk = Sq_g1;
      if (p.castling & C_wqs) p.h ^= ZT[0][13];
      if (p.castling & C_wks) p.h ^= ZT[7][13];
      p.castling &= ~(C_wks | C_wqs);
      p.h ^= ZT[Sq_h1][P_wr + PieceShift]; // remove rook
      p.h ^= ZT[Sq_e1][P_wk + PieceShift]; // remove king
      p.h ^= ZT[Sq_f1][P_wr + PieceShift]; // add rook
      p.h ^= ZT[Sq_g1][P_wk + PieceShift]; // add king
      break;

      case T_wqs:
      p.b[Sq_a1] = P_none; p.b[Sq_b1] = P_none; p.b[Sq_c1] = P_wk; p.b[Sq_d1] = P_wr; p.b[Sq_e1] = P_none;
      p.wk = Sq_c1;
      if (p.castling & C_wqs) p.h ^= ZT[0][13];
      if (p.castling & C_wks) p.h ^= ZT[7][13];
      p.castling &= ~(C_wks | C_wqs);
      p.h ^= ZT[Sq_a1][P_wr + PieceShift]; // remove rook
      p.h ^= ZT[Sq_e1][P_wk + PieceShift]; // remove king
      p.h ^= ZT[Sq_d1][P_wr + PieceShift]; // add rook
      p.h ^= ZT[Sq_c1][P_wk + PieceShift]; // add king
      break;

      case T_bks:
      p.b[Sq_e8] = P_none; p.b[Sq_f8] = P_br; p.b[Sq_g8] = P_bk; p.b[Sq_h8] = P_none;
      p.bk = Sq_g8;
      if (p.castling & C_bqs) p.h ^= ZT[56][13];
      if (p.castling & C_bks) p.h ^= ZT[63][13];
      p.castling &= ~(C_bks | C_bqs);
      p.h ^= ZT[Sq_h8][P_br + PieceShift]; // remove rook
      p.h ^= ZT[Sq_e8][P_bk + PieceShift]; // remove king
      p.h ^= ZT[Sq_f8][P_br + PieceShift]; // add rook
      p.h ^= ZT[Sq_g8][P_bk + PieceShift]; // add king
      break;

      case T_bqs:
      p.b[Sq_a8] = P_none; p.b[Sq_b8] = P_none; p.b[Sq_c8] = P_bk; p.b[Sq_d8] = P_br; p.b[Sq_e8] = P_none;
      p.bk = Sq_c8;
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

   p.c = opponentColor(p.c);
   if ( toP != P_none || abs(fromP) == P_wp ) p.fifty = 0;
   else ++p.fifty;
   if ( p.c == Co_Black ) ++p.moves;

   p.h ^= ZT[3][13]; p.h ^= ZT[4][13];

   return true;
}

namespace Book {
    //struct to hold the value:
    template<typename T> struct bits_t { T t; };
    template<typename T> bits_t<T&> bits(T &t) { return bits_t<T&>{t}; }
    template<typename T> bits_t<const T&> bits(const T& t) { return bits_t<const T&>{t};}
    template<typename S, typename T> S& operator<<(S &s, bits_t<T>  b) { s.write((char*)&b.t, sizeof(T)); return s;}
    template<typename S, typename T> S& operator>>(S& s, bits_t<T&> b) { s.read ((char*)&b.t, sizeof(T)); return s;}

    std::unordered_map<Hash, std::set<Move> > book;

    bool fileExists(const std::string& name){ return std::ifstream(name.c_str()).good(); }

    bool readBinaryBook(std::ifstream & stream) {
        Position ps;
        readFEN(startPosition,ps);
        Position p = ps;
        Move hash = 0;
        while (!stream.eof()) {
            hash = 0;
            stream >> bits(hash);
            if (hash == INVALIDMOVE) {
                p = ps;
                stream >> bits(hash);
                if (stream.eof()) break;
            }
            const Hash h = computeHash(p);
            if ( ! apply(p,hash)) return false;
            book[h].insert(hash);
        }
        return true;
    }

#ifdef IMPORTBOOK
    #include "bookGenerationTools.h"
#endif

    template<typename Iter, typename RandomGenerator>
    Iter select_randomly(Iter start, Iter end, RandomGenerator& g) {
        std::uniform_int_distribution<> dis(0, (int)std::distance(start, end) - 1);
        std::advance(start, dis(g));
        return start;
    }

    template<typename Iter>
    Iter select_randomly(Iter start, Iter end) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        return select_randomly(start, end, gen);
    }

    const Move Get(const Hash h){
       std::unordered_map<Hash, std::set<Move> >::iterator it = book.find(h);
       if ( it == book.end() ) return INVALIDMOVE;
       return *select_randomly(it->second.begin(),it->second.end());
    }

}

struct SortThreatsFunctor {
    const Position & _p;
    SortThreatsFunctor(const Position & p):_p(p){}
    bool operator()(const Square s1,const Square s2) { return std::abs(getValue(_p,s1)) < std::abs(getValue(_p,s2));}
};

// Static Exchange Evaluation (from stockfish)
bool SEE(const Position & p, const Move & m, ScoreType threshold){
    // Only deal with normal moves
    if (! isCapture(m)) return true;
    Square from       = Move2From(m);
    Square to         = Move2To(m);
    Piece nextVictim  = p.b[from];
    Color us          = getColor(p,from);
    ScoreType balance = std::abs(getValue(p,to)) - threshold; // The opponent may be able to recapture so this is the best result we can hope for.
    if (balance < 0) return false;
    balance -= std::abs(Values[nextVictim+PieceShift]); // Now assume the worst possible result: that the opponent can capture our piece for free.
    if (balance >= 0) return true;
    if (getPieceType(p, to) == P_wk) return false; // we shall not capture king !
    Position p2 = p;
    if (!apply(p2, m)) return false;
    std::vector<Square> stmAttackers;
    bool endOfSEE = false;
    while (!endOfSEE){
        p2.c = opponentColor(p2.c);
        bool threatsFound = getAttackers(p2, to, stmAttackers);
        p2.c = opponentColor(p2.c);
        if (!threatsFound) break;
        std::sort(stmAttackers.begin(),stmAttackers.end(),SortThreatsFunctor(p2));
        bool validThreatFound = false;
        int threatId = 0;
        while (!validThreatFound && threatId < stmAttackers.size()) {
            Move mm = ToMove(stmAttackers[threatId], to, T_capture); ///@todo prom ????
            nextVictim = p2.b[stmAttackers[threatId]];
            ++threatId;
            if ( ! apply(p2,mm) ) continue;
            validThreatFound = true;
            balance = -balance - 1 - std::abs(Values[nextVictim+PieceShift]);
            if (balance >= 0) {
                if (std::abs(nextVictim) == P_wk) p2.c = opponentColor(p2.c);
                endOfSEE = true;
            }
        }
        if (!validThreatFound) endOfSEE = true;
    }
    return us != p2.c; // We break the above loop when stm loses
}

bool sameMove(const Move & a, const Move & b) { return (a & 0x0000FFFF) == (b & 0x0000FFFF);}

struct MoveSorter{
   MoveSorter(const Position & p, DepthType ply, const TT::Entry * e = NULL):p(p),ply(ply),e(e){ assert(e==0||e->h!=0||e->m==INVALIDMOVE); }
   bool operator()(Move & a, Move & b){
      assert( a != INVALIDMOVE);
      assert( b != INVALIDMOVE);

      ScoreType s1 = Move2Score(a);
      ScoreType s2 = Move2Score(b);

      if (s1 == 0) {
          const MType t1     = Move2Type(a);
          const Square from1 = Move2From(a);
          const Square to1   = Move2To(a);

          s1 = MoveScoring[t1];
          if (isCapture(t1)) s1 += MvvLvaScores[getPieceType(p,to1)-1][getPieceType(p,from1)-1];
          if (isCapture(t1) && !SEE(p,a,0)) s1 -= 2000;
          if (e && sameMove(e->m,a)) s1 += 3000;
          if (sameMove(a,KillerT::killers[0][ply])) s1 += 290;
          if (sameMove(a,KillerT::killers[1][ply])) s1 += 260;
          if (t1 == T_std) s1 += HistoryT::history[getPieceIndex(p,from1)][to1];
          if (t1 == T_std) {
              const int sign1 = getSign(p, from1);
              s1 += PST[getPieceType(p, from1) - 1][sign1 > 0 ? (to1 ^ 56) : to1] - PST[getPieceType(p, from1) - 1][sign1 > 0 ? (from1 ^ 56) : from1];
          }
          a = ToMove(from1, to1, t1, s1);
      }
      if (s2 == 0) {
          const MType t2     = Move2Type(b);
          const Square from2 = Move2From(b);
          const Square to2   = Move2To(b);

          s2 = MoveScoring[t2];
          if (isCapture(t2)) s2 += MvvLvaScores[getPieceType(p,to2)-1][getPieceType(p,from2)-1];
          if (isCapture(t2) && !SEE(p,b,0)) s2 -= 2000;
          if (e && sameMove(e->m,b)) s2 += 3000;
          if (sameMove(b,KillerT::killers[0][ply])) s2 += 290;
          if (sameMove(b,KillerT::killers[1][ply])) s2 += 260;
          if (t2 == T_std) s2 += HistoryT::history[getPieceIndex(p,from2)][to2];
          if (t2 == T_std) {
              const int sign2 = getSign(p, from2);
              s2 += PST[getPieceType(p, from2) - 1][sign2 > 0 ? (to2 ^ 56) : to2] - PST[getPieceType(p, from2) - 1][sign2 > 0 ? (from2 ^ 56) : from2];
          }
          b = ToMove(from2, to2, t2, s2);
      }
      return s1 > s2;
   }
   const Position & p;
   const TT::Entry * e;
   const DepthType ply;
};

void sort(std::vector<Move> & moves, const Position & p, DepthType ply, const TT::Entry * e = NULL){ std::sort(moves.begin(),moves.end(),MoveSorter(p,ply,e));}

bool isDraw(const Position & p, unsigned int ply, bool isPV = true){
   ///@todo FIDE draws
   int count = 0;
   const int limit = isPV?3:1;
   for (int k = ply-1; k >=0; --k) {
       if (hashStack[k] == 0) break;
       if (hashStack[k] == p.h) ++count;
       if (count >= limit) return true;
   }
   if ( p.fifty >= 100 ) return true;
   return false;
}

ScoreType eval(const Position & p, float & gp){
   ScoreType sc = 0;
   gp = gamePhase(p)/100.f;
   const bool white2Play = p.c == Co_White;
   for( Square k = 0 ; k < 64 ; ++k){
      if ( p.b[k] != P_none ){
         sc += ScoreType( gp*getValue(p,k) + (1.f-gp)*getValue(p,k) );
         const int s = getSign(p,k);
         Square kk = k;
         if ( s > 0 ) kk = kk^56;
         const Piece ptype = getPieceType(p,k);
         sc += ScoreType(s * (gp*PST[ptype-1][kk] + (1.f-gp)*PSTEG[ptype-1][kk] ) );
      }
   }
   return (white2Play?+1:-1)*sc;
}

ScoreType qsearch(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth){
  if ( ply >= MAX_PLY-1 ) return 0;
  alpha = std::max(alpha, (ScoreType)(-MATE + ply));
  beta  = std::min(beta , (ScoreType)(MATE - ply + 1));
  if (alpha >= beta) return alpha;

  if ((int)ply > seldepth) seldepth = ply;

  ++stats.qnodes;

  float gp = 0;
  ScoreType val = eval(p,gp);
  if ( val >= beta ) return val;
  if ( val > alpha) alpha = val;

  const bool isInCheck = isAttacked(p, kingSquare(p));

  std::vector<Move> moves;
  generate(p,moves,true);
  sort(moves,p,ply);

  for(auto it = moves.begin() ; it != moves.end() ; ++it){
     // qfutility
     if ( doQFutility && !isInCheck && val + qfutilityMargin + std::abs(getValue(p,Move2To(*it))) <= alpha) continue;
     // SEE
     if (!SEE(p, *it, 0)) continue;
     Position p2 = p;
     if ( ! apply(p2,*it) ) continue;
     if (p.c == Co_White && Move2To(*it) == p.bk) return MATE - ply;
     if (p.c == Co_Black && Move2To(*it) == p.wk) return MATE - ply;
     val = -qsearch(-beta,-alpha,p2,ply+1,seldepth);
     if ( val > alpha ){
        if ( val >= beta ) return val;
        alpha = val;
     }
  }
  return val;
}

inline void updatePV(std::vector<Move> & pv, const Move & m, const std::vector<Move> & childPV) {
    pv.clear();
    pv.push_back(m);
    std::copy(childPV.begin(), childPV.end(), std::back_inserter(pv));
}

inline void updateHistoryKillers(const Position & p, DepthType depth, int ply, const Move m) {
    KillerT::killers[1][ply] = KillerT::killers[0][ply];
    KillerT::killers[0][ply] = m;
    HistoryT::update(depth, m, p, true);
}

ScoreType pvs(ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, bool pvnode, unsigned int ply, std::vector<Move> & pv, DepthType & seldepth){
  if ( std::max(1,(int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count()) > currentMoveMs ){
    stopFlag = true;
    return STOPSCORE;
  }

  if ((int)ply > seldepth) seldepth = ply;

  if ( depth <= 0 ) return qsearch(alpha,beta,p,ply,seldepth);

  ++stats.nodes;

  if ( ply >= MAX_PLY-1 || isDraw(p,ply,pvnode) ) return 0;

  alpha = std::max(alpha, (ScoreType)(-MATE + ply));
  beta  = std::min(beta,  (ScoreType)(MATE  - ply + 1));
  if (alpha >= beta) return alpha;

  const bool rootnode = ply == 1;

  TT::Entry e;
  if (TT::getEntry(computeHash(p), depth, e)) {
      if (e.h != 0 && !rootnode && std::abs(e.score) < MATE - MAX_DEPTH && !pvnode &&
          ( (e.b == TT::B_alpha && e.score <= alpha) || (e.b == TT::B_beta  && e.score >= beta) || (e.b == TT::B_exact) ) ) {
          pv.push_back(e.m);
          return e.score;
      }
  }

  const bool isInCheck = isAttacked(p, kingSquare(p));
  float gp = 0;
  ScoreType val;
  if (isInCheck) val = -MATE + ply;
  else if (!TT::getEvalEntry(computeHash(p), val, gp)) {
      val = eval(p, gp);
      TT::setEvalEntry({ val, gp, computeHash(p) });
  }
  scoreStack[ply] = val; ///@todo can e.score be used as val here ???

  bool futility = false, lmp = false;

  // prunings
  if ( !mateFinder && !rootnode && gp > 0.2 && !pvnode && !isInCheck
      && std::abs(alpha) < MATE-MAX_DEPTH && std::abs(beta) < MATE-MAX_DEPTH ){

     // static null move
     if ( doStaticNullMove && depth <= staticNullMoveMaxDepth && val >= beta + staticNullMoveDepthCoeff *depth ) return val;

     // razoring
     int rAlpha = alpha - razoringMargin;
     if ( doRazoring && depth <= razoringMaxDepth && val <= rAlpha ){
         ScoreType qval = qsearch(rAlpha,rAlpha+1,p,ply,seldepth);
         if ( ! stopFlag && qval <= alpha ) return qval;
     }

     // null move
     if ( doNullMove && pv.size() > 1 && depth >= nullMoveMinDepth && p.ep == INVALIDSQUARE && val >= beta){
       Position pN = p;
       pN.c = opponentColor(pN.c);
       p.h ^= ZT[3][13];
       p.h ^= ZT[4][13];
       int R = depth/4 + 3 + std::min((val-beta)/80,3); // adaptative
       std::vector<Move> nullPV;
       ScoreType nullscore = -pvs(-beta,-beta+1,pN,depth-R,false,ply+1,nullPV,seldepth);
       if ( !stopFlag && nullscore >= beta ) return nullscore;
     }

     // LMP
     if (doLMP && depth <= lmpMaxDepth) lmp = true;

     // futility
     if (doFutility && val <= alpha - futilityDepthCoeff*depth ) futility = true;
  }

  // IID
  if ( (e.h == 0 /*|| e.d < depth/3*/) && pvnode && depth >= iidMinDepth){
     std::vector<Move> iidPV;
     pvs(alpha,beta,p,depth/2,pvnode,ply,iidPV,seldepth);
     TT::getEntry(computeHash(p), depth, e);
  }

  int validMoveCount = 0;
  bool alphaUpdated = false;
  Move bestMove = INVALIDMOVE;

  // try the tt move first
  if (e.h != 0) { // should be the case thanks to iid at pvnode
      Position p2 = p;
      if (apply(p2, e.m)) {
          validMoveCount++;
          std::vector<Move> childPV;
          hashStack[ply] = p.h;
          // extensions
          int extension = 0;
          if (isInCheck) ++extension;
          ScoreType ttval = -pvs(-beta, -alpha, p2, depth - 1, pvnode, ply + 1, childPV, seldepth);
          if (!stopFlag && ttval > alpha) {
              alphaUpdated = true;
              updatePV(pv, e.m, childPV);
              if (ttval >= beta) {
                  //if (Move2Type(e.m) == T_std && !isInCheck) updateHistoryKillers(p, depth, ply, e.m);
                  TT::setEntry({ e.m,ttval,TT::B_beta,depth,computeHash(p) });
                  return ttval;
              }
              alpha = ttval;
              bestMove = e.m;
          }
      }
  }

  std::vector<Move> moves;
  generate(p,moves);
  if ( moves.empty() ) return isInCheck?-MATE + ply : 0;
  sort(moves,p,ply,&e);

  if (bestMove == INVALIDMOVE)  bestMove = moves[0]; // so that B_alpha are stored in TT

  bool improving = (!isInCheck && ply >= 2 && val >= scoreStack[ply - 2]);

  for(auto it = moves.begin() ; it != moves.end() && !stopFlag ; ++it){
     if ( e.h != 0 && sameMove(e.m, *it)) continue; // already tried
     Position p2 = p;
     if ( ! apply(p2,*it) ) continue;
     const Square to = Move2To(*it);
     if (p.c == Co_White && to == p.bk) return MATE - ply;
     if (p.c == Co_Black && to == p.wk) return MATE - ply;
     validMoveCount++;
     hashStack[ply] = p.h;
     std::vector<Move> childPV;
     // extensions
     int extension = 0;
     if (isInCheck) ++extension;
     if ( validMoveCount == 1 || !doPVS) val = -pvs(-beta,-alpha,p2,depth-1+extension,pvnode,ply+1,childPV,seldepth);
     else{
        // reductions & prunings
        int reduction = 0;
        bool isCheck = isAttacked(p2, kingSquare(p2));
        bool isAdvancedPawnPush = getPieceType(p,Move2From(*it)) == P_wp && (SQRANK(to) > 5 || SQRANK(to) < 2);
        bool isPrunable = !isInCheck && !isCheck && !isAdvancedPawnPush && Move2Type(*it) == T_std && !sameMove(*it, KillerT::killers[0][ply]) && !sameMove(*it, KillerT::killers[1][ply]);
        // futility
        if ( futility && isPrunable) continue;
        // LMP
        if ( lmp && isPrunable && validMoveCount >= lmpLimit[0][depth] ) continue;
        // SEE
        //if (!SEE(p, *it, -100*depth)) continue;
        if ( futility && Move2Score(*it) < -900 ) continue;
        // LMR
        if ( doLMR && !mateFinder && depth >= lmrMinDepth && isPrunable && std::abs(alpha) < MATE-MAX_DEPTH && std::abs(beta) < MATE-MAX_DEPTH ) reduction = lmrReduction[std::min((int)depth,MAX_DEPTH-1)][std::min(validMoveCount,MAX_DEPTH)];
        if (pvnode && reduction > 0) --reduction;
        if (!improving) ++reduction;
        // PVS
        val = -pvs(-alpha-1,-alpha,p2,depth-1-reduction+extension,false,ply+1,childPV,seldepth);
        if ( reduction > 0 && val > alpha ){
           childPV.clear();
           val = -pvs(-alpha-1,-alpha,p2,depth-1+extension,false,ply+1,childPV,seldepth);
        }
        if ( val > alpha && val < beta ){
           childPV.clear();
           val = -pvs(-beta,-alpha,p2,depth-1+extension,true,ply+1,childPV,seldepth); // potential new pv node
        }
     }

     if ( !stopFlag && val > alpha ){
        alphaUpdated = true;
        updatePV(pv, *it, childPV);
        if ( val >= beta ){
           if ( Move2Type(*it) == T_std && !isInCheck){
              updateHistoryKillers(p, depth, ply, *it);
              for(auto it2 = moves.begin() ; *it2!=*it; ++it2){
                  if ( Move2Type(*it2) == T_std ) HistoryT::update(depth,*it2,p,false);
              }
           }
           TT::setEntry({*it,val,TT::B_beta,depth,computeHash(p)});
           return val;
        }
        alpha = val;
        bestMove = *it;
     }
  }

  if ( validMoveCount == 0 ) return isInCheck?-MATE + ply : 0;
  if ( bestMove != INVALIDMOVE && !stopFlag) TT::setEntry({bestMove,alpha,alphaUpdated?TT::B_exact:TT::B_alpha,depth,computeHash(p)});

  return stopFlag?STOPSCORE:alpha;
}

std::vector<Move> search(const Position & p, Move & m, DepthType & d, ScoreType & sc, DepthType & seldepth){
  std::cout << "# Search called" << std::endl;
  std::cout << "# currentMoveMs " << currentMoveMs << std::endl;
  std::cout << "# requested depth " << (int) d << std::endl;
  stopFlag = false;
  stats.init();
  ///@todo do not reset that
  KillerT::initKillers();
  HistoryT::initHistory();
  ///@todo do not reset this either
  TT::clearTT();
  TT::clearETT();

  TimeMan::startTime = Clock::now();

  DepthType reachedDepth = 0;
  std::vector<Move> pv;
  ScoreType bestScore = 0;
  m = INVALIDMOVE;

  hashStack[0] = p.h;

  Counter previousNodeCount = 1;

  const Move bookMove = Book::Get(computeHash(p));
  if ( bookMove != INVALIDMOVE){
      pv .push_back(bookMove);
      m = pv[0];
      d = reachedDepth;
      sc = bestScore;
      return pv;
  }

  for(DepthType depth = 1 ; depth <= d && !stopFlag ; ++depth ){
    std::cout << "# Iterative deepening " << (int)depth << std::endl;
    std::vector<Move> pvLoc;
    ScoreType delta = (doWindow && depth>4)?25:MATE; // MATE not INFSCORE in order to enter the loop below once
    ScoreType alpha = std::max(ScoreType(bestScore - delta), ScoreType (-INFSCORE));
    ScoreType beta  = std::min(ScoreType(bestScore + delta), INFSCORE);
    ScoreType score = 0;
    while( delta <= MATE ){
       pvLoc.clear();
       score = pvs(alpha,beta,p,depth,true,(p.moves-1)*2+1+p.c==Co_Black?1:0,pvLoc,seldepth);
       if ( stopFlag ) break;
       delta *= 2;
       if (score <= alpha) alpha = std::max(ScoreType(score - delta), ScoreType(-MATE) );
       else if (score >= beta ) beta  = std::min(ScoreType(score + delta),  MATE );
       else break;
    }

    if ( !stopFlag ){
       pv = pvLoc;
       reachedDepth = depth;
       bestScore    = score;
       const int ms = std::max(1,(int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count());
       std::cout << int(depth)     << " " << bestScore << " " << ms/10 << " " << stats.nodes + stats.qnodes << " "
                 << (int)seldepth  << " " << int((stats.nodes + stats.qnodes)/(ms/1000.f)/1000.) << " " << stats.tthits/1000 << "\t"
                 << ToString(pv)   << " " << "EBF: " << float(stats.nodes + stats.qnodes)/previousNodeCount << std::endl;
       previousNodeCount = stats.nodes + stats.qnodes;
    }

    if (bestScore <= -MATE+1) break;
    if ( mateFinder && bestScore >= MATE - MAX_DEPTH ) break;
  }

  if (bestScore <= -MATE+1) {
      std::cout << "# Mated ..." << std::endl;
      pv.clear();
      pv.push_back(INVALIDMOVE);
  }

  if (bestScore >= MATE-MAX_DEPTH) std::cout << "# Forced mate found ..." << std::endl;

  if (pv.empty()) m = INVALIDMOVE;
  else m = pv[0];
  d = reachedDepth;
  sc = bestScore;
  return pv;
}

struct PerftAccumulator{
   PerftAccumulator(): pseudoNodes(0), validNodes(0), captureNodes(0), epNodes(0), checkNode(0), checkMateNode(0){}
   Counter pseudoNodes,validNodes,captureNodes,epNodes,checkNode,checkMateNode;

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
   if ( depth == 1 ) acc.pseudoNodes += moves.size();
   int validMoves = 0;
   for(auto it = moves.begin() ; it != moves.end() ; ++it){
     Position p2 = p;
     if ( ! apply(p2,*it) ) continue;
     ++validMoves;
     if ( divide && depth == 2 ) std::cout << "#" << ToString(p2) << std::endl;
     Counter nNodes = perft(p2,depth-1,acc,divide);
     if ( divide && depth == 2 ) std::cout << "#=> after " << ToString(*it) << " " << nNodes << std::endl;
     if ( divide && depth == 1 ) std::cout << "#" << (int)depth << " " <<  ToString(*it) << std::endl;
   }
   if ( depth == 1 ) acc.validNodes += validMoves;
   if ( divide && depth == 2 ) std::cout << "#********************" << std::endl;
   return acc.validNodes;
}

namespace XBoard{
   bool display;
   bool pondering;

   enum Mode : unsigned char { m_play_white = 0, m_play_black = 1, m_force = 2, m_analyze = 3 };
   Mode mode;

   enum SideToMove : unsigned char { stm_white = 0, stm_black = 1 };
   SideToMove stm;

   enum Ponder : unsigned char { p_off = 0, p_on = 1 };
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
      readFEN(startPosition,position);
   }

   SideToMove opponent(SideToMove & s) { return s == stm_white ? stm_black : stm_white; }

   void readLine() {
       command.clear();
       std::getline(std::cin, command);
       std::cout << "#Receive command : " << command << std::endl;
   }

   bool sideToMoveFromFEN(const std::string & fen) {
     bool b = readFEN(fen,position);
     stm = position.c == Co_White ? stm_white : stm_black;
     return b;
   }

   Move thinkUntilTimeUp(){
      std::cout << "#Thinking... " << std::endl;
      ScoreType score = 0;
      Move m;
      if ( depth < 0 ) depth = 64;
      std::cout << "#depth " << (int)depth << std::endl;
      currentMoveMs = TimeMan::GetNextMSecPerMove(position);
      std::cout << "#time  " << currentMoveMs << std::endl;
      std::cout << ToString(position) << std::endl;
      DepthType reachedDepth = depth;
      DepthType seldepth = 0;
      std::vector<Move> pv = search(position,m,reachedDepth,score,seldepth);
      std::cout << "#...done returning move " << ToString(m) << std::endl;
      return m;
   }

   bool makeMove(Move m,bool disp){
      if ( disp && m != INVALIDMOVE) std::cout << "move " << ToString(m) << std::endl;
      std::cout << ToString(position) << std::endl;
      return apply(position,m);
   }

   void ponderUntilInput(){
      std::cout << "#Pondering... " << std::endl;
      ScoreType score = 0;
      Move m;
      DepthType d = 1;
      DepthType seldepth = 0;
      std::vector<Move> pv = search(position,m,d,score,seldepth);
   }

   void stop(){ stopFlag = true; }

   void stopPonder(){
      if ((int)mode == (int)opponent(stm) && ponder == p_on && pondering) {
        pondering = false;
        stop();
      }
   }

   void xboard();

}

void XBoard::xboard(){
    std::cout << "#Starting XBoard main loop" << std::endl;
    ///@todo more feature disable !!
    std::cout << "feature ping=1 setboard=1 colors=0 usermove=1 memory=0 sigint=0 sigterm=0 otime=0 time=0 nps=0 myname=\"Minic " << MinicVersion << "\"" << std::endl;
    std::cout << "feature done=1" << std::endl;

    while(true) {
        std::cout << "#XBoard: mode " << mode << std::endl;
        std::cout << "#XBoard: stm  " << stm << std::endl;
        if(mode == m_analyze){
            //AnalyzeUntilInput(); ///@todo
        }
        // move as computer if mode is equal to stm
        if((int)mode == (int)stm) { // mouarfff
            move = thinkUntilTimeUp();
            if(move == INVALIDMOVE){ // game ends
                mode = m_force;
            }
            else{
                if ( ! makeMove(move,true) ){
                    std::cout << "#Bad computer move !" << std::endl;
                    std::cout << ToString(position) << std::endl;
                    mode = m_force;
                }
                stm = opponent(stm);
            }
        }

        // if not our turn, and ponder on, let's think ...
        if(move != INVALIDMOVE && (int)mode == (int)opponent(stm) && ponder == p_on && !pondering)  ponderUntilInput();

        bool commandOK = true;
        int once = 0;

        while(once++ == 0 || !commandOK){ // loop until a good command is found
            commandOK = true;
            // read next command !
            readLine();

            if ( command == "force")        mode = m_force;
            else if ( command == "xboard")  std::cout << "#This is minic!" << std::endl;
            else if ( command == "post")    display = true;
            else if ( command == "nopost")  display = false;
            else if ( command == "computer"){ }
            else if ( strncmp(command.c_str(),"protover",8) == 0){ }
            else if ( strncmp(command.c_str(),"accepted",8) == 0){ }
            else if ( strncmp(command.c_str(),"rejected",8) == 0){ }
            else if ( strncmp(command.c_str(),"ping",4) == 0){
                std::string str(command);
                size_t p = command.find("ping");
                str = str.substr(p+4);
                str = trim(str);
                std::cout << "pong " << str << std::endl;
            }
            else if ( command == "new"){
                stopPonder();
                stop();

                mode = (Mode)((int)stm); ///@todo should not be here !!! I thought start mode shall be analysis ...
                readFEN(startPosition,position);
                move = INVALIDMOVE;
                if(mode != m_analyze){
                    mode = m_play_black;
                    stm = stm_white;
                }
            }
            else if ( command == "white"){ // deprecated
                stopPonder();
                stop();

                mode = m_play_black;
                stm = stm_white;
            }
            else if ( command == "black"){ // deprecated
                stopPonder();
                stop();

                mode = m_play_white;
                stm = stm_black;
            }
            else if ( command == "go"){
                stopPonder();
                stop();

                mode = (Mode)((int)stm);
            }
            else if ( command == "playother"){
                stopPonder();
                stop();

                mode = (Mode)((int)opponent(stm));
            }
            else if ( strncmp(command.c_str(),"usermove",8) == 0){
                stopPonder();
                stop();

                std::string mstr(command);
                size_t p = command.find("usermove");
                mstr = mstr.substr(p + 8);
                mstr = trim(mstr);
                Square from = INVALIDSQUARE;
                Square to   = INVALIDSQUARE;
                MType mtype = T_std;
                readMove(position,mstr,from,to,mtype);
                Move m = ToMove(from,to,mtype);
                bool whiteToMove = position.c==Co_White;
                // convert castling Xboard notation to internal castling style if needed
                if ( mtype == T_std &&
                  from == (whiteToMove?Sq_e1:Sq_e8) &&
                  position.b[from] == (whiteToMove?P_wk:P_bk) ){
                  if ( to == (whiteToMove?Sq_c1:Sq_c8)) m = ToMove(from,to,whiteToMove?T_wqs:T_bqs);
                  else if ( to == (whiteToMove?Sq_g1:Sq_g8)) m = ToMove(from,to,whiteToMove?T_wks:T_bks);
                }
                if(!makeMove(m,false)){ // make move
                    commandOK = false;
                    std::cout << "#Bad opponent move !" << std::endl;
                    std::cout << ToString(position) << std::endl;
                    mode = m_force;
                }
                else{
                    stm = opponent(stm);
                }
            }
            else if (  strncmp(command.c_str(),"setboard",8) == 0){
                stopPonder();
                stop();

                std::string fen(command);
                size_t p = command.find("setboard");
                fen = fen.substr(p+8);
                if (!sideToMoveFromFEN(fen)){
                    std::cout << "#Illegal FEN" << std::endl;
                    commandOK = false;
                }
            }
            else if ( strncmp(command.c_str(),"result",6) == 0){
                stopPonder();
                stop();
                mode = m_force;
            }
            else if ( command == "analyze"){
                stopPonder();
                stop();
                mode = m_analyze;
            }
            else if ( command == "exit"){
                stopPonder();
                stop();
                mode = m_force;
            }
            else if ( command == "easy"){
                stopPonder();
                ponder = p_off;
            }
            else if ( command == "hard"){
                ponder = p_off; ///@todo
            }
            else if ( command == "quit"){
                _exit(0);
            }
            else if ( command == "pause"){
                stopPonder();
                readLine();
                while( command != "resume"){
                    std::cout << "#Error (paused): " << command << std::endl;
                    readLine();
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
            }
            else if(strncmp(command.c_str(), "level",5) == 0) {
                int timeLeft = 0;
                int sec = 0;
                int inc = 0;
                int mps = 0;
                if( sscanf(command.c_str(), "level %d %d %d", &mps, &timeLeft, &inc) != 3) sscanf(command.c_str(), "level %d %d:%d %d", &mps, &timeLeft, &sec, &inc);
                timeLeft *= 60000;
                timeLeft += sec * 1000;
                int msecinc = inc * 1000;
                TimeMan::isDynamic                = false;
                TimeMan::nbMoveInTC               = mps;
                TimeMan::msecPerMove              = -1;
                TimeMan::msecWholeGame            = timeLeft;
                TimeMan::msecInc                  = msecinc;
                depth                             = 64; // infinity
            }
            else if ( command == "edit"){ }
            else if ( command == "?"){
                stop();
            }
            else if ( command == "draw")  { }
            else if ( command == "undo")  { }
            else if ( command == "remove"){ }
            //************ end of Xboard command ********//
            else std::cout << "#Xboard does not know this command " << command << std::endl;
        } // readline
    } // while true

}

void perft_test(const std::string & fen, DepthType d, unsigned long long int expected) {
    Position p;
    readFEN(fen, p);
    std::cout << ToString(p) << std::endl;
    PerftAccumulator acc;
    if (perft(p, d, acc, false) != expected) std::cout << "Error !! " << fen << " " << expected << std::endl;
    acc.Display();
    std::cout << "##########################" << std::endl;
}

int main(int argc, char ** argv){

   if ( argc < 2 ) return 1;

   initHash();
   TT::initTable();
   TT::initETable();
   stats.init();
   init_lmr();
   initMvvLva();

   std::string cli = argv[1];

   if ( Book::fileExists("book.bin") ) {
       std::ifstream bbook = std::ifstream("book.bin",std::ios::in | std::ios::binary);
       Book::readBinaryBook(bbook);
   }

   if ( cli == "-xboard" ){
      XBoard::init();
      TimeMan::init();
      XBoard::xboard();
      return 0;
   }

   if ( cli == "-perft_test" ){
       perft_test(startPosition, 5, 4865609);
       perft_test("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ", 4, 4085603);
       perft_test("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ", 6, 11030083);
       perft_test("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5, 15833292);
   }

   if ( argc < 3 ) return 1;

#ifdef IMPORTBOOK
   if ( cli == "-buildBook"){
      Book::readBook(argv[2]);
      return 0;
   }
#endif

   std::string fen = argv[2];

   if ( fen == "start") fen = startPosition;
   if ( fen == "fine70") fen = fine70;

   Position p;
   if ( ! readFEN(fen,p) ){
      std::cout << "#Error reading fen" << std::endl;
      return 1;
   }
   std::cout << ToString(p) << std::endl;

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
         std::cout << ToString(*it,true) << std::endl;
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
      if ( argc >= 3 ) d = atoi(argv[3]);
      PerftAccumulator acc;
      perft(p,d,acc,false);
      acc.Display();
      return 0;
   }

   if ( cli == "-analyze" ){
      DepthType d = 5;
      if ( argc >= 3 ) d = atoi(argv[3]);
      Move bestMove = INVALIDMOVE;
      ScoreType s;
      TimeMan::isDynamic                = false;
      TimeMan::nbMoveInTC               = -1;
      TimeMan::msecPerMove              = 60*60*1000*24; // 1 day == infinity ...
      TimeMan::msecWholeGame            = -1;
      TimeMan::msecInc                  = -1;
      currentMoveMs = TimeMan::GetNextMSecPerMove(p);
      DepthType seldepth = 0;
      std::vector<Move> pv = search(p,bestMove,d,s,seldepth);
      std::cout << "#best move is " << ToString(bestMove) << " " << (int)d << " " << s << " pv : " << ToString(pv) << std::endl;
      return 0;
   }

   if ( cli == "-mateFinder" ){
      mateFinder = true;
      DepthType d = 5;
      if ( argc >= 3 ) d = atoi(argv[3]);
      Move bestMove = INVALIDMOVE;
      ScoreType s;
      TimeMan::isDynamic                = false;
      TimeMan::nbMoveInTC               = -1;
      TimeMan::msecPerMove              = 60*60*1000*24; // 1 day == infinity ...
      TimeMan::msecWholeGame            = -1;
      TimeMan::msecInc                  = -1;
      currentMoveMs = TimeMan::GetNextMSecPerMove(p);
      DepthType seldepth = 0;
      std::vector<Move> pv = search(p,bestMove,d,s,seldepth);
      std::cout << "#best move is " << ToString(bestMove) << " " << (int)d << " " << s << " pv : " << ToString(pv) << std::endl;
      return 0;
   }

   std::cout << "Error : unknown command line" << std::endl;
   return 1;
}
