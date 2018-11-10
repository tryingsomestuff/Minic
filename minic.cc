#include <algorithm>
#include <assert.h>
#include <bitset>
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
#include <intrin.h>
typedef uint64_t u_int64_t;
#else
#include <unistd.h>
#endif

//#define IMPORTBOOK

const std::string MinicVersion = "0.14";

typedef std::chrono::high_resolution_clock Clock;
typedef char DepthType;
typedef int Move;    // invalid if < 0
typedef char Square; // invalid if < 0
typedef uint64_t Hash; // invalid if == 0
typedef uint64_t Counter;
typedef short int ScoreType;
typedef uint64_t BitBoard;

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
const int       singularExtensionDepth   = 8;

const int lmpLimit[][lmpMaxDepth + 1] = {
    { 0, 3, 4, 6, 10, 15, 21, 28, 36, 45, 55 } ,
    { 0, 5, 6, 9, 15, 23, 32, 42, 54, 68, 83 } };

int lmrReduction[MAX_DEPTH][MAX_MOVE];

void init_lmr(){
    std::cout << "# Init lmr" << std::endl;
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
std::string fine70        = "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - -";

int currentMoveMs = 777;

struct Stats{
   Counter nodes, qnodes, tthits;
   void init(){
      std::cout << "# Init stat" << std::endl;
      nodes  = 0;  qnodes = 0;   tthits = 0;
   }
};

Stats stats;

enum Piece : char{ P_bk = -6,   P_bq = -5,   P_br = -4,   P_bb = -3,   P_bn = -2,   P_bp = -1,   P_none = 0,   P_wp = 1,    P_wn = 2,    P_wb = 3,    P_wr = 4,    P_wq = 5,    P_wk = 6 };

const int PieceShift = 6;

ScoreType   Values[13]    = { -8000, -1025, -477, -365, -337, -82, 0, 82, 337, 365, 477, 1025, 8000 };
ScoreType   ValuesEG[13]  = { -8000,  -936, -512, -297, -281, -94, 0, 94, 281, 297, 512,  936, 8000 };
std::string Names[13]     = { "k", "q", "r", "b", "n", "p", " ", "P", "N", "B", "R", "Q", "K" };

std::string Squares[64] = { "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3", "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6", "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8" };

enum Sq : char { Sq_a1 =  0,Sq_b1,Sq_c1,Sq_d1,Sq_e1,Sq_f1,Sq_g1,Sq_h1,Sq_a2,Sq_b2,Sq_c2,Sq_d2,Sq_e2,Sq_f2,Sq_g2,Sq_h2,Sq_a3,Sq_b3,Sq_c3,Sq_d3,Sq_e3,Sq_f3,Sq_g3,Sq_h3,Sq_a4,Sq_b4,Sq_c4,Sq_d4,Sq_e4,Sq_f4,Sq_g4,Sq_h4,Sq_a5,Sq_b5,Sq_c5,Sq_d5,Sq_e5,Sq_f5,Sq_g5,Sq_h5,Sq_a6,Sq_b6,Sq_c6,Sq_d6,Sq_e6,Sq_f6,Sq_g6,Sq_h6,Sq_a7,Sq_b7,Sq_c7,Sq_d7,Sq_e7,Sq_f7,Sq_g7,Sq_h7,Sq_a8,Sq_b8,Sq_c8,Sq_d8,Sq_e8,Sq_f8,Sq_g8,Sq_h8};

enum Castling : char{ C_none= 0, C_wks = 1, C_wqs = 2, C_bks = 4, C_bqs = 8 };

int MvvLvaScores[6][6];

void initMvvLva(){
    std::cout << "# Init mvv-lva" << std::endl;
    static ScoreType IValues[6] = { 1, 2, 3, 5, 9, 20 };
    for(int v = 0; v < 6 ; ++v)
        for(int a = 0; a < 6 ; ++a)
           MvvLvaScores[v][a] = IValues[v] * 20 - IValues[a];
}

enum MType : char{
   T_std        = 0,   T_capture    = 1,   T_ep         = 2,   T_check      = 3, // not used yet
   T_promq      = 4,   T_promr      = 5,   T_promb      = 6,   T_promn      = 7,
   T_cappromq   = 8,   T_cappromr   = 9,   T_cappromb   = 10,  T_cappromn   = 11,
   T_wks        = 12,  T_wqs        = 13,  T_bks        = 14,  T_bqs        = 15
};

enum Color : char{ Co_None  = -1,   Co_White = 0,   Co_Black = 1 };

// ttmove 3000, promcap >1000, cap, checks, killers, castling, other by history < 200.
ScoreType MoveScoring[16] = { 0, 1000, 1100, 300, 950, 500, 350, 300, 1950, 1500, 1350, 1300, 250, 250, 250, 250 };

Color Colors[13] = { Co_Black, Co_Black, Co_Black, Co_Black, Co_Black, Co_Black, Co_None, Co_White, Co_White, Co_White, Co_White, Co_White, Co_White};

#ifdef __MINGW32__
#define POPCOUNT(x)   int(__builtin_popcountll(x))
inline int BitScanForward(u_int64_t bb) { assert(bb != 0); return __builtin_ctzll(bb);}
#define bsf(x,i)      (i=BitScanForward(x))
#define swapbits(x)   (__builtin_bswap64 (x))
#else
#ifdef _WIN32
#define POPCOUNT(x)   __popcnt64(x)
#define bsf(x,i)      _BitScanForward64(&i,x)
#define swapbits(x)   (_byteswap_uint64 (x))
#else // linux
#define POPCOUNT(x)   int(__builtin_popcountll(x))
inline int BitScanForward(u_int64_t bb) { assert(bb != 0); return __builtin_ctzll(bb);}
#define bsf(x,i)      (i=BitScanForward(x))
#define swapbits(x)   (__builtin_bswap64 (x))
#endif
#endif

#define SquareToBitboard(k) (1ull<<k)
inline uint64_t  countBit(const BitBoard & b) { return POPCOUNT(b);}
inline void      setBit  (BitBoard & b, Square k) { b |= SquareToBitboard(k);}
inline void      unSetBit(BitBoard & b, Square k) { b &= ~SquareToBitboard(k);}
inline bool      isSet   (const BitBoard & b, Square k) { return (SquareToBitboard(k) & b) != 0;}

const BitBoard whiteSquare        = 0x55AA55AA55AA55AA;
const BitBoard blackSquare        = 0xAA55AA55AA55AA55;
const BitBoard whiteSideSquare    = 0x00000000FFFFFFFF;
const BitBoard blackSideSquare    = 0xFFFFFFFF00000000;
const BitBoard whiteKingQueenSide = 0x0000000000000007;
const BitBoard whiteKingKingSide  = 0x00000000000000E0;
const BitBoard blackKingQueenSide = 0x0700000000000000;
const BitBoard blackKingKingSide  = 0xe000000000000000;
const BitBoard fileA              = 0x0101010101010101;
const BitBoard fileH              = 0x8080808080808080;
const BitBoard rank1              = 0x00000000000000ff;
const BitBoard rank2              = 0x000000000000ff00;
const BitBoard rank3              = 0x0000000000ff0000;
const BitBoard rank4              = 0x00000000ff000000;
const BitBoard rank5              = 0x000000ff00000000;
const BitBoard rank6              = 0x0000ff0000000000;
const BitBoard rank7              = 0x00ff000000000000;
const BitBoard rank8              = 0xff00000000000000;
BitBoard dummy                    = 0x0ull;

std::string showBitBoard(const BitBoard & b) {
    std::bitset<64> bs(b);
    std::stringstream ss;
    ss << std::endl;
    for (int j = 7; j >= 0; --j) {
        ss << "# +-+-+-+-+-+-+-+-+" << std::endl;
        ss << "# |";
        for (int i = 0; i < 8; ++i) ss << (bs[i + j * 8] ? "X" : " ") << '|';
        ss << std::endl;
    }
    ss << "# +-+-+-+-+-+-+-+-+";
    return ss.str();
}

struct Position{
  Piece b[64];
  BitBoard whitePawn,whiteKnight,whiteBishop,whiteRook,whiteQueen,whiteKing;
  BitBoard blackPawn,blackKnight,blackBishop,blackRook,blackQueen,blackKing;
  BitBoard whitePiece, blackPiece, occupancy;

  unsigned char fifty;
  unsigned char moves;
  unsigned char ply; // this is not the "same" play as the one used to get seldepth
  unsigned int castling;
  Square ep, wk, bk;
  Color c;
  mutable Hash h;
  Move lastMove;

  unsigned char nwk = 0;
  unsigned char nwq = 0;
  unsigned char nwr = 0;
  unsigned char nwb = 0;
  unsigned char nwn = 0;
  unsigned char nwp = 0;
  unsigned char nbk = 0;
  unsigned char nbq = 0;
  unsigned char nbr = 0;
  unsigned char nbb = 0;
  unsigned char nbn = 0;
  unsigned char nbp = 0;
  unsigned char nk  = 0;
  unsigned char nq  = 0;
  unsigned char nr  = 0;
  unsigned char nb  = 0;
  unsigned char nn  = 0;
  unsigned char np  = 0;
  unsigned char nwM = 0;
  unsigned char nbM = 0;
  unsigned char nwm = 0;
  unsigned char nbm = 0;
  unsigned char nwt = 0;
  unsigned char nbt = 0;

};

void initMaterial(Position & p){
    p.nwk = (unsigned char)countBit(p.whiteKing);
    p.nwq = (unsigned char)countBit(p.whiteQueen);
    p.nwr = (unsigned char)countBit(p.whiteRook);
    p.nwb = (unsigned char)countBit(p.whiteBishop);
    p.nwn = (unsigned char)countBit(p.whiteKnight);
    p.nwp = (unsigned char)countBit(p.whitePawn);
    p.nbk = (unsigned char)countBit(p.blackKing);
    p.nbq = (unsigned char)countBit(p.blackQueen);
    p.nbr = (unsigned char)countBit(p.blackRook);
    p.nbb = (unsigned char)countBit(p.blackBishop);
    p.nbn = (unsigned char)countBit(p.blackKnight);
    p.nbp = (unsigned char)countBit(p.blackPawn);
    p.nk = p.nwk + p.nbk;
    p.nq = p.nwq + p.nbq;
    p.nr = p.nwr + p.nbr;
    p.nb = p.nwb + p.nbb;
    p.nn = p.nwn + p.nbn;
    p.np = p.nwp + p.nbp;
    p.nwt = p.nwq + p.nwr + p.nwb + p.nwn;
    p.nbt = p.nbq + p.nbr + p.nbb + p.nbn;
    p.nwM = p.nwq + p.nwr;
    p.nbM = p.nbq + p.nbr;
    p.nwm = p.nwb + p.nwn;
    p.nbm = p.nbb + p.nbn;
}

void initBitBoards(Position & p) {
    p.whitePawn = p.whiteKnight = p.whiteBishop = p.whiteRook = p.whiteQueen = p.whiteKing = 0ull;
    p.blackPawn = p.blackKnight = p.blackBishop = p.blackRook = p.blackQueen = p.blackKing = 0ull;
    p.whitePiece = p.blackPiece = p.occupancy = 0ull;
}

void setBitBoards(Position & p) {
    initBitBoards(p);
    for (Square k = 0; k < 64; ++k) {
        switch (p.b[k]){
        case P_none: break;
        case P_wp: p.whitePawn   |= SquareToBitboard(k); break;
        case P_wn: p.whiteKnight |= SquareToBitboard(k); break;
        case P_wb: p.whiteBishop |= SquareToBitboard(k); break;
        case P_wr: p.whiteRook   |= SquareToBitboard(k); break;
        case P_wq: p.whiteQueen  |= SquareToBitboard(k); break;
        case P_wk: p.whiteKing   |= SquareToBitboard(k); break;
        case P_bp: p.blackPawn   |= SquareToBitboard(k); break;
        case P_bn: p.blackKnight |= SquareToBitboard(k); break;
        case P_bb: p.blackBishop |= SquareToBitboard(k); break;
        case P_br: p.blackRook   |= SquareToBitboard(k); break;
        case P_bq: p.blackQueen  |= SquareToBitboard(k); break;
        case P_bk: p.blackKing   |= SquareToBitboard(k); break;
        default: assert(false);
        }
   }
   p.whitePiece = p.whitePawn | p.whiteKnight | p.whiteBishop | p.whiteRook | p.whiteQueen | p.whiteKing;
   p.blackPiece = p.blackPawn | p.blackKnight | p.blackBishop | p.blackRook | p.blackQueen | p.blackKing;
   p.occupancy  = p.whitePiece | p.blackPiece;
}

inline Piece getPieceIndex (const Position &p, Square k){ assert(k >= 0 && k < 64); return Piece(p.b[k] + PieceShift);}

inline Piece getPieceType  (const Position &p, Square k){ assert(k >= 0 && k < 64); return (Piece)std::abs(p.b[k]);}

inline Color getColor      (const Position &p, Square k){ assert(k >= 0 && k < 64); return Colors[getPieceIndex(p,k)];}

inline std::string getName (const Position &p, Square k){ assert(k >= 0 && k < 64); return Names[getPieceIndex(p,k)];}

inline ScoreType getValue  (const Position &p, Square k){ assert(k >= 0 && k < 64); return Values[getPieceIndex(p,k)];}

inline ScoreType getValueEG(const Position &p, Square k){ assert(k >= 0 && k < 64); return ValuesEG[getPieceIndex(p,k)];}

inline void unSetBit(Position & p, Square k) { ///@todo keep this lookup table in position and implemente copy CTOR and operator
    assert(k >= 0 && k < 64);
    BitBoard* allB[13] = { &(p.blackKing),&(p.blackQueen),&(p.blackRook),&(p.blackBishop),&(p.blackKnight),&(p.blackPawn),&dummy,&(p.whitePawn),&(p.whiteKnight),&(p.whiteBishop),&(p.whiteRook),&(p.whiteQueen),&(p.whiteKing) };
    unSetBit(*allB[getPieceIndex(p, k)], k);
}
inline void unSetBit(Position &p, Square k, Piece pp) { ///@todo keep this lookup table in position and implemente copy CTOR and operator
    assert(k >= 0 && k < 64);
    BitBoard* allB[13] = { &(p.blackKing),&(p.blackQueen),&(p.blackRook),&(p.blackBishop),&(p.blackKnight),&(p.blackPawn),&dummy,&(p.whitePawn),&(p.whiteKnight),&(p.whiteBishop),&(p.whiteRook),&(p.whiteQueen),&(p.whiteKing) };
    unSetBit(*allB[pp + PieceShift], k);
}
inline void setBit(Position &p, Square k, Piece pp) { ///@todo keep this lookup table in position and implemente copy CTOR and operator
    assert(k >= 0 && k < 64);
    BitBoard* allB[13] = { &(p.blackKing),&(p.blackQueen),&(p.blackRook),&(p.blackBishop),&(p.blackKnight),&(p.blackPawn),&dummy,&(p.whitePawn),&(p.whiteKnight),&(p.whiteBishop),&(p.whiteRook),&(p.whiteQueen),&(p.whiteKing) };
    setBit(*allB[pp + PieceShift], k);
}

inline ScoreType Move2Score(Move h) { assert(h != INVALIDMOVE); return (h >> 16) & 0xFFFF; }
inline Square    Move2From (Move h) { assert(h != INVALIDMOVE); return (h >> 10) & 0x3F  ; }
inline Square    Move2To   (Move h) { assert(h != INVALIDMOVE); return (h >>  4) & 0x3F  ; }
inline MType     Move2Type (Move h) { assert(h != INVALIDMOVE); return MType(h & 0xF)    ; }
inline Move      ToMove(Square from, Square to, MType type)                  { assert(from >= 0 && from < 64); assert(to >= 0 && to < 64); return                 (from << 10) | (to << 4) | type; }
inline Move      ToMove(Square from, Square to, MType type, ScoreType score) { assert(from >= 0 && from < 64); assert(to >= 0 && to < 64); return (score << 16) | (from << 10) | (to << 4) | type; }

Hash randomInt(){
    static std::mt19937 mt(42); // fixed seed for ZHash !!!
    static std::uniform_int_distribution<unsigned long long int> dist(0, UINT64_MAX);
    return dist(mt);
}

Hash ZT[64][14]; // should be 13 but last ray is for castling[0 7 56 63][13] and ep [k][13] and color [3 4][13]

void initHash(){
   std::cout << "#Init hash" << std::endl;
   for(int k = 0 ; k < 64 ; ++k)
      for(int j = 0 ; j < 14 ; ++j)
         ZT[k][j] = randomInt();
}

Hash computeHash(const Position &p){
   if (p.h != 0) return p.h;
   for (int k = 0; k < 64; ++k){
      const Piece pp = p.b[k];
      if ( pp != P_none) p.h ^= ZT[k][pp+PieceShift];
   }
   if ( p.ep != INVALIDSQUARE ) p.h ^= ZT[p.ep][13];
   if ( p.castling & C_wks)     p.h ^= ZT[7][13];
   if ( p.castling & C_wqs)     p.h ^= ZT[0][13];
   if ( p.castling & C_bks)     p.h ^= ZT[63][13];
   if ( p.castling & C_bqs)     p.h ^= ZT[56][13];
   if ( p.c == Co_White)        p.h ^= ZT[3][13];
   if ( p.c == Co_Black)        p.h ^= ZT[4][13];
   return p.h;
}

namespace TT{ ///@todo make this a namespace will get some lines back
   enum Bound{ B_exact = 0, B_alpha = 1, B_beta  = 2 };
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

   unsigned int powerFloor(unsigned int x) {
       unsigned int power = 1;
       while (power < x) power *= 2;
       return power/2;
   }

   static unsigned int ttSize = 0;
   static Bucket * table = 0;

   void initTable(){
      std::cout << "# Init TT" << std::endl;
      ttSize = powerFloor(ttSizeMb * 1024 * 1024 / (unsigned int)sizeof(Bucket));
      table = new Bucket[ttSize];
      std::cout << "# Size of TT " << ttSize * sizeof(Bucket) / 1024 / 1024 << "Mb" << std::endl;
   }

   void clearTT() {
       for (unsigned int k = 0; k < ttSize; ++k) {
           table[k].e[0] = { INVALIDMOVE, 0, B_alpha, 0, 0 };
           table[k].e[1] = { INVALIDMOVE, 0, B_alpha, 0, 0 };
       }
   }

   bool getEntry(Hash h, DepthType d, Entry & e, int nbuck = 0) {
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

   void setEntry(const Entry & e){
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

   unsigned int ttESize = 0;
   EvalEntry * evalTable = 0;

   void initETable() {
       std::cout << "# Init eval TT" << std::endl;
       ttESize = powerFloor(ttESizeMb * 1024 * 1024 / (unsigned int)sizeof(EvalEntry));
       evalTable = new EvalEntry[ttESize];
       std::cout << "# Size of ETT " << ttESize * sizeof(EvalEntry) / 1024 / 1024 << "Mb" << std::endl;
   }

   void clearETT() {
       for (unsigned int k = 0; k < ttESize; ++k) evalTable[k] = { 0, 0., 0 };
   }

   bool getEvalEntry(Hash h, ScoreType & score, float & gp) {
       assert(h > 0);
       const EvalEntry & _e = evalTable[h%ttESize];
       if (_e.h != h) return false;
       score = _e.score;
       gp = _e.gp;
       return true;
   }

   void setEvalEntry(const EvalEntry & e) {
       assert(e.h > 0);
       evalTable[e.h%ttESize] = e; // always replace
   }

}

namespace KillerT{
   Move killers[2][MAX_PLY];
   void initKillers(){
      std::cout << "# Init killers" << std::endl;
      for(int i = 0; i < 2; ++i)
          for(int k = 0 ; k < MAX_PLY; ++k)
              killers[i][k] = INVALIDMOVE;
   }
}

namespace HistoryT{
   ScoreType history[13][64];
   void initHistory(){
      std::cout << "# Init history" << std::endl;
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
                if (count != 0) { ss << count; count = 0; }
                ss << getName(p,k);
            }
            if (j == 7) {
                if (count != 0) { ss << count; count = 0; }
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
    if (p.castling & C_wks) { ss << "K"; withCastling = true; }
    if (p.castling & C_wqs) { ss << "Q"; withCastling = true; }
    if (p.castling & C_bks) { ss << "k"; withCastling = true; }
    if (p.castling & C_bqs) { ss << "q"; withCastling = true; }
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

std::string ToString(const Move & m, bool withScore = false){
   if ( m == INVALIDMOVE ) return "invalid move";
   std::stringstream ss;
   std::string prom;
   const std::string score = (withScore ? " (" + std::to_string(Move2Score(m)) + ")" : "");
   switch (Move2Type(m)) {
   case T_bks: case T_wks:
       return "O-O" + score;
   case T_bqs: case T_wqs:
       return "O-O-O" + score;
   default:
       static const std::string suffixe[] = { "","","","","q","r","b","n","q","r","b","n" };
       prom = suffixe[Move2Type(m)];
       break;
   }
   ss << Squares[Move2From(m)] << Squares[Move2To(m)];
   return ss.str() + prom + score;
}

std::string ToString(const std::vector<Move> & moves){
   std::stringstream ss;
   for(size_t k = 0 ; k < moves.size(); ++k) ss << ToString(moves[k]) << " ";
   return ss.str();
}

ScoreType eval(const Position & p, float & gp); //forward decl

std::string ToString(const Position & p){
    std::stringstream ss;
    ss << "#" << std::endl;
    for (Square j = 7; j >= 0; --j) {
        ss << "# +-+-+-+-+-+-+-+-+" << std::endl;
        ss << "# |";
        for (Square i = 0; i < 8; ++i) ss << getName(p,i+j*8) << '|';
        ss << std::endl; ;
    }
    ss << "# +-+-+-+-+-+-+-+-+" << "#" << std::endl;
    if ( p.ep >=0 ) ss << "# ep " << Squares[p.ep]<< std::endl;
    ss << "# wk " << Squares[p.wk] << std::endl << "# bk " << Squares[p.bk] << std::endl;
    ss << "# Turn " << (p.c == Co_White ? "white" : "black") << std::endl;
    ScoreType sc = 0;
    float gp = 0;
    sc = eval(p, gp);
    ss << "# Phase " << gp << std::endl << "# Static score " << sc << std::endl << "# Hash " << computeHash(p) << std::endl << "# FEN " << GetFEN(p) << std::endl;
    return ss.str();
}

inline Color opponentColor(const Color c){ return Color((c+1)%2);}

bool stopFlag = false;

const int mailbox[120] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7, -1, -1,  8,  9, 10, 11, 12, 13, 14, 15, -1, -1, 16, 17, 18, 19, 20, 21, 22, 23, -1, -1, 24, 25, 26, 27, 28, 29, 30, 31, -1, -1, 32, 33, 34, 35, 36, 37, 38, 39, -1, -1, 40, 41, 42, 43, 44, 45, 46, 47, -1, -1, 48, 49, 50, 51, 52, 53, 54, 55, -1, -1, 56, 57, 58, 59, 60, 61, 62, 63, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

const int mailbox64[64] = { 21, 22, 23, 24, 25, 26, 27, 28, 31, 32, 33, 34, 35, 36, 37, 38, 41, 42, 43, 44, 45, 46, 47, 48, 51, 52, 53, 54, 55, 56, 57, 58, 61, 62, 63, 64, 65, 66, 67, 68, 71, 72, 73, 74, 75, 76, 77, 78, 81, 82, 83, 84, 85, 86, 87, 88, 91, 92, 93, 94, 95, 96, 97, 98};

bool Slide[6] = {false, false, true, true, true, false};
int Offsets[6] = {8, 8, 4, 4, 8, 8};
int Offset[6][8] = { { -20, -10, -11, -9, 9, 11, 10, 20 }, { -21, -19, -12, -8, 8, 12, 19, 21 }, { -11,  -9,   9, 11, 0,  0,  0,  0 }, { -10,  -1,   1, 10, 0,  0,  0,  0 }, { -11, -10,  -9, -1, 1,  9, 10, 11 }, { -11, -10,  -9, -1, 1,  9, 10, 11 } };

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

    // reset position
    p.h = 0;
    for(Square k = 0 ; k < 64 ; ++k) p.b[k] = P_none;

    Square j = 1, i = 0;
    while ((j <= 64) && (i <= (char)strList[0].length())){
        char letter = strList[0].at(i);
        ++i;
        Square k = (7 - (j - 1) / 8) * 8 + ((j - 1) % 8);
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
        else { std::cout << "#FEN ERROR 1" << std::endl; return false; }
    }

    // Initialize all castle possibilities (default is none)
    p.castling = C_none;
    if (strList.size() >= 3){
        bool found = false;
        if (strList[2].find('K') != std::string::npos){ p.castling |= C_wks; found = true; }
        if (strList[2].find('Q') != std::string::npos){ p.castling |= C_wqs; found = true; }
        if (strList[2].find('k') != std::string::npos){ p.castling |= C_bks; found = true; }
        if (strList[2].find('q') != std::string::npos){ p.castling |= C_bqs; found = true; }
        if ( ! found ){ std::cout << "#No castling right given" << std::endl; }
    }
    else std::cout << "#No castling right given" << std::endl;

    // read en passant and save it (default is invalid)
    p.ep = INVALIDSQUARE;
    if ((strList.size() >= 4) && strList[3] != "-" ){
        if (strList[3].length() >= 2){
            if ((strList[3].at(0) >= 'a') && (strList[3].at(0) <= 'h') && ((strList[3].at(1) == '3') || (strList[3].at(1) == '6'))) p.ep = (strList[3].at(0)-97) + 8*(strList[3].at(1));
            else { std::cout << "#FEN ERROR 3-1 : bad en passant square : " << strList[3] << std::endl; return false; }
        }
        else{ std::cout << "#FEN ERROR 3-2 : bad en passant square : " << strList[3] << std::endl; return false; }
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

    p.ply = (p.moves - 1) * 2 + 1 + p.c == Co_Black ? 1 : 0;

    p.h = computeHash(p);

    setBitBoards(p);

    initMaterial(p);

    return true;
}

void tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters = " " ){
  std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
  std::string::size_type pos     = str.find_first_of(delimiters, lastPos);
  while (std::string::npos != pos || std::string::npos != lastPos) {
    tokens.push_back(str.substr(lastPos, pos - lastPos));
    lastPos = str.find_first_not_of(delimiters, pos);
    pos     = str.find_first_of(delimiters, lastPos);
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
    if ( strList.empty()){ std::cout << "#Trying to read bad move, seems empty " << str << std::endl; return false; }

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
        if ( strList.size() == 1 ){ std::cout << "#Trying to read bad move, malformed (=1) " << str << std::endl; return false; }
        if ( strList.size() > 2 && strList[2] != "ep"){ std::cout << "#Trying to read bad move, malformed (>2)" << str << std::endl; return false; }

        if (strList[0].size() == 2 && (strList[0].at(0) >= 'a') && (strList[0].at(0) <= 'h') && ((strList[0].at(1) >= 1) && (strList[0].at(1) <= '8'))) from = stringToSquare(strList[0]);
        else { std::cout << "#Trying to read bad move, invalid from square " << str << std::endl; return false; }

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
        else { std::cout << "#Trying to read bad move, invalid to square " << str << std::endl; return false; }
    }

    if (getPieceType(p,from) == P_wp && to == p.ep) moveType = T_ep;

    return true;
}

namespace TimeMan{
   int msecPerMove, msecWholeGame, nbMoveInTC, msecInc;
   bool isDynamic;

   std::chrono::time_point<Clock> startTime;

   void init(){
      std::cout << "# Init timemane" << std::endl;
      msecPerMove   = 777;
      msecWholeGame = -1;
      nbMoveInTC    = -1;
      msecInc       = -1;
      isDynamic     = false;
   }

   int GetNextMSecPerMove(const Position & p){
      int ms = -1;
      std::cout << "# msecPerMove   " << msecPerMove   << std::endl;
      std::cout << "# msecWholeGame " << msecWholeGame << std::endl;
      std::cout << "# msecInc       " << msecInc       << std::endl;
      std::cout << "# nbMoveInTC    " << nbMoveInTC    << std::endl;
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
         ms = int(0.85 * (msecWholeGame+((msecInc>0)?p.moves*msecInc:0))/ (float)nmoves);
      }
      return ms;
   }
}

inline void addMove(Square from, Square to, MType type, std::vector<Move> & moves){ assert( from >= 0 && from < 64); assert( to >=0 && to < 64); moves.push_back(ToMove(from,to,type,0));}

Square kingSquare(const Position & p) { return (p.c == Co_White) ? p.wk : p.bk; }

namespace BB {
    int ranks[512];
    struct Mask {
        BitBoard diagonal, antidiagonal, file;
        BitBoard pawnAttack[2], push[2]; // one for each colors
        BitBoard enpassant, knight, king;
        Mask() :diagonal(0ull), antidiagonal(0ull), file(0ull), pawnAttack{ 0ull,0ull }, push{ 0ull,0ull }, enpassant(0ull), knight(0ull), king(0ull){}
    };
    Mask mask[64];

    inline void initMask() {
        std::cout << "# Init mask" << std::endl;
        int d[64][64] = { 0 };
        for (Square x = 0; x < 64; ++x) {
            for (int i = -1; i <= 1; ++i) {
                for (int j = -1; j <= 1; ++j) {
                    if (i == 0 && j == 0) continue;
                    for (int r = (x >> 3) + i, f = (x & 7) + j; 0 <= r && r < 8 && 0 <= f && f < 8; r += i, f += j) {
                        int y = 8 * r + f;
                        d[x][y] = (8 * i + j);
                        //mask[x].direction[y] = abs(d[x][y]);
                        //for (int z = x + d[x][y]; z != y; z += d[x][y]) mask[x].between[y] |= 1UL << z;
                    }
                }
            }

            //mask[x].bit = 1UL << x;

            for (int y = x - 9; y >= 0 && d[x][y] == -9; y -= 9) mask[x].diagonal |= 1ull << y;
            for (int y = x + 9; y < 64 && d[x][y] ==  9; y += 9) mask[x].diagonal |= 1ull << y;

            for (int y = x - 7; y >= 0 && d[x][y] == -7; y -= 7) mask[x].antidiagonal |= 1ull << y;
            for (int y = x + 7; y < 64 && d[x][y] ==  7; y += 7) mask[x].antidiagonal |= 1ull << y;

            for (int y = x - 8; y >= 0; y -= 8) mask[x].file |= 1ull << y;
            for (int y = x + 8; y < 64; y += 8) mask[x].file |= 1ull << y;

            int f = x & 07;
            int r = x >> 3;
            for (int i = -1, c = 1; i <= 1; i += 2, c = 0) {
                for (int j = -1; j <= 1; j += 2) {
                    if (0 <= r + i && r + i < 8 && 0 <= f + j && f + j < 8) {  mask[x].pawnAttack[c] |= 1ull << ((r + i) * 8 + (f + j)); }
                }
                if (0 <= r + i && r + i < 8) { mask[x].push[c] = 1ull << ((r + i) * 8 + f); }
            }
            if (r == 3 || r == 4) {
                if (f > 0) mask[x].enpassant |= 1ull << (x - 1);
                if (f < 7) mask[x].enpassant |= 1ull << (x + 1);
            }

            for (int i = -2; i <= 2; i = (i == -1 ? 1 : i + 1)) {
                for (int j = -2; j <= 2; ++j) {
                    if (i == j || i == -j || j == 0) continue;
                    if (0 <= r + i && r + i < 8 && 0 <= f + j && f + j < 8) { mask[x].knight |= 1ull << (8 * (r + i) + (f + j)); }
                }
            }

            for (int i = -1; i <= 1; ++i) {
                for (int j = -1; j <= 1; ++j) {
                    if (i == 0 && j == 0) continue;
                    if (0 <= r + i && r + i < 8 && 0 <= f + j && f + j < 8) { mask[x].king |= 1ull << (8 * (r + i) + (f + j)); }
                }
            }
            //mask[x].castling = 15;
        }

        //foreach(k; 0 .. 6) mask[castlingX[k]].castling = castling[k];

        for (Square o = 0; o < 64; ++o) {
            for (int k = 0; k < 8; ++k) {
                int y = 0;
                for (int x = k - 1; x >= 0; --x) {
                    BitBoard b = 1ull << x;
                    y |= b;
                    if (((o << 1) & b) == b) break;
                }
                for (int x = k + 1; x < 8; ++x) {
                    BitBoard b = 1ull << x;
                    y |= b;
                    if (((o << 1) & b) == b) break;
                }
                ranks[o * 8 + k] = y;
            }
        }
    }

    inline BitBoard attack(const BitBoard occupancy, const Square x, const BitBoard m) {
        BitBoard forward = occupancy & m;
        BitBoard reverse = swapbits(forward);
        forward -= (1ull << x);
        reverse -= (1ull << (x ^ 63));
        forward ^= swapbits(reverse);
        forward &= m;
        return forward;
    }

    inline BitBoard rankAttack(const BitBoard occupancy, const Square x) {
        const int f = x & 7;
        const int r = x & 56;
        const BitBoard o = (occupancy >> r) & 126;
        return BitBoard(ranks[o * 4 + f]) << r;
    }

    inline BitBoard fileAttack(const BitBoard occupancy, const Square x) { return attack(occupancy, x, mask[x].file); }

    inline BitBoard diagonalAttack(const BitBoard occupancy, const Square x) { return attack(occupancy, x, mask[x].diagonal); }

    inline BitBoard antidiagonalAttack(const BitBoard occupancy, const Square x) { return attack(occupancy, x, mask[x].antidiagonal); }

    template < Piece > BitBoard coverage(const Square x, const BitBoard occupancy = 0, const Color c = Co_White) { assert(false); return 0ull; }
    template <       > BitBoard coverage<P_wp>(const Square x, const BitBoard occupancy, const Color c) { return mask[x].pawnAttack[c]; }
    template <       > BitBoard coverage<P_wn>(const Square x, const BitBoard occupancy, const Color c) { return mask[x].knight; }
    template <       > BitBoard coverage<P_wb>(const Square x, const BitBoard occupancy, const Color c) { return diagonalAttack(occupancy, x) + antidiagonalAttack(occupancy, x); }
    template <       > BitBoard coverage<P_wr>(const Square x, const BitBoard occupancy, const Color c) { return fileAttack(occupancy, x) + rankAttack(occupancy, x); }
    template <       > BitBoard coverage<P_wq>(const Square x, const BitBoard occupancy, const Color c) { return diagonalAttack(occupancy, x) + antidiagonalAttack(occupancy, x) + fileAttack(occupancy, x) + rankAttack(occupancy, x); }
    template <       > BitBoard coverage<P_wk>(const Square x, const BitBoard occupancy, const Color c) { return mask[x].king; }

    template < Piece pp >
    inline BitBoard attack(const Square x, const BitBoard target, const BitBoard occupancy = 0, const Color c = Co_White) { return coverage<pp>(x, occupancy, c) & target; }

    int popBit(BitBoard & b) {
        unsigned long i = 0;
        bsf(b, i);
        b &= b - 1;
        return i;
    }

    BitBoard isAttackedBB(const Position &p, const Square x) {
        if (p.c == Co_White) return attack<P_wb>(x, p.blackBishop | p.blackQueen, p.occupancy) | attack<P_wr>(x, p.blackRook | p.blackQueen, p.occupancy) | attack<P_wn>(x, p.blackKnight) | attack<P_wp>(x, p.blackPawn, p.occupancy, Co_White) | attack<P_wk>(x, p.blackKing);
        else                 return attack<P_wb>(x, p.whiteBishop | p.whiteQueen, p.occupancy) | attack<P_wr>(x, p.whiteRook | p.whiteQueen, p.occupancy) | attack<P_wn>(x, p.whiteKnight) | attack<P_wp>(x, p.whitePawn, p.occupancy, Co_Black) | attack<P_wk>(x, p.whiteKing);
    }

    bool getAttackers(const Position & p, const Square k, std::vector<Square> & attakers) {
        attakers.clear();
        BitBoard attack = isAttackedBB(p, k);
        while (attack) { attakers.push_back(popBit(attack)); }
        return !attakers.empty();
    }
}

inline bool isAttacked(const Position & p, const Square k) { return BB::isAttackedBB(p, k) != 0ull;}

inline bool getAttackers(const Position & p, const Square k, std::vector<Square> & attakers) { return BB::getAttackers(p, k, attakers);}

void generate(const Position & p, std::vector<Move> & moves, bool onlyCap = false){ ///@todo use bitboards in a better way here !
   moves.clear();
   const Color side = p.c;
   const Color opponent = opponentColor(p.c);
   BitBoard myPieceBB  = (p.c == Co_White ? p.whitePiece : p.blackPiece);
   BitBoard oppPieceBB = (p.c != Co_White ? p.whitePiece : p.blackPiece);
   BitBoard myPieceBBiterator = myPieceBB;
   while (myPieceBBiterator) {
       const Square from = BB::popBit(myPieceBBiterator);
       const Piece piece = p.b[from];
       const Piece ptype = (Piece)std::abs(piece);
       static BitBoard(*const pf[])(const Square, const BitBoard, const  Color) = { &BB::coverage<P_wp>, &BB::coverage<P_wn>, &BB::coverage<P_wb>, &BB::coverage<P_wr>, &BB::coverage<P_wq>, &BB::coverage<P_wk> };
       if (ptype != P_wp) {
         BitBoard bb = pf[ptype-1](from, p.occupancy, p.c) & ~myPieceBB;
         if (onlyCap) bb &= oppPieceBB;
         while (bb) {
           const Square to = BB::popBit(bb);
           const bool isCap = (p.occupancy&SquareToBitboard(to)) != 0ull;
           if (isCap) addMove(from,to,T_capture,moves);
           else addMove(from,to,T_std,moves);
         }
         if ( ptype == P_wk && !onlyCap ){ // castling
           if ( side == Co_White) {
              bool e1Attacked = isAttacked(p,Sq_e1);
              if ( (p.castling & C_wqs) && p.b[Sq_b1] == P_none && p.b[Sq_c1] == P_none && p.b[Sq_d1] == P_none && !isAttacked(p,Sq_c1) && !isAttacked(p,Sq_d1) && !e1Attacked) addMove(from, Sq_c1, T_wqs, moves); // wqs
              if ( (p.castling & C_wks) && p.b[Sq_f1] == P_none && p.b[Sq_g1] == P_none && !e1Attacked && !isAttacked(p,Sq_f1) && !isAttacked(p,Sq_g1)) addMove(from, Sq_g1, T_wks, moves); // wks
           }
           else{
              const bool e8Attacked = isAttacked(p,Sq_e8);
              if ( (p.castling & C_bqs) && p.b[Sq_b8] == P_none && p.b[Sq_c8] == P_none && p.b[Sq_d8] == P_none && !isAttacked(p,Sq_c8) && !isAttacked(p,Sq_d8) && !e8Attacked) addMove(from, Sq_c8, T_bqs, moves); // bqs
              if ( (p.castling & C_bks) && p.b[Sq_f8] == P_none && p.b[Sq_g8] == P_none && !e8Attacked && !isAttacked(p,Sq_f8) && !isAttacked(p,Sq_g8)) addMove(from, Sq_g8, T_bks, moves); // bks
           }
         }
       }
       else {
          ///@todo use bitboard !!!
          //case P_wp: bb = (BB::coverage<P_wp>(from, p.occupancy, p.c) + BB::mask[from].push[p.c]) & ~p.whitePiece; break;
          const int pawnOffsetTrick = side==Co_White?4:0;
          for (Square j = 0; j < 4; ++j) {
             const int offset = Offset[ptype-1][j+pawnOffsetTrick];
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

inline void movePiece(Position & p, Square from, Square to, Piece fromP, Piece toP, bool isCapture = false, Piece prom = P_none) {
    const int fromId   = fromP + PieceShift;
    const int toId     = toP + PieceShift;
    const Piece toPnew = prom != P_none ? prom : fromP;
    const int toIdnew  = prom != P_none ? (prom + PieceShift) : fromId;
    p.b[from] = P_none;
    p.b[to]   = toPnew;
    unSetBit(p, from, fromP);
    unSetBit(p, to,   toP); // usefull only if move is a capture
    setBit  (p, to,   toPnew);

    p.h ^= ZT[from][fromId]; // remove fromP at from
    if (isCapture){
        p.h ^= ZT[to][toId]; // if capture remove toP at to
    }
    p.h ^= ZT[to][toIdnew]; // add fromP (or prom) at to
}

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
      movePiece(p, from, to, fromP, toP, type == T_capture);

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

      case T_ep: {
          const Square epCapSq = p.ep + (p.c == Co_White ? -8 : +8);
          unSetBit(p, epCapSq); // BEFORE setting p.b new shape !!!
          unSetBit(p, from);
          setBit(p, to, fromP);
          p.b[from] = P_none;
          p.b[to] = fromP;
          p.b[epCapSq] = P_none;
          p.h ^= ZT[from][fromId]; // remove fromP at from
          p.h ^= ZT[p.ep + epCapSq][(p.c == Co_White ? P_bp : P_wp) + PieceShift];
          p.h ^= ZT[to][fromId]; // add fromP at to
      }
      break;

      case T_promq:
      case T_cappromq: {
          const Piece promP = (p.c == Co_White ? P_wq : P_bq);
          movePiece(p, from, to, fromP, toP, type == T_cappromq,promP);
      }
      break;

      case T_promr:
      case T_cappromr: {
          const Piece promP = (p.c == Co_White ? P_wr : P_br);
          movePiece(p, from, to, fromP, toP, type == T_cappromq, promP);
      }
      break;

      case T_promb:
      case T_cappromb: {
          const Piece promP = (p.c == Co_White ? P_wb : P_bb);
          movePiece(p, from, to, fromP, toP, type == T_cappromq, promP);
      }
      break;

      case T_promn:
      case T_cappromn: {
          const Piece promP = (p.c == Co_White ? P_wn : P_bn);
          movePiece(p, from, to, fromP, toP, type == T_cappromq, promP);
      }
      break;

      case T_wks:
          movePiece(p, Sq_e1, Sq_g1, P_wk, P_none);
          movePiece(p, Sq_h1, Sq_f1, P_wr, P_none);
          p.wk = Sq_g1;
          if (p.castling & C_wqs) p.h ^= ZT[0][13];
          if (p.castling & C_wks) p.h ^= ZT[7][13];
          p.castling &= ~(C_wks | C_wqs);
      break;

      case T_wqs:
          movePiece(p, Sq_e1, Sq_c1, P_wk, P_none);
          movePiece(p, Sq_a1, Sq_d1, P_wr, P_none);
          p.wk = Sq_c1;
          if (p.castling & C_wqs) p.h ^= ZT[0][13];
          if (p.castling & C_wks) p.h ^= ZT[7][13];
          p.castling &= ~(C_wks | C_wqs);
      break;

      case T_bks:
          movePiece(p, Sq_e8, Sq_g8, P_bk, P_none);
          movePiece(p, Sq_h8, Sq_f8, P_br, P_none);
          p.b[Sq_e8] = P_none; p.b[Sq_f8] = P_br; p.b[Sq_g8] = P_bk; p.b[Sq_h8] = P_none;
          p.bk = Sq_g8;
          if (p.castling & C_bqs) p.h ^= ZT[56][13];
          if (p.castling & C_bks) p.h ^= ZT[63][13];
          p.castling &= ~(C_bks | C_bqs);
      break;

      case T_bqs:
          movePiece(p, Sq_e8, Sq_c8, P_bk, P_none);
          movePiece(p, Sq_a8, Sq_d8, P_br, P_none);
          p.bk = Sq_c8;
          if (p.castling & C_bqs) p.h ^= ZT[56][13];
          if (p.castling & C_bks) p.h ^= ZT[63][13];
          p.castling &= ~(C_bks | C_bqs);
      break;
   }

   p.whitePiece = p.whitePawn | p.whiteKnight | p.whiteBishop | p.whiteRook | p.whiteQueen | p.whiteKing;
   p.blackPiece = p.blackPawn | p.blackKnight | p.blackBishop | p.blackRook | p.blackQueen | p.blackKing;
   p.occupancy = p.whitePiece | p.blackPiece;

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
   ++p.ply;

   p.h ^= ZT[3][13]; p.h ^= ZT[4][13];

   p.lastMove = m;

   initMaterial(p);

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

    template<typename Iter>
    Iter select_randomly(Iter start, Iter end) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, (int)std::distance(start, end) - 1);
        std::advance(start, dis(gen));
        return start;
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
    const Square from       = Move2From(m);
    const Square to         = Move2To(m);
    Piece nextVictim  = p.b[from];
    const Color us          = getColor(p,from);
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
            const Move mm = ToMove(stmAttackers[threatId], to, T_capture); ///@todo prom ????
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

   MoveSorter(const Position & p, const TT::Entry * e = NULL):p(p),e(e){ assert(e==0||e->h!=0||e->m==INVALIDMOVE); }

   void computeScore(Move & m)const{
      const MType  t    = Move2Type(m);
      const Square from = Move2From(m);
      const Square to   = Move2To(m);
      int s = MoveScoring[t];
      if (e && sameMove(e->m,m)) s += 3000;
      else if (sameMove(m,KillerT::killers[0][p.ply])) s += 290;
      else if (sameMove(m,KillerT::killers[1][p.ply])) s += 260;
      if (isCapture(t)){
          s += MvvLvaScores[getPieceType(p,to)-1][getPieceType(p,from)-1];
          if ( !SEE(p,m,0)) s -= 2000;
      }
      else if ( t == T_std){
          s += HistoryT::history[getPieceIndex(p,from)][to];
          const bool isWhite = (p.whitePiece & (SquareToBitboard(from))) != 0ull;
          s += PST[getPieceType(p, from) - 1][isWhite ? (to ^ 56) : to] - PST[getPieceType(p, from) - 1][isWhite ? (from ^ 56) : from];
      }
      m = ToMove(from, to, t, s);
   }

   bool operator()(const Move & a, const Move & b)const{
      assert( a != INVALIDMOVE); assert( b != INVALIDMOVE);
      return Move2Score(a) > Move2Score(b);
   }
   const Position & p;
   const TT::Entry * e;
};

void sort(std::vector<Move> & moves, const Position & p, const TT::Entry * e = NULL){
   const MoveSorter ms(p,e);
   for(auto it = moves.begin() ; it != moves.end() ; ++it){ ms.computeScore(*it); }
   std::sort(moves.begin(),moves.end(),ms);
}

bool isDraw(const Position & p, bool isPV = true){
   int count = 0;
   const Hash h = computeHash(p);
   const int limit = isPV?3:1;
   for (int k = p.ply-1; k >= 0; --k) {
       if (hashStack[k] == 0) break;
       if (hashStack[k] == h) ++count;
       if (count >= limit) return true;
   }
   if ( p.fifty >= 100 ) return true;

   if (p.np == 0 ) {
       if (p.nwt + p.nbt == 0) return true;
       else if (p.nwm == 1 && p.nwM == 0 && p.nbt == 0) return true;
       else if (p.nbm == 1 && p.nbM == 0 && p.nwt == 0) return true;
       else if (p.nwn == 2 && p.nwb == 0 && p.nwM == 0 && p.nbt == 0) return true;
       else if (p.nbn == 2 && p.nbb == 0 && p.nbM == 0 && p.nwt == 0) return true;
       else if (p.nwt == 1 && p.nwm == 1 && p.nbt == 1 && p.nbm == 1) return true;
       else if (p.nwM == 0 && p.nwm == 2 && p.nwb != 2 && p.nbM == 0 && p.nbm == 1) return true;
       else if (p.nbM == 0 && p.nbm == 2 && p.nbb != 2 && p.nwM == 0 && p.nwm == 1) return true;
       ///@todo others ...
   }
   else { // some pawn are present
          ///@todo ... KPK
   }

   return false;
}

int manhattanDistance(Square sq1, Square sq2) {
    Square file1 = sq1 & 7; Square file2 = sq2 & 7;
    Square rank1 = sq1 >> 3; Square rank2 = sq2 >> 3;
    return std::abs(rank2 - rank1) + std::abs(file2 - file1);
}

ScoreType eval(const Position & p, float & gp){
   static const int absValues[7]   = { 0, Values[P_wp + PieceShift], Values[P_wn + PieceShift], Values[P_wb + PieceShift], Values[P_wr + PieceShift], Values[P_wq + PieceShift], Values[P_wk + PieceShift] };
   static const int absValuesEG[7] = { 0, ValuesEG[P_wp + PieceShift], ValuesEG[P_wn + PieceShift], ValuesEG[P_wb + PieceShift], ValuesEG[P_wr + PieceShift], ValuesEG[P_wq + PieceShift], ValuesEG[P_wk + PieceShift] };
   int absscore = (p.nwk+p.nbk) * absValues[P_wk] + (p.nwq+p.nbq) * absValues[P_wq] + (p.nwr+p.nbr) * absValues[P_wr] + (p.nwb+p.nbb) * absValues[P_wb] + (p.nwn+p.nbn) * absValues[P_wn] + (p.nwp+p.nbp) * absValues[P_wp];
   absscore = 100 * (absscore - 16000) / (24140 - 16000);
   int pawnScore = 100 * p.np / 16;
   int pieceScore = 100 * (p.nq + p.nr + p.nb + p.nn) / 14;
   gp = (absscore*0.4f + pieceScore*0.3f + pawnScore*0.3f)/100.f;
   ScoreType sc = (p.nwk - p.nbk) * ScoreType(gp*absValues[P_wk] + (1.f - gp)*absValuesEG[P_wk])
                + (p.nwq - p.nbq) * ScoreType(gp*absValues[P_wq] + (1.f - gp)*absValuesEG[P_wq])
                + (p.nwr - p.nbr) * ScoreType(gp*absValues[P_wr] + (1.f - gp)*absValuesEG[P_wr])
                + (p.nwb - p.nbb) * ScoreType(gp*absValues[P_wb] + (1.f - gp)*absValuesEG[P_wb])
                + (p.nwn - p.nbn) * ScoreType(gp*absValues[P_wn] + (1.f - gp)*absValuesEG[P_wn])
                + (p.nwp - p.nbp) * ScoreType(gp*absValues[P_wp] + (1.f - gp)*absValuesEG[P_wp]);
   const bool white2Play = p.c == Co_White;
   BitBoard pieceBBiterator = p.whitePiece;
   while (pieceBBiterator) {
      const Square k = BB::popBit(pieceBBiterator);
      Square kk = k^56;
      const Piece ptype = getPieceType(p,k);
      sc += ScoreType((gp*PST[ptype - 1][kk] + (1.f - gp)*PSTEG[ptype - 1][kk] ) );
   }
   pieceBBiterator = p.blackPiece;
   while (pieceBBiterator) {
       const Square k = BB::popBit(pieceBBiterator);
       const Piece ptype = getPieceType(p, k);
       sc -= ScoreType((gp*PST[ptype - 1][k] + (1.f - gp)*PSTEG[ptype - 1][k]));
   }
   // in very end game winning king must be near the other king
   if (gp < 0.2) sc -= (sc>0?+1:-1)*manhattanDistance(p.wk, p.bk)*15;
   return (white2Play?+1:-1)*sc;
}

ScoreType qsearch(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth){
  float gp = 0;
  if ( ply >= MAX_PLY-1 ) return eval(p,gp);
  alpha = std::max(alpha, (ScoreType)(-MATE + ply));
  beta  = std::min(beta , (ScoreType)(MATE - ply + 1));
  if (alpha >= beta) return alpha;

  if ((int)ply > seldepth) seldepth = ply;

  ++stats.qnodes;

  ScoreType val = eval(p,gp);
  if ( val >= beta ) return val;
  if ( val > alpha) alpha = val;

  //const bool isInCheck = isAttacked(p, kingSquare(p));

  std::vector<Move> moves;
  generate(p,moves,true);
  sort(moves,p);

  ScoreType alphaInit = alpha;

  for(auto it = moves.begin() ; it != moves.end() ; ++it){
     if ( Move2Score(*it) < -900) continue; // see
     if ( doQFutility /*&& !isInCheck*/ && val + qfutilityMargin + std::abs(getValue(p,Move2To(*it))) <= alphaInit) continue;
     Position p2 = p;
     if ( ! apply(p2,*it) ) continue;
     if (p.c == Co_White && Move2To(*it) == p.bk) return MATE - ply + 1;
     if (p.c == Co_Black && Move2To(*it) == p.wk) return MATE - ply + 1;
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

inline void updateHistoryKillers(const Position & p, DepthType depth, const Move m) {
    KillerT::killers[1][p.ply] = KillerT::killers[0][p.ply];
    KillerT::killers[0][p.ply] = m;
    HistoryT::update(depth, m, p, true);
}

ScoreType pvs(ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, bool pvnode, unsigned int ply, std::vector<Move> & pv, DepthType & seldepth, const Move skipMove = INVALIDMOVE); //forward decl

inline bool singularExtension(ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, const TT::Entry & e, const Move m, bool rootnode, int ply) {
    if (depth >= singularExtensionDepth && sameMove(m, e.m) && !rootnode && std::abs(e.score) < MATE - MAX_DEPTH && e.b == TT::B_beta && e.d >= depth - 3) {
        const ScoreType betaC = e.score - depth;
        std::vector<Move> sePV;
        DepthType seSeldetph;
        ScoreType score = pvs(betaC - 1, betaC, p, depth/2, false, ply, sePV, seSeldetph, m);
        if (!stopFlag && score < betaC) return true;
    }
    return false;
}

ScoreType pvs(ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, bool pvnode, unsigned int ply, std::vector<Move> & pv, DepthType & seldepth, const Move skipMove){
  if ( std::max(1,(int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count()) > currentMoveMs ){
    stopFlag = true;
    return STOPSCORE;
  }

  if ((int)ply > seldepth) seldepth = ply;

  if ( depth <= 0 ) return qsearch(alpha,beta,p,ply,seldepth);

  ++stats.nodes;

  const bool rootnode = ply == 1;

  float gp = 0;
  if (isDraw(p, pvnode)) {
      if (!rootnode) return 0;
      std::cout << "Draw at root node" << std::endl;
      std::vector<Move> moves;
      generate(p, moves);
      const bool isInCheck = isAttacked(p, kingSquare(p));
      if (moves.empty()) return isInCheck ? -MATE + ply : 0;
      sort(moves, p, NULL);
      Position p2 = p;
      auto it = moves.begin();
      while (!apply(p2, *it)) { ++it; if ( it == moves.end() ) break; }
      if ( it != moves.end()) pv.push_back(*it); // return first valid move, all are leading to a draw
      else std::cout << "No valid moves for draw" << std::endl;
      return 0;
  }
  if (ply >= MAX_PLY - 1 || depth >= MAX_DEPTH - 1) return eval(p, gp);

  alpha = std::max(alpha, (ScoreType)(-MATE + ply));
  beta  = std::min(beta,  (ScoreType)(MATE  - ply + 1));
  if (alpha >= beta) return alpha;

  TT::Entry e;
  if (skipMove==INVALIDMOVE && TT::getEntry(computeHash(p), depth, e)) { // if not skipmove
      if (e.h != 0 && !rootnode && std::abs(e.score) < MATE - MAX_DEPTH && !pvnode &&
          ( (e.b == TT::B_alpha && e.score <= alpha) || (e.b == TT::B_beta  && e.score >= beta) || (e.b == TT::B_exact) ) ) {
          pv.push_back(e.m);
          return e.score;
      }
  }

  const bool isInCheck = isAttacked(p, kingSquare(p));
  ScoreType val;
  if (isInCheck) val = -MATE + ply;
  else if (!TT::getEvalEntry(computeHash(p), val, gp)) {
      val = eval(p, gp);
      TT::setEvalEntry({ val, gp, computeHash(p) });
  }
  scoreStack[p.ply] = val; ///@todo can e.score be used as val here ???

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
       const ScoreType nullscore = -pvs(-beta,-beta+1,pN,depth-R,false,ply+1,nullPV,seldepth);
       if ( !stopFlag && nullscore >= beta ) return nullscore;
       if ( stopFlag ) return STOPSCORE;
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
     if ( !stopFlag) TT::getEntry(computeHash(p), depth, e);
     if (stopFlag) return STOPSCORE;
  }

  int validMoveCount = 0;
  bool alphaUpdated = false;
  Move bestMove = INVALIDMOVE;

  // try the tt move first (if not skipped move)
  if (e.h != 0 && !sameMove(e.m,skipMove)) { // should be the case thanks to iid at pvnode
      Position p2 = p;
      if (apply(p2, e.m)) {
          validMoveCount++;
          std::vector<Move> childPV;
          hashStack[p.ply] = p.h;
          // extensions
          int extension = 0;
          if (isInCheck) ++extension;
          if (singularExtension(alpha, beta, p, depth, e, e.m, rootnode, ply)) ++extension;
          ScoreType ttval = -pvs(-beta, -alpha, p2, depth - 1, pvnode, ply + 1, childPV, seldepth);
          if (!stopFlag && ttval > alpha) {
              alphaUpdated = true;
              updatePV(pv, e.m, childPV);
              if (ttval >= beta) {
                  //if (Move2Type(e.m) == T_std && !isInCheck) updateHistoryKillers(p, depth, e.m);
                  if ( skipMove==INVALIDMOVE) TT::setEntry({ e.m,ttval,TT::B_beta,depth,computeHash(p) });
                  return ttval;
              }
              alpha = ttval;
              bestMove = e.m;
          }
          if (stopFlag) return STOPSCORE;
      }
  }

  std::vector<Move> moves;
  generate(p,moves);
  if ( moves.empty() ) return isInCheck?-MATE + ply : 0;
  sort(moves,p,&e);

  if (bestMove == INVALIDMOVE)  bestMove = moves[0]; // so that B_alpha are stored in TT

  bool improving = (!isInCheck && ply >= 2 && val >= scoreStack[p.ply - 2]);

  for(auto it = moves.begin() ; it != moves.end() && !stopFlag ; ++it){
     if (sameMove(skipMove, *it)) continue; // skipmove
     if ( e.h != 0 && sameMove(e.m, *it)) continue; // already tried
     Position p2 = p;
     if ( ! apply(p2,*it) ) continue;
     const Square to = Move2To(*it);
     if (p.c == Co_White && to == p.bk) return MATE - ply + 1;
     if (p.c == Co_Black && to == p.wk) return MATE - ply + 1;
     validMoveCount++;
     hashStack[p.ply] = p.h;
     std::vector<Move> childPV;
     // extensions
     int extension = 0;
     if (isInCheck && Move2Score(*it) > -100) ++extension;
     if ( validMoveCount == 1 || !doPVS) val = -pvs(-beta,-alpha,p2,depth-1+extension,pvnode,ply+1,childPV,seldepth);
     else{
        // reductions & prunings
        int reduction = 0;
        const bool isCheck = isAttacked(p2, kingSquare(p2));
        const bool isAdvancedPawnPush = getPieceType(p,Move2From(*it)) == P_wp && (SQRANK(to) > 5 || SQRANK(to) < 2);
        const bool isPrunable = !isInCheck && !isCheck && !isAdvancedPawnPush && Move2Type(*it) == T_std && !sameMove(*it, KillerT::killers[0][p.ply]) && !sameMove(*it, KillerT::killers[1][p.ply]);
        // futility
        if ( futility && isPrunable) continue;
        // LMP
        if ( lmp && isPrunable && validMoveCount >= lmpLimit[improving][depth] ) continue;
        // SEE
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
              updateHistoryKillers(p, depth, *it);
              for(auto it2 = moves.begin() ; *it2!=*it; ++it2){
                  if ( Move2Type(*it2) == T_std ) HistoryT::update(depth,*it2,p,false);
              }
           }
           if (skipMove == INVALIDMOVE) TT::setEntry({*it,val,TT::B_beta,depth,computeHash(p)});
           return val;
        }
        alpha = val;
        bestMove = *it;
     }
  }

  if ( validMoveCount == 0 ) return isInCheck?-MATE + ply : 0;
  if ( bestMove != INVALIDMOVE && skipMove == INVALIDMOVE && !stopFlag) TT::setEntry({bestMove,alpha,alphaUpdated?TT::B_exact:TT::B_alpha,depth,computeHash(p)});

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

  hashStack[p.ply] = p.h;

  Counter previousNodeCount = 1;

  const Move bookMove = Book::Get(computeHash(p));
  if ( bookMove != INVALIDMOVE){
      pv .push_back(bookMove);
      m = pv[0];
      d = reachedDepth;
      sc = bestScore;
      return pv;
  }

  for(DepthType depth = 1 ; depth <= std::min(d,DepthType(MAX_DEPTH-6)) && !stopFlag ; ++depth ){ // -6 so that draw can be found for sure
    std::cout << "# Iterative deepening " << (int)depth << std::endl;
    std::vector<Move> pvLoc;
    ScoreType delta = (doWindow && depth>4)?25:MATE; // MATE not INFSCORE in order to enter the loop below once
    ScoreType alpha = std::max(ScoreType(bestScore - delta), ScoreType (-INFSCORE));
    ScoreType beta  = std::min(ScoreType(bestScore + delta), INFSCORE);
    ScoreType score = 0;
    while( delta <= MATE ){
       pvLoc.clear();
       score = pvs(alpha,beta,p,depth,true,1,pvLoc,seldepth);
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

    if (bestScore <= -MATE+1) break; // already mated
    if (mateFinder && bestScore >= MATE - MAX_DEPTH) break; // forced mate found
    if (depth >= MAX_DEPTH - 2 && bestScore >=  MATE - MAX_DEPTH) break; // mate found
    if (depth >= MAX_DEPTH - 2 && bestScore <= -MATE + MAX_DEPTH) break; // mated
  }

  if (bestScore <= -MATE+1) {
      std::cout << "# Mated ..." << std::endl;
      pv.clear();
      pv.push_back(INVALIDMOVE);
  }

  if (bestScore >= MATE-MAX_DEPTH) std::cout << "# Forced mate found ..." << std::endl;

  if (pv.empty()){
      std::cout << "# Empty pv" << std::endl;
      m = INVALIDMOVE;
  }
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
      std::cout << "# Init xboard" << std::endl;
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
   BB::initMask();

   std::string cli = argv[1];

   if ( Book::fileExists("book.bin") ) {
       std::ifstream bbook("book.bin",std::ios::in | std::ios::binary);
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

   if (cli == "-attacked") {
       Square k = Sq_e4;
       if (argc >= 3) k = atoi(argv[3]);
       std::cout << showBitBoard(BB::isAttackedBB(p, k));
       return 0;
   }

   if (cli == "-cov") {
       Square k = Sq_e4;
       if (argc >= 3) k = atoi(argv[3]);
       switch (p.b[k]) {
       case P_wp:
           std::cout << showBitBoard((BB::coverage<P_wp>(k, p.occupancy, p.c) + BB::mask[k].push[p.c]) & ~p.whitePiece);
           break;
       case P_wn:
           std::cout << showBitBoard(BB::coverage<P_wn>(k, p.occupancy, p.c) & ~p.whitePiece);
           break;
       case P_wb:
           std::cout << showBitBoard(BB::coverage<P_wb>(k, p.occupancy, p.c) & ~p.whitePiece);
           break;
       case P_wr:
           std::cout << showBitBoard(BB::coverage<P_wr>(k, p.occupancy, p.c) & ~p.whitePiece);
           break;
       case P_wq:
           std::cout << showBitBoard(BB::coverage<P_wq>(k, p.occupancy, p.c) & ~p.whitePiece);
           break;
       case P_wk:
           std::cout << showBitBoard(BB::coverage<P_wk>(k, p.occupancy, p.c) & ~p.whitePiece);
           break;
       case P_bk:
           std::cout << showBitBoard(BB::coverage<P_wk>(k, p.occupancy, p.c) & ~p.blackPiece);
           break;
       case P_bq:
           std::cout << showBitBoard(BB::coverage<P_wq>(k, p.occupancy, p.c )& ~p.blackPiece);
           break;
       case P_br:
           std::cout << showBitBoard(BB::coverage<P_wr>(k, p.occupancy, p.c) & ~p.blackPiece);
           break;
       case P_bb:
           std::cout << showBitBoard(BB::coverage<P_wb>(k, p.occupancy, p.c) & ~p.blackPiece);
           break;
       case P_bn:
           std::cout << showBitBoard(BB::coverage<P_wn>(k, p.occupancy, p.c) & ~p.blackPiece);
           break;
       case P_bp:
           std::cout << showBitBoard((BB::coverage<P_wp>(k, p.occupancy, p.c) + BB::mask[k].push[p.c])& ~p.blackPiece);
           break;
       default:
           std::cout << showBitBoard(0ull);
       }
       return 0;
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
