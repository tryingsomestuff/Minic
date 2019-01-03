#include <algorithm>
#include <cassert>
#include <bitset>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <iterator>
#include <vector>
#include <sstream>
#include <set>
#include <chrono>
#include <random>
#include <unordered_map>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <stdlib.h>
#include <intrin.h>
typedef uint64_t u_int64_t;
#else
#include <unistd.h>
#endif

#include "json.hpp"

//#define IMPORTBOOK
//#define WITH_TEXEL_TUNING
//#define DEBUG_TOOL
#define WITH_TEST_SUITE

const std::string MinicVersion = "mat";

typedef std::chrono::system_clock Clock;
typedef char DepthType;
typedef int Move;    // invalid if < 0
typedef char Square; // invalid if < 0
typedef uint64_t Hash; // invalid if == 0
typedef uint64_t Counter;
typedef short int ScoreType;
typedef uint64_t BitBoard;

#define STOPSCORE   ScoreType(20000)
#define INFSCORE    ScoreType(15000)
#define MATE        ScoreType(10000)
#define INVALIDMOVE    -1
#define INVALIDSQUARE  -1
#define MAX_PLY       512
#define MAX_MOVE      512
#define MAX_DEPTH     64

#define SQFILE(s) (s%8)
#define SQRANK(s) (s/8)

namespace StaticConfig{
const bool doWindow         = true;
const bool doPVS            = true;
const bool doNullMove       = true;
const bool doFutility       = true;
const bool doLMR            = true;
const bool doLMP            = true;
const bool doStaticNullMove = true;
const bool doRazoring       = true;
const bool doQFutility      = true;
const bool doProbcut        = false; ///@todo

const ScoreType qfutilityMargin          = 128;
const int       staticNullMoveMaxDepth   = 6;
const ScoreType staticNullMoveDepthCoeff = 80;
const ScoreType razoringMargin           = 200;
const int       razoringMaxDepth         = 3;
const int       nullMoveMinDepth         = 2;
const int       lmpMaxDepth              = 10;
const ScoreType futilityDepthCoeff       = 160;
const int       iidMinDepth              = 5;
const int       probCutMinDepth          = 5;
const int       probCutMaxMoves          = 3;
const ScoreType probCutMargin            = 80;
const int       lmrMinDepth              = 3;
const int       singularExtensionDepth   = 8;
}

namespace DynamicConfig{
bool mateFinder = false;
bool disableTT = false;
unsigned int ttSizeMb  = 512; // here in Mb, will be converted to real size next
unsigned int ttESizeMb = 512; // here in Mb, will be converted to real size next
}

const int lmpLimit[][StaticConfig::lmpMaxDepth + 1] = { { 0, 3, 4, 6, 10, 15, 21, 28, 36, 45, 55 } , { 0, 5, 6, 9, 15, 23, 32, 42, 54, 68, 83 } };

int lmrReduction[MAX_DEPTH][MAX_MOVE];

std::string startPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
std::string fine70        = "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - -";
std::string shirov        = "6r1/2rp1kpp/2qQp3/p3Pp1P/1pP2P2/1P2KP2/P5R1/6R1 w - - 0 1";

int currentMoveMs = 777; // a dummy initial value, useful for debug

inline void hellooo(){ std::cout << "# This is Minic version " << MinicVersion << std::endl; }

inline std::string showDate() {
    std::stringstream str;
    auto now = Clock::now();
    auto msecEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    char buffer[64];
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::time_point(msecEpoch));
    std::strftime(buffer,63,"%Y-%m-%d %H:%M:%S",localtime(&tt));
    str << buffer << "-";
    str << std::setw(3) << std::setfill('0') << msecEpoch.count()%1000;
    return str.str();
}

enum LogLevel : unsigned char{ logTrace = 0, logDebug = 1, logInfo  = 2, logWarn  = 3, logError = 4, logFatal = 5, logGUI   = 6};

std::string backtrace(){return "@todo:: backtrace";} ///@todo find a very simple portable implementation

Square stringToSquare(const std::string & str){ return (str.at(1) - 49) * 8 + (str.at(0) - 97); }

class LogIt{
public:
    LogIt(LogLevel loglevel):_level(loglevel){}

    template <typename T> LogIt & operator<<(T const & value) {
        _buffer << value;
        return *this;
    }

    ~LogIt(){
        static const std::string _levelNames[7] = { "# Trace ", "# Debug ", "# Info  ", "# Warn  ", "# Error ", "# Fatal ", "" };
        std::lock_guard<std::mutex> lock(_mutex);
        if ( _level != logGUI) std::cout << _levelNames[_level] << showDate() << ": " << _buffer.str() << std::endl;
        else std::cout << _buffer.str()  << std::flush << std::endl;
        if (_level == logFatal){ std::cout << backtrace() << std::endl; exit(1);}
        else if (_level == logError) std::cout << backtrace() << std::endl;
    }

private:
    static std::mutex     _mutex;
    std::ostringstream    _buffer;
    LogLevel              _level;
};

std::mutex LogIt::_mutex;

void initLMR(){
    LogIt(logInfo) << "Init lmr" ;
    for (int d = 0; d < MAX_DEPTH; d++)
        for (int m = 0; m < MAX_MOVE; m++)
            lmrReduction[d][m] = (int)sqrt(float(d) * m / 8.f);
}

struct Stats{
    std::atomic<Counter> nodes, qnodes, tthits;
    void init(){
        static std::mutex statsMutex;
        std::lock_guard<std::mutex> lock(statsMutex);
        LogIt(logInfo) << "Init stat" ;
        nodes = qnodes = tthits = 0;
    }
};

Stats stats;

enum Piece : char{ P_bk = -6, P_bq = -5, P_br = -4, P_bb = -3, P_bn = -2, P_bp = -1, P_none = 0, P_wp = 1, P_wn = 2, P_wb = 3, P_wr = 4, P_wq = 5, P_wk = 6 };
const int PieceShift = 6;

ScoreType   Values[13]    = { -8000, -1025, -477, -365, -337, -82, 0, 82, 337, 365, 477, 1025, 8000 };
ScoreType   ValuesEG[13]  = { -8000,  -936, -512, -297, -281, -94, 0, 94, 281, 297, 512,  936, 8000 };
std::string Names[13]     = { "k", "q", "r", "b", "n", "p", " ", "P", "N", "B", "R", "Q", "K" };

ScoreType   passerBonus[8]        = { 0,  0,  3,  8, 15, 24, 34, 0};
ScoreType   passerBonusEG[8]      = { 0,  4, 18, 42, 75,118,170, 0};
ScoreType   kingNearPassedPawnEG  = 11;
ScoreType   doublePawnMalus       = 5;
ScoreType   doublePawnMalusEG     = 15;
ScoreType   isolatedPawnMalus     = -5; // yes negative ...
ScoreType   isolatedPawnMalusEG   = 15;
float   protectedPasserFactor     = 0.25;
float   freePasserFactor          = 0.9;

ScoreType   adjKnight[9]  = { -24, -18, -12, -6,  0,  6,  12, 18, 24 };
ScoreType   adjRook[9]    = {  48,  36,  24, 12,  0,-12, -24,-36,-48 };

ScoreType   bishopPairBonus =  20;
ScoreType   knightPairMalus = -8;
ScoreType   rookPairMalus   = -16;

ScoreType   blockedBishopByPawn  = -24;
ScoreType   blockedKnight        = -150;
ScoreType   blockedKnight2       = -100;
ScoreType   blockedBishop        = -150;
ScoreType   blockedBishop2       = -100;
ScoreType   blockedBishop3       = -50;
ScoreType   returningBishopBonus =  16;
ScoreType   blockedRookByKing    = -22;

std::string Squares[64] = { "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3", "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6", "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8" };
std::string Files[8] = { "a", "b", "c", "d", "e", "f", "g", "h" };
std::string Ranks[8] = { "1", "2", "3", "4", "5", "6", "7", "8" };
enum Sq : char { Sq_a1 =  0,Sq_b1,Sq_c1,Sq_d1,Sq_e1,Sq_f1,Sq_g1,Sq_h1,Sq_a2,Sq_b2,Sq_c2,Sq_d2,Sq_e2,Sq_f2,Sq_g2,Sq_h2,Sq_a3,Sq_b3,Sq_c3,Sq_d3,Sq_e3,Sq_f3,Sq_g3,Sq_h3,Sq_a4,Sq_b4,Sq_c4,Sq_d4,Sq_e4,Sq_f4,Sq_g4,Sq_h4,Sq_a5,Sq_b5,Sq_c5,Sq_d5,Sq_e5,Sq_f5,Sq_g5,Sq_h5,Sq_a6,Sq_b6,Sq_c6,Sq_d6,Sq_e6,Sq_f6,Sq_g6,Sq_h6,Sq_a7,Sq_b7,Sq_c7,Sq_d7,Sq_e7,Sq_f7,Sq_g7,Sq_h7,Sq_a8,Sq_b8,Sq_c8,Sq_d8,Sq_e8,Sq_f8,Sq_g8,Sq_h8};

enum Castling : char{ C_none= 0, C_wks = 1, C_wqs = 2, C_bks = 4, C_bqs = 8 };

int MvvLvaScores[6][6];
void initMvvLva(){
    LogIt(logInfo) << "Init mvv-lva" ;
    static ScoreType IValues[6] = { 1, 2, 3, 5, 9, 20 }; ///@todo try N=B=3 !
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
inline int BitScanForward(BitBoard bb) { assert(bb != 0); return __builtin_ctzll(bb);}
#define bsf(x,i)      (i=BitScanForward(x))
#define swapbits(x)   (__builtin_bswap64 (x))
#else
#ifdef _WIN32
#define POPCOUNT(x)   __popcnt64(x)
#define bsf(x,i)      _BitScanForward64(&i,x)
#define swapbits(x)   (_byteswap_uint64 (x))
#else // linux
#define POPCOUNT(x)   int(__builtin_popcountll(x))
inline int BitScanForward(BitBoard bb) { assert(bb != 0ull); return __builtin_ctzll(bb);}
#define bsf(x,i)      (i=BitScanForward(x))
#define swapbits(x)   (__builtin_bswap64 (x))
#endif
#endif

#define SquareToBitboard(k) (1ull<<(k))
#define SquareToBitboardTable(k) BB::mask[k].bbsquare
inline uint64_t  countBit(const BitBoard & b)           { return POPCOUNT(b);}
inline void      setBit  (      BitBoard & b, Square k) { b |= SquareToBitboard(k);}
inline void      unSetBit(      BitBoard & b, Square k) { b &= ~SquareToBitboard(k);}
inline bool      isSet   (const BitBoard & b, Square k) { return (SquareToBitboard(k) & b) != 0;}

enum BBSq : BitBoard { BBSq_a1 = SquareToBitboard(Sq_a1),BBSq_b1 = SquareToBitboard(Sq_b1),BBSq_c1 = SquareToBitboard(Sq_c1),BBSq_d1 = SquareToBitboard(Sq_d1),BBSq_e1 = SquareToBitboard(Sq_e1),BBSq_f1 = SquareToBitboard(Sq_f1),BBSq_g1 = SquareToBitboard(Sq_g1),BBSq_h1 = SquareToBitboard(Sq_h1),
                       BBSq_a2 = SquareToBitboard(Sq_a2),BBSq_b2 = SquareToBitboard(Sq_b2),BBSq_c2 = SquareToBitboard(Sq_c2),BBSq_d2 = SquareToBitboard(Sq_d2),BBSq_e2 = SquareToBitboard(Sq_e2),BBSq_f2 = SquareToBitboard(Sq_f2),BBSq_g2 = SquareToBitboard(Sq_g2),BBSq_h2 = SquareToBitboard(Sq_h2),
                       BBSq_a3 = SquareToBitboard(Sq_a3),BBSq_b3 = SquareToBitboard(Sq_b3),BBSq_c3 = SquareToBitboard(Sq_c3),BBSq_d3 = SquareToBitboard(Sq_d3),BBSq_e3 = SquareToBitboard(Sq_e3),BBSq_f3 = SquareToBitboard(Sq_f3),BBSq_g3 = SquareToBitboard(Sq_g3),BBSq_h3 = SquareToBitboard(Sq_h3),
                       BBSq_a4 = SquareToBitboard(Sq_a4),BBSq_b4 = SquareToBitboard(Sq_b4),BBSq_c4 = SquareToBitboard(Sq_c4),BBSq_d4 = SquareToBitboard(Sq_d4),BBSq_e4 = SquareToBitboard(Sq_e4),BBSq_f4 = SquareToBitboard(Sq_f4),BBSq_g4 = SquareToBitboard(Sq_g4),BBSq_h4 = SquareToBitboard(Sq_h4),
                       BBSq_a5 = SquareToBitboard(Sq_a5),BBSq_b5 = SquareToBitboard(Sq_b5),BBSq_c5 = SquareToBitboard(Sq_c5),BBSq_d5 = SquareToBitboard(Sq_d5),BBSq_e5 = SquareToBitboard(Sq_e5),BBSq_f5 = SquareToBitboard(Sq_f5),BBSq_g5 = SquareToBitboard(Sq_g5),BBSq_h5 = SquareToBitboard(Sq_h5),
                       BBSq_a6 = SquareToBitboard(Sq_a6),BBSq_b6 = SquareToBitboard(Sq_b6),BBSq_c6 = SquareToBitboard(Sq_c6),BBSq_d6 = SquareToBitboard(Sq_d6),BBSq_e6 = SquareToBitboard(Sq_e6),BBSq_f6 = SquareToBitboard(Sq_f6),BBSq_g6 = SquareToBitboard(Sq_g6),BBSq_h6 = SquareToBitboard(Sq_h6),
                       BBSq_a7 = SquareToBitboard(Sq_a7),BBSq_b7 = SquareToBitboard(Sq_b7),BBSq_c7 = SquareToBitboard(Sq_c7),BBSq_d7 = SquareToBitboard(Sq_d7),BBSq_e7 = SquareToBitboard(Sq_e7),BBSq_f7 = SquareToBitboard(Sq_f7),BBSq_g7 = SquareToBitboard(Sq_g7),BBSq_h7 = SquareToBitboard(Sq_h7),
                       BBSq_a8 = SquareToBitboard(Sq_a8),BBSq_b8 = SquareToBitboard(Sq_b8),BBSq_c8 = SquareToBitboard(Sq_c8),BBSq_d8 = SquareToBitboard(Sq_d8),BBSq_e8 = SquareToBitboard(Sq_e8),BBSq_f8 = SquareToBitboard(Sq_f8),BBSq_g8 = SquareToBitboard(Sq_g8),BBSq_h8 = SquareToBitboard(Sq_h8)};

const BitBoard whiteSquare        = 0x55AA55AA55AA55AA;
const BitBoard blackSquare        = 0xAA55AA55AA55AA55;
const BitBoard whiteSideSquare    = 0x00000000FFFFFFFF;
const BitBoard blackSideSquare    = 0xFFFFFFFF00000000;
const BitBoard whiteKingQueenSide = 0x0000000000000007;
const BitBoard whiteKingKingSide  = 0x00000000000000E0;
const BitBoard blackKingQueenSide = 0x0700000000000000;
const BitBoard blackKingKingSide  = 0xe000000000000000;
const BitBoard fileA              = 0x0101010101010101;
const BitBoard fileB              = 0x0202020202020202;
const BitBoard fileC              = 0x0404040404040404;
const BitBoard fileD              = 0x0808080808080808;
const BitBoard fileE              = 0x1010101010101010;
const BitBoard fileF              = 0x2020202020202020;
const BitBoard fileG              = 0x4040404040404040;
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
    ss ;
    for (int j = 7; j >= 0; --j) {
        ss << "# +-+-+-+-+-+-+-+-+" << std::endl << "# |";
        for (int i = 0; i < 8; ++i) ss << (bs[i + j * 8] ? "X" : " ") << '|';
        ss << std::endl;
    }
    ss << "# +-+-+-+-+-+-+-+-+";
    return ss.str();
}

struct Position; // forward decl
bool readFEN(const std::string & fen, Position & p, bool silent = false); // forward decl

struct Position{
    Piece b[64] = {P_none};
    BitBoard whitePawn=0ull,whiteKnight=0ull,whiteBishop=0ull,whiteRook=0ull,whiteQueen=0ull,whiteKing=0ull;
    BitBoard blackPawn=0ull,blackKnight=0ull,blackBishop=0ull,blackRook=0ull,blackQueen=0ull,blackKing=0ull;
    BitBoard whitePiece=0ull, blackPiece=0ull, occupancy=0ull;

    Position(){}
    Position(const std::string & fen){readFEN(fen,*this);}

    unsigned char fifty = 0;
    unsigned char moves = 0;
    unsigned char ply; // this is not the "same" ply as the one used to get seldepth
    unsigned int castling = 0;
    Square ep = INVALIDSQUARE, wk = INVALIDSQUARE, bk = INVALIDSQUARE;
    Color c = Co_White;
    mutable Hash h = 0ull;
    Move lastMove = INVALIDMOVE;

    struct Material {
        unsigned char nwk = 0; unsigned char nwq = 0; unsigned char nwr = 0; unsigned char nwb = 0; unsigned char nwbl = 0; unsigned char nwbd = 0; unsigned char nwn = 0; unsigned char nwp = 0;
        unsigned char nbk = 0; unsigned char nbq = 0; unsigned char nbr = 0; unsigned char nbb = 0; unsigned char nbbl = 0; unsigned char nbbd = 0; unsigned char nbn = 0; unsigned char nbp = 0;
        unsigned char nk  = 0; unsigned char nq  = 0; unsigned char nr  = 0; unsigned char nb  = 0; unsigned char nn   = 0; unsigned char np   = 0;
        unsigned char nwM = 0; unsigned char nbM = 0; unsigned char nwm = 0; unsigned char nbm = 0; unsigned char nwt  = 0; unsigned char nbt  = 0;
    };

    Material mat;
};
 
namespace MaterialHash { // from Gull
    const int MatWQ = 1;
    const int MatBQ = 3;
    const int MatWR = (3 * 3);
    const int MatBR = (3 * 3 * 3);
    const int MatWL = (3 * 3 * 3 * 3);
    const int MatBL = (3 * 3 * 3 * 3 * 2);
    const int MatWD = (3 * 3 * 3 * 3 * 2 * 2);
    const int MatBD = (3 * 3 * 3 * 3 * 2 * 2 * 2);
    const int MatWN = (3 * 3 * 3 * 3 * 2 * 2 * 2 * 2);
    const int MatBN = (3 * 3 * 3 * 3 * 2 * 2 * 2 * 2 * 3);
    const int MatWP = (3 * 3 * 3 * 3 * 2 * 2 * 2 * 2 * 3 * 3);
    const int MatBP = (3 * 3 * 3 * 3 * 2 * 2 * 2 * 2 * 3 * 3 * 9);
    const int TotalMat = ((2 * (MatWQ + MatBQ) + MatWL + MatBL + MatWD + MatBD + 2 * (MatWR + MatBR + MatWN + MatBN) + 8 * (MatWP + MatBP)) + 1);
    const int UnknownMaterialHash = -1;

    Hash getMaterialHash(const Position::Material & mat) {
        if (mat.nwq > 2 || mat.nbq > 2 || mat.nwr > 2 || mat.nbr > 2 || mat.nwbl > 1 || mat.nbbl > 1 || mat.nwbd > 1 || mat.nbbd > 1 || mat.nwn > 2 || mat.nbn > 2 || mat.nwp > 8 || mat.nbp > 8) return -1;
        return mat.nwp * MatWP + mat.nbp * MatBP + mat.nwn * MatWN + mat.nbn * MatBN + mat.nwbl * MatWL + mat.nbbl * MatBL + mat.nwbd * MatWD + mat.nbbd * MatBD + mat.nwr * MatWR + mat.nbr * MatBR + mat.nwq * MatWQ + mat.nbq * MatBQ;
    }

    Position::Material getMatFromHash(Hash index) {
        Position::Material mat;
        mat.nwq = (int)(index % 3); index /= 3;        mat.nbq = (int)(index % 3); index /= 3;
        mat.nwr = (int)(index % 3); index /= 3;        mat.nbr = (int)(index % 3); index /= 3;
        mat.nwbl = (int)(index % 2); index /= 2;        mat.nbbl = (int)(index % 2); index /= 2;
        mat.nwbd = (int)(index % 2); index /= 2;        mat.nbbd = (int)(index % 2); index /= 2;
        mat.nwn = (int)(index % 3); index /= 3;        mat.nbn = (int)(index % 3); index /= 3;
        mat.nwp = (int)(index % 9); index /= 9;        mat.nbp = (int)(index);
        return mat;
    }

    Position::Material getMatReverseColor(const Position::Material & mat) {
        Position::Material rev;
        rev.nwk = mat.nbk;        rev.nbk = mat.nwk;
        rev.nwq = mat.nbq;        rev.nbq = mat.nwq;
        rev.nwr = mat.nbr;        rev.nbr = mat.nwr;
        rev.nwb = mat.nbb;        rev.nbb = mat.nwb;        rev.nwbl = mat.nbbl;      rev.nbbl = mat.nwbl;        rev.nwbd = mat.nbbd;      rev.nbbd = mat.nwbd;
        rev.nwn = mat.nbn;        rev.nbn = mat.nwn;
        rev.nwp = mat.nbp;        rev.nbp = mat.nwp;
        rev.nwM = mat.nbM;        rev.nbM = mat.nwM;        rev.nwm = mat.nbm;        rev.nbm = mat.nwm;          rev.nwt = mat.nbt;        rev.nbt = mat.nwt;
        return rev;
    }

    std::string ToString(const Position::Material & mat) {
        std::stringstream str;
        str << "\n" << "Q  :" << (int)mat.nwq << "\n" << "R  :" << (int)mat.nwr << "\n" << "B  :" << (int)mat.nwb << "\n" << "L  :" << (int)mat.nwbl << "\n" << "D  :" << (int)mat.nwbd << "\n" << "N  :" << (int)mat.nwn << "\n" << "Ma :" << (int)mat.nwM << "\n" << "Mi :" << (int)mat.nwm << "\n" << "T  :" << (int)mat.nwt << "\n";
        str << "\n" << "q  :" << (int)mat.nbq << "\n" << "r  :" << (int)mat.nbr << "\n" << "b  :" << (int)mat.nbb << "\n" << "l  :" << (int)mat.nbbl << "\n" << "d  :" << (int)mat.nbbd << "\n" << "n  :" << (int)mat.nbn << "\n" << "ma :" << (int)mat.nbM << "\n" << "mi :" << (int)mat.nbm << "\n" << "t  :" << (int)mat.nbt << "\n";
        return str.str();
    }

    Position::Material materialFromString(const std::string & strMat) {
        Position::Material mat;
        bool isWhite = false;
        for (auto it = strMat.begin(); it != strMat.end(); ++it) {
            switch (*it) {
            case 'K':
                isWhite = !isWhite;
                (isWhite ? mat.nwk : mat.nbk) += 1;
                break;
            case 'Q':
                (isWhite ? mat.nwq : mat.nbq) += 1;
                (isWhite ? mat.nwM : mat.nbM) += 1;
                (isWhite ? mat.nwt : mat.nbt) += 1;
                break;
            case 'R':
                (isWhite ? mat.nwr : mat.nbr) += 1;
                (isWhite ? mat.nwM : mat.nbM) += 1;
                (isWhite ? mat.nwt : mat.nbt) += 1;
                break;
            case 'L':
                (isWhite ? mat.nwbl : mat.nbbl) += 1;
                (isWhite ? mat.nwb : mat.nbb) += 1;
                (isWhite ? mat.nwm : mat.nbm) += 1;
                (isWhite ? mat.nwt : mat.nbt) += 1;
                break;
            case 'D':
                (isWhite ? mat.nwbd : mat.nbbd) += 1;
                (isWhite ? mat.nwb : mat.nbb) += 1;
                (isWhite ? mat.nwm : mat.nbm) += 1;
                (isWhite ? mat.nwt : mat.nbt) += 1;
                break;
            case 'N':
                (isWhite ? mat.nwn : mat.nbn) += 1;
                (isWhite ? mat.nwm : mat.nbm) += 1;
                (isWhite ? mat.nwt : mat.nbt) += 1;
                break;
            default:
                LogIt(logFatal) << "Bad char in material definition";
            }
        }
        return mat;
    }

    enum Terminaison : unsigned char { Ter_Unknown = 0, Ter_WhiteWin, Ter_BlackWin, Ter_Draw, Ter_LikelyDraw, Ter_HardToWin };

    Terminaison reverseTerminaison(Terminaison t) {
        switch (t) {
        case Ter_Unknown:
        case Ter_Draw:
        case Ter_LikelyDraw:
            return t;
        case Ter_WhiteWin:
            return Ter_BlackWin;
        case Ter_BlackWin:
            return Ter_WhiteWin;
        default:
            return Ter_Unknown;
        }
    }

    Terminaison materialHashTable[TotalMat];

    struct MaterialHashInitializer {
        MaterialHashInitializer(const Position::Material & mat, Terminaison t) {
            materialHashTable[getMaterialHash(mat)] = t;
        }
        static void init() { std::memset(materialHashTable, Ter_Unknown, sizeof(Terminaison)*TotalMat); }
    };

#define TO_STR2(x) #x
#define TO_STR(x) TO_STR2(x)
#define LINE_NAME(prefix) JOIN(prefix,__LINE__)
#define JOIN(symbol1,symbol2) _DO_JOIN(symbol1,symbol2 )
#define _DO_JOIN(symbol1,symbol2) symbol1##symbol2
#define DEF_MAT(x,t) const Position::Material MAT##x = materialFromString( TO_STR(x) ); \
                     MaterialHashInitializer LINE_NAME(dummyMaterialInitializer)( MAT##x ,t);
#define DEF_MAT_REV(rev,x) const Position::Material MAT##rev = MaterialHash::getMatReverseColor(MAT##x); \
                           MaterialHashInitializer LINE_NAME(dummyMaterialInitializer)( MAT##rev,reverseTerminaison(materialHashTable[getMaterialHash(MAT##x)]));

    // sym (and pseudo sym) : all should be draw
    DEF_MAT(KK, Ter_Draw)
    DEF_MAT(KQQKQQ, Ter_Draw)
    DEF_MAT(KQKQ, Ter_Draw)
    DEF_MAT(KRRKRR, Ter_Draw)
    DEF_MAT(KRKR, Ter_Draw)
    DEF_MAT(KLDKLD, Ter_Draw)
    DEF_MAT(KLLKLL, Ter_Draw)
    DEF_MAT(KDDKDD, Ter_Draw)
    DEF_MAT(KLDKLL, Ter_Draw)
    DEF_MAT(KLDKDD, Ter_Draw)
    DEF_MAT(KLKL, Ter_Draw)
    DEF_MAT(KDKD, Ter_Draw)
    DEF_MAT(KLKD, Ter_Draw)
    DEF_MAT(KNNKNN, Ter_Draw)
    DEF_MAT(KNKN, Ter_Draw)
    
    DEF_MAT_REV(KDKL, KLKD)
    DEF_MAT_REV(KLLKLD, KLDKLL)
    DEF_MAT_REV(KDDKLD, KLDKDD)
    
    // M m ///@todo 

    // 2M M
    DEF_MAT(KQQKQ, Ter_WhiteWin)
    DEF_MAT(KQQKR, Ter_WhiteWin)
    DEF_MAT(KRRKQ, Ter_LikelyDraw)
    DEF_MAT(KRRKR, Ter_WhiteWin)
    DEF_MAT(KQRKQ, Ter_WhiteWin)
    DEF_MAT(KQRKR, Ter_WhiteWin)
    
    DEF_MAT_REV(KQKQQ,KQQKQ)
    DEF_MAT_REV(KRKRR,KRRKR)
    DEF_MAT_REV(KQKRR,KRRKQ)
    DEF_MAT_REV(KRKQQ,KQQKR)
    DEF_MAT_REV(KQKQR,KQRKQ)
    DEF_MAT_REV(KRKQR,KQRKR)
    
    // 2M m ///@todo Win
    // 2m M ///@todo Draw

    // 2m m : all draw
    DEF_MAT(KLDKL, Ter_Draw)
    DEF_MAT(KLDKD, Ter_Draw)
    DEF_MAT(KLDKN, Ter_Draw)
    DEF_MAT(KLLKL, Ter_Draw)
    DEF_MAT(KLLKD, Ter_Draw)
    DEF_MAT(KLLKN, Ter_Draw)
    DEF_MAT(KDDKL, Ter_Draw)
    DEF_MAT(KDDKD, Ter_Draw)
    DEF_MAT(KDDKN, Ter_Draw)
    DEF_MAT(KNNKN, Ter_Draw)
    DEF_MAT(KLNKN, Ter_Draw)
    DEF_MAT(KDNKN, Ter_Draw)
    
    DEF_MAT_REV(KLKLD,KLDKL)
    DEF_MAT_REV(KDKLD,KLDKD)
    DEF_MAT_REV(KNKLD,KLDKN)
    DEF_MAT_REV(KLKLL,KLLKL)
    DEF_MAT_REV(KDKLL,KLLKD)
    DEF_MAT_REV(KNKLL,KLLKN)
    DEF_MAT_REV(KLKDD,KDDKL)
    DEF_MAT_REV(KDKDD,KDDKD)
    DEF_MAT_REV(KNKDD,KDDKN)
    DEF_MAT_REV(KNKNN,KNNKN)
    DEF_MAT_REV(KNKLN,KLNKN)
    DEF_MAT_REV(KNKDN,KDNKN)
    
    // Q x : all should be win
    DEF_MAT(KQKR, Ter_WhiteWin)
    DEF_MAT(KQKL, Ter_WhiteWin)
    DEF_MAT(KQKD, Ter_WhiteWin)
    DEF_MAT(KQKN, Ter_WhiteWin)
    
    DEF_MAT_REV(KRKQ,KQKR)
    DEF_MAT_REV(KLKQ,KQKL)
    DEF_MAT_REV(KDKQ,KQKD)
    DEF_MAT_REV(KNKQ,KQKN)
    
    // R x : all should be draw
    DEF_MAT(KRKL, Ter_LikelyDraw)
    DEF_MAT(KRKD, Ter_LikelyDraw)
    DEF_MAT(KRKN, Ter_LikelyDraw)
    
    DEF_MAT_REV(KLKR,KRKL)
    DEF_MAT_REV(KDKR,KRKD)
    DEF_MAT_REV(KNKR,KRKN)
    
    // B x : all are draw
    DEF_MAT(KLKN, Ter_Draw)
    DEF_MAT(KDKN, Ter_Draw)
    
    DEF_MAT_REV(KNKL,KLKN)
    DEF_MAT_REV(KNKD,KDKN)
    
    // X 0 : QR win, BN draw
    DEF_MAT(KQK, Ter_WhiteWin)
    DEF_MAT(KRK, Ter_WhiteWin)
    DEF_MAT(KLK, Ter_Draw)
    DEF_MAT(KDK, Ter_Draw)
    DEF_MAT(KNK, Ter_Draw)
    
   DEF_MAT_REV(KKQ,KQK)
   DEF_MAT_REV(KKR,KRK)
   DEF_MAT_REV(KKL,KLK)
   DEF_MAT_REV(KKD,KDK)
   DEF_MAT_REV(KKN,KNK)
    
    // 2X 0 : all win except LL, DD, NN
    DEF_MAT(KQQK, Ter_WhiteWin)
    DEF_MAT(KRRK, Ter_WhiteWin)
    DEF_MAT(KLDK, Ter_WhiteWin)
    DEF_MAT(KLLK, Ter_Draw)
    DEF_MAT(KDDK, Ter_Draw)
    DEF_MAT(KNNK, Ter_Draw)
    DEF_MAT(KLNK, Ter_WhiteWin)
    DEF_MAT(KDNK, Ter_WhiteWin)
    
    DEF_MAT_REV(KKQQ,KQQK)
    DEF_MAT_REV(KKRR,KRRK)
    DEF_MAT_REV(KKLD,KLDK)
    DEF_MAT_REV(KKLL,KLLK)
    DEF_MAT_REV(KKDD,KDDK)
    DEF_MAT_REV(KKNN,KNNK)
    DEF_MAT_REV(KKLN,KLNK)
    DEF_MAT_REV(KKDN,KDNK)
    
    ///@todo other

    inline Terminaison probeMaterialHashTable(const Position::Material & mat) {
        return materialHashTable[getMaterialHash(mat)];
    }

}

void initMaterial(Position & p){
    p.mat.nwk = (unsigned char)countBit(p.whiteKing);
    p.mat.nwq = (unsigned char)countBit(p.whiteQueen);
    p.mat.nwr = (unsigned char)countBit(p.whiteRook);
    p.mat.nwb = (unsigned char)countBit(p.whiteBishop);
    p.mat.nwbl= (unsigned char)countBit(p.whiteBishop&whiteSquare);
    p.mat.nwbd= (unsigned char)countBit(p.whiteBishop&blackSquare);
    p.mat.nwn = (unsigned char)countBit(p.whiteKnight);
    p.mat.nwp = (unsigned char)countBit(p.whitePawn);
    p.mat.nbk = (unsigned char)countBit(p.blackKing);
    p.mat.nbq = (unsigned char)countBit(p.blackQueen);
    p.mat.nbr = (unsigned char)countBit(p.blackRook);
    p.mat.nbb = (unsigned char)countBit(p.blackBishop);
    p.mat.nbbl= (unsigned char)countBit(p.blackBishop&whiteSquare);
    p.mat.nbbd= (unsigned char)countBit(p.blackBishop&blackSquare);
    p.mat.nbn = (unsigned char)countBit(p.blackKnight);
    p.mat.nbp = (unsigned char)countBit(p.blackPawn);
    p.mat.nk  = p.mat.nwk + p.mat.nbk; p.mat.nq  = p.mat.nwq + p.mat.nbq; p.mat.nr  = p.mat.nwr + p.mat.nbr; p.mat.nb  = p.mat.nwb + p.mat.nbb; p.mat.nn  = p.mat.nwn + p.mat.nbn; p.mat.np  = p.mat.nwp + p.mat.nbp;
    p.mat.nwM = p.mat.nwq + p.mat.nwr; p.mat.nbM = p.mat.nbq + p.mat.nbr; p.mat.nwm = p.mat.nwb + p.mat.nwn; p.mat.nbm = p.mat.nbb + p.mat.nbn; p.mat.nwt = p.mat.nwM + p.mat.nwm; p.mat.nbt = p.mat.nbM + p.mat.nbm;
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

inline Piece getPieceIndex  (const Position &p, Square k){ assert(k >= 0 && k < 64); return Piece(p.b[k] + PieceShift);}

inline Piece getPieceType   (const Position &p, Square k){ assert(k >= 0 && k < 64); return (Piece)std::abs(p.b[k]);}

inline Color getColor       (const Position &p, Square k){ assert(k >= 0 && k < 64); return Colors[getPieceIndex(p,k)];}

inline std::string getName  (const Position &p, Square k){ assert(k >= 0 && k < 64); return Names[getPieceIndex(p,k)];}

inline ScoreType getValue   (const Position &p, Square k){ assert(k >= 0 && k < 64); return Values[getPieceIndex(p,k)];}

inline ScoreType getAbsValue(const Position &p, Square k){ assert(k >= 0 && k < 64); return std::abs(Values[getPieceIndex(p,k)]); }

inline void unSetBit(Position & p, Square k) { ///@todo keep this lookup table in position and implemente copy CTOR and operator
    assert(k >= 0 && k < 64);
    BitBoard * const allB[13] = { &(p.blackKing),&(p.blackQueen),&(p.blackRook),&(p.blackBishop),&(p.blackKnight),&(p.blackPawn),&dummy,&(p.whitePawn),&(p.whiteKnight),&(p.whiteBishop),&(p.whiteRook),&(p.whiteQueen),&(p.whiteKing) };
    unSetBit(*allB[getPieceIndex(p, k)], k);
}
inline void unSetBit(Position &p, Square k, Piece pp) { ///@todo keep this lookup table in position and implemente copy CTOR and operator
    assert(k >= 0 && k < 64);
    BitBoard * const allB[13] = { &(p.blackKing),&(p.blackQueen),&(p.blackRook),&(p.blackBishop),&(p.blackKnight),&(p.blackPawn),&dummy,&(p.whitePawn),&(p.whiteKnight),&(p.whiteBishop),&(p.whiteRook),&(p.whiteQueen),&(p.whiteKing) };
    unSetBit(*allB[pp + PieceShift], k);
}
inline void setBit(Position &p, Square k, Piece pp) { ///@todo keep this lookup table in position and implemente copy CTOR and operator
    assert(k >= 0 && k < 64);
    BitBoard * const allB[13] = { &(p.blackKing),&(p.blackQueen),&(p.blackRook),&(p.blackBishop),&(p.blackKnight),&(p.blackPawn),&dummy,&(p.whitePawn),&(p.whiteKnight),&(p.whiteBishop),&(p.whiteRook),&(p.whiteQueen),&(p.whiteKing) };
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
    LogIt(logInfo) << "Init hash" ;
    for(int k = 0 ; k < 64 ; ++k)
        for(int j = 0 ; j < 14 ; ++j)
            ZT[k][j] = randomInt();
}

std::string ToString(const Move & m    , bool withScore = false);
std::string ToString(const Position & p, bool noEval = false);

//#define DEBUG_HASH

Hash computeHash(const Position &p){
#ifdef DEBUG_HASH
    Hash h = p.h;
    p.h = 0ull;
#endif
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
#ifdef DEBUG_HASH
    if ( h != 0ull && h != p.h ){
        LogIt(logError) << "Hash error " << ToString(p.lastMove);
        LogIt(logError) << ToString(p);
        exit(1);
    }
#endif
    return p.h;
}

namespace TT{
enum Bound{ B_exact = 0, B_alpha = 1, B_beta  = 2 };
struct Entry{
    Entry():m(INVALIDMOVE),score(0),b(B_alpha),d(-1),h(0ull){}
    Entry(Move m, ScoreType s, Bound b, DepthType d, Hash h) : m(m), score(s), b(b), d(d), h(h){}
    Move m;
    ScoreType score;
    Bound b;
    DepthType d;
    Hash h;
};

struct Bucket {
    static const int nbBucket = 2;
    Entry e[nbBucket]; // first are replace always, last is replace by depth
};

unsigned int powerFloor(unsigned int x) {
    unsigned int power = 1;
    while (power < x) power *= 2;
    return power/2;
}

static unsigned int ttSize = 0;
static Bucket * table = 0;

void initTable(){
    LogIt(logInfo) << "Init TT" ;
    ttSize = powerFloor(DynamicConfig::ttSizeMb * 1024 * 1024 / (unsigned int)sizeof(Bucket));
    table = new Bucket[ttSize];
    LogIt(logInfo) << "Size of TT " << ttSize * sizeof(Bucket) / 1024 / 1024 << "Mb" ;
}

void clearTT() {
    for (unsigned int k = 0; k < ttSize; ++k)
        for (unsigned int i = 0 ; i < Bucket::nbBucket ; ++i)
            table[k].e[i] = { INVALIDMOVE, 0, B_alpha, 0, 0ull };
}

bool getEntry(Hash h, DepthType d, Entry & e, int nbuck = 0) {
    assert(h > 0);
    if ( DynamicConfig::disableTT ) return false;
    if (nbuck >= Bucket::nbBucket) return false;
    const Entry & _e = table[h%ttSize].e[nbuck];
    if ( _e.h != h ) return getEntry(h,d,e,nbuck+1);
    e = _e; // only if no collision is detected !
    if ( _e.d >= d ){ ++stats.tthits; return true; }
    return getEntry(h,d,e,nbuck+1);
}

void setEntry(const Entry & e){
    assert(e.h > 0);
    if ( DynamicConfig::disableTT ) return;
    for (unsigned int i = 0 ; i < Bucket::nbBucket-1 ; ++i){
        Entry & _eAlways = table[e.h%ttSize].e[i];
        //if ( _eAlways.h != e.h ){ // always replace (if hash is not the same)
           _eAlways = e;
        //   break;
        //}
    }
    Entry & _eDepth = table[e.h%ttSize].e[Bucket::nbBucket-1];
    if ( false && e.d >= _eDepth.d ) _eDepth = e; // replace if better depth
}

struct EvalEntry {
    ScoreType score;
    float gp;
    Hash h;
};

unsigned int ttESize = 0;
EvalEntry * evalTable = 0;

void initETable() {
    LogIt(logInfo) << "Init eval TT" ;
    ttESize = powerFloor(DynamicConfig::ttESizeMb * 1024 * 1024 / (unsigned int)sizeof(EvalEntry));
    evalTable = new EvalEntry[ttESize];
    LogIt(logInfo) << "Size of ETT " << ttESize * sizeof(EvalEntry) / 1024 / 1024 << "Mb" ;
}

void clearETT() {
    for (unsigned int k = 0; k < ttESize; ++k) evalTable[k] = { 0, 0., 0ull };
}

bool getEvalEntry(Hash h, ScoreType & score, float & gp) {
    assert(h > 0);
    if ( DynamicConfig::disableTT ) return false;
    const EvalEntry & _e = evalTable[h%ttESize];
    if (_e.h != h) return false;
    score = _e.score;
    gp = _e.gp;
    return true;
}

void setEvalEntry(const EvalEntry & e) {
    assert(e.h > 0);
    if ( DynamicConfig::disableTT ) return;
    evalTable[e.h%ttESize] = e; // always replace
}

} // TT

struct ThreadContext; // forward decl

struct ThreadData{
    DepthType depth;
    DepthType seldepth;
    ScoreType sc;
    Position p;
    Move best;
    std::vector<Move> pv;
};

// Sizes and phases of the skip-blocks, used for distributing search depths across the threads, from stockfish
const int SkipSize[20]  = { 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4 };
const int SkipPhase[20] = { 0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7 };

// singleton pool of threads
class ThreadPool : public std::vector<ThreadContext*> {
public:
    static ThreadPool & instance();
    ~ThreadPool();
    void setup(unsigned int n);
    ThreadContext & main();
    Move searchSync(const ThreadData & d);
    void searchASync(const ThreadData & d);
    void startOthers();
    bool stop;
private:
    ThreadPool();
};

// thread from Stockfish
struct ThreadContext{

    static bool stopFlag;

    Hash hashStack[MAX_PLY] = { 0 };
    ScoreType scoreStack[MAX_PLY] = { 0 };

    struct KillerT{
        Move killers[2][MAX_PLY];
        inline void initKillers(){
            LogIt(logInfo) << "Init killers" ;
            for(int i = 0; i < 2; ++i)
                for(int k = 0 ; k < MAX_PLY; ++k)
                    killers[i][k] = INVALIDMOVE;
        }
    };

    struct HistoryT{
        ScoreType history[13][64];
        inline void initHistory(){
            LogIt(logInfo) << "Init history" ;
            for(int i = 0; i < 13; ++i)
                for(int k = 0 ; k < 64; ++k)
                    history[i][k] = 0;
        }
        inline void update(DepthType depth, Move m, const Position & p, bool plus){
            if ( Move2Type(m) == T_std ) history[getPieceIndex(p,Move2From(m))][Move2To(m)] += ScoreType( (plus?+1:-1) * (depth*depth/6.f) - (history[getPieceIndex(p,Move2From(m))][Move2To(m)] * depth*depth/6.f / 200.f));
        }
    };

    KillerT killerT;
    HistoryT historyT;

    ScoreType pvs    (ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, bool pvnode, unsigned int ply, std::vector<Move> & pv, DepthType & seldepth, const Move skipMove = INVALIDMOVE);
    ScoreType qsearch(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth);
    bool SEE(const Position & p, const Move & m, ScoreType threshold)const;
    template< bool display = false> ScoreType SEEVal(const Position & p, const Move & m)const;
    std::vector<Move> search(const Position & p, Move & m, DepthType & d, ScoreType & sc, DepthType & seldepth);
    MaterialHash::Terminaison interorNodeRecognizer(const Position & p, bool withRep = true, bool isPV = true)const;

    void idleLoop(){
        while (true){
            std::unique_lock<std::mutex> lock(_mutex);
            _searching = false;
            _cv.notify_one(); // Wake up anyone waiting for search finished
            _cv.wait(lock, [&]{ return _searching; });
            if (_exit) return;
            lock.unlock();
            search();
        }
    }

    void start(){
        std::lock_guard<std::mutex> lock(_mutex);
        LogIt(logInfo) << "Starting worker " << id() ;
        _searching = true;
        _cv.notify_one(); // Wake up the thread in IdleLoop()
    }

    void wait(){
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [&]{ return !_searching; });
    }

    void search(){
        LogIt(logInfo) << "Search launched " << id() ;
        if ( isMainThread() ){ ThreadPool::instance().startOthers(); }
        _data.pv = search(_data.p, _data.best, _data.depth, _data.sc, _data.seldepth);
    }

    size_t id()const { return _index;}
    bool   isMainThread()const { return id() == 0 ; }

    ThreadContext(size_t n):_index(n),_exit(false),_searching(true),_stdThread(&ThreadContext::idleLoop, this){ wait(); }

    ~ThreadContext(){
        _exit = true;
        start();
        LogIt(logInfo) << "Waiting for workers to join...";
        _stdThread.join();
    }

    void setData(const ThreadData & d){ _data = d;}
    const ThreadData & getData()const{ return _data;}

    static std::atomic<bool> startLock;

private:
    ThreadData              _data;
    size_t                  _index;
    std::mutex              _mutex;
    static std::mutex       _mutexDisplay;
    std::condition_variable _cv;
    // next two MUST be initialized BEFORE _stdThread (can be here to be sure ...)
    bool                    _exit;
    bool                    _searching;
    std::thread             _stdThread;
};

bool ThreadContext::stopFlag = false;

std::atomic<bool> ThreadContext::startLock;

ThreadPool & ThreadPool::instance(){ static ThreadPool pool; return pool;}

ThreadPool::~ThreadPool(){
    while (size()) { delete back(); pop_back(); }
    LogIt(logInfo) << "... ok threadPool deleted";
}

void ThreadPool::setup(unsigned int n){
    assert(n > 0);
    LogIt(logInfo) << "Using " << n << " threads";
    while (size() < (unsigned int)n) push_back(new ThreadContext(size()));
}

ThreadContext & ThreadPool::main() { return *(front()); }

Move ThreadPool::searchSync(const ThreadData & d){
    LogIt(logInfo) << "Search Sync" ;
    LogIt(logInfo) << "Wait for workers to be ready" ;
    for (auto s : *this) (*s).wait();
    ThreadContext::startLock.store(true);
    LogIt(logInfo) << "...ok" ;
    for (auto s : *this) (*s).setData(d); // this is a copy
    LogIt(logInfo) << "Calling main thread search" ;
    main().search();
    ThreadContext::stopFlag = true;
    LogIt(logInfo) << "Wait for workers to finish" ;
    for(auto s : *this) if (!(*s).isMainThread()) (*s).wait();
    LogIt(logInfo) << "...ok" ;
    return main().getData().best;
}

void ThreadPool::searchASync(const ThreadData & d){} ///@todo for pondering

void ThreadPool::startOthers(){ for (auto s : *this) if (!(*s).isMainThread()) (*s).start();}

ThreadPool::ThreadPool():stop(false){ push_back(new ThreadContext(size()));} // this one will be called "Main" thread

ScoreType eval(const Position & p, float & gp); // forward decl

inline bool isMatingScore (ScoreType s) { return (s >=  MATE - MAX_DEPTH); }
inline bool isMatedScore  (ScoreType s) { return (s <= -MATE + MAX_DEPTH); }
inline bool isMateScore   (ScoreType s) { return (std::abs(s) >= MATE - MAX_DEPTH); }

inline bool isCapture(const MType & mt){ return mt == T_capture || mt == T_ep || mt == T_cappromq || mt == T_cappromr || mt == T_cappromb || mt == T_cappromn; }

inline bool isCapture(const Move & m){ return isCapture(Move2Type(m)); }

inline bool isPromotion(const MType & mt){ return mt >= T_promq && mt <= T_cappromn;}

inline bool isPromotion(const Move & m){ return isPromotion(Move2Type(m));}

inline bool isUnderPromotion(const MType & mt) { return mt >= T_promr && mt <= T_cappromn && mt != T_cappromq; }

inline bool isUnderPromotion(const Move & m) { return isUnderPromotion(Move2Type(m)); }

inline bool isTactical(const Move m) { return isCapture(m) || isPromotion(m); }

inline bool isSafe(const Move m, const Position & p) {
    if (isUnderPromotion(m)) return false;
    Piece pp = getPieceType(p, Move2From(m));
    if (pp == P_wk) return true;
    if (isCapture(m) && getAbsValue(p, Move2To(m)) >= Values[pp + PieceShift]) return true;
    return Move2Score(m) > -900; // see
}

inline bool isDangerous(const Move m, bool isInCheck, bool isCheck) { return isTactical(m) || isCheck || isInCheck; }

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

std::string ToString(const Move & m, bool withScore){
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

std::string ToString(const Position & p, bool noEval){
    std::stringstream ss;
    ss << "Position" << std::endl;
    for (Square j = 7; j >= 0; --j) {
        ss << "# +-+-+-+-+-+-+-+-+" << std::endl << "# |";
        for (Square i = 0; i < 8; ++i) ss << getName(p,i+j*8) << '|';
        ss << std::endl;
    }
    ss << "# +-+-+-+-+-+-+-+-+" << std::endl;
    if ( p.ep >=0 ) ss << "# ep " << Squares[p.ep] << std::endl;
    ss << "# wk " << (p.wk!=INVALIDSQUARE?Squares[p.wk]:"none")  << std::endl << "# bk " << (p.bk!=INVALIDSQUARE?Squares[p.bk]:"none") << std::endl;
    ss << "# Turn " << (p.c == Co_White ? "white" : "black") << std::endl;
    ScoreType sc = 0;
    if ( ! noEval ){
        float gp = 0;
        sc = eval(p, gp);
        ss << "# Phase " << gp  << std::endl << "# Static score " << sc << std::endl << "# Hash " << computeHash(p) << std::endl << "# FEN " << GetFEN(p) << std::endl;
    }
    ss << "#";
    return ss.str();
}

inline Color opponentColor(const Color c){ return Color((c+1)%2);}

// from Rofchade
const ScoreType PST[6][64] = {
    {   //pawn
          0,   0,   0,   0,   0,   0,  0,   0,
         98, 134,  61,  95,  68, 126, 34, -11,
         -6,   7,  26,  31,  65,  56, 25, -20,
        -14,  13,   6,  21,  23,  12, 17, -23,
        -27,  -2,  -5,  12,  17,   6, 10, -25,
        -26,  -4,  -4, -10,   3,   3, 33, -12,
        -35,  -1, -20, -23, -15,  24, 38, -22,
        0,   0,   0,   0,   0,   0,  0,   0
    },{ //knight
        -167, -89, -34, -49,  61, -97, -15, -107,
         -73, -41,  72,  36,  23,  62,   7,  -17,
         -47,  60,  37,  65,  84, 129,  73,   44,
          -9,  17,  19,  53,  37,  69,  18,   22,
         -13,   4,  16,  13,  28,  19,  21,   -8,
         -23,  -9,  12,  10,  19,  17,  25,  -16,
         -29, -53, -12,  -3,  -1,  18, -14,  -19,
        -105, -21, -58, -33, -17, -28, -19,  -23
    },{ //bishop
        -29,   4, -82, -37, -25, -42,   7,  -8,
        -26,  16, -18, -13,  30,  59,  18, -47,
        -16,  37,  43,  40,  35,  50,  37,  -2,
         -4,   5,  19,  50,  37,  37,   7,  -2,
         -6,  13,  13,  26,  34,  12,  10,   4,
          0,  15,  15,  15,  14,  27,  18,  10,
          4,  15,  16,   0,   7,  21,  33,   1,
        -33,  -3, -14, -21, -13, -12, -39, -21
    },{ //rook
         32,  42,  32,  51, 63,  9,  31,  43,
         27,  32,  58,  62, 80, 67,  26,  44,
         -5,  19,  26,  36, 17, 45,  61,  16,
        -24, -11,   7,  26, 24, 35,  -8, -20,
        -36, -26, -12,  -1,  9, -7,   6, -23,
        -45, -25, -16, -17,  3,  0,  -5, -33,
        -44, -16, -20,  -9, -1, 11,  -6, -71,
        -19, -13,   1,  17, 16,  7, -37, -26
    },{ //queen
        -28,   0,  29,  12,  59,  44,  43,  45,
        -24, -39,  -5,   1, -16,  57,  28,  54,
        -13, -17,   7,   8,  29,  56,  47,  57,
        -27, -27, -16, -16,  -1,  17,  -2,   1,
         -9, -26,  -9, -10,  -2,  -4,   3,  -3,
        -14,   2, -11,  -2,  -5,   2,  14,   5,
        -35,  -8,  11,   2,   8,  15,  -3,   1,
         -1, -18,  -9,  10, -15, -25, -31, -50
    },{ //king
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
    {   //pawn
          0,   0,   0,   0,   0,   0,   0,   0,
        178, 173, 158, 134, 147, 132, 165, 187,
         94, 100,  85,  67,  56,  53,  82,  84,
         32,  24,  13,   5,  -2,   4,  17,  17,
         13,   9,  -3,  -7,  -7,  -8,   3,  -1,
          4,   7,  -6,   1,   0,  -5,  -1,  -8,
         13,   8,   8,  10,  13,   0,   2,  -7,
          0,   0,   0,   0,   0,   0,   0,   0
    },{ //knight
        -58, -38, -13, -28, -31, -27, -63, -99,
        -25,  -8, -25,  -2,  -9, -25, -24, -52,
        -24, -20,  10,   9,  -1,  -9, -19, -41,
        -17,   3,  22,  22,  22,  11,   8, -18,
        -18,  -6,  16,  25,  16,  17,   4, -18,
        -23,  -3,  -1,  15,  10,  -3, -20, -22,
        -42, -20, -10,  -5,  -2, -20, -23, -44,
        -29, -51, -23, -15, -22, -18, -50, -64
    },{ //bishop
        -14, -21, -11,  -8, -7,  -9, -17, -24,
         -8,  -4,   7, -12, -3, -13,  -4, -14,
          2,  -8,   0,  -1, -2,   6,   0,   4,
         -3,   9,  12,   9, 14,  10,   3,   2,
         -6,   3,  13,  19,  7,  10,  -3,  -9,
        -12,  -3,   8,  10, 13,   3,  -7, -15,
        -14, -18,  -7,  -1,  4,  -9, -15, -27,
        -23,  -9, -23,  -5, -9, -16,  -5, -17
    },{ //rook
        13, 10, 18, 15, 12,  12,   8,   5,
        11, 13, 13, 11, -3,   3,   8,   3,
         7,  7,  7,  5,  4,  -3,  -5,  -3,
         4,  3, 13,  1,  2,   1,  -1,   2,
         3,  5,  8,  4, -5,  -6,  -8, -11,
        -4,  0, -5, -1, -7, -12,  -8, -16,
        -6, -6,  0,  2, -9,  -9, -11,  -3,
        -9,  2,  3, -1, -5, -13,   4, -20
    },{ //queen
         -9,  22,  22,  27,  27,  19,  10,  20,
        -17,  20,  32,  41,  58,  25,  30,   0,
        -20,   6,   9,  49,  47,  35,  19,   9,
          3,  22,  24,  45,  57,  40,  57,  36,
        -18,  28,  19,  47,  31,  34,  39,  23,
        -16, -27,  15,   6,   9,  17,  10,   5,
        -22, -23, -30, -16, -16, -23, -36, -32,
        -33, -28, -22, -43,  -5, -32, -20, -41
    },{ //king
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

template < typename T > T readFromString(const std::string & s){ std::stringstream ss(s); T tmp; ss >> tmp; return tmp;}

bool readFEN(const std::string & fen, Position & p, bool silent){
    std::vector<std::string> strList;
    std::stringstream iss(fen);
    std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));

    if ( !silent) LogIt(logInfo) << "Reading fen " << fen ;

    // reset position
    p.h = 0;
    for(Square k = 0 ; k < 64 ; ++k) p.b[k] = P_none;

    Square j = 1, i = 0;
    while ((j <= 64) && (i <= (char)strList[0].length())){
        char letter = strList[0].at(i);
        ++i;
        const Square k = (7 - (j - 1) / 8) * 8 + ((j - 1) % 8);
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
        default: LogIt(logFatal) << "FEN ERROR 0 : " << letter ;
        }
        j++;
    }

    // set the turn; default is white
    p.c = Co_White;
    if (strList.size() >= 2){
        if (strList[1] == "w")      p.c = Co_White;
        else if (strList[1] == "b") p.c = Co_Black;
        else { LogIt(logFatal) << "FEN ERROR 1" ; return false; }
    }

    // Initialize all castle possibilities (default is none)
    p.castling = C_none;
    if (strList.size() >= 3){
        bool found = false;
        if (strList[2].find('K') != std::string::npos){ p.castling |= C_wks; found = true; }
        if (strList[2].find('Q') != std::string::npos){ p.castling |= C_wqs; found = true; }
        if (strList[2].find('k') != std::string::npos){ p.castling |= C_bks; found = true; }
        if (strList[2].find('q') != std::string::npos){ p.castling |= C_bqs; found = true; }
        if ( ! found ){ if ( !silent) LogIt(logWarn) << "No castling right given" ; }
    }
    else if ( !silent) LogIt(logInfo) << "No castling right given" ;

    // read en passant and save it (default is invalid)
    p.ep = INVALIDSQUARE;
    if ((strList.size() >= 4) && strList[3] != "-" ){
        if (strList[3].length() >= 2){
            if ((strList[3].at(0) >= 'a') && (strList[3].at(0) <= 'h') && ((strList[3].at(1) == '3') || (strList[3].at(1) == '6'))) p.ep = stringToSquare(strList[3]);
            else { LogIt(logFatal) << "FEN ERROR 3-1 : bad en passant square : " << strList[3] ; return false; }
        }
        else{ LogIt(logFatal) << "FEN ERROR 3-2 : bad en passant square : " << strList[3] ; return false; }
    }
    else if ( !silent) LogIt(logInfo) << "No en passant square given" ;

    assert(p.ep == INVALIDSQUARE || (SQRANK(p.ep) == 2 || SQRANK(p.ep) == 5));

    // read 50 moves rules
    if (strList.size() >= 5) p.fifty = (unsigned char)readFromString<int>(strList[4]);
    else p.fifty = 0;

    // read number of move
    if (strList.size() >= 6) p.moves = (unsigned char)readFromString<int>(strList[5]);
    else p.moves = 1;

    p.ply = (int(p.moves) - 1) * 2 + 1 + (p.c == Co_Black ? 1 : 0);

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

bool readMove(const Position & p, const std::string & ss, Square & from, Square & to, MType & moveType ) {

    if ( ss.empty()){
        LogIt(logFatal) << "Trying to read empty move ! " ;
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
    if ( strList.empty()){ LogIt(logFatal) << "Trying to read bad move, seems empty " << str ; return false; }

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
        if ( strList.size() == 1 ){ LogIt(logFatal) << "Trying to read bad move, malformed (=1) " << str ; return false; }
        if ( strList.size() > 2 && strList[2] != "ep"){ LogIt(logFatal) << "Trying to read bad move, malformed (>2)" << str ; return false; }

        if (strList[0].size() == 2 && (strList[0].at(0) >= 'a') && (strList[0].at(0) <= 'h') && ((strList[0].at(1) >= 1) && (strList[0].at(1) <= '8'))) from = stringToSquare(strList[0]);
        else { LogIt(logFatal) << "Trying to read bad move, invalid from square " << str ; return false; }

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
                else{ LogIt(logFatal) << "Trying to read bad move, invalid to square " << str ; return false; }
            }
            else{
                to = stringToSquare(strList[1]);
                isCapture = p.b[to] != P_none;
                if(isCapture) moveType = T_capture;
            }
        }
        else { LogIt(logFatal) << "Trying to read bad move, invalid to square " << str ; return false; }
    }

    if (getPieceType(p,from) == P_wp && to == p.ep) moveType = T_ep;

    return true;
}

namespace TimeMan{
int msecPerMove, msecInTC, nbMoveInTC, msecInc, msecUntilNextTC;
bool isDynamic;

std::chrono::time_point<Clock> startTime;

void init(){
    LogIt(logInfo) << "Init timeman" ;
    msecPerMove     = 777;
    msecInTC        = -1;
    nbMoveInTC      = -1;
    msecInc         = -1;
    msecUntilNextTC = -1;
    isDynamic       = false;
}

int GetNextMSecPerMove(const Position & p){
    static const int msecMargin = 200;
    int ms = -1;
    LogIt(logInfo) << "msecPerMove     " << msecPerMove;
    LogIt(logInfo) << "msecInTC        " << msecInTC   ;
    LogIt(logInfo) << "msecInc         " << msecInc    ;
    LogIt(logInfo) << "nbMoveInTC      " << nbMoveInTC ;
    LogIt(logInfo) << "msecUntilNextTC " << msecUntilNextTC;
    LogIt(logInfo) << "currentNbMoves  " << int(p.moves);
    int msecIncLoc = (msecInc > 0) ? msecInc : 0;
    if ( msecPerMove > 0 ) ms =  msecPerMove;
    else if ( nbMoveInTC > 0){ // mps is given
        assert(msecInTC > 0);
        assert(nbMoveInTC > 0);
        LogIt(logInfo) << "TC mode";
        if (!isDynamic) ms = int((msecInTC - msecMargin) / (float)nbMoveInTC) + msecIncLoc ;
        else ms = int((msecUntilNextTC - msecMargin) / (float)(nbMoveInTC-((p.moves-1)%nbMoveInTC))) + msecIncLoc;
    }
    else{ // mps is not given
        ///@todo something better using the real time command
        LogIt(logInfo) << "Fix time mode";
        const int nmoves = 24; // always be able to play this more moves !
        LogIt(logInfo) << "nmoves    " << nmoves;
        LogIt(logInfo) << "p.moves   " << int(p.moves);
        assert(nmoves > 0);
        if (!isDynamic) ms = std::min(msecInTC - msecMargin, int((msecInTC+p.moves*msecIncLoc) / (float)(nmoves+p.moves)) - msecMargin);
        else ms = std::min(msecInTC - msecMargin, int(msecUntilNextTC / (float)nmoves + 0.75*msecIncLoc) - msecMargin);
    }
    return ms;
}
} // TimeMan

inline void addMove(Square from, Square to, MType type, std::vector<Move> & moves){ assert( from >= 0 && from < 64); assert( to >=0 && to < 64); moves.push_back(ToMove(from,to,type,0));}

Square kingSquare(const Position & p) { return (p.c == Co_White) ? p.wk : p.bk; }

// HQ BB code from Amoeba
namespace BB {
inline BitBoard shiftSouth    (BitBoard bitBoard) { return bitBoard >> 8; }
inline BitBoard shiftNorth    (BitBoard bitBoard) { return bitBoard << 8; }
inline BitBoard shiftWest     (BitBoard bitBoard) { return bitBoard >> 1 & ~fileH; }
inline BitBoard shiftEast     (BitBoard bitBoard) { return bitBoard << 1 & ~fileA; }
inline BitBoard shiftNorthEast(BitBoard bitBoard) { return bitBoard << 9 & ~fileA; }
inline BitBoard shiftNorthWest(BitBoard bitBoard) { return bitBoard << 7 & ~fileH; }
inline BitBoard shiftSouthEast(BitBoard bitBoard) { return bitBoard >> 7 & ~fileA; }
inline BitBoard shiftSouthWest(BitBoard bitBoard) { return bitBoard >> 9 & ~fileH; }

int ranks[512];
struct Mask {
    BitBoard bbsquare, diagonal, antidiagonal, file, kingZone;
    BitBoard pawnAttack[2], push[2], dpush[2]; // one for each colors
    BitBoard enpassant, knight, king;
    BitBoard between[64];
    BitBoard frontSpan[2], rearSpan[2], passerSpan[2], attackFrontSpan[2];
    Mask():bbsquare(0ull), diagonal(0ull), antidiagonal(0ull), file(0ull), kingZone(0ull), pawnAttack{ 0ull,0ull }, push{ 0ull,0ull }, dpush{ 0ull,0ull }, enpassant(0ull), knight(0ull), king(0ull), between{0ull}, frontSpan{0ull}, rearSpan{0ull}, passerSpan{0ull}, attackFrontSpan{0ull}{}
};
Mask mask[64];

inline void initMask() {
    LogIt(logInfo) << "Init mask" ;
    int d[64][64] = { 0 };
    for (Square x = 0; x < 64; ++x) {
        mask[x].bbsquare = SquareToBitboard(x);
        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i == 0 && j == 0) continue;
                for (int r = (x >> 3) + i, f = (x & 7) + j; 0 <= r && r < 8 && 0 <= f && f < 8; r += i, f += j) {
                    const int y = 8 * r + f;
                    d[x][y] = (8 * i + j);
                    for (int z = x + d[x][y]; z != y; z += d[x][y]) mask[x].between[y] |= SquareToBitboard(z);
                }
                const int r = x >> 3;
                const int f = x & 7;
                if ( 0 <= r+i && r+i < 8 && 0 <= f+j && f+j < 8) mask[x].kingZone |= SquareToBitboard(((x >> 3) + i)*8+(x & 7) + j);
            }
        }

        for (int y = x - 9; y >= 0 && d[x][y] == -9; y -= 9) mask[x].diagonal |= SquareToBitboard(y);
        for (int y = x + 9; y < 64 && d[x][y] ==  9; y += 9) mask[x].diagonal |= SquareToBitboard(y);

        for (int y = x - 7; y >= 0 && d[x][y] == -7; y -= 7) mask[x].antidiagonal |= SquareToBitboard(y);
        for (int y = x + 7; y < 64 && d[x][y] ==  7; y += 7) mask[x].antidiagonal |= SquareToBitboard(y);

        for (int y = x - 8; y >= 0; y -= 8) mask[x].file |= SquareToBitboard(y);
        for (int y = x + 8; y < 64; y += 8) mask[x].file |= SquareToBitboard(y);

        int f = x & 07;
        int r = x >> 3;
        for (int i = -1, c = 1, dp = 6; i <= 1; i += 2, c = 0, dp = 1) {
            for (int j = -1; j <= 1; j += 2) if (0 <= r + i && r + i < 8 && 0 <= f + j && f + j < 8) {  mask[x].pawnAttack[c] |= SquareToBitboard((r + i) * 8 + (f + j)); }
            if (0 <= r + i && r + i < 8) {
                mask[x].push[c] = SquareToBitboard((r + i) * 8 + f);
                if ( r == dp ) mask[x].dpush[c] = SquareToBitboard((r + 2*i) * 8 + f); // double push
            }
        }
        if (r == 3 || r == 4) {
            if (f > 0) mask[x].enpassant |= SquareToBitboard(x - 1);
            if (f < 7) mask[x].enpassant |= SquareToBitboard(x + 1);
        }

        for (int i = -2; i <= 2; i = (i == -1 ? 1 : i + 1)) {
            for (int j = -2; j <= 2; ++j) {
                if (i == j || i == -j || j == 0) continue;
                if (0 <= r + i && r + i < 8 && 0 <= f + j && f + j < 8) { mask[x].knight |= SquareToBitboard(8 * (r + i) + (f + j)); }
            }
        }

        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i == 0 && j == 0) continue;
                if (0 <= r + i && r + i < 8 && 0 <= f + j && f + j < 8) { mask[x].king |= SquareToBitboard(8 * (r + i) + (f + j)); }
            }
        }

        BitBoard wspan = SquareToBitboardTable(x);
        wspan |= wspan << 8;
        wspan |= wspan << 16;
        wspan |= wspan << 32;
        wspan = shiftNorth(wspan);
        BitBoard bspan = SquareToBitboardTable(x);
        bspan |= bspan >> 8;
        bspan |= bspan >> 16;
        bspan |= bspan >> 32;
        bspan = shiftSouth(bspan);

        mask[x].frontSpan[Co_White] = mask[x].rearSpan[Co_Black] = wspan;
        mask[x].frontSpan[Co_Black] = mask[x].rearSpan[Co_White] = bspan;

        mask[x].passerSpan[Co_White] = wspan;
        mask[x].passerSpan[Co_White] |= shiftWest(wspan);
        mask[x].passerSpan[Co_White] |= shiftEast(wspan);
        mask[x].passerSpan[Co_Black] = bspan;
        mask[x].passerSpan[Co_Black] |= shiftWest(bspan);
        mask[x].passerSpan[Co_Black] |= shiftEast(bspan);

        mask[x].attackFrontSpan[Co_White] = shiftWest(wspan);
        mask[x].attackFrontSpan[Co_White] |= shiftEast(wspan);
        mask[x].attackFrontSpan[Co_Black] = shiftWest(bspan);
        mask[x].attackFrontSpan[Co_Black] |= shiftEast(bspan);

    }

    for (Square o = 0; o < 64; ++o) {
        for (int k = 0; k < 8; ++k) {
            int y = 0;
            for (int x = k - 1; x >= 0; --x) {
                const BitBoard b = SquareToBitboard(x);
                y |= b;
                if (((o << 1) & b) == b) break;
            }
            for (int x = k + 1; x < 8; ++x) {
                const BitBoard b = SquareToBitboard(x);
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
    forward -= SquareToBitboard(x);
    reverse -= SquareToBitboard(x^63);
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
template <       > BitBoard coverage<P_wb>(const Square x, const BitBoard occupancy, const Color c) { return diagonalAttack(occupancy, x) | antidiagonalAttack(occupancy, x); }
template <       > BitBoard coverage<P_wr>(const Square x, const BitBoard occupancy, const Color c) { return fileAttack(occupancy, x) | rankAttack(occupancy, x); }
template <       > BitBoard coverage<P_wq>(const Square x, const BitBoard occupancy, const Color c) { return diagonalAttack(occupancy, x) | antidiagonalAttack(occupancy, x) | fileAttack(occupancy, x) | rankAttack(occupancy, x); }
template <       > BitBoard coverage<P_wk>(const Square x, const BitBoard occupancy, const Color c) { return mask[x].king; }

template < Piece pp > inline BitBoard attack(const Square x, const BitBoard target, const BitBoard occupancy = 0, const Color c = Co_White) { return coverage<pp>(x, occupancy, c) & target; }

int popBit(BitBoard & b) {
    unsigned long i = 0;
    assert( b != 0ull);
    bsf(b, i);
    b &= b - 1;
    return i;
}

BitBoard isAttackedBB(const Position &p, const Square x, Color c) {
    if (c == Co_White) return attack<P_wb>(x, p.blackBishop | p.blackQueen, p.occupancy) | attack<P_wr>(x, p.blackRook | p.blackQueen, p.occupancy) | attack<P_wn>(x, p.blackKnight) | attack<P_wp>(x, p.blackPawn, p.occupancy, Co_White) | attack<P_wk>(x, p.blackKing);
    else               return attack<P_wb>(x, p.whiteBishop | p.whiteQueen, p.occupancy) | attack<P_wr>(x, p.whiteRook | p.whiteQueen, p.occupancy) | attack<P_wn>(x, p.whiteKnight) | attack<P_wp>(x, p.whitePawn, p.occupancy, Co_Black) | attack<P_wk>(x, p.whiteKing);
}

bool getAttackers(const Position & p, const Square k, std::vector<Square> & attakers) {
    attakers.clear();
    BitBoard attack = isAttackedBB(p, k, p.c);
    while (attack) attakers.push_back(popBit(attack));
    return !attakers.empty();
}

} // BB

inline bool isAttacked(const Position & p, const Square k) { return k!=INVALIDSQUARE && BB::isAttackedBB(p, k, p.c) != 0ull;}

inline bool getAttackers(const Position & p, const Square k, std::vector<Square> & attakers) { return k!=INVALIDSQUARE && BB::getAttackers(p, k, attakers);}

enum GenPhase{ GP_all = 0, GP_cap = 1, GP_quiet = 2};

void generateSquare(const Position & p, std::vector<Move> & moves, Square from, GenPhase phase = GP_all){
    assert(from != INVALIDSQUARE);
    const Color side = p.c;
    const BitBoard myPieceBB  = ((p.c == Co_White) ? p.whitePiece : p.blackPiece);
    const BitBoard oppPieceBB = ((p.c != Co_White) ? p.whitePiece : p.blackPiece);
    const Piece piece = p.b[from];
    const Piece ptype = (Piece)std::abs(piece);
    assert ( ptype != P_none ) ;
    static BitBoard(*const pf[])(const Square, const BitBoard, const  Color) = { &BB::coverage<P_wp>, &BB::coverage<P_wn>, &BB::coverage<P_wb>, &BB::coverage<P_wr>, &BB::coverage<P_wq>, &BB::coverage<P_wk> };
    if (ptype != P_wp) {
        BitBoard bb = pf[ptype-1](from, p.occupancy, p.c) & ~myPieceBB;
        if      (phase == GP_cap)   bb &= oppPieceBB;  // only target opponent piece
        else if (phase == GP_quiet) bb &= ~oppPieceBB; // do not target opponent piece
        while (bb) {
            const Square to = BB::popBit(bb);
            const bool isCap = (phase == GP_cap) || ((oppPieceBB&SquareToBitboard(to)) != 0ull);
            if (isCap) addMove(from,to,T_capture,moves);
            else addMove(from,to,T_std,moves);
        }
        if ( phase != GP_cap && ptype == P_wk ){ // castling
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
        BitBoard pawnmoves = 0ull;
        if ( phase != GP_cap) pawnmoves |= BB::mask[from].push[p.c] & ~p.occupancy;
        if ((phase != GP_cap) && (BB::mask[from].push[p.c] & p.occupancy) == 0ull) pawnmoves |= BB::mask[from].dpush[p.c] & ~p.occupancy;
        if ( phase != GP_quiet) pawnmoves |= BB::mask[from].pawnAttack[p.c] & ~myPieceBB & oppPieceBB;
        while (pawnmoves) {
            const Square to = BB::popBit(pawnmoves);
            const bool isCap = (phase == GP_cap) || ((oppPieceBB&SquareToBitboard(to)) != 0ull);
            if (isCap){
                if ( SQRANK(to) == 0 || SQRANK(to) == 7) {
                    addMove(from, to, T_cappromq, moves); // pawn capture with promotion
                    addMove(from, to, T_cappromr, moves); // pawn capture with promotion
                    addMove(from, to, T_cappromb, moves); // pawn capture with promotion
                    addMove(from, to, T_cappromn, moves); // pawn capture with promotion
                }
                else addMove(from,to,T_capture,moves);
            }
            else{
                if ( SQRANK(to) == 0 || SQRANK(to) == 7) {
                    addMove(from, to, T_promq, moves); // promotion Q
                    addMove(from, to, T_promr, moves); // promotion R
                    addMove(from, to, T_promb, moves); // promotion B
                    addMove(from, to, T_promn, moves); // promotion N
                }
                else addMove(from,to,T_std,moves);
            }
        }
        if ( p.ep != INVALIDSQUARE && phase != GP_quiet ) pawnmoves |= BB::mask[from].pawnAttack[p.c] & ~myPieceBB & SquareToBitboard(p.ep);
        while (pawnmoves) addMove(from,BB::popBit(pawnmoves),T_ep,moves);
    }
}

void generate(const Position & p, std::vector<Move> & moves, GenPhase phase = GP_all){
    moves.clear();
    BitBoard myPieceBBiterator = ( (p.c == Co_White) ? p.whitePiece : p.blackPiece);
    while (myPieceBBiterator) generateSquare(p,moves,BB::popBit(myPieceBBiterator),phase);
}

inline void movePiece(Position & p, Square from, Square to, Piece fromP, Piece toP, bool isCapture = false, Piece prom = P_none) {
    const int fromId   = fromP + PieceShift;
    const int toId     = toP + PieceShift;
    const Piece toPnew = prom != P_none ? prom : fromP;
    const int toIdnew  = prom != P_none ? (prom + PieceShift) : fromId;
    assert(from>=0 && from <64);
    assert(to>=0 && to <64);
    p.b[from] = P_none;
    p.b[to]   = toPnew;
    unSetBit(p, from, fromP);
    unSetBit(p, to,   toP); // usefull only if move is a capture
    setBit  (p, to,   toPnew);
    p.h ^= ZT[from][fromId]; // remove fromP at from
    if (isCapture) p.h ^= ZT[to][toId]; // if capture remove toP at to
    p.h ^= ZT[to][toIdnew]; // add fromP (or prom) at to
}

/*
bool validate(const Position &p, const Move &m){
    if ( m == INVALIDMOVE ) return false;
    const BitBoard side = p.whitePiece;
    const BitBoard opponent = p.blackPiece;
    if ( p.c == Co_Black ) std::swap(side,opponent);
    const Square from = Move2From(m);
    if ( (SquareToBitboard(from) & side) == 0ull ) return false; // from piece is not ours
    const Piece ptype = (Piece)std::abs(p.b[from]);
    assert(ptype != P_none);
    static BitBoard(*const pf[])(const Square, const BitBoard, const  Color) = { &BB::coverage<P_wp>, &BB::coverage<P_wn>, &BB::coverage<P_wb>, &BB::coverage<P_wr>, &BB::coverage<P_wq>, &BB::coverage<P_wk> };
    const Square to = Move2To(m);
    if ( (SquareToBitboard(to) & side) != 0ull ) return false; // to piece is ours
    const BitBoard bb = pf[ptype-1](from, p.occupancy, p.c) & ~side;
    if ( (SquareToBitboard(to) & bb) == 0ull ) return false; // to square is not reachable with this kind of piece, or there is something between from and to for sliders
    return true;
}
*/

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
        // king capture : is that necessary ???
        if ( toP == P_wk ) p.wk = INVALIDSQUARE;
        if ( toP == P_bk ) p.bk = INVALIDSQUARE;

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
        assert(p.ep != INVALIDSQUARE);
        assert(SQRANK(p.ep) == 2 || SQRANK(p.ep) == 5);
        const Square epCapSq = p.ep + (p.c == Co_White ? -8 : +8);
        assert(epCapSq>=0 && epCapSq<64);
        unSetBit(p, epCapSq); // BEFORE setting p.b new shape !!!
        unSetBit(p, from);
        setBit(p, to, fromP);
        p.b[from] = P_none;
        p.b[to] = fromP;
        p.b[epCapSq] = P_none;
        p.h ^= ZT[from][fromId]; // remove fromP at from
        p.h ^= ZT[epCapSq][(p.c == Co_White ? P_bp : P_wp) + PieceShift]; // remove captured pawn
        p.h ^= ZT[to][fromId]; // add fromP at to
    }
        break;

    case T_promq:
    case T_cappromq:
        movePiece(p, from, to, fromP, toP, type == T_cappromq,(p.c == Co_White ? P_wq : P_bq));
        break;
    case T_promr:
    case T_cappromr:
        movePiece(p, from, to, fromP, toP, type == T_cappromr, (p.c == Co_White ? P_wr : P_br));
        break;
    case T_promb:
    case T_cappromb:
        movePiece(p, from, to, fromP, toP, type == T_cappromb, (p.c == Co_White ? P_wb : P_bb));
        break;
    case T_promn:
    case T_cappromn:
        movePiece(p, from, to, fromP, toP, type == T_cappromn, (p.c == Co_White ? P_wn : P_bn));
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

    if ( isAttacked(p,kingSquare(p)) ) return false; // this is the only legal move validation needed

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
    assert(p.ep == INVALIDSQUARE || (SQRANK(p.ep) == 2 || SQRANK(p.ep) == 5));
    if (p.ep != INVALIDSQUARE) p.h ^= ZT[p.ep][13];

    p.c = opponentColor(p.c);
    if ( toP != P_none || abs(fromP) == P_wp ) p.fifty = 0;
    else ++p.fifty;
    if ( p.c == Co_White ) ++p.moves;
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

template<typename Iter>
Iter select_randomly(Iter start, Iter end) {
    static std::random_device rd;
    static std::mt19937 gen(rd()); // here really random
    std::uniform_int_distribution<> dis(0, (int)std::distance(start, end) - 1);
    std::advance(start, dis(gen));
    return start;
}

const Move Get(const Hash h){
    std::unordered_map<Hash, std::set<Move> >::iterator it = book.find(h);
    if ( it == book.end() ) return INVALIDMOVE;
    return *select_randomly(it->second.begin(),it->second.end());
}

} // Book

struct SortThreatsFunctor {
    const Position & _p;
    SortThreatsFunctor(const Position & p):_p(p){}
    bool operator()(const Square s1,const Square s2) { return std::abs(getValue(_p,s1)) < std::abs(getValue(_p,s2));}
};

// see value from xiphos (and CPW...)
template< bool display>
ScoreType ThreadContext::SEEVal(const Position & pos, const Move & move)const{
  ScoreType gain[64]; // 64 shall be bullet proof here...
  Square sq = Move2To(move);
  Square from = Move2From(move);
  Piece p = pos.b[from];
  Piece captured = pos.b[sq];
  Color side = pos.c;
  ScoreType pv = Values[std::abs(p)+PieceShift];
  ScoreType cv = 0;

  if (captured != P_none){
    cv = Values[std::abs(captured)+PieceShift];
    if (pv <= cv) return 0;
  }

  bool isProm = isPromotion(move);
  BitBoard occ = pos.occupancy ^ SquareToBitboard(from);
  ScoreType pqv = Values[P_wq + PieceShift] - Values[P_wp + PieceShift];
  gain[0] = cv;
  if (isProm && std::abs(p) == P_wp){
    pv += pqv;
    gain[0] += pqv;
  }
  else if (sq == pos.ep && p == P_wp)  {
    occ ^= SquareToBitboard(sq ^ 8); // toggling the fourth bit of sq (+/- 8)
    gain[0] = Values[P_wp+PieceShift];
  }

  BitBoard bq = pos.whiteBishop | pos.whiteQueen | pos.blackBishop | pos.blackQueen;
  BitBoard rq = pos.whiteRook   | pos.whiteQueen | pos.blackRook   | pos.blackQueen;
  const BitBoard occCol[2] = { pos.whitePiece, pos.blackPiece };
  const BitBoard piece_occ[5] = { pos.whitePawn | pos.blackPawn, pos.whiteKnight | pos.blackKnight, pos.whiteBishop | pos.blackBishop, pos.whiteRook | pos.blackRook, pos.whiteQueen | pos.blackQueen };

  int cnt = 1;
  BitBoard att = BB::isAttackedBB(pos, sq, pos.c) | BB::isAttackedBB(pos, sq, opponentColor(pos.c)); // initial attack bitboard (both color)
  while(att){
    //std::cout << showBitBoard(att) << std::endl;
    side = opponentColor(side);
    BitBoard side_att = att & occCol[side]; // attack bitboard relative to side
    if (side_att == 0) break; // no more threat
    int pp = 0;
    long long int pb = 0ll;
    for (pp = P_wp; pp <= P_wq; ++pp) if ((pb = side_att & piece_occ[pp-1])) break; // looking for the smaller attaker
    if (!pb) pb = side_att; // in this case thi is the king
    pb = pb & -pb; // get the LSB
    if (display) { unsigned long int i = INVALIDSQUARE; bsf(pb,i); LogIt(logInfo) << "Capture from " << Squares[i]; }
    occ ^= pb; // remove this piece from occ
    if (pp == P_wp || pp == P_wb || pp == P_wq) att |= BB::attack<P_wb>(sq, bq, occ); // new sliders might be behind
    if (pp == P_wr || pp == P_wq) att |= BB::attack<P_wr>(sq, rq, occ); // new sliders might be behind
    att &= occ; 
    gain[cnt] = pv - gain[cnt - 1];
    pv = Values[pp+PieceShift];
    if (isProm && p == P_wp) { pv += pqv; gain[cnt] += pqv; }
    cnt++;
  }
  while (--cnt) if (gain[cnt - 1] > -gain[cnt]) gain[cnt - 1] = -gain[cnt];
  return gain[0];
}

// Static Exchange Evaluation (cutoff version algorithm from stockfish)
bool ThreadContext::SEE(const Position & p, const Move & m, ScoreType threshold) const{
    // Only deal with normal moves
    if (! isCapture(m)) return true;
    const Square from = Move2From(m);
    const Square to   = Move2To(m);
    Piece nextVictim  = p.b[from];
    const Color us    = getColor(p,from);
    ScoreType balance = std::abs(getValue(p,to)) - threshold; // The opponent may be able to recapture so this is the best result we can hope for.
    if (balance < 0) return false;
    balance -= std::abs(Values[nextVictim+PieceShift]); // Now assume the worst possible result: that the opponent can capture our piece for free.
    if (balance >= 0) return true;
    if (getPieceType(p, to) == P_wk) return false; // capture king !
    Position p2 = p;
    if (!apply(p2, m)) return false;
    std::vector<Square> stmAttackers;
    bool endOfSEE = false;
    while (!endOfSEE){
        p2.c = opponentColor(p2.c);
        bool threatsFound = getAttackers(p2, to, stmAttackers);
        p2.c = opponentColor(p2.c);
        if (!threatsFound) break;
        std::sort(stmAttackers.begin(),stmAttackers.end(),SortThreatsFunctor(p2)); ///@todo this costs a lot ...
        bool validThreatFound = false;
        int threatId = 0;
        while (!validThreatFound && threatId < stmAttackers.size()) {
            const Move mm = ToMove(stmAttackers[threatId], to, T_capture); ///@todo prom ????
            nextVictim = p2.b[stmAttackers[threatId]];
            if (std::abs(nextVictim) == P_wk) return false; // capture king !
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
    return us != p2.c; // we break the above loop when stm loses
}

bool sameMove(const Move & a, const Move & b) { return (a & 0x0000FFFF) == (b & 0x0000FFFF);}

struct MoveSorter{

    MoveSorter(const ThreadContext & context, const Position & p, const TT::Entry * e = NULL):context(context),p(p),e(e){ assert(e==0||e->h!=0||e->m==INVALIDMOVE); }

    void computeScore(Move & m)const{
        const MType  t    = Move2Type(m);
        const Square from = Move2From(m);
        const Square to   = Move2To(m);
        ScoreType s = MoveScoring[t];
        if (e && sameMove(e->m,m)) s += 3000;
        else if (sameMove(m,context.killerT.killers[0][p.ply])) s += 290;
        else if (sameMove(m,context.killerT.killers[1][p.ply])) s += 260;
        if (isCapture(t)){
            s += MvvLvaScores[getPieceType(p,to)-1][getPieceType(p,from)-1];
            if ( !context.SEE(p,m,0)) s -= 2000;
            //s += context.SEEVal(p, m);
        }
        else if ( t == T_std){
            s += context.historyT.history[getPieceIndex(p,from)][to];
            const bool isWhite = (p.whitePiece & SquareToBitboard(from)) != 0ull;
            s += PST[getPieceType(p, from) - 1][isWhite ? (to ^ 56) : to] - PST[getPieceType(p, from) - 1][isWhite ? (from ^ 56) : from];
        }
        m = ToMove(from, to, t, s);
    }

    bool operator()(const Move & a, const Move & b)const{ assert( a != INVALIDMOVE); assert( b != INVALIDMOVE); return Move2Score(a) > Move2Score(b); }

    const Position & p;
    const TT::Entry * e;
    const ThreadContext & context;
};

void sort(const ThreadContext & context, std::vector<Move> & moves, const Position & p, const TT::Entry * e = NULL){
    const MoveSorter ms(context,p,e);
    for(auto it = moves.begin() ; it != moves.end() ; ++it){ ms.computeScore(*it); }
    std::sort(moves.begin(),moves.end(),ms);
}

MaterialHash::Terminaison ThreadContext::interorNodeRecognizer(const Position & p, bool withRep, bool isPV)const{
    int count = 0;
    const Hash h = computeHash(p);
    if (withRep) {
        const int limit = isPV ? 3 : 1;
        for (int k = p.ply - 1; k >= 0; --k) {
            if (hashStack[k] == 0) break;
            if (hashStack[k] == h) ++count;
            if (count >= limit) return MaterialHash::Ter_Draw;
        }
    }
    if ( p.fifty >= 100 ) return MaterialHash::Ter_Draw;
    if (p.mat.np == 0 )   return MaterialHash::probeMaterialHashTable(p.mat);
    else { // some pawn are present
        ///@todo ... KPK
    }
    return MaterialHash::Ter_Unknown;
}

//int manhattanDistance(Square sq1, Square sq2) { return std::abs((sq2 >> 3) - (sq1 >> 3)) + std::abs((sq2 & 7) - (sq1 & 7));}
int chebyshevDistance(Square sq1, Square sq2) { return std::max(std::abs((sq2 >> 3) - (sq1 >> 3)) , std::abs((sq2 & 7) - (sq1 & 7))); }

ScoreType eval(const Position & p, float & gp){

    ScoreType sc = 0;
    if (TT::getEvalEntry(computeHash(p), sc, gp)) return sc;

    static ScoreType dummyScore = 0;
    static ScoreType *absValues[7]   = { &dummyScore, &Values  [P_wp + PieceShift], &Values  [P_wn + PieceShift], &Values  [P_wb + PieceShift], &Values  [P_wr + PieceShift], &Values  [P_wq + PieceShift], &Values  [P_wk + PieceShift] };
    static ScoreType *absValuesEG[7] = { &dummyScore, &ValuesEG[P_wp + PieceShift], &ValuesEG[P_wn + PieceShift], &ValuesEG[P_wb + PieceShift], &ValuesEG[P_wr + PieceShift], &ValuesEG[P_wq + PieceShift], &ValuesEG[P_wk + PieceShift] };

    // game phase
    //gp = (p.mat.nn + p.mat.nb + 3.f * p.mat.nr + 6.f * p.mat.nq)/32.f;
    const float totalAbsScore = 2.f * *absValues[P_wq] + 4.f * *absValues[P_wr] + 4.f * *absValues[P_wb] + 4.f * *absValues[P_wn] + 16.f * *absValues[P_wp];
    const float absscore = ((p.mat.nwq + p.mat.nbq) * *absValues[P_wq] + (p.mat.nwr + p.mat.nbr) * *absValues[P_wr] + (p.mat.nwb + p.mat.nbb) * *absValues[P_wb] + (p.mat.nwn + p.mat.nbn) * *absValues[P_wn] + (p.mat.nwp + p.mat.nbp) * *absValues[P_wp]) / totalAbsScore;
    const float pawnScore = p.mat.np / 16.f;
    const float pieceScore = (p.mat.nq + p.mat.nr + p.mat.nb + p.mat.nn) / 14.f;
    gp = (absscore*0.4f + pieceScore * 0.3f + pawnScore * 0.3f);

    // material
    sc += (p.mat.nwk - p.mat.nbk) * ScoreType(gp* *absValues[P_wk] + (1.f - gp)* *absValuesEG[P_wk])
        + (p.mat.nwq - p.mat.nbq) * ScoreType(gp* *absValues[P_wq] + (1.f - gp)* *absValuesEG[P_wq])
        + (p.mat.nwr - p.mat.nbr) * ScoreType(gp* *absValues[P_wr] + (1.f - gp)* *absValuesEG[P_wr])
        + (p.mat.nwb - p.mat.nbb) * ScoreType(gp* *absValues[P_wb] + (1.f - gp)* *absValuesEG[P_wb])
        + (p.mat.nwn - p.mat.nbn) * ScoreType(gp* *absValues[P_wn] + (1.f - gp)* *absValuesEG[P_wn])
        + (p.mat.nwp - p.mat.nbp) * ScoreType(gp* *absValues[P_wp] + (1.f - gp)* *absValuesEG[P_wp]);
    const bool white2Play = p.c == Co_White;
    // pst
    BitBoard pieceBBiterator = p.whitePiece;
    while (pieceBBiterator) {
        const Square k = BB::popBit(pieceBBiterator);
        const Square kk = k^56;
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
    if (gp < 0.25 && p.wk != INVALIDSQUARE && p.bk != INVALIDSQUARE) sc -= (sc>0?+1:-1)*chebyshevDistance(p.wk, p.bk)*15;

    // passer
    ///@todo candidate passed
    ///@todo unstoppable passed (king too far)
    pieceBBiterator = p.whitePawn;
    while (pieceBBiterator) {
        const Square k = BB::popBit(pieceBBiterator);
        const bool passed = (BB::mask[k].passerSpan[Co_White] & p.blackPawn) == 0ull;
        if (passed) {
            const double factorProtected = 1;// +((BB::shiftSouthWest(SquareToBitboard(k)) & p.whitePawn) || (BB::shiftSouthEast(SquareToBitboard(k)) & p.whitePawn)) * protectedPasserFactor;
            const double factorFree = 1;// +((BB::mask[k].passerSpan[Co_White] & p.blackPiece) == 0ull) * freePasserFactor * (1.f - gp);
            const double kingNearBonus = 0;// kingNearPassedPawnEG * (1.f - gp) * (chebyshevDistance(p.bk, k) - chebyshevDistance(p.wk, k));
            const bool unstoppable = false;// (p.nbq + p.nbr + p.nbb + p.nbn == 0)*((chebyshevDistance(p.bk, SQFILE(k) + 56) - (!white2Play)) > std::min(5, chebyshevDistance(SQFILE(k) + 56, k)));
            if (unstoppable) sc += *absValues[P_wq] - *absValues[P_wp];
            else             sc += ScoreType( factorProtected * factorFree * (gp*passerBonus[SQRANK(k)] + (1.f - gp)*passerBonusEG[SQRANK(k)] + kingNearBonus));
        }
    }
    pieceBBiterator = p.blackPawn;
    while (pieceBBiterator) {
        const Square k = BB::popBit(pieceBBiterator);
        const bool passed = (BB::mask[k].passerSpan[Co_Black] & p.whitePawn) == 0ull;
        if (passed) {
            const double factorProtected = 1;// +((BB::shiftNorthWest(SquareToBitboard(k)) & p.blackPawn) || (BB::shiftNorthEast(SquareToBitboard(k)) & p.blackPawn)) * protectedPasserFactor;
            const double factorFree = 1;// +((BB::mask[k].passerSpan[Co_White] & p.whitePiece) == 0ull) * freePasserFactor * (1.f - gp);
            const double kingNearBonus = 0;// kingNearPassedPawnEG * (1.f - gp) * (chebyshevDistance(p.wk, k) - chebyshevDistance(p.bk, k));
            const bool unstoppable = false;// (p.nwq + p.nwr + p.nwb + p.nwn == 0)*((chebyshevDistance(p.wk, SQFILE(k)) - white2Play) > std::min(5, chebyshevDistance(SQFILE(k), k)));
            if (unstoppable) sc -= *absValues[P_wq] - *absValues[P_wp];
            else             sc -= ScoreType( factorProtected * factorFree * (gp*passerBonus[7 - SQRANK(k)] + (1.f - gp)*passerBonusEG[7 - SQRANK(k)] + kingNearBonus));
        }
    }

    /*
    // count pawn per file
    ///@todo use a cache for that !
    const uint64_t nbWPA=countBit(p.whitePawn & fileA);
    const uint64_t nbWPB=countBit(p.whitePawn & fileB);
    const uint64_t nbWPC=countBit(p.whitePawn & fileC);
    const uint64_t nbWPD=countBit(p.whitePawn & fileD);
    const uint64_t nbWPE=countBit(p.whitePawn & fileE);
    const uint64_t nbWPF=countBit(p.whitePawn & fileF);
    const uint64_t nbWPG=countBit(p.whitePawn & fileG);
    const uint64_t nbWPH=countBit(p.whitePawn & fileH);
    const uint64_t nbBPA=countBit(p.blackPawn & fileA);
    const uint64_t nbBPB=countBit(p.blackPawn & fileB);
    const uint64_t nbBPC=countBit(p.blackPawn & fileC);
    const uint64_t nbBPD=countBit(p.blackPawn & fileD);
    const uint64_t nbBPE=countBit(p.blackPawn & fileE);
    const uint64_t nbBPF=countBit(p.blackPawn & fileF);
    const uint64_t nbBPG=countBit(p.blackPawn & fileG);
    const uint64_t nbBPH=countBit(p.blackPawn & fileH);

    // double pawn malus
    sc -= ScoreType((nbWPA>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc -= ScoreType((nbWPB>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc -= ScoreType((nbWPC>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc -= ScoreType((nbWPD>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc -= ScoreType((nbWPE>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc -= ScoreType((nbWPF>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc -= ScoreType((nbWPG>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc -= ScoreType((nbWPH>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc += ScoreType((nbBPA>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc += ScoreType((nbBPB>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc += ScoreType((nbBPC>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc += ScoreType((nbBPD>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc += ScoreType((nbBPE>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc += ScoreType((nbBPF>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc += ScoreType((nbBPG>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));
    sc += ScoreType((nbBPH>>1)*(gp*doublePawnMalus+(1.f - gp)*doublePawnMalusEG));

    // isolated pawn malus
    sc -= ScoreType((        nbWPA&&!nbWPB)*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    sc -= ScoreType((!nbWPA&&nbWPB&&!nbWPC)*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    sc -= ScoreType((!nbWPB&&nbWPC&&!nbWPD)*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    sc -= ScoreType((!nbWPC&&nbWPD&&!nbWPE)*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    sc -= ScoreType((!nbWPD&&nbWPE&&!nbWPF)*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    sc -= ScoreType((!nbWPE&&nbWPF&&!nbWPG)*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    sc -= ScoreType((!nbWPG&&nbWPH        )*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    sc += ScoreType((        nbBPA&&!nbBPB)*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    sc += ScoreType((!nbBPA&&nbBPB&&!nbBPC)*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    sc += ScoreType((!nbBPB&&nbBPC&&!nbBPD)*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    sc += ScoreType((!nbBPC&&nbBPD&&!nbBPE)*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    sc += ScoreType((!nbBPD&&nbBPE&&!nbBPF)*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    sc += ScoreType((!nbBPE&&nbBPF&&!nbBPG)*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    sc += ScoreType((!nbBPG&&nbBPH        )*(gp*isolatedPawnMalus+(1.f - gp)*isolatedPawnMalusEG));
    */
    
    /*
    // blocked piece
    // white
    // bishop blocked by own pawn
    if (    (p.whiteBishop & BBSq_c1)
         && (p.whitePawn   & BBSq_d2)
         && (p.occupancy   & BBSq_d3) ) sc += blockedBishopByPawn;
    if (    (p.whiteBishop & BBSq_f1)
         && (p.whitePawn   & BBSq_e2)
         && (p.occupancy   & BBSq_e3) ) sc += blockedBishopByPawn;

    // trapped knight
    if (     (p.whiteKnight & BBSq_a8)
         && ((p.blackPawn   & BBSq_a7)
         ||  (p.blackPawn   & BBSq_c7) ) ) sc += blockedKnight;
    if (     (p.whiteKnight & BBSq_h8)
         && ((p.blackPawn   & BBSq_h7)
         ||  (p.blackPawn   & BBSq_f7) ) ) sc += blockedKnight;
    if (     (p.whiteKnight & BBSq_a7)
         && ((p.blackPawn   & BBSq_a6)
         ||  (p.blackPawn   & BBSq_b7) ) ) sc += blockedKnight2;
    if (     (p.whiteKnight & BBSq_h7)
         && ((p.blackPawn   & BBSq_h6)
         ||  (p.blackPawn   & BBSq_g7) ) ) sc += blockedKnight2;

    // trapped bishop
    if (    (p.whiteBishop & BBSq_a7)
         && (p.blackPawn   & BBSq_b6) )  sc += blockedBishop;
    if (    (p.whiteBishop & BBSq_h7)
         && (p.blackPawn   & BBSq_g6) )  sc += blockedBishop;
    if (    (p.whiteBishop & BBSq_b8)
         && (p.blackPawn   & BBSq_c7) )  sc += blockedBishop2;
    if (    (p.whiteBishop & BBSq_g8)
         && (p.blackPawn   & BBSq_f7) )  sc += blockedBishop2;
    if (    (p.whiteBishop & BBSq_a6)
         && (p.blackPawn   & BBSq_b5) )  sc += blockedBishop3;
    if (    (p.whiteBishop & BBSq_h6)
         && (p.blackPawn   & BBSq_g5) )  sc += blockedBishop3;

    // bishop near castled king (bonus)
    if (    (p.whiteBishop & BBSq_f1)
         && (p.whiteKing   & BBSq_g1) )  sc += returningBishopBonus;
    if (    (p.whiteBishop & BBSq_c1)
         && (p.whiteKing   & BBSq_b1) )  sc += returningBishopBonus;

    // king blocking rook
    if ( (  (p.whiteKing & BBSq_f1)
         || (p.whiteKing & BBSq_g1) )
         &&((p.whiteRook & BBSq_h1)
         || (p.whiteRook & BBSq_g1) )) sc += blockedRookByKing;
    if ( (  (p.whiteKing & BBSq_c1)
         || (p.whiteKing & BBSq_b1) )
         &&((p.whiteRook & BBSq_a1)
         || (p.whiteRook & BBSq_b1) )) sc += blockedRookByKing;

    // black
    // bishop blocked by own pawn
    if (    (p.blackBishop & BBSq_c8)
         && (p.blackPawn   & BBSq_d7)
         && (p.occupancy   & BBSq_d6) ) sc += blockedBishopByPawn;
    if (    (p.blackBishop & BBSq_f8)
         && (p.blackPawn   & BBSq_e7)
         && (p.occupancy   & BBSq_e6) ) sc += blockedBishopByPawn;

    // trapped knight
    if (     (p.blackKnight & BBSq_a1)
         && ((p.whitePawn   & BBSq_a2)
         ||  (p.whitePawn   & BBSq_c2) ) ) sc += blockedKnight;
    if (     (p.blackKnight & BBSq_h1)
         && ((p.whitePawn   & BBSq_h2)
         ||  (p.whitePawn   & BBSq_f2) ) ) sc += blockedKnight;
    if (     (p.blackKnight & BBSq_a2)
         && ((p.whitePawn   & BBSq_a3)
         ||  (p.whitePawn   & BBSq_b2) ) ) sc += blockedKnight2;
    if (     (p.blackKnight & BBSq_h2)
         && ((p.whitePawn   & BBSq_h3)
         ||  (p.whitePawn   & BBSq_g2) ) ) sc += blockedKnight2;

    // trapped bishop
    if (    (p.blackBishop & BBSq_a2)
         && (p.whitePawn   & BBSq_b3) )  sc += blockedBishop;
    if (    (p.blackBishop & BBSq_h2)
         && (p.whitePawn   & BBSq_g3) )  sc += blockedBishop;
    if (    (p.blackBishop & BBSq_b1)
         && (p.whitePawn   & BBSq_c2) )  sc += blockedBishop2;
    if (    (p.blackBishop & BBSq_g1)
         && (p.whitePawn   & BBSq_f2) )  sc += blockedBishop2;
    if (    (p.blackBishop & BBSq_a3)
         && (p.whitePawn   & BBSq_b4) )  sc += blockedBishop3;
    if (    (p.blackBishop & BBSq_h3)
         && (p.whitePawn   & BBSq_g4) )  sc += blockedBishop3;

    // bishop near castled king (bonus)
    if (    (p.blackBishop & BBSq_f8)
         && (p.blackKing   & BBSq_g8) )  sc += returningBishopBonus;
    if (    (p.blackBishop & BBSq_c8)
         && (p.blackKing   & BBSq_b8) )  sc += returningBishopBonus;

    // king blocking rook
    if ( (  (p.blackKing   & BBSq_f8)
         || (p.blackKing   & BBSq_g8) )
         &&((p.blackRook   & BBSq_h8)
         || (p.blackRook   & BBSq_g8) )) sc += blockedRookByKing;
    if ( (  (p.blackKing   & BBSq_c8)
         || (p.blackKing   & BBSq_b8) )
         &&((p.blackRook   & BBSq_a8)
         || (p.blackRook   & BBSq_b8) )) sc += blockedRookByKing;
    */

    /*
    // number of pawn and piece type
    sc += p.nwr * adjRook  [p.nwp];
    sc -= p.nbr * adjRook  [p.nbp];
    sc += p.nwn * adjKnight[p.nwp];
    sc -= p.nbn * adjKnight[p.nbp];
    // bishop pair
    sc += ( (p.nwb > 1 ? bishopPairBonus : 0)-(p.nbb > 1 ? bishopPairBonus : 0) );
    // knight pair
    sc += ( (p.nwn > 1 ? knightPairMalus : 0)-(p.nbn > 1 ? knightPairMalus : 0) );
    // rook pair
    sc += ( (p.nwr > 1 ? rookPairMalus   : 0)-(p.nbr > 1 ? rookPairMalus   : 0) );
    */
    
    /*
    // king attack
    ScoreType attSc = 0;
    if ( p.wk != INVALIDSQUARE ){
      BitBoard wKingZone = BB::mask[p.wk].kingZone;
      while(wKingZone){
        Square z = BB::popBit(wKingZone);
        BitBoard attack = BB::isAttackedBB(p, z, Co_White);
        while(attack){
           Square from = BB::popBit(attack);
           attSc += getValue(p,from);
        }
      }
    }
    if ( p.bk != INVALIDSQUARE ){
      BitBoard bKingZone = BB::mask[p.bk].kingZone;
      while(bKingZone){
        Square z = BB::popBit(bKingZone);
        BitBoard attack = BB::isAttackedBB(p, z, Co_Black);
        while(attack){
          Square from = BB::popBit(attack);
          attSc += getValue(p,from);
        }
      }
    }
    sc+=attSc/50;
    */

    sc = (white2Play?+1:-1)*sc;
    TT::setEvalEntry({ sc, gp, computeHash(p) });
    return sc;
}

ScoreType ThreadContext::qsearch(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth){
    float gp = 0;
    if ( ply >= MAX_PLY-1 ) return eval(p,gp);
    alpha = std::max(alpha, (ScoreType)(-MATE + ply));
    beta  = std::min(beta , (ScoreType)( MATE - ply + 1));
    if (alpha >= beta) return alpha;

    if ((int)ply > seldepth) seldepth = ply;

    ++stats.qnodes;

    ScoreType val = eval(p,gp);
    MaterialHash::Terminaison drawStatus = interorNodeRecognizer(p, false, false);
    if (drawStatus == MaterialHash::Ter_HardToWin || drawStatus == MaterialHash::Ter_LikelyDraw) val = ScoreType(val/3.f);
    if ( val >= beta ) return val;
    if ( val > alpha) alpha = val;

    //const bool isInCheck = isAttacked(p, kingSquare(p));

    std::vector<Move> moves;
    generate(p,moves,GP_cap);
    sort(*this,moves,p);

    const ScoreType alphaInit = alpha;

    for(auto it = moves.begin() ; it != moves.end() ; ++it){
        if ( StaticConfig::doQFutility /*&& !isInCheck*/ && val + StaticConfig::qfutilityMargin + getAbsValue(p,Move2To(*it)) <= alphaInit) continue;
        //if ( SEEVal(p,*it) < -0 /* && !isCheck*/) continue; // see (prune bad capture)
        if ( Move2Score(*it) < -900 /* && !isCheck*/) continue; // see (from move sorter, SEE<0 add -2000 if bad capture)
        //if ( !SEE(p,*it) /* && !isCheck*/) continue; // see (prune all bad capture)
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

inline void updateHistoryKillers(ThreadContext & context, const Position & p, DepthType depth, const Move m) {
    context.killerT.killers[1][p.ply] = context.killerT.killers[0][p.ply];
    context.killerT.killers[0][p.ply] = m;
    context.historyT.update(depth, m, p, true);
}

inline bool singularExtension(ThreadContext & context, ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, const TT::Entry & e, const Move m, bool rootnode, int ply) {
    if ( depth >= StaticConfig::singularExtensionDepth && sameMove(m, e.m) && !rootnode && !isMateScore(e.score) && e.b == TT::B_beta && e.d >= depth - 3) {
        const ScoreType betaC = e.score - depth;
        std::vector<Move> sePV;
        DepthType seSeldetph;
        const ScoreType score = context.pvs(betaC - 1, betaC, p, depth/2, false, ply, sePV, seSeldetph, m);
        if (!ThreadContext::stopFlag && score < betaC) return true;
    }
    return false;
}

ScoreType adjustHashScore(ScoreType score, DepthType ply){
  if      (isMatingScore(score)) score -= ply;
  else if (isMatedScore (score)) score += ply;
  return score;
}

// pvs inspired by Xiphos
ScoreType ThreadContext::pvs(ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, bool pvnode, unsigned int ply, std::vector<Move> & pv, DepthType & seldepth, const Move skipMove){

    if ( stopFlag || std::max(1,(int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count()) > currentMoveMs ){ stopFlag = true; return STOPSCORE; }

    if ((int)ply > seldepth) seldepth = ply;

    if ( depth <= 0 ) return qsearch(alpha,beta,p,ply,seldepth);

    ++stats.nodes;

    alpha = std::max(alpha, (ScoreType)(-MATE + ply));
    beta  = std::min(beta,  (ScoreType)( MATE - ply + 1));
    if (alpha >= beta) return alpha;

    const bool rootnode = ply == 1;

    float gp = 0;
    MaterialHash::Terminaison drawStatus = interorNodeRecognizer(p, true, pvnode);
    if (!rootnode && drawStatus == MaterialHash::Ter_Draw) return 0;
    if (ply >= MAX_PLY - 1 || depth >= MAX_DEPTH - 1) return eval(p, gp);

    TT::Entry e;
    if (skipMove==INVALIDMOVE && TT::getEntry(computeHash(p), depth, e)) { // if not skipmove
        if ( e.h != 0 && !rootnode && !isMateScore(e.score) && !pvnode && ( (e.b == TT::B_alpha && e.score <= alpha) || (e.b == TT::B_beta  && e.score >= beta) || (e.b == TT::B_exact) ) ) {
            if ( e.m != INVALIDMOVE) pv.push_back(e.m); // here e.m might be INVALIDMOVE if B_alpha
            return adjustHashScore(e.score, ply);
        }
    }

    const bool isInCheck = isAttacked(p, kingSquare(p));
    ScoreType val;
    const bool hashUsable = e.h != 0 && ((e.b == TT::B_alpha && e.score <= alpha) || (e.b == TT::B_beta  && e.score >= beta) || (e.b == TT::B_exact));
    if (isInCheck) val = -MATE + ply;
    else val = hashUsable?e.score:eval(p, gp);
    scoreStack[p.ply] = val;

    bool futility = false, lmp = false, moveGenerated = false;
    std::vector<Move> moves;

    // prunings
    if ( !DynamicConfig::mateFinder && !rootnode && gp > 0.2 && !pvnode && !isInCheck && !isMateScore(alpha) && !isMateScore(beta) ){

        // static null move
        if ( StaticConfig::doStaticNullMove && depth <= StaticConfig::staticNullMoveMaxDepth && val >= beta + StaticConfig::staticNullMoveDepthCoeff *depth ) return val;

        // razoring
        int rAlpha = alpha - StaticConfig::razoringMargin;
        if ( StaticConfig::doRazoring && depth <= StaticConfig::razoringMaxDepth && val <= rAlpha ){
            const ScoreType qval = qsearch(rAlpha,rAlpha+1,p,ply,seldepth);
            if ( ! stopFlag && qval <= alpha ) return qval;
            if ( stopFlag ) return STOPSCORE;
        }

        // null move
        ///@todo use null move to detect mate treats
        if ( StaticConfig::doNullMove && pv.size() > 1 && depth >= StaticConfig::nullMoveMinDepth && p.ep == INVALIDSQUARE && val >= beta){
            Position pN = p;
            pN.c = opponentColor(pN.c);
            pN.h ^= ZT[3][13];
            pN.h ^= ZT[4][13];
            const int R = depth/4 + 3 + std::min((val-beta)/80,3); // adaptative
            std::vector<Move> nullPV;
            const ScoreType nullscore = -pvs(-beta,-beta+1,pN,depth-R,false,ply+1,nullPV,seldepth);
            if ( !stopFlag && nullscore >= beta ) return nullscore;
            if ( stopFlag ) return STOPSCORE;
        }

        // LMP
        if (StaticConfig::doLMP && depth <= StaticConfig::lmpMaxDepth) lmp = true;

        // futility
        if (StaticConfig::doFutility && val <= alpha - StaticConfig::futilityDepthCoeff*depth ) futility = true;

        // ProbCut
        if ( StaticConfig::doProbcut && depth >= StaticConfig::probCutMinDepth ){
          int probCutCount = 0;
          const ScoreType betaPC = beta + StaticConfig::probCutMargin;
          if (!moveGenerated){
             generate(p,moves,GP_cap);
             moveGenerated = true;
             if ( moves.empty() ) return isInCheck?-MATE + ply : 0;
             sort(*this,moves,p,&e);
          }
          for (auto it = moves.begin() ; it != moves.end() && probCutCount < StaticConfig::probCutMaxMoves; ++it){
            if ( e.h != 0 && sameMove(e.m, *it) && (Move2Score(*it) < 100) ) continue; // quiet moves and bad captures
            Position p2 = p;
            if ( ! apply(p2,*it) ) continue;
            ++probCutCount;
            ScoreType scorePC = -qsearch(-betaPC,-betaPC+1,p2,ply+1,seldepth);
            std::vector<Move> pvPC;
            if (!stopFlag && scorePC >= betaPC) scorePC = -pvs(-betaPC,-betaPC+1,p2,depth-StaticConfig::probCutMinDepth+1,pvnode,ply+1,pvPC,seldepth);
            if (!stopFlag && scorePC >= betaPC) return scorePC;
            if (stopFlag) return STOPSCORE;
          }
        }
    }

    // IID
    if ( (e.h == 0 /*|| e.d < depth/3*/) && pvnode && depth >= StaticConfig::iidMinDepth){
        std::vector<Move> iidPV;
        pvs(alpha,beta,p,depth/2,pvnode,ply,iidPV,seldepth);
        if ( !stopFlag) TT::getEntry(computeHash(p), depth, e);
        else return STOPSCORE;
    }

    int validMoveCount = 0;
    bool alphaUpdated = false;
    ScoreType bestScore = -INFSCORE;
    Move bestMove = INVALIDMOVE;

    // try the tt move before move generation (if not skipped move)
    if (e.h != 0 && e.m != INVALIDMOVE && !sameMove(e.m,skipMove)) { // should be the case thanks to iid at pvnode
        Position p2 = p;
        if (apply(p2, e.m)) {
            validMoveCount++;
            std::vector<Move> childPV;
            hashStack[p.ply] = p.h;
            // extensions
            int extension = 0;
            if (isInCheck) ++extension;
            if (!extension && skipMove==INVALIDMOVE && singularExtension(*this,alpha, beta, p, depth, e, e.m, rootnode, ply)) ++extension;
            const ScoreType ttval = -pvs(-beta, -alpha, p2, depth-1+extension, pvnode, ply + 1, childPV, seldepth);
            if (stopFlag) return STOPSCORE;
            bestScore = ttval;
            bestMove = e.m;
            if (ttval > alpha) {
                updatePV(pv, e.m, childPV);
                if (ttval >= beta) {
                    //if (Move2Type(e.m) == T_std && !isInCheck) updateHistoryKillers(*this, p, depth, e.m);
                    if ( skipMove==INVALIDMOVE) TT::setEntry({ e.m,ttval,TT::B_beta,depth,computeHash(p) }); // this can only decrease depth ????
                    return ttval;
                }
                alphaUpdated = true;
                alpha = ttval;
            }
        }
    }

    if (!moveGenerated){
       generate(p,moves);
       moveGenerated = true;
       if ( moves.empty() ) return isInCheck?-MATE + ply : 0;
       sort(*this,moves,p,&e);
    }
    const bool improving = (!isInCheck && ply >= 2 && val >= scoreStack[p.ply - 2]);

    TT::Bound hashBound = TT::B_alpha;

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
        if (isInCheck && depth <= 4) extension = 1; // we are in check (extension)
        //if (depth <=3 && p.lastMove != INVALIDMOVE && Move2Type(p.lastMove) == T_capture && Move2To(*it) == Move2To(p.lastMove)) extension = 1; // recapture extension
        if (validMoveCount == 1 || !StaticConfig::doPVS) val = -pvs(-beta,-alpha,p2,depth-1+extension,pvnode,ply+1,childPV,seldepth);
        else{
            // reductions & prunings
            int reduction = 0;
            const bool isCheck = isAttacked(p2, kingSquare(p2));
            const bool isAdvancedPawnPush = getPieceType(p,Move2From(*it)) == P_wp && (SQRANK(to) > 5 || SQRANK(to) < 2);
            //if (isCheck && Move2Score(*it) > -900) extension = 1; // we give check with a non risky move
            //if (isAdvancedPawnPush && depth < 5)   extension = 1; // a pawn is near promotion
            const bool isPrunable = !isInCheck && !isCheck && !isAdvancedPawnPush && !sameMove(*it, killerT.killers[0][p.ply]) && !sameMove(*it, killerT.killers[1][p.ply]) && !isMateScore(alpha) && !isMateScore(beta);
            const bool isPrunableStd = isPrunable && Move2Type(*it) == T_std;
            const bool isPrunableCap = isPrunable && Move2Type(*it) == T_capture;
            const bool isBadCap = isPrunableCap && Move2Score(*it) < -900;
            //const bool isBadCap = isPrunableCap && !SEE(p2, *it);
            //const bool isBadCap = isPrunableCap && SEEVal(p2, *it) < -100*depth;
            // futility
            if (futility && isPrunableStd) continue;
            // LMP
            if (lmp && isPrunableStd && validMoveCount >= lmpLimit[improving][depth] ) continue;
            // SEE
            if ((futility/*||depth<=4*/) && isBadCap) continue;
            // LMR
            if (StaticConfig::doLMR && !DynamicConfig::mateFinder && (isPrunableStd /*|| isBadCap*/) && depth >= StaticConfig::lmrMinDepth ) reduction = lmrReduction[std::min((int)depth,MAX_DEPTH-1)][std::min(validMoveCount,MAX_DEPTH)];
            if (isPrunableStd && !DynamicConfig::mateFinder && !improving) ++reduction;
            if (pvnode && reduction > 0) --reduction;
            //if (isPrunableStd) reduction -= 2*int(Move2Score(*it) / 250.f); // history reduction/extension
            //if (reduction < 0) reduction = 0;
            //if (reduction >= depth) reduction = depth - 1;
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
        if (stopFlag) return STOPSCORE;
        if ( val > bestScore ){
            bestScore = val;
            bestMove = *it;
            if ( val > alpha ){
                updatePV(pv, *it, childPV);
                alphaUpdated = true;
                alpha = val;
                hashBound = TT::B_exact;
                if ( val >= beta ){
                    if ( Move2Type(*it) == T_std && !isInCheck){
                        updateHistoryKillers(*this, p, depth, *it);
                        for(auto it2 = moves.begin() ; it2 != moves.end() && !sameMove(*it2,*it); ++it2)
                            if ( Move2Type(*it2) == T_std ) historyT.update(depth,*it2,p,false);
                    }
                    hashBound = TT::B_beta;
                    break;
                }
            }
        }
    }

    if ( validMoveCount==0 ) return (isInCheck || skipMove!=INVALIDMOVE)?-MATE + ply : 0;
    if ( skipMove==INVALIDMOVE && alphaUpdated ) TT::setEntry({bestMove,bestScore,hashBound,depth,computeHash(p)});

    return bestScore;
}

std::vector<Move> ThreadContext::search(const Position & p, Move & m, DepthType & d, ScoreType & sc, DepthType & seldepth){
    if ( isMainThread() ){
        LogIt(logInfo) << "Search called" ;
        LogIt(logInfo) << "requested time  " << currentMoveMs ;
        LogIt(logInfo) << "requested depth " << (int) d ;
        stats.init();
        stopFlag = false;
        startLock.store(false);
    }
    else{
        LogIt(logInfo) << "helper thread waiting ... " << id() ;
        while(startLock.load()){;}
        LogIt(logInfo) << "... go for id " << id() ;
    }
    ///@todo do not reset that
    //TT::clearTT(); // to be used for reproductible results
    killerT.initKillers();
    historyT.initHistory();

    TimeMan::startTime = Clock::now();

    DepthType reachedDepth = 0;
    std::vector<Move> pv;
    ScoreType bestScore = 0;
    m = INVALIDMOVE;

    hashStack[p.ply] = p.h;

    static Counter previousNodeCount;
    previousNodeCount = 1;

    const Move bookMove = Book::Get(computeHash(p));
    if ( bookMove != INVALIDMOVE){
        pv .push_back(bookMove);
        m = pv[0];
        d = reachedDepth;
        sc = bestScore;
        return pv;
    }

    for(DepthType depth = 1 ; depth <= std::min(d,DepthType(MAX_DEPTH-6)) && !stopFlag ; ++depth ){ // -6 so that draw can be found for sure
        if (!isMainThread()){ // stockfish like thread  management
            const int i = (id()-1)%20;
            if (((depth + SkipPhase[i]) / SkipSize[i]) % 2) continue;
        }
        std::vector<Move> pvLoc;
        ScoreType delta = (StaticConfig::doWindow && depth>4)?8:MATE; // MATE not INFSCORE in order to enter the loop below once
        ScoreType alpha = std::max(ScoreType(bestScore - delta), ScoreType (-INFSCORE));
        ScoreType beta  = std::min(ScoreType(bestScore + delta), INFSCORE);
        ScoreType score = 0;
        while( delta <= MATE ){
            pvLoc.clear();
            score = pvs(alpha,beta,p,depth,true,1,pvLoc,seldepth);
            if ( stopFlag ) break;
            delta += 2 + delta/2; // from xiphos ...
            if      (score <= alpha) alpha = std::max(ScoreType(score - delta), ScoreType(-MATE) );
            else if (score >= beta ) beta  = std::min(ScoreType(score + delta), ScoreType( MATE) );
            else break;
        }
        if (stopFlag) break;
        pv = pvLoc;
        reachedDepth = depth;
        bestScore    = score;
        const int ms = std::max(1,(int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count());
        if ( isMainThread() ){
            LogIt(logGUI) << int(depth) << " " << bestScore << " " << ms/10 << " " << stats.nodes + stats.qnodes << " "
                          << (int)seldepth << " " << int((stats.nodes + stats.qnodes)/(ms/1000.f)/1000.) << " " << stats.tthits/1000 << "\t"
                          << ToString(pv)  << " " << "EBF: " << float(stats.nodes + stats.qnodes)/previousNodeCount ;
            previousNodeCount = stats.nodes + stats.qnodes;
        }
    }
    if (pv.empty()){
        LogIt(logWarn) << "Empty pv" ;
        m = INVALIDMOVE;
    }
    else m = pv[0];
    d = reachedDepth;
    sc = bestScore;
    return pv;
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
    LogIt(logInfo) << "Init xboard" ;
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
    LogIt(logInfo) << "Receive command : " << command ;
}

bool sideToMoveFromFEN(const std::string & fen) {
    const bool b = readFEN(fen,position);
    stm = position.c == Co_White ? stm_white : stm_black;
    return b;
}

Move thinkUntilTimeUp(){
    LogIt(logInfo) << "Thinking... " ;
    ScoreType score = 0;
    Move m = INVALIDMOVE;
    if ( depth < 0 ) depth = 64;
    LogIt(logInfo) << "depth          " << (int)depth ;
    currentMoveMs = TimeMan::GetNextMSecPerMove(position);
    LogIt(logInfo) << "currentMoveMs  " << currentMoveMs ;
    LogIt(logInfo) << ToString(position) ;
    DepthType seldepth = 0;
    std::vector<Move> pv;
    const ThreadData d = {depth,seldepth/*dummy*/,score/*dummy*/,position,m/*dummy*/,pv/*dummy*/}; // only input coef
    ThreadPool::instance().searchSync(d);
    m = ThreadPool::instance().main().getData().best; // here output results
    LogIt(logInfo) << "...done returning move " << ToString(m) ;
    return m;
}

bool makeMove(Move m,bool disp){
    if ( disp && m != INVALIDMOVE) LogIt(logGUI) << "move " << ToString(m) ;
    LogIt(logInfo) << ToString(position) ;
    return apply(position,m);
}

void ponderUntilInput(){
    LogIt(logInfo) << "Pondering... " ;
    ScoreType score = 0;
    Move m = INVALIDMOVE;
    depth = 64;
    DepthType seldepth = 0;
    std::vector<Move> pv;
    const ThreadData d = {depth,seldepth,score,position,m,pv};
    ThreadPool::instance().searchASync(d);
}

void stop(){ ThreadContext::stopFlag = true; }

void stopPonder(){ if ((int)mode == (int)opponent(stm) && ponder == p_on && pondering) { pondering = false; stop(); } }

void xboard();

} // XBoard

void XBoard::xboard(){
    LogIt(logInfo) << "Starting XBoard main loop" ;
    ///@todo more feature disable !!
    LogIt(logGUI) << "feature ping=1 setboard=1 colors=0 usermove=1 memory=0 sigint=0 sigterm=0 otime=0 time=1 nps=0 myname=\"Minic " << MinicVersion << "\"" ;
    LogIt(logGUI) << "feature done=1" ;

    while(true) {
        LogIt(logInfo) << "XBoard: mode " << mode ;
        LogIt(logInfo) << "XBoard: stm  " << stm ;
        if(mode == m_analyze){
            //AnalyzeUntilInput(); ///@todo
        }
        // move as computer if mode is equal to stm
        if((int)mode == (int)stm) { // mouarfff
            move = thinkUntilTimeUp();
            if(move == INVALIDMOVE){ mode = m_force; }// game ends
            else{
                if ( ! makeMove(move,true) ){
                    LogIt(logInfo) << "Bad computer move !" ;
                    LogIt(logInfo) << ToString(position) ;
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
            else if ( command == "xboard")  LogIt(logInfo) << "This is minic!" ;
            else if ( command == "post")    display = true;
            else if ( command == "nopost")  display = false;
            else if ( command == "computer"){ }
            else if ( strncmp(command.c_str(),"protover",8) == 0){ }
            else if ( strncmp(command.c_str(),"accepted",8) == 0){ }
            else if ( strncmp(command.c_str(),"rejected",8) == 0){ }
            else if ( strncmp(command.c_str(),"ping",4) == 0){
                std::string str(command);
                const size_t p = command.find("ping");
                str = str.substr(p+4);
                str = trim(str);
                LogIt(logGUI) << "pong " << str ;
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
                const size_t p = command.find("usermove");
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
                    if      ( to == (whiteToMove?Sq_c1:Sq_c8)) m = ToMove(from,to,whiteToMove?T_wqs:T_bqs);
                    else if ( to == (whiteToMove?Sq_g1:Sq_g8)) m = ToMove(from,to,whiteToMove?T_wks:T_bks);
                }
                if(!makeMove(m,false)){ // make move
                    commandOK = false;
                    LogIt(logInfo) << "Bad opponent move !" ;
                    LogIt(logInfo) << ToString(position) ;
                    mode = m_force;
                }
                else stm = opponent(stm);
            }
            else if (  strncmp(command.c_str(),"setboard",8) == 0){
                stopPonder();
                stop();
                std::string fen(command);
                const size_t p = command.find("setboard");
                fen = fen.substr(p+8);
                if (!sideToMoveFromFEN(fen)){
                    LogIt(logInfo) << "Illegal FEN" ;
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
                    LogIt(logInfo) << "Error (paused): " << command ;
                    readLine();
                }
            }
            else if( strncmp(command.c_str(), "time",4) == 0) {
                int centisec = 0;
                sscanf(command.c_str(), "time %d", &centisec);
                // just updating remaining time in curren TC (shall be classic TC or sudden death)
                TimeMan::isDynamic        = true;
                //TimeMan::nbMoveInTC       = -1;
                //TimeMan::msecPerMove      = -1;
                //TimeMan::msecInTC         = -1;
                //TimeMan::msecInc          = -1;
                TimeMan::msecUntilNextTC  = centisec*10;
                //depth                     = 64; // infinity
            }
            else if ( strncmp(command.c_str(), "st", 2) == 0) {
                int msecPerMove = 0;
                sscanf(command.c_str(), "st %d", &msecPerMove);
                msecPerMove *= 1000;
                // forced move time
                TimeMan::isDynamic        = false;
                TimeMan::nbMoveInTC       = -1;
                TimeMan::msecPerMove      = msecPerMove;
                TimeMan::msecInTC         = -1;
                TimeMan::msecInc          = -1;
                TimeMan::msecUntilNextTC  = -1;
                depth                     = 64; // infinity
            }
            else if ( strncmp(command.c_str(), "sd", 2) == 0) {
                int d = 0;
                sscanf(command.c_str(), "sd %d", &d);
                depth = d;
                if(depth<0) depth = 8;
                // forced move depth
                TimeMan::isDynamic        = false;
                TimeMan::nbMoveInTC       = -1;
                TimeMan::msecPerMove      = 60*60*1000*24; // 1 day == infinity ...
                TimeMan::msecInTC         = -1;
                TimeMan::msecInc          = -1;
                TimeMan::msecUntilNextTC  = -1;
            }
            else if(strncmp(command.c_str(), "level",5) == 0) {
                int timeTC = 0;
                int secTC = 0;
                int inc = 0;
                int mps = 0;
                if( sscanf(command.c_str(), "level %d %d %d", &mps, &timeTC, &inc) != 3) sscanf(command.c_str(), "level %d %d:%d %d", &mps, &timeTC, &secTC, &inc);
                timeTC *= 60000;
                timeTC += secTC * 1000;
                int msecinc = inc * 1000;
                // classic TC is mps>0, else sudden death
                TimeMan::isDynamic        = false;
                TimeMan::nbMoveInTC       = mps;
                TimeMan::msecPerMove      = -1;
                TimeMan::msecInTC         = timeTC;
                TimeMan::msecInc          = msecinc;
                TimeMan::msecUntilNextTC  = timeTC; // just an init here, with be managed using "time" command later
                depth                     = 64; // infinity
            }
            else if ( command == "edit"){ }
            else if ( command == "?"){ stop(); }
            else if ( command == "draw")  { }
            else if ( command == "undo")  { }
            else if ( command == "remove"){ }
            //************ end of Xboard command ********//
            else LogIt(logInfo) << "Xboard does not know this command " << command ;
        } // readline
    } // while true

}

nlohmann::json json;
std::vector<std::string> args;

void initOptions(int argc, char ** argv) {
    for(int i = 1 ; i < argc ; ++i) args.push_back(argv[i]);
    std::ifstream str("minic.json");
    if (!str.is_open()) LogIt(logError) << "Cannot open minic.json";
    else {
        str >> json;
        if (!json.is_object()) LogIt(logError) << "JSON is not an object";
    }
}

template<typename T> struct OptionValue {};
template<> struct OptionValue<bool> {
    const bool value = false;
    const std::function<bool(const nlohmann::json::reference)> validator = &nlohmann::json::is_boolean;
};
template<> struct OptionValue<int> {
    const int value = 0;
    const std::function<bool(const nlohmann::json::reference)> validator = &nlohmann::json::is_number_integer;
};
template<> struct OptionValue<float> {
    const float value = 0.f;
    const std::function<bool(const nlohmann::json::reference)> validator = &nlohmann::json::is_number_float;
};

// from argv (override json)
template<typename T> T getOptionCLI(bool & found, const std::string & key, T defaultValue = OptionValue<T>().value){
    found = false;
    auto it = std::find(args.begin(),args.end(),std::string("-")+key);
    if (it == args.end()) {
        LogIt(logWarn) << "ARG key not given, " << key;
        return defaultValue;
    }
    std::stringstream str;
    ++it;
    if (it == args.end()) {
        LogIt(logError) << "ARG value not given, " << key;
        return defaultValue;
    }
    str << *it;
    T ret = defaultValue;
    str >> ret;
    LogIt(logInfo) << "From ARG, " << key << " : " << ret;
    found = true;
    return ret;
}

template<typename T> struct Validator{
   Validator():hasMin(false),hasMax(false){}
   Validator & setMin(const T & v){ minValue=v; hasMin=true; return *this;}
   Validator & setMax(const T & v){ maxValue=v; hasMax=true; return *this;}
   bool hasMin,hasMax;
   T minValue, maxValue;
   T get(const T & v)const {return std::min(hasMax?maxValue:v,std::max(hasMin?minValue:v,v));}
};

// from json
template<typename T> T getOption(const std::string & key, T defaultValue = OptionValue<T>().value, const Validator<T> & v = Validator<T>()) {
    bool found = false;
    const T cliValue = getOptionCLI(found,key,defaultValue);
    if ( found ) return v.get(cliValue);
    auto it = json.find(key);
    if (it == json.end()) {
        LogIt(logError) << "JSON key not given, " << key;
        return defaultValue;
    }
    if (!OptionValue<T>().validator(it.value())) {
        LogIt(logError) << "JSON value does not have expected type, " << it.key() << " : " << it.value();
        return defaultValue;
    }
    else {
        LogIt(logInfo) << "From config file, " << it.key() << " : " << it.value();
        return v.get((T)it.value());
    }
}

#define GETOPT(  name,type)                 DynamicConfig::name = getOption<type>(#name);
#define GETOPT_D(name,type,def)             DynamicConfig::name = getOption<type>(#name,def);
#define GETOPT_M(name,type,def,mini,maxi)   DynamicConfig::name = getOption<type>(#name,def,Validator<type>().setMin(mini).setMax(maxi));

#ifdef DEBUG_TOOL
#include "cli.cc"
#endif

#ifdef WITH_TEXEL_TUNING
#include "texelTuning.cc"
#endif

#ifdef IMPORTBOOK
#include "bookGenerationTools.cc"
#endif

#ifdef WITH_TEST_SUITE
#include "testSuite.cc"
#endif

int main(int argc, char ** argv){
    hellooo();
    initOptions(argc,argv);
    initHash();
    GETOPT_D(ttSizeMb,int,128)
    GETOPT_D(ttESizeMb,int,128)
    TT::initTable();
    TT::initETable();
    stats.init();
    initLMR();
    initMvvLva();
    BB::initMask();
    MaterialHash::MaterialHashInitializer::init();
    ThreadPool::instance().setup(getOption<int>("threads",1,Validator<int>().setMin(1).setMax(64)));
    GETOPT(mateFinder, bool)

    //std::cout << MaterialHash::ToString(MaterialHash::MATKQRLKRRNN) << std::endl;

    if ( getOption<bool>("book") && Book::fileExists("book.bin") ) {
        std::ifstream bbook("book.bin",std::ios::in | std::ios::binary);
        Book::readBinaryBook(bbook);
    }

#ifdef WITH_TEST_SUITE
    if ( argc > 1 && test(argv[1])) return 0;
#endif

#ifdef WITH_TEXEL_TUNING
    if ( argc > 1 && std::string(argv[1]) == "-texel" ) TexelTuning("tuning/minic.fens.json");
#endif

#ifdef DEBUG_TOOL
    if ( argc < 2 ) LogIt(logFatal) << "Hint: You can use -xboard command line option to enter xboard mode";
    return cliManagement(argv[1],argc,argv);
#else
    XBoard::init();
    TimeMan::init();
    XBoard::xboard();
    return 0;
#endif
}
