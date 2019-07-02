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
#include <future>
#ifdef _WIN32
#include <stdlib.h>
#include <intrin.h>
typedef uint64_t u_int64_t;
#else
#include <unistd.h>
#endif

#include "json.hpp"

//#define IMPORTBOOK
#define WITH_TEXEL_TUNING
//#define DEBUG_TOOL
//#define WITH_TEST_SUITE
//#define WITH_SYZYGY
//#define WITH_UCI

const std::string MinicVersion = "0.74";

/*
//todo
-Root move ordering
-Candidate and backward
-test more extension
*/

#define STOPSCORE   ScoreType(-20000)
#define INFSCORE    ScoreType(15000)
#define MATE        ScoreType(10000)
#define WIN         ScoreType(6000)
#define INVALIDMOVE    -1
#define INVALIDSQUARE  -1
#define MAX_PLY       512
#define MAX_MOVE      256   // 256 is enough I guess ...
#define MAX_DEPTH     127   // DepthType is a char, do not go above 127
#define MAX_CAP        64   // seems enough for standard position...
#define MAX_HISTORY  1000.f

#define SQFILE(s) ((s)&7)
#define SQRANK(s) ((s)>>3)
#define MakeSquare(f,r) Square(((r)<<3) + (f))
#define VFlip(s) ((s)^Sq_a8)
#define HFlip(s) ((s)^7)
#define SQR(x) ((x)*(x))
#define HSCORE(depth) ScoreType(SQR(std::min((int)depth, 16))*4)

#define TO_STR2(x) #x
#define TO_STR(x) TO_STR2(x)
#define LINE_NAME(prefix) JOIN(prefix,__LINE__)
#define JOIN(symbol1,symbol2) _DO_JOIN(symbol1,symbol2 )
#define _DO_JOIN(symbol1,symbol2) symbol1##symbol2

typedef std::chrono::system_clock Clock;
typedef char DepthType;
typedef int Move;      // invalid if < 0
typedef char Square;   // invalid if < 0
typedef uint64_t Hash; // invalid if == 0
typedef uint64_t Counter;
typedef short int ScoreType;
typedef uint64_t BitBoard;

template < typename T, int sizeT >
struct OptList {
   std::array<T, sizeT> _m;
   typedef typename std::array<T, sizeT>::iterator iterator;
   typedef typename std::array<T, sizeT>::const_iterator const_iterator;
   unsigned char n = 0;
   iterator begin(){return _m.begin();}
   const_iterator begin()const{return _m.begin();}
   iterator end(){return _m.begin()+n;}
   const_iterator end()const{return _m.begin()+n;}
   void clear(){n=0;}
   size_t size(){return n;}
   void push_back(const T & m) { assert(n<sizeT);  _m[n] = m; n++; }
   bool empty(){return n==0;}
   T & operator [](size_t k) { assert(k < sizeT); return _m[k]; }
   const T & operator [](size_t k) const { assert(k < sizeT);  return _m[k]; }
};

typedef OptList<Move,   MAX_MOVE> MoveList;
typedef std::vector<Square> SquareList; //typedef OptList<Square, MAX_CAP>  SquareList; is slower
typedef std::vector<Move> PVList; //struct PVList : public std::vector<Move> { PVList():std::vector<Move>(MAX_DEPTH, INVALIDMOVE) {}; }; is slower

namespace DynamicConfig{
    bool mateFinder        = false;
    bool disableTT         = false;
    unsigned int ttSizeMb  = 128; // here in Mb, will be converted to real size next
    bool fullXboardOutput  = false;
    bool debugMode         = false;
    std::string debugFile  = "minic.debug";
    unsigned int level     = 10;
    bool book              = true;
    std::string bookFile   = "book.bin";
    int threads            = 1;
}

namespace Logging {
    enum COMType { CT_xboard = 0, CT_uci = 1 };
    COMType ct = CT_xboard;
    inline void hellooo() { std::cout << "# This is Minic version " << MinicVersion << std::endl; }
    inline std::string showDate() {
        std::stringstream str;
        auto msecEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch());
        char buffer[64];
        auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::time_point(msecEpoch));
        std::strftime(buffer, 63, "%Y-%m-%d %H:%M:%S", localtime(&tt));
        str << buffer << "-" << std::setw(3) << std::setfill('0') << msecEpoch.count() % 1000;
        return str.str();
    }
    enum LogLevel : unsigned char { logTrace = 0, logDebug = 1, logInfo = 2, logGUI = 3, logWarn = 4, logError = 5, logFatal = 6};
    std::string backtrace() { return "@todo:: backtrace"; } ///@todo find a very simple portable implementation
    class LogIt {
        friend void init();
    public:
        LogIt(LogLevel loglevel) :_level(loglevel) {}

        template <typename T> Logging::LogIt & operator<<(T const & value) { _buffer << value; return *this; }

        ~LogIt() {
            static const std::string _levelNames[7] = { "# Trace ", "# Debug ", "# Info  ", "", "# Warn  ", "# Error ", "# Fatal " };
            std::lock_guard<std::mutex> lock(_mutex);
            if (_level != logGUI) {
                std::cout       << _levelNames[_level] << showDate() << ": " << _buffer.str() << std::endl;
                if (_of) (*_of) << _levelNames[_level] << showDate() << ": " << _buffer.str() << std::endl;
            }
            else {
                std::cout       << _buffer.str() << std::flush << std::endl;
                if (_of) (*_of) << _buffer.str() << std::flush << std::endl;
            }
            if (_level >= logError) std::cout << backtrace() << std::endl;
            if (_level >= logFatal) exit(1);
        }
    private:
        static std::mutex      _mutex;
        std::ostringstream     _buffer;
        LogLevel               _level;
        static std::unique_ptr<std::ofstream> _of;
    };
    void init(){
        if ( DynamicConfig::debugMode ){ if ( DynamicConfig::debugFile.empty()) DynamicConfig::debugFile = "minic.debug"; }
        LogIt::_of = std::unique_ptr<std::ofstream>(new std::ofstream(DynamicConfig::debugFile));
    }
    std::mutex LogIt::_mutex;
    std::unique_ptr<std::ofstream> LogIt::_of;
}

namespace Options { // after Logging
    nlohmann::json json;
    std::vector<std::string> args;
    void readOptions(int argc, char ** argv) {
        for (int i = 1; i < argc; ++i) args.push_back(argv[i]);
        std::ifstream str("minic.json");
        if (!str.is_open()) Logging::LogIt(Logging::logError) << "Cannot open minic.json";
        else {
            str >> json;
            if (!json.is_object()) Logging::LogIt(Logging::logError) << "JSON is not an object";
        }
    }
    // from argv (override json)
    template<typename T> bool getOptionCLI(T & value, const std::string & key) {
        auto it = std::find(args.begin(), args.end(), std::string("-") + key);
        if (it == args.end()) { Logging::LogIt(Logging::logWarn) << "ARG key not given, " << key; return false; }
        std::stringstream str;
        ++it;
        if (it == args.end()) { Logging::LogIt(Logging::logError) << "ARG value not given, " << key; return false; }
        str << *it;
        str >> value;
        Logging::LogIt(Logging::logInfo) << "From ARG, " << key << " : " << value;
        return true;
    }
    // from json
    template<typename T> bool getOption(T & value, const std::string & key) {
        T cliValue;
        if (getOptionCLI(cliValue, key)) return true;
        auto it = json.find(key);
        if (it == json.end()) { Logging::LogIt(Logging::logWarn) << "JSON key not given, " << key; return false; }
        value = it.value();
        Logging::LogIt(Logging::logInfo) << "From config file, " << it.key() << " : " << value;
        return true;
    }
#define GETOPT(name,type) Options::getOption<type>(DynamicConfig::name,#name);
    void initOptions(int argc, char ** argv){
       readOptions(argc,argv);
       GETOPT(debugMode,        bool)
       GETOPT(debugFile,        std::string)
       GETOPT(book,             bool)
       GETOPT(bookFile,         std::string)
       GETOPT(ttSizeMb,         unsigned int)
       GETOPT(threads,          int)
       GETOPT(mateFinder,       bool)
       GETOPT(fullXboardOutput, bool)
       GETOPT(level,            unsigned int)
   }
}

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
const bool doProbcut        = true;
const bool doHistoryPruning = true; ///@todo
// first value if eval score is used, second if hash score is used
const ScoreType qfutilityMargin          [2] = {90, 90};
const int       staticNullMoveMaxDepth   [2] = {6  , 6};
const ScoreType staticNullMoveDepthCoeff [2] = {80 , 80}; // -50*improving
const ScoreType staticNullMoveDepthInit  [2] = {0  , 0};
const ScoreType razoringMarginDepthCoeff [2] = {0  , 0};
const ScoreType razoringMarginDepthInit  [2] = {200, 200};
const int       razoringMaxDepth         [2] = {3  , 3};
const int       nullMoveMinDepth             = 2;
const int       lmpMaxDepth                  = 10;
const int       historyPruningMaxDepth       = 3;
const ScoreType historyPruningThreshold      = 0;
const int       futilityMaxDepth         [2] = {10 , 10};
const ScoreType futilityDepthCoeff       [2] = {160, 160};
const ScoreType futilityDepthInit        [2] = {0  ,0};
const int       iidMinDepth                  = 5;
const int       iidMinDepth2                 = 8;
const int       probCutMinDepth              = 5;
const int       probCutMaxMoves              = 5;
const ScoreType probCutMargin                = 80;
const int       lmrMinDepth                  = 3;
const int       singularExtensionDepth       = 8;

const int lmpLimit[][StaticConfig::lmpMaxDepth + 1] = { { 0, 3, 4, 6, 10, 15, 21, 28, 36, 45, 55 } ,{ 0, 5, 6, 9, 15, 23, 32, 42, 54, 68, 83 } };

int lmrReduction[MAX_DEPTH][MAX_MOVE];
void initLMR() {
    Logging::LogIt(Logging::logInfo) << "Init lmr";
    for (int d = 0; d < MAX_DEPTH; d++) for (int m = 0; m < MAX_MOVE; m++) lmrReduction[d][m] = int(1.0 + log(d) * log(m) * 0.5); //(int)sqrt(float(d) * m / 8.f);
}

int MvvLvaScores[6][6];
void initMvvLva(){
    Logging::LogIt(Logging::logInfo) << "Init mvv-lva" ;
    static const ScoreType IValues[6] = { 1, 2, 3, 5, 9, 20 }; ///@todo try N=B=3 !
    for(int v = 0; v < 6 ; ++v) for(int a = 0; a < 6 ; ++a) MvvLvaScores[v][a] = IValues[v] * 20 - IValues[a];
}

}

namespace EvalConfig {

// from Rofchade (on talkchess)
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

ScoreType   pawnShieldBonus       = 7;

ScoreType   passerBonus[8]        = { 0, -3, -2,-10, 14, 26, 38, 0};
ScoreType   passerBonusEG[8]      = { 0,-10,  6, 21, 52,120,197, 0};
ScoreType   rookBehindPassed      = 25;
ScoreType   kingNearPassedPawnEG  = 10;
ScoreType   doublePawnMalus       = -4;
ScoreType   doublePawnMalusEG     = 26;
ScoreType   isolatedPawnMalus     = 27;
ScoreType   isolatedPawnMalusEG   = 5;
ScoreType   protectedPasserFactor = 8;  // 108%
ScoreType   freePasserFactor      = 37; // 137%

ScoreType   rookOnOpenFile        = 23;
ScoreType   rookOnOpenSemiFileOur = 11;
ScoreType   rookOnOpenSemiFileOpp = -2;

ScoreType   adjKnight[9]  = { -24, -13, -5, -3, -1,  0,  2,  4, 8 };
ScoreType   adjRook[9]    = {  24,   6,  0, -3, -6, -6, -6, -3, 0 };

ScoreType   bishopPairBonus =  38;
ScoreType   knightPairMalus =   7;
ScoreType   rookPairMalus   =  -5;
ScoreType   bishopPairBonusEG =  47;
ScoreType   knightPairMalusEG =  -3;
ScoreType   rookPairMalusEG   = -16;

ScoreType   blockedBishopByPawn  = -24;
ScoreType   blockedKnight        = -150;
ScoreType   blockedKnight2       = -100;
ScoreType   blockedBishop        = -150;
ScoreType   blockedBishop2       = -100;
ScoreType   blockedBishop3       = -50;
ScoreType   returningBishopBonus =  16;
ScoreType   blockedRookByKing    = -22;

ScoreType MOB[6][29] = { {0,0,0,0},
                         {-22,41,49,45,56,50,47,49,46},
                         {-11,5,11,15,12,24,19,21,23,18,21,31,36,38,40},
                         {15,21,26,30,23,30,37,46,45,49,66,65,73,67,44},
                         {-8,1,12,16,29,37,36,36,35,39,40,37,31,35,35,27,32,35,35,38,41,43,46,48,49,50,51,52,53},
                         {20,3,3,-2,-16,-42,-14,-34,3} };

ScoreType MOBEG[6][29] = { {0,0,0,0},
                           {-22,-15,7,19,12,18,20,23,22},
                           {-18,-8,-1,11,25,26,33,42,40,40,39,33,36,38,40},
                           {-20,-4,25,31,56,55,56,59,68,68,68,73,75,83,89},
                           {-19,-18,-16,-14,-12,-10,0,3,6,9,12,15,19,23,26,33,32,39,41,44,42,47,46,48,49,50,51,52,53},
                           {-20,-3,9,35,50,69,57,66,63} };

ScoreType kingAttMax    = 284;
ScoreType kingAttTrans  = 31;
ScoreType kingAttScale  = 13;
ScoreType kingAttOffset = 17;
enum katt_att_def : unsigned char { katt_attack = 0, katt_defence = 1 };
ScoreType kingAttWeight[2][7] = { {0, 2, 7, 6, 2, 9, -1}, {0, 5, 7, 5, 0, -1, 0} };
ScoreType kingAttOpenfile = 11;
ScoreType kingAttSemiOpenfileOur = 5;
ScoreType kingAttSemiOpenfileOpp = 2;
ScoreType kingAttTable[64] = {0};

ScoreType pawnMobility[2] = {4,9};

}

std::string startPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
std::string fine70 = "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1";
std::string shirov = "6r1/2rp1kpp/2qQp3/p3Pp1P/1pP2P2/1P2KP2/P5R1/6R1 w - - 0 1";

enum Piece    : char{ P_bk = -6, P_bq = -5, P_br = -4, P_bb = -3, P_bn = -2, P_bp = -1, P_none = 0, P_wp = 1, P_wn = 2, P_wb = 3, P_wr = 4, P_wq = 5, P_wk = 6 };
Piece operator++(Piece & pp){pp=Piece(pp+1); return pp;}
const int PieceShift = 6;
enum Mat      : unsigned char{ M_t = 0, M_p, M_n, M_b, M_r, M_q, M_k, M_bl, M_bd, M_M, M_m };

ScoreType   Values[13]        = { -8000, -1103, -538, -393, -359, -85, 0, 85, 359, 393, 538, 1103, 8000 };
ScoreType   ValuesEG[13]      = { -8000, -1076, -518, -301, -290, -93, 0, 93, 290, 301, 518, 1076, 8000 };
std::string PieceNames[13]    = { "k", "q", "r", "b", "n", "p", " ", "P", "N", "B", "R", "Q", "K" };

std::string SquareNames[64]   = { "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3", "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6", "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8" };
std::string FileNames[8]      = { "a", "b", "c", "d", "e", "f", "g", "h" };
std::string RankNames[8]      = { "1", "2", "3", "4", "5", "6", "7", "8" };
enum Sq   : unsigned char { Sq_a1  = 0,Sq_b1,Sq_c1,Sq_d1,Sq_e1,Sq_f1,Sq_g1,Sq_h1,Sq_a2,Sq_b2,Sq_c2,Sq_d2,Sq_e2,Sq_f2,Sq_g2,Sq_h2,Sq_a3,Sq_b3,Sq_c3,Sq_d3,Sq_e3,Sq_f3,Sq_g3,Sq_h3,Sq_a4,Sq_b4,Sq_c4,Sq_d4,Sq_e4,Sq_f4,Sq_g4,Sq_h4,Sq_a5,Sq_b5,Sq_c5,Sq_d5,Sq_e5,Sq_f5,Sq_g5,Sq_h5,Sq_a6,Sq_b6,Sq_c6,Sq_d6,Sq_e6,Sq_f6,Sq_g6,Sq_h6,Sq_a7,Sq_b7,Sq_c7,Sq_d7,Sq_e7,Sq_f7,Sq_g7,Sq_h7,Sq_a8,Sq_b8,Sq_c8,Sq_d8,Sq_e8,Sq_f8,Sq_g8,Sq_h8};
enum File : unsigned char { File_a = 0,File_b,File_c,File_d,File_e,File_f,File_g,File_h};
enum Rank : unsigned char { Rank_1 = 0,Rank_2,Rank_3,Rank_4,Rank_5,Rank_6,Rank_7,Rank_8};

enum Castling : unsigned char{ C_none= 0, C_wks = 1, C_wqs = 2, C_bks = 4, C_bqs = 8 };
Castling operator&(const Castling & a, const Castling &b){return Castling(char(a)&char(b));}
Castling operator|(const Castling & a, const Castling &b){return Castling(char(a)|char(b));}
Castling operator~(const Castling & a){return Castling(~char(a));}
void operator&=(Castling & a, const Castling &b){ a = a & b;}
void operator|=(Castling & a, const Castling &b){ a = a | b;}

inline Square stringToSquare(const std::string & str) { return (str.at(1) - 49) * 8 + (str.at(0) - 97); }

enum MType : unsigned char{
    T_std        = 0,   T_capture    = 1,   T_ep         = 2,   T_check      = 3, // T_check not used yet
    T_promq      = 4,   T_promr      = 5,   T_promb      = 6,   T_promn      = 7,
    T_cappromq   = 8,   T_cappromr   = 9,   T_cappromb   = 10,  T_cappromn   = 11,
    T_wks        = 12,  T_wqs        = 13,  T_bks        = 14,  T_bqs        = 15
};

Piece promShift(MType mt){ assert(mt>=T_promq); assert(mt<=T_cappromn); return Piece(P_wq - (mt%4));} // awfull hack

enum Color : char{ Co_None  = -1,   Co_White = 0,   Co_Black = 1,   Co_End };
constexpr Color operator~(Color c){return Color(c^Co_Black);} // switch color
Color operator++(Color & c){c=Color(c+1); return c;}

// ttmove 10000, promcap >7000, cap 7000, checks 6000, killers 1600-1500-1400, counter 1300, castling 2000, other by -1000 < history < 1000, bad cap <-7000.
ScoreType MoveScoring[16] = { 0, 7000, 7100, 6000, 2950, 2500, 2350, 2300, 7950, 7500, 7350, 7300, 20000, 20000, 2000, 2000 };

Color Colors[13] = { Co_Black, Co_Black, Co_Black, Co_Black, Co_Black, Co_Black, Co_None, Co_White, Co_White, Co_White, Co_White, Co_White, Co_White};

#ifdef __MINGW32__
#define POPCOUNT(x)   int(__builtin_popcountll(x))
inline int BitScanForward(BitBoard bb) { assert(bb != 0); return __builtin_ctzll(bb);}
#define bsf(x,i)      (i=BitScanForward(x))
#define swapbits(x)   (__builtin_bswap64 (x))
#define swapbits32(x) (__builtin_bswap32 (x))
#else
#ifdef _WIN32
#ifdef _WIN64
#define POPCOUNT(x)   __popcnt64(x)
#define bsf(x,i)      _BitScanForward64(&i,x)
#define swapbits(x)   (_byteswap_uint64 (x))
#define swapbits32(x) (_byteswap_ulong  (x))
#else // we are _WIN32 but not _WIN64
int popcount(uint64_t b){
    b = (b & 0x5555555555555555LU) + (b >> 1 & 0x5555555555555555LU);
    b = (b & 0x3333333333333333LU) + (b >> 2 & 0x3333333333333333LU);
    b = b + (b >> 4) & 0x0F0F0F0F0F0F0F0FLU;
    b = b + (b >> 8);
    b = b + (b >> 16);
    b = b + (b >> 32) & 0x0000007F;
    return (int)b;
}
const int index64[64] = {
    0,  1, 48,  2, 57, 49, 28,  3,
   61, 58, 50, 42, 38, 29, 17,  4,
   62, 55, 59, 36, 53, 51, 43, 22,
   45, 39, 33, 30, 24, 18, 12,  5,
   63, 47, 56, 27, 60, 41, 37, 16,
   54, 35, 52, 21, 44, 32, 23, 11,
   46, 26, 40, 15, 34, 20, 31, 10,
   25, 14, 19,  9, 13,  8,  7,  6
};
int bitScanForward(int64_t bb) {
    const uint64_t debruijn64 = 0x03f79d71b4cb0a89;
    assert(bb != 0);
    return index64[((bb & -bb) * debruijn64) >> 58];
}
#define POPCOUNT(x)   popcount(x)
#define bsf(x,i)      (i=bitScanForward(x))
#define swapbits(x)   (_byteswap_uint64 (x))
#define swapbits32(x) (_byteswap_ulong  (x))
#endif // _WIN64
#else // linux
#define POPCOUNT(x)   int(__builtin_popcountll(x))
inline int BitScanForward(BitBoard bb) { assert(bb != 0ull); return __builtin_ctzll(bb);}
#define bsf(x,i)      (i=BitScanForward(x))
#define swapbits(x)   (__builtin_bswap64 (x))
#define swapbits32(x) (__builtin_bswap32 (x))
#endif // linux
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

const BitBoard whiteSquare               = 0x55AA55AA55AA55AA; const BitBoard blackSquare               = 0xAA55AA55AA55AA55;
const BitBoard whiteSideSquare           = 0x00000000FFFFFFFF; const BitBoard blackSideSquare           = 0xFFFFFFFF00000000;
const BitBoard whiteKingQueenSide        = 0x0000000000000007; const BitBoard whiteKingKingSide         = 0x00000000000000e0;
const BitBoard blackKingQueenSide        = 0x0700000000000000; const BitBoard blackKingKingSide         = 0xe000000000000000;
const BitBoard whiteQueenSidePawnShield1 = 0x0000000000000700; const BitBoard whiteKingSidePawnShield1  = 0x000000000000e000;
const BitBoard blackQueenSidePawnShield1 = 0x0007000000000000; const BitBoard blackKingSidePawnShield1  = 0x00e0000000000000;
const BitBoard whiteQueenSidePawnShield2 = 0x0000000000070000; const BitBoard whiteKingSidePawnShield2  = 0x0000000000e00000;
const BitBoard blackQueenSidePawnShield2 = 0x0000070000000000; const BitBoard blackKingSidePawnShield2 = 0x0000e00000000000;
const BitBoard fileA                     = 0x0101010101010101;
const BitBoard fileB                     = 0x0202020202020202;
const BitBoard fileC                     = 0x0404040404040404;
const BitBoard fileD                     = 0x0808080808080808;
const BitBoard fileE                     = 0x1010101010101010;
const BitBoard fileF                     = 0x2020202020202020;
const BitBoard fileG                     = 0x4040404040404040;
const BitBoard fileH                     = 0x8080808080808080;
const BitBoard files[8] = {fileA,fileB,fileC,fileD,fileE,fileF,fileG,fileH};
const BitBoard rank1                     = 0x00000000000000ff;
const BitBoard rank2                     = 0x000000000000ff00;
const BitBoard rank3                     = 0x0000000000ff0000;
const BitBoard rank4                     = 0x00000000ff000000;
const BitBoard rank5                     = 0x000000ff00000000;
const BitBoard rank6                     = 0x0000ff0000000000;
const BitBoard rank7                     = 0x00ff000000000000;
const BitBoard rank8                     = 0xff00000000000000;
const BitBoard ranks[8] = {rank1,rank2,rank3,rank4,rank5,rank6,rank7,rank8};
const BitBoard center = BBSq_d4 | BBSq_d5 | BBSq_e4 | BBSq_e5;

std::string showBitBoard(const BitBoard & b) {
    std::bitset<64> bs(b);
    std::stringstream ss;
    for (int j = 7; j >= 0; --j) {
        ss << "\n# +-+-+-+-+-+-+-+-+" << std::endl << "# |";
        for (int i = 0; i < 8; ++i) ss << (bs[i + j * 8] ? "X" : " ") << '|';
    }
    ss << "\n# +-+-+-+-+-+-+-+-+";
    return ss.str();
}

struct Position; // forward decl
bool readFEN(const std::string & fen, Position & p, bool silent = false); // forward decl

struct Position{
    std::array<Piece,64> b {{ P_none }};
    std::array<BitBoard,13> allB {{ 0ull }};

    inline BitBoard & blackKing  () {return allB[0];}
    inline BitBoard & blackQueen () {return allB[1];}
    inline BitBoard & blackRook  () {return allB[2];}
    inline BitBoard & blackBishop() {return allB[3];}
    inline BitBoard & blackKnight() {return allB[4];}
    inline BitBoard & blackPawn  () {return allB[5];}
    inline BitBoard & whitePawn  () {return allB[7];}
    inline BitBoard & whiteKnight() {return allB[8];}
    inline BitBoard & whiteBishop() {return allB[9];}
    inline BitBoard & whiteRook  () {return allB[10];}
    inline BitBoard & whiteQueen () {return allB[11];}
    inline BitBoard & whiteKing  () {return allB[12];}

    template<Piece pp>
    inline BitBoard & pieces(Color c){ return allB[(1-2*c)*pp+PieceShift]; }
    inline BitBoard & pieces(Color c, Piece pp){ return allB[(1-2*c)*pp+PieceShift]; }

    inline const BitBoard & blackKing  ()const {return allB[0];}
    inline const BitBoard & blackQueen ()const {return allB[1];}
    inline const BitBoard & blackRook  ()const {return allB[2];}
    inline const BitBoard & blackBishop()const {return allB[3];}
    inline const BitBoard & blackKnight()const {return allB[4];}
    inline const BitBoard & blackPawn  ()const {return allB[5];}
    inline const BitBoard & whitePawn  ()const {return allB[7];}
    inline const BitBoard & whiteKnight()const {return allB[8];}
    inline const BitBoard & whiteBishop()const {return allB[9];}
    inline const BitBoard & whiteRook  ()const {return allB[10];}
    inline const BitBoard & whiteQueen ()const {return allB[11];}
    inline const BitBoard & whiteKing  ()const {return allB[12];}

    template<Piece pp>
    inline const BitBoard & pieces(Color c)const{ return allB[(1-2*c)*pp+PieceShift]; }
    inline const BitBoard & pieces(Color c, Piece pp)const{ return allB[(1-2*c)*pp+PieceShift]; }

    BitBoard allPieces[2] = {0ull};
    BitBoard occupancy  = 0ull;

    Position(){}
    Position(const std::string & fen){readFEN(fen,*this,true);}

    unsigned char fifty = 0, moves = 0, halfmoves = 0; // this is not the same as "ply", the one used to get seldepth
    Castling castling = C_none;
    Square ep = INVALIDSQUARE, king[2] = { INVALIDSQUARE };
    Color c = Co_White;
    mutable Hash h = 0ull;
    Move lastMove = INVALIDMOVE;

    // t p n b r q k bl bd M n  (total is first so that pawn to king is same a Piece)
    typedef std::array<std::array<char,11>,2> Material;
    Material mat = {{{{0}}}};
};

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
    BitBoard bbsquare, diagonal, antidiagonal, file, kingZone, pawnAttack[2], push[2], dpush[2], enpassant, knight, king, frontSpan[2], rearSpan[2], passerSpan[2], attackFrontSpan[2], between[64];
    Mask():bbsquare(0ull), diagonal(0ull), antidiagonal(0ull), file(0ull), kingZone(0ull), pawnAttack{ 0ull,0ull }, push{ 0ull,0ull }, dpush{ 0ull,0ull }, enpassant(0ull), knight(0ull), king(0ull), frontSpan{0ull}, rearSpan{0ull}, passerSpan{0ull}, attackFrontSpan{0ull}, between{0ull}{}
};
Mask mask[64];

inline void initMask() {
    Logging::LogIt(Logging::logInfo) << "Init mask" ;
    int d[64][64] = { 0 };
    for (Square x = 0; x < 64; ++x) {
        mask[x].bbsquare = SquareToBitboard(x);
        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i == 0 && j == 0) continue;
                for (int r = SQRANK(x) + i, f = SQFILE(x) + j; 0 <= r && r < 8 && 0 <= f && f < 8; r += i, f += j) {
                    const int y = 8 * r + f;
                    d[x][y] = (8 * i + j);
                    for (int z = x + d[x][y]; z != y; z += d[x][y]) mask[x].between[y] |= SquareToBitboard(z);
                }
                const int r = SQRANK(x);
                const int f = SQFILE(x);
                if ( 0 <= r+i && r+i < 8 && 0 <= f+j && f+j < 8) mask[x].kingZone |= SquareToBitboard((SQRANK(x) + i)*8+SQFILE(x) + j);
            }
        }

        for (int y = x - 9; y >= 0 && d[x][y] == -9; y -= 9) mask[x].diagonal |= SquareToBitboard(y);
        for (int y = x + 9; y < 64 && d[x][y] ==  9; y += 9) mask[x].diagonal |= SquareToBitboard(y);

        for (int y = x - 7; y >= 0 && d[x][y] == -7; y -= 7) mask[x].antidiagonal |= SquareToBitboard(y);
        for (int y = x + 7; y < 64 && d[x][y] ==  7; y += 7) mask[x].antidiagonal |= SquareToBitboard(y);

        for (int y = x - 8; y >= 0; y -= 8) mask[x].file |= SquareToBitboard(y);
        for (int y = x + 8; y < 64; y += 8) mask[x].file |= SquareToBitboard(y);

        int f = SQFILE(x);
        int r = SQRANK(x);
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
        wspan |= wspan << 8; wspan |= wspan << 16; wspan |= wspan << 32;
        wspan = shiftNorth(wspan);
        BitBoard bspan = SquareToBitboardTable(x);
        bspan |= bspan >> 8; bspan |= bspan >> 16; bspan |= bspan >> 32;
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
    const int f = SQFILE(x); const int r = x & 56;
    return BitBoard(ranks[((occupancy >> r) & 126) * 4 + f]) << r;
}

inline BitBoard fileAttack(const BitBoard occupancy, const Square x) { return attack(occupancy, x, mask[x].file); }

inline BitBoard diagonalAttack(const BitBoard occupancy, const Square x) { return attack(occupancy, x, mask[x].diagonal); }

inline BitBoard antidiagonalAttack(const BitBoard occupancy, const Square x) { return attack(occupancy, x, mask[x].antidiagonal); }

template < Piece > BitBoard coverage(const Square x, const BitBoard occupancy = 0, const Color c = Co_White) { assert(false); return 0ull; }
template <       > BitBoard coverage<P_wp>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return mask[x].pawnAttack[c]; }
template <       > BitBoard coverage<P_wn>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return mask[x].knight; }
template <       > BitBoard coverage<P_wb>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return diagonalAttack(occupancy, x) | antidiagonalAttack(occupancy, x); }
template <       > BitBoard coverage<P_wr>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return fileAttack(occupancy, x) | rankAttack(occupancy, x); }
template <       > BitBoard coverage<P_wq>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return diagonalAttack(occupancy, x) | antidiagonalAttack(occupancy, x) | fileAttack(occupancy, x) | rankAttack(occupancy, x); }
template <       > BitBoard coverage<P_wk>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return mask[x].king; }

template < Piece pp > inline BitBoard attack(const Square x, const BitBoard target, const BitBoard occupancy = 0, const Color c = Co_White) { return coverage<pp>(x, occupancy, c) & target; }

int popBit(BitBoard & b) {
    assert( b != 0ull);
    unsigned long i = 0;
    bsf(b, i);
    b &= b - 1;
    return i;
}

Square SquareFromBitBoard(const BitBoard & b) {
    assert(b != 0ull);
    unsigned long i = 0;
    bsf(b, i);
    return Square(i);
}

BitBoard isAttackedBB(const Position &p, const Square x, Color c) {
    if (c == Co_White) return attack<P_wb>(x, p.blackBishop() | p.blackQueen(), p.occupancy) | attack<P_wr>(x, p.blackRook() | p.blackQueen(), p.occupancy) | attack<P_wn>(x, p.blackKnight()) | attack<P_wp>(x, p.blackPawn(), p.occupancy, Co_White) | attack<P_wk>(x, p.blackKing());
    else               return attack<P_wb>(x, p.whiteBishop() | p.whiteQueen(), p.occupancy) | attack<P_wr>(x, p.whiteRook() | p.whiteQueen(), p.occupancy) | attack<P_wn>(x, p.whiteKnight()) | attack<P_wp>(x, p.whitePawn(), p.occupancy, Co_Black) | attack<P_wk>(x, p.whiteKing());
}

// generated incrementally to avoid expensive sorting after generation
bool getAttackers(const Position & p, const Square x, SquareList & attakers) {
    attakers.clear();
    if (p.c == Co_White){
        BitBoard att = attack<P_wb>(x, p.blackQueen(), p.occupancy);
        while (att) attakers.push_back(popBit(att));
        att = attack<P_wr>(x, p.blackQueen(), p.occupancy);
        while (att) attakers.push_back(popBit(att));
        att = attack<P_wr>(x, p.blackRook(), p.occupancy);
        while (att) attakers.push_back(popBit(att));
        att = attack<P_wb>(x, p.blackBishop(), p.occupancy);
        while (att) attakers.push_back(popBit(att));
        att = attack<P_wn>(x, p.blackKnight());
        while (att) attakers.push_back(popBit(att));
        att = attack<P_wp>(x, p.blackPawn(), p.occupancy, Co_White);
        while (att) attakers.push_back(popBit(att));
    }
    else{
        BitBoard att = attack<P_wb>(x, p.whiteQueen(), p.occupancy);
        while (att) attakers.push_back(popBit(att));
        att = attack<P_wr>(x, p.whiteQueen(), p.occupancy);
        while (att) attakers.push_back(popBit(att));
        att = attack<P_wr>(x, p.whiteRook(), p.occupancy);
        while (att) attakers.push_back(popBit(att));
        att = attack<P_wb>(x, p.whiteBishop(), p.occupancy);
        while (att) attakers.push_back(popBit(att));
        att = attack<P_wn>(x, p.whiteKnight());
        while (att) attakers.push_back(popBit(att));
        att = attack<P_wp>(x, p.whitePawn(), p.occupancy, Co_Black);
        while (att) attakers.push_back(popBit(att));
    }
    return !attakers.empty();
}

} // BB

inline ScoreType Move2Score(Move h) { assert(h != INVALIDMOVE); return (h >> 16) & 0xFFFF; }
inline Square    Move2From (Move h) { assert(h != INVALIDMOVE); return (h >> 10) & 0x3F  ; }
inline Square    Move2To   (Move h) { assert(h != INVALIDMOVE); return (h >>  4) & 0x3F  ; }
inline MType     Move2Type (Move h) { assert(h != INVALIDMOVE); return MType(h & 0xF)    ; }
inline Move      ToMove(Square from, Square to, MType type)                  { assert(from >= 0 && from < 64); assert(to >= 0 && to < 64); return                 (from << 10) | (to << 4) | type; }
inline Move      ToMove(Square from, Square to, MType type, ScoreType score) { assert(from >= 0 && from < 64); assert(to >= 0 && to < 64); return (score << 16) | (from << 10) | (to << 4) | type; }

inline Piece getPieceIndex  (const Position &p, Square k){ assert(k >= 0 && k < 64); return Piece(p.b[k] + PieceShift);}
inline Piece getPieceType   (const Position &p, Square k){ assert(k >= 0 && k < 64); return (Piece)std::abs(p.b[k]);}
inline std::string getName  (const Position &p, Square k){ assert(k >= 0 && k < 64); return PieceNames[getPieceIndex(p,k)];}
inline ScoreType getValue   (const Position &p, Square k){ assert(k >= 0 && k < 64); return Values[getPieceIndex(p,k)];}
inline ScoreType getAbsValue(const Position &p, Square k){ assert(k >= 0 && k < 64); return std::abs(Values[getPieceIndex(p,k)]); }

inline bool isMatingScore (ScoreType s) { return (s >=  MATE - MAX_DEPTH); }
inline bool isMatedScore  (ScoreType s) { return (s <= -MATE + MAX_DEPTH); }
inline bool isMateScore   (ScoreType s) { return (std::abs(s) >= MATE - MAX_DEPTH); }

inline bool isCapture  (const MType & mt){ return mt == T_capture || mt == T_ep || mt == T_cappromq || mt == T_cappromr || mt == T_cappromb || mt == T_cappromn; }
inline bool isCapture  (const Move & m  ){ return isCapture(Move2Type(m)); }
inline bool isCastling (const MType & mt){ return mt == T_bks || mt == T_bqs || mt == T_wks || mt == T_wqs; }
inline bool isCastling (const Move & m  ){ return isCastling(Move2Type(m)); }
inline bool isPromotion(const MType & mt){ return mt >= T_promq && mt <= T_cappromn;}
inline bool isPromotion(const Move & m  ){ return isPromotion(Move2Type(m));}
inline bool isBadCap   (const Move & m  ){ return Move2Score(m) < -900;}

inline int chebyshevDistance(Square sq1, Square sq2) { return std::max(std::abs(SQRANK(sq2) - SQRANK(sq1)) , std::abs(SQFILE(sq2) - SQFILE(sq1))); }

std::string ToString(const Move & m    , bool withScore = false);
std::string ToString(const Position & p, bool noEval = false);
std::string ToString(const Position::Material & mat);

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

    inline Hash getMaterialHash(const Position::Material & mat) {
        if (mat[Co_White][M_q] > 2 || mat[Co_Black][M_q] > 2 || mat[Co_White][M_r] > 2 || mat[Co_Black][M_r] > 2 || mat[Co_White][M_bl] > 1 || mat[Co_Black][M_bl] > 1 || mat[Co_White][M_bd] > 1 || mat[Co_Black][M_bd] > 1 || mat[Co_White][M_n] > 2 || mat[Co_Black][M_n] > 2 || mat[Co_White][M_p] > 8 || mat[Co_Black][M_p] > 8) return 0;
        return mat[Co_White][M_p] * MatWP + mat[Co_Black][M_p] * MatBP + mat[Co_White][M_n] * MatWN + mat[Co_Black][M_n] * MatBN + mat[Co_White][M_bl] * MatWL + mat[Co_Black][M_bl] * MatBL + mat[Co_White][M_bd] * MatWD + mat[Co_Black][M_bd] * MatBD + mat[Co_White][M_r] * MatWR + mat[Co_Black][M_r] * MatBR + mat[Co_White][M_q] * MatWQ + mat[Co_Black][M_q] * MatBQ;
    }

    inline Position::Material getMatReverseColor(const Position::Material & mat) {
        Position::Material rev = {0};
        rev[Co_White][M_k]  = mat[Co_Black][M_k];   rev[Co_Black][M_k]  = mat[Co_White][M_k];
        rev[Co_White][M_q]  = mat[Co_Black][M_q];   rev[Co_Black][M_q]  = mat[Co_White][M_q];
        rev[Co_White][M_r]  = mat[Co_Black][M_r];   rev[Co_Black][M_r]  = mat[Co_White][M_r];
        rev[Co_White][M_b]  = mat[Co_Black][M_b];   rev[Co_Black][M_b]  = mat[Co_White][M_b];
        rev[Co_White][M_bl] = mat[Co_Black][M_bl];  rev[Co_Black][M_bl] = mat[Co_White][M_bl];
        rev[Co_White][M_bd] = mat[Co_Black][M_bd];  rev[Co_Black][M_bd] = mat[Co_White][M_bd];
        rev[Co_White][M_n]  = mat[Co_Black][M_n];   rev[Co_Black][M_n]  = mat[Co_White][M_n];
        rev[Co_White][M_p]  = mat[Co_Black][M_p];   rev[Co_Black][M_p]  = mat[Co_White][M_p];
        rev[Co_White][M_M]  = mat[Co_Black][M_M];   rev[Co_Black][M_M]  = mat[Co_White][M_M];
        rev[Co_White][M_m]  = mat[Co_Black][M_m];   rev[Co_Black][M_m]  = mat[Co_White][M_m];
        rev[Co_White][M_t]  = mat[Co_Black][M_t];   rev[Co_Black][M_t]  = mat[Co_White][M_t];
        return rev;
    }

    Position::Material materialFromString(const std::string & strMat) {
        Position::Material mat = {0};
        Color c = Co_Black;
        for (auto it = strMat.begin(); it != strMat.end(); ++it) {
            switch (*it) {
            case 'K': c = ~c;             mat[c][M_k] += 1;   break;
            case 'Q': mat[c][M_q] += 1;                       mat[c][M_M] += 1;  mat[c][M_t] += 1;  break;
            case 'R': mat[c][M_r] += 1;                       mat[c][M_M] += 1;  mat[c][M_t] += 1;  break;
            case 'L': mat[c][M_bl]+= 1;   mat[c][M_b] += 1;   mat[c][M_m] += 1;  mat[c][M_t] += 1;  break;
            case 'D': mat[c][M_bd]+= 1;   mat[c][M_b] += 1;   mat[c][M_m] += 1;  mat[c][M_t] += 1;  break;
            case 'N': mat[c][M_n] += 1;                       mat[c][M_m] += 1;  mat[c][M_t] += 1;  break;
            case 'P': mat[c][M_p] += 1;   break;
            default:  Logging::LogIt(Logging::logFatal) << "Bad char in material definition";
            }
        }
        return mat;
    }

    enum Terminaison : unsigned char { Ter_Unknown = 0, Ter_WhiteWinWithHelper, Ter_WhiteWin, Ter_BlackWinWithHelper, Ter_BlackWin, Ter_Draw, Ter_MaterialDraw, Ter_LikelyDraw, Ter_HardToWin };

    inline Terminaison reverseTerminaison(Terminaison t) {
        switch (t) {
        case Ter_Unknown: case Ter_Draw: case Ter_MaterialDraw: case Ter_LikelyDraw: case Ter_HardToWin: return t;
        case Ter_WhiteWin:             return Ter_BlackWin;
        case Ter_WhiteWinWithHelper:   return Ter_BlackWinWithHelper;
        case Ter_BlackWin:             return Ter_WhiteWin;
        case Ter_BlackWinWithHelper:   return Ter_WhiteWinWithHelper;
        default:                       return Ter_Unknown;
        }
    }

    const int pushToEdges[64] = {
      100, 90, 80, 70, 70, 80, 90, 100,
       90, 70, 60, 50, 50, 60, 70,  90,
       80, 60, 40, 30, 30, 40, 60,  80,
       70, 50, 30, 20, 20, 30, 50,  70,
       70, 50, 30, 20, 20, 30, 50,  70,
       80, 60, 40, 30, 30, 40, 60,  80,
       90, 70, 60, 50, 50, 60, 70,  90,
      100, 90, 80, 70, 70, 80, 90, 100
    };

    const int pushToCorners[64] = {
      200, 190, 180, 170, 160, 150, 140, 130,
      190, 180, 170, 160, 150, 140, 130, 140,
      180, 170, 155, 140, 140, 125, 140, 150,
      170, 160, 140, 120, 110, 140, 150, 160,
      160, 150, 140, 110, 120, 140, 160, 170,
      150, 140, 125, 140, 140, 155, 170, 180,
      140, 130, 140, 150, 160, 170, 180, 190,
      130, 140, 150, 160, 170, 180, 190, 200
    };

    const int pushClose[8] = { 0, 0, 100, 80, 60, 40, 20,  10 };
    const int pushAway [8] = { 0, 5,  20, 40, 60, 80, 90, 100 };

    ScoreType helperKXK(const Position &p, Color winningSide, ScoreType s){
        if (p.c != winningSide ){ // stale mate detection for losing side
           ///@todo
        }
        const Square winningK = p.king[winningSide];
        const Square losingK  = p.king[~winningSide];
        const ScoreType sc = pushToEdges[losingK] + pushClose[chebyshevDistance(winningK,losingK)];
        return s + ((winningSide == Co_White)?(sc+WIN):(-sc-WIN));
    }
    ScoreType helperKmmK(const Position &p, Color winningSide, ScoreType s){
        Square winningK = p.king[winningSide];
        Square losingK  = p.king[~winningSide];
        if ( ((p.whiteBishop()|p.blackBishop()) & whiteSquare) != 0 ){
            winningK = VFlip(winningK);
            losingK  = VFlip(losingK);
        }
        const ScoreType sc = pushToCorners[losingK] + pushClose[chebyshevDistance(winningK,losingK)];
        return s + ((winningSide == Co_White)?(sc+WIN):(-sc-WIN));
    }
    ScoreType helperDummy(const Position &, Color , ScoreType){ return 0; }

    // idea taken from Stockfish (or public-domain KPK from HGM)
    namespace KPK{

    Square normalizeSquare(const Position& p, Color strongSide, Square sq) {
       assert(countBit(p.pieces<P_wp>(strongSide)) == 1); // only for KPK !
       if (SQFILE(BB::SquareFromBitBoard(p.pieces<P_wp>(strongSide))) >= File_e) sq = Square(HFlip(sq));
       return strongSide == Co_White ? sq : VFlip(sq);
    }

    constexpr unsigned maxIndex = 2*24*64*64; // color x pawn x wk x bk
    uint32_t KPKBitbase[maxIndex/32]; // force 32bit uint
    unsigned index(Color us, Square bksq, Square wksq, Square psq) { return wksq | (bksq << 6) | (us << 12) | (SQFILE(psq) << 13) | ((6 - SQRANK(psq)) << 15);}
    enum kpk_result : unsigned char { kpk_invalid = 0, kpk_unknown = 1, kpk_draw = 2, kpk_win = 4};
    kpk_result& operator|=(kpk_result& r, kpk_result v) { return r = kpk_result(r | v); }

    struct KPKPosition {
        KPKPosition() = default;
        explicit KPKPosition(unsigned idx){ // first init
            ksq[Co_White] = Square( idx & 0x3F); ksq[Co_Black] = Square((idx >> 6) & 0x3F); us = Color ((idx >> 12) & 0x01);  psq = MakeSquare(File((idx >> 13) & 0x3), Rank(6 - ((idx >> 15) & 0x7)));
            if ( chebyshevDistance(ksq[Co_White], ksq[Co_Black]) <= 1 || ksq[Co_White] == psq || ksq[Co_Black] == psq || (us == Co_White && (BB::mask[psq].pawnAttack[Co_White] & SquareToBitboard(ksq[Co_Black])))) result = kpk_invalid;
            else if ( us == Co_White && SQRANK(psq) == 6 && ksq[us] != psq + 8 && ( chebyshevDistance(ksq[~us], psq + 8) > 1 || (BB::mask[ksq[us]].king & SquareToBitboard(psq + 8)))) result = kpk_win;
            else if ( us == Co_Black && ( !(BB::mask[ksq[us]].king & ~(BB::mask[ksq[~us]].king | BB::mask[psq].pawnAttack[~us])) || (BB::mask[ksq[us]].king & SquareToBitboard(psq) & ~BB::mask[ksq[~us]].king))) result = kpk_draw;
            else result = kpk_unknown; // done later
        }
        operator kpk_result() const { return result; }
        kpk_result preCompute(const std::vector<KPKPosition>& db) { return us == Co_White ? preCompute<Co_White>(db) : preCompute<Co_Black>(db); }
        template<Color Us> kpk_result preCompute(const std::vector<KPKPosition>& db) {
            constexpr Color Them = (Us == Co_White ? Co_Black : Co_White);
            constexpr kpk_result good = (Us == Co_White ? kpk_win  : kpk_draw);
            constexpr kpk_result bad  = (Us == Co_White ? kpk_draw : kpk_win);
            kpk_result r = kpk_invalid;
            BitBoard b = BB::mask[ksq[us]].king;
            while (b){ r |= (Us == Co_White ? db[index(Them, ksq[Them], BB::popBit(b), psq)] : db[index(Them, BB::popBit(b), ksq[Them], psq)]); }
            if (Us == Co_White){
                if (SQRANK(psq) < 6) r |= db[index(Them, ksq[Them], ksq[Us], psq + 8)];
                if (SQRANK(psq) == 1 && psq + 8 != ksq[Us] && psq + 8 != ksq[Them]) r |= db[index(Them, ksq[Them], ksq[Us], psq + 8 + 8)];
            }
            return result = r & good ? good : r & kpk_unknown ? kpk_unknown : bad;
        }
        Color us;
        Square ksq[2], psq;
        kpk_result result;
    };

    bool probe(Square wksq, Square wpsq, Square bksq, Color us) {
        assert(SQFILE(wpsq) <= 4);
        const unsigned idx = index(us, bksq, wksq, wpsq);
        assert(idx >= 0);
        assert(idx < maxIndex);
        return KPKBitbase[idx/32] & (1<<(idx&0x1F));
    }

    void init(){
        Logging::LogIt(Logging::logInfo) << "KPK init";
        std::vector<KPKPosition> db(maxIndex);
        unsigned idx, repeat = 1;
        for (idx = 0; idx < maxIndex; ++idx) db[idx] = KPKPosition(idx); // init
        while (repeat) for (repeat = idx = 0; idx < maxIndex; ++idx) repeat |= (db[idx] == kpk_unknown && db[idx].preCompute(db) != kpk_unknown); // loop
        for (idx = 0; idx < maxIndex; ++idx){ if (db[idx] == kpk_win) { KPKBitbase[idx / 32] |= 1 << (idx & 0x1F); } } // compress
    }

    } // KPK

    ScoreType helperKPK(const Position &p, Color winningSide, ScoreType ){
       ///@todo maybe an incrementally updated piece square list can be cool to avoid those SquareFromBitBoard...
       const Square psq = KPK::normalizeSquare(p, winningSide, BB::SquareFromBitBoard(p.pieces<P_wp>(winningSide)));
       if (!KPK::probe(KPK::normalizeSquare(p, winningSide, BB::SquareFromBitBoard(p.pieces<P_wk>(winningSide))), psq, KPK::normalizeSquare(p, winningSide, BB::SquareFromBitBoard(p.pieces<P_wk>(~winningSide))), winningSide == p.c ? Co_White:Co_Black)) return 0;
       return ((winningSide == Co_White)?+1:-1)*(WIN + ValuesEG[P_wp+PieceShift] + 10*SQRANK(psq));
    }

    ScoreType (* helperTable[TotalMat])(const Position &, Color, ScoreType );
    Terminaison materialHashTable[TotalMat];

    struct MaterialHashInitializer {
        MaterialHashInitializer(const Position::Material & mat, Terminaison t) { materialHashTable[getMaterialHash(mat)] = t; }
        MaterialHashInitializer(const Position::Material & mat, Terminaison t, ScoreType (*helper)(const Position &, Color, ScoreType) ) { materialHashTable[getMaterialHash(mat)] = t; helperTable[getMaterialHash(mat)] = helper; }
        static void init() {
            Logging::LogIt(Logging::logInfo) << "Material hash total : " << TotalMat;
            std::memset(materialHashTable, Ter_Unknown, sizeof(Terminaison)*TotalMat);
            for(size_t k = 0 ; k < TotalMat ; ++k) helperTable[k] = &helperDummy;
#define DEF_MAT(x,t)     const Position::Material MAT##x = materialFromString(TO_STR(x)); MaterialHashInitializer LINE_NAME(dummyMaterialInitializer)( MAT##x ,t   );
#define DEF_MAT_H(x,t,h) const Position::Material MAT##x = materialFromString(TO_STR(x)); MaterialHashInitializer LINE_NAME(dummyMaterialInitializer)( MAT##x ,t, h);
#define DEF_MAT_REV(rev,x)     const Position::Material MAT##rev = MaterialHash::getMatReverseColor(MAT##x); MaterialHashInitializer LINE_NAME(dummyMaterialInitializerRev)( MAT##rev,reverseTerminaison(materialHashTable[getMaterialHash(MAT##x)])   );
#define DEF_MAT_REV_H(rev,x,h) const Position::Material MAT##rev = MaterialHash::getMatReverseColor(MAT##x); MaterialHashInitializer LINE_NAME(dummyMaterialInitializerRev)( MAT##rev,reverseTerminaison(materialHashTable[getMaterialHash(MAT##x)]), h);

            ///@todo some Ter_MaterialDraw are Ter_Draw (FIDE)

            // other FIDE draw
            DEF_MAT(KLKL,   Ter_MaterialDraw)
            DEF_MAT(KDKD,   Ter_MaterialDraw)

            // sym (and pseudo sym) : all should be draw (or very nearly)
            DEF_MAT(KK,     Ter_MaterialDraw)
            DEF_MAT(KQQKQQ, Ter_MaterialDraw)
            DEF_MAT(KQKQ,   Ter_MaterialDraw)
            DEF_MAT(KRRKRR, Ter_MaterialDraw)
            DEF_MAT(KRKR,   Ter_MaterialDraw)
            DEF_MAT(KLDKLD, Ter_MaterialDraw)
            DEF_MAT(KLLKLL, Ter_MaterialDraw)
            DEF_MAT(KDDKDD, Ter_MaterialDraw)
            DEF_MAT(KLDKLL, Ter_MaterialDraw)        DEF_MAT_REV(KLLKLD, KLDKLL)
            DEF_MAT(KLDKDD, Ter_MaterialDraw)        DEF_MAT_REV(KDDKLD, KLDKDD)
            DEF_MAT(KLKD,   Ter_MaterialDraw)        DEF_MAT_REV(KDKL,   KLKD)
            DEF_MAT(KNNKNN, Ter_MaterialDraw)
            DEF_MAT(KNKN,   Ter_MaterialDraw)

            // 2M M
            DEF_MAT(KQQKQ, Ter_WhiteWin)             DEF_MAT_REV(KQKQQ, KQQKQ)
            DEF_MAT(KQQKR, Ter_WhiteWin)             DEF_MAT_REV(KRKQQ, KQQKR)
            DEF_MAT(KRRKQ, Ter_LikelyDraw)           DEF_MAT_REV(KQKRR, KRRKQ)
            DEF_MAT(KRRKR, Ter_WhiteWin)             DEF_MAT_REV(KRKRR, KRRKR)
            DEF_MAT(KQRKQ, Ter_WhiteWin)             DEF_MAT_REV(KQKQR, KQRKQ)
            DEF_MAT(KQRKR, Ter_WhiteWin)             DEF_MAT_REV(KRKQR, KQRKR)

            // 2M m
            DEF_MAT(KQQKL, Ter_WhiteWin)             DEF_MAT_REV(KLKQQ, KQQKL)
            DEF_MAT(KRRKL, Ter_WhiteWin)             DEF_MAT_REV(KLKRR, KRRKL)
            DEF_MAT(KQRKL, Ter_WhiteWin)             DEF_MAT_REV(KLKQR, KQRKL)
            DEF_MAT(KQQKD, Ter_WhiteWin)             DEF_MAT_REV(KDKQQ, KQQKD)
            DEF_MAT(KRRKD, Ter_WhiteWin)             DEF_MAT_REV(KDKRR, KRRKD)
            DEF_MAT(KQRKD, Ter_WhiteWin)             DEF_MAT_REV(KDKQR, KQRKD)
            DEF_MAT(KQQKN, Ter_WhiteWin)             DEF_MAT_REV(KNKQQ, KQQKN)
            DEF_MAT(KRRKN, Ter_WhiteWin)             DEF_MAT_REV(KNKRR, KRRKN)
            DEF_MAT(KQRKN, Ter_WhiteWin)             DEF_MAT_REV(KNKQR, KQRKN)

            // 2m M
            DEF_MAT(KLDKQ, Ter_MaterialDraw)         DEF_MAT_REV(KQKLD,KLDKQ)
            DEF_MAT(KLDKR, Ter_MaterialDraw)         DEF_MAT_REV(KRKLD,KLDKR)
            DEF_MAT(KLLKQ, Ter_MaterialDraw)         DEF_MAT_REV(KQKLL,KLLKQ)
            DEF_MAT(KLLKR, Ter_MaterialDraw)         DEF_MAT_REV(KRKLL,KLLKR)
            DEF_MAT(KDDKQ, Ter_MaterialDraw)         DEF_MAT_REV(KQKDD,KDDKQ)
            DEF_MAT(KDDKR, Ter_MaterialDraw)         DEF_MAT_REV(KRKDD,KDDKR)
            DEF_MAT(KNNKQ, Ter_MaterialDraw)         DEF_MAT_REV(KQKNN,KNNKQ)
            DEF_MAT(KNNKR, Ter_MaterialDraw)         DEF_MAT_REV(KRKNN,KNNKR)
            DEF_MAT(KLNKQ, Ter_MaterialDraw)         DEF_MAT_REV(KQKLN,KLNKQ)
            DEF_MAT(KLNKR, Ter_MaterialDraw)         DEF_MAT_REV(KRKLN,KLNKR)
            DEF_MAT(KDNKQ, Ter_MaterialDraw)         DEF_MAT_REV(KQKDN,KDNKQ)
            DEF_MAT(KDNKR, Ter_MaterialDraw)         DEF_MAT_REV(KRKDN,KDNKR)

            // 2m m : all draw
            DEF_MAT(KLDKL, Ter_MaterialDraw)         DEF_MAT_REV(KLKLD,KLDKL)
            DEF_MAT(KLDKD, Ter_MaterialDraw)         DEF_MAT_REV(KDKLD,KLDKD)
            DEF_MAT(KLDKN, Ter_MaterialDraw)         DEF_MAT_REV(KNKLD,KLDKN)
            DEF_MAT(KLLKL, Ter_MaterialDraw)         DEF_MAT_REV(KLKLL,KLLKL)
            DEF_MAT(KLLKD, Ter_MaterialDraw)         DEF_MAT_REV(KDKLL,KLLKD)
            DEF_MAT(KLLKN, Ter_MaterialDraw)         DEF_MAT_REV(KNKLL,KLLKN)
            DEF_MAT(KDDKL, Ter_MaterialDraw)         DEF_MAT_REV(KLKDD,KDDKL)
            DEF_MAT(KDDKD, Ter_MaterialDraw)         DEF_MAT_REV(KDKDD,KDDKD)
            DEF_MAT(KDDKN, Ter_MaterialDraw)         DEF_MAT_REV(KNKDD,KDDKN)
            DEF_MAT(KNNKL, Ter_MaterialDraw)         DEF_MAT_REV(KLKNN,KNNKL)
            DEF_MAT(KNNKD, Ter_MaterialDraw)         DEF_MAT_REV(KDKNN,KNNKD)
            DEF_MAT(KNNKN, Ter_MaterialDraw)         DEF_MAT_REV(KNKNN,KNNKN)
            DEF_MAT(KLNKL, Ter_MaterialDraw)         DEF_MAT_REV(KLKLN,KLNKL)
            DEF_MAT(KLNKD, Ter_MaterialDraw)         DEF_MAT_REV(KDKLN,KLNKD)
            DEF_MAT(KLNKN, Ter_MaterialDraw)         DEF_MAT_REV(KNKLN,KLNKN)
            DEF_MAT(KDNKL, Ter_MaterialDraw)         DEF_MAT_REV(KLKDN,KDNKL)
            DEF_MAT(KDNKD, Ter_MaterialDraw)         DEF_MAT_REV(KDKDN,KDNKD)
            DEF_MAT(KDNKN, Ter_MaterialDraw)         DEF_MAT_REV(KNKDN,KDNKN)

            // Q x : all should be win
            DEF_MAT(KQKR, Ter_WhiteWin)              DEF_MAT_REV(KRKQ,KQKR)
            DEF_MAT(KQKL, Ter_WhiteWin)              DEF_MAT_REV(KLKQ,KQKL)
            DEF_MAT(KQKD, Ter_WhiteWin)              DEF_MAT_REV(KDKQ,KQKD)
            DEF_MAT(KQKN, Ter_WhiteWin)              DEF_MAT_REV(KNKQ,KQKN)

            // R x : all should be draw
            DEF_MAT(KRKL, Ter_LikelyDraw)            DEF_MAT_REV(KLKR,KRKL)
            DEF_MAT(KRKD, Ter_LikelyDraw)            DEF_MAT_REV(KDKR,KRKD)
            DEF_MAT(KRKN, Ter_LikelyDraw)            DEF_MAT_REV(KNKR,KRKN)

            // B x : all are draw
            DEF_MAT(KLKN, Ter_MaterialDraw)          DEF_MAT_REV(KNKL,KLKN)
            DEF_MAT(KDKN, Ter_MaterialDraw)          DEF_MAT_REV(KNKD,KDKN)

            // X 0 : QR win, BN draw
            DEF_MAT_H(KQK, Ter_WhiteWinWithHelper,&helperKXK)   DEF_MAT_REV_H(KKQ,KQK,&helperKXK)
            DEF_MAT_H(KRK, Ter_WhiteWinWithHelper,&helperKXK)   DEF_MAT_REV_H(KKR,KRK,&helperKXK)
            DEF_MAT(KLK, Ter_MaterialDraw)                      DEF_MAT_REV(KKL,KLK)
            DEF_MAT(KDK, Ter_MaterialDraw)                      DEF_MAT_REV(KKD,KDK)
            DEF_MAT(KNK, Ter_MaterialDraw)                      DEF_MAT_REV(KKN,KNK)

            // 2X 0 : all win except LL, DD, NN 
            DEF_MAT(KQQK, Ter_WhiteWin)                         DEF_MAT_REV(KKQQ,KQQK)
            DEF_MAT(KRRK, Ter_WhiteWin)                         DEF_MAT_REV(KKRR,KRRK)
            DEF_MAT_H(KLDK, Ter_WhiteWinWithHelper,&helperKmmK) DEF_MAT_REV_H(KKLD,KLDK,&helperKmmK)
            DEF_MAT(KLLK, Ter_MaterialDraw)                     DEF_MAT_REV(KKLL,KLLK)
            DEF_MAT(KDDK, Ter_MaterialDraw)                     DEF_MAT_REV(KKDD,KDDK)
            DEF_MAT(KNNK, Ter_MaterialDraw)                     DEF_MAT_REV(KKNN,KNNK)
            DEF_MAT_H(KLNK, Ter_WhiteWinWithHelper,&helperKmmK) DEF_MAT_REV_H(KKLN,KLNK,&helperKmmK)
            DEF_MAT_H(KDNK, Ter_WhiteWinWithHelper,&helperKmmK) DEF_MAT_REV_H(KKDN,KDNK,&helperKmmK)

            // Rm R : likely draws
            DEF_MAT(KRNKR, Ter_LikelyDraw)            DEF_MAT_REV(KRKRN,KRNKR)
            DEF_MAT(KRLKR, Ter_LikelyDraw)            DEF_MAT_REV(KRKRL,KRLKR)
            DEF_MAT(KRDKR, Ter_LikelyDraw)            DEF_MAT_REV(KRKRD,KRDKR)

            // Rm m : hard to win
            DEF_MAT(KRNKN, Ter_HardToWin)             DEF_MAT_REV(KNKRN,KRNKN)
            DEF_MAT(KRNKL, Ter_HardToWin)             DEF_MAT_REV(KLKRN,KRNKL)
            DEF_MAT(KRNKD, Ter_HardToWin)             DEF_MAT_REV(KDKRN,KRNKD)
            DEF_MAT(KRLKN, Ter_HardToWin)             DEF_MAT_REV(KNKRL,KRLKN)
            DEF_MAT(KRLKL, Ter_HardToWin)             DEF_MAT_REV(KLKRL,KRLKL)
            DEF_MAT(KRLKD, Ter_HardToWin)             DEF_MAT_REV(KDKRL,KRLKD)
            DEF_MAT(KRDKN, Ter_HardToWin)             DEF_MAT_REV(KNKRD,KRDKN)
            DEF_MAT(KRDKL, Ter_HardToWin)             DEF_MAT_REV(KLKRD,KRDKL)
            DEF_MAT(KRDKD, Ter_HardToWin)             DEF_MAT_REV(KDKRD,KRDKD)

            // Qm Q : hard to win
            DEF_MAT_H(KQNKQ, Ter_HardToWin,&helperKXK)             DEF_MAT_REV_H(KQKQN,KQNKQ,&helperKXK)
            DEF_MAT_H(KQLKQ, Ter_HardToWin,&helperKXK)             DEF_MAT_REV_H(KQKQL,KQLKQ,&helperKXK)
            DEF_MAT_H(KQDKQ, Ter_HardToWin,&helperKXK)             DEF_MAT_REV_H(KQKQD,KQDKQ,&helperKXK)

            // Qm R : wins
            DEF_MAT_H(KQNKR, Ter_WhiteWin,&helperKXK)              DEF_MAT_REV_H(KRKQN,KQNKR,&helperKXK)
            DEF_MAT_H(KQLKR, Ter_WhiteWin,&helperKXK)              DEF_MAT_REV_H(KRKQL,KQLKR,&helperKXK)
            DEF_MAT_H(KQDKR, Ter_WhiteWin,&helperKXK)              DEF_MAT_REV_H(KRKQD,KQRKR,&helperKXK)

            // Q Rm : hard to win
            DEF_MAT_H(KQKRN, Ter_HardToWin,&helperKXK)             DEF_MAT_REV_H(KRNKQ,KQKRN,&helperKXK)
            DEF_MAT_H(KQKRL, Ter_HardToWin,&helperKXK)             DEF_MAT_REV_H(KRLKQ,KQKRL,&helperKXK)
            DEF_MAT_H(KQKRD, Ter_HardToWin,&helperKXK)             DEF_MAT_REV_H(KRDKQ,KQKRD,&helperKXK)

            // Opposite bishop with P
            DEF_MAT(KLPKD, Ter_LikelyDraw)            DEF_MAT_REV(KDKLP,KLPKD)
            DEF_MAT(KDPKL, Ter_LikelyDraw)            DEF_MAT_REV(KLKDP,KDPKL)
            DEF_MAT(KLPPKD, Ter_LikelyDraw)           DEF_MAT_REV(KDKLPP,KLPPKD)
            DEF_MAT(KDPPKL, Ter_LikelyDraw)           DEF_MAT_REV(KLKDPP,KDPPKL)

            // KPK
            DEF_MAT_H(KPK, Ter_WhiteWinWithHelper,&helperKPK)    DEF_MAT_REV_H(KKP,KPK,&helperKPK)

            ///@todo other (with pawn ...)
        }
    };

    inline Terminaison probeMaterialHashTable(const Position::Material & mat) { return materialHashTable[getMaterialHash(mat)]; }
}

void updateMaterialOther(Position & p){
    p.mat[Co_White][M_M] = p.mat[Co_White][M_q] + p.mat[Co_White][M_r];  p.mat[Co_Black][M_M] = p.mat[Co_Black][M_q] + p.mat[Co_Black][M_r];
    p.mat[Co_White][M_m] = p.mat[Co_White][M_b] + p.mat[Co_White][M_n];  p.mat[Co_Black][M_m] = p.mat[Co_Black][M_b] + p.mat[Co_Black][M_n];
    p.mat[Co_White][M_t] = p.mat[Co_White][M_M] + p.mat[Co_White][M_m];  p.mat[Co_Black][M_t] = p.mat[Co_Black][M_M] + p.mat[Co_Black][M_m];
    if (p.whiteBishop()) {
        p.mat[Co_White][M_bl] = (unsigned char)countBit(p.whiteBishop()&whiteSquare);
        p.mat[Co_White][M_bd] = (unsigned char)countBit(p.whiteBishop()&blackSquare);
    }
    if (p.blackBishop()) {
        p.mat[Co_Black][M_bl] = (unsigned char)countBit(p.blackBishop()&whiteSquare);
        p.mat[Co_Black][M_bd] = (unsigned char)countBit(p.blackBishop()&blackSquare);
    }
}

void initMaterial(Position & p){ // M_p .. M_k is the same as P_wp .. P_wk
    for( Color c = Co_White ; c < Co_End ; ++c) for( Piece pp = P_wp ; pp <= P_wk ; ++pp) p.mat[c][pp] = (unsigned char)countBit(p.pieces(c,pp));
    updateMaterialOther(p);
}

void updateMaterialStd(Position &p, const Square toBeCaptured){
    p.mat[~p.c][getPieceType(p,toBeCaptured)]--; // capture if to square is not empty
}

void updateMaterialEp(Position &p){
    p.mat[~p.c][M_p]--; // ep if to square is empty
}

void updateMaterialProm(Position &p, const Square toBeCaptured, MType mt){
    p.mat[~p.c][getPieceType(p, toBeCaptured)]--; // capture if to square is not empty
    p.mat[p.c][P_wp]--; // pawn
    p.mat[p.c][promShift(mt)]++;   // prom piece
}

void initBitBoards(Position & p) {
    p.whitePawn() = p.whiteKnight() = p.whiteBishop() = p.whiteRook() = p.whiteQueen() = p.whiteKing() = 0ull;
    p.blackPawn() = p.blackKnight() = p.blackBishop() = p.blackRook() = p.blackQueen() = p.blackKing() = 0ull;
    p.allPieces[Co_White] = p.allPieces[Co_Black] = p.occupancy = 0ull;
}

void setBitBoards(Position & p) {
    initBitBoards(p);
    for (Square k = 0; k < 64; ++k) {
        ///@todo use all BB => 1 line ;-)
        switch (p.b[k]){
        case P_none: break;
        case P_wp: p.whitePawn  () |= SquareToBitboard(k); break;
        case P_wn: p.whiteKnight() |= SquareToBitboard(k); break;
        case P_wb: p.whiteBishop() |= SquareToBitboard(k); break;
        case P_wr: p.whiteRook  () |= SquareToBitboard(k); break;
        case P_wq: p.whiteQueen () |= SquareToBitboard(k); break;
        case P_wk: p.whiteKing  () |= SquareToBitboard(k); break;
        case P_bp: p.blackPawn  () |= SquareToBitboard(k); break;
        case P_bn: p.blackKnight() |= SquareToBitboard(k); break;
        case P_bb: p.blackBishop() |= SquareToBitboard(k); break;
        case P_br: p.blackRook  () |= SquareToBitboard(k); break;
        case P_bq: p.blackQueen () |= SquareToBitboard(k); break;
        case P_bk: p.blackKing  () |= SquareToBitboard(k); break;
        default: assert(false);
        }
    }
    p.allPieces[Co_White] = p.whitePawn() | p.whiteKnight() | p.whiteBishop() | p.whiteRook() | p.whiteQueen() | p.whiteKing();
    p.allPieces[Co_Black] = p.blackPawn() | p.blackKnight() | p.blackBishop() | p.blackRook() | p.blackQueen() | p.blackKing();
    p.occupancy  = p.allPieces[Co_White] | p.allPieces[Co_Black];
}

inline void unSetBit(Position & p, Square k)           { assert(k >= 0 && k < 64); unSetBit(p.allB[getPieceIndex(p, k)], k);}
inline void unSetBit(Position & p, Square k, Piece pp) { assert(k >= 0 && k < 64); unSetBit(p.allB[pp + PieceShift]    , k);}
inline void setBit  (Position & p, Square k, Piece pp) { assert(k >= 0 && k < 64); setBit  (p.allB[pp + PieceShift]    , k);}

namespace Zobrist {
    Hash randomInt() {
        static std::mt19937 mt(42); // fixed seed for ZHash !!!
        static std::uniform_int_distribution<unsigned long long int> dist(0, UINT64_MAX);
        return dist(mt);
    }
    Hash ZT[64][14]; // should be 13 but last ray is for castling[0 7 56 63][13] and ep [k][13] and color [3 4][13]
    void initHash() {
        Logging::LogIt(Logging::logInfo) << "Init hash";
        for (int k = 0; k < 64; ++k) for (int j = 0; j < 14; ++j) ZT[k][j] = randomInt();
    }
}

//#define DEBUG_HASH

Hash computeHash(const Position &p){
#ifdef DEBUG_HASH
    Hash h = p.h;
    p.h = 0ull;
#endif
    if (p.h != 0) return p.h;
    for (int k = 0; k < 64; ++k){
        const Piece pp = p.b[k];
        if ( pp != P_none) p.h ^= Zobrist::ZT[k][pp+PieceShift];
    }
    if ( p.ep != INVALIDSQUARE ) p.h ^= Zobrist::ZT[p.ep][13];
    if ( p.castling & C_wks)     p.h ^= Zobrist::ZT[7][13];
    if ( p.castling & C_wqs)     p.h ^= Zobrist::ZT[0][13];
    if ( p.castling & C_bks)     p.h ^= Zobrist::ZT[63][13];
    if ( p.castling & C_bqs)     p.h ^= Zobrist::ZT[56][13];
    if ( p.c == Co_White)        p.h ^= Zobrist::ZT[3][13];
    if ( p.c == Co_Black)        p.h ^= Zobrist::ZT[4][13];
#ifdef DEBUG_HASH
    if ( h != 0ull && h != p.h ){
        Logging::LogIt(logError) << "Hash error " << ToString(p.lastMove);
        Logging::LogIt(logError) << ToString(p,true);
        exit(1);
    }
#endif
    return p.h;
}

struct ThreadContext; // forward decl

struct ThreadData{
    DepthType depth, seldepth;
    ScoreType sc;
    Position p;
    Move best;
    PVList pv;
};

struct Stats{
    enum StatId { sid_nodes = 0, sid_qnodes, sid_tthits,sid_staticNullMove, sid_razoringTry, sid_razoring, sid_nullMoveTry, sid_nullMoveTry2, sid_nullMove, sid_probcutTry, sid_probcutTry2, sid_probcut, sid_lmp, sid_historyPruning, sid_futility, sid_see, sid_seeQuiet, sid_iid, sid_ttalpha, sid_ttbeta, sid_checkExtension, sid_checkExtension2, sid_recaptureExtension, sid_castlingExtension, sid_pawnPushExtension, sid_singularExtension, sid_mateThreadExtension, sid_maxid };
    static const std::string Names[sid_maxid] ;
    Counter counters[sid_maxid];
    void init(){
        Logging::LogIt(Logging::logInfo) << "Init stat" ;
        for(size_t k = 0 ; k < sid_maxid ; ++k) counters[k] = Counter(0);
    }
};

const std::string Stats::Names[Stats::sid_maxid] = { "nodes", "qnodes", "tthits", "staticNullMove", "razoringTry", "razoring", "nullMoveTry", "nullMoveTry2", "nullMove", "probcutTry", "probcutTry2", "probcut", "lmp", "historyPruning", "futility", "see", "seeQuiet", "iid", "ttalpha", "ttbeta", "checkExtension", "checkExtension2", "recaptureExtension", "castlingExtension", "pawnPushExtension", "singularExtension", "mateThreadExtension"};

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
    void wait(bool otherOnly = false);
    bool stop;
    // gathering info from all threads
    Counter counter(Stats::StatId id) const;
    void DisplayStats()const{for(size_t k = 0 ; k < Stats::sid_maxid ; ++k) Logging::LogIt(Logging::logInfo) << Stats::Names[k] << " " << counter((Stats::StatId)k);}
    // Sizes and phases of the skip-blocks, used for distributing search depths across the threads, from stockfish
    static const int skipSize[20], skipPhase[20];
private:
    ThreadPool();
};

const int ThreadPool::skipSize[20]  = { 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4 };
const int ThreadPool::skipPhase[20] = { 0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7 };

namespace MoveDifficultyUtil {
    enum MoveDifficulty { MD_forced = 0, MD_easy, MD_std, MD_hardDefense, MD_hardAttack };
    const DepthType emergencyMinDepth = 9;
    const ScoreType emergencyMargin   = 50;
    const ScoreType easyMoveMargin    = 250;
    const int       emergencyFactor   = 5;
    const float     maxStealFraction  = 0.3f; // of remaining time
}

// thread from Stockfish
struct ThreadContext{
    static bool stopFlag;
    static MoveDifficultyUtil::MoveDifficulty moveDifficulty;
    static int currentMoveMs;
    static int getCurrentMoveMs(); // use this (and not the variable) to take emergency time into account !

    Hash hashStack[MAX_PLY] = { 0ull };
    ScoreType evalStack[MAX_PLY] = { 0 };

    Stats stats;

    struct KillerT{
        Move killers[2][MAX_PLY];
        inline void initKillers(){
            Logging::LogIt(Logging::logInfo) << "Init killers" ;
            for(int i = 0; i < 2; ++i) for(int k = 0 ; k < MAX_PLY; ++k) killers[i][k] = INVALIDMOVE;
        }
    };

    struct HistoryT{
        ScoreType history[13][64];
        inline void initHistory(){
            Logging::LogIt(Logging::logInfo) << "Init history" ;
            for(int i = 0; i < 13; ++i) for(int k = 0 ; k < 64; ++k) history[i][k] = 0;
        }
        template<int S>
        inline void update(DepthType depth, Move m, const Position & p){
            const int pp = getPieceIndex(p,Move2From(m));
            const Square to = Move2To(m);
            if ( Move2Type(m) == T_std ) history[pp][to] += ScoreType( ( S - history[pp][to] / MAX_HISTORY) * HSCORE(depth) );
        }
    };

    struct CounterT{
        ScoreType counter[64][64];
        inline void initCounter(){
            Logging::LogIt(Logging::logInfo) << "Init counter" ;
            for(int i = 0; i < 64; ++i) for(int k = 0 ; k < 64; ++k) counter[i][k] = 0;
        }
        inline void update(Move m, const Position & p){ if ( p.lastMove != INVALIDMOVE && Move2Type(m) == T_std ) counter[Move2From(p.lastMove)][Move2To(p.lastMove)] = m; }
    };
    KillerT killerT;
    HistoryT historyT;
    CounterT counterT;

    struct RootScores { Move m; ScoreType s; };

    template <bool pvnode, bool canPrune = true> ScoreType pvs(ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, unsigned int ply, PVList & pv, DepthType & seldepth, bool isInCheck, const Move skipMove = INVALIDMOVE, std::vector<RootScores> * rootScores = 0);
    template <bool qRoot, bool pvnode>
    ScoreType qsearch(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth);
    ScoreType qsearchNoPruning(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth);
    bool SEE(const Position & p, const Move & m, ScoreType threshold)const;
    PVList search(const Position & p, Move & m, DepthType & d, ScoreType & sc, DepthType & seldepth);
    template< bool withRep = true, bool isPv = true, bool INR = true> MaterialHash::Terminaison interiorNodeRecognizer(const Position & p)const;
    bool isRep(const Position & p, bool isPv)const;

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
        Logging::LogIt(Logging::logInfo) << "Starting worker " << id() ;
        _searching = true;
        _cv.notify_one(); // Wake up the thread in IdleLoop()
    }

    void wait(){
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [&]{ return !_searching; });
    }

    void search(){
        Logging::LogIt(Logging::logInfo) << "Search launched " << id() ;
        if ( isMainThread() ){ ThreadPool::instance().startOthers(); } // started but locked for now ...
        _data.pv = search(_data.p, _data.best, _data.depth, _data.sc, _data.seldepth);
    }

    size_t id()const { return _index;}
    bool   isMainThread()const { return id() == 0 ; }

    ThreadContext(size_t n):_index(n),_exit(false),_searching(true),_stdThread(&ThreadContext::idleLoop, this){ wait(); }

    ~ThreadContext(){
        _exit = true;
        start();
        Logging::LogIt(Logging::logInfo) << "Waiting for workers to join...";
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

bool ThreadContext::stopFlag      = true;
int  ThreadContext::currentMoveMs = 777; // a dummy initial value, useful for debug
MoveDifficultyUtil::MoveDifficulty ThreadContext::moveDifficulty = MoveDifficultyUtil::MD_std;
std::atomic<bool> ThreadContext::startLock;

ThreadPool & ThreadPool::instance(){ static ThreadPool pool; return pool;}

ThreadPool::~ThreadPool(){
    while (size()) { delete back(); pop_back(); }
    Logging::LogIt(Logging::logInfo) << "... ok threadPool deleted";
}

void ThreadPool::setup(unsigned int n){
    assert(n > 0);
    Logging::LogIt(Logging::logInfo) << "Using " << n << " threads";
    while (size() < (unsigned int)n) push_back(new ThreadContext(size()));
}

ThreadContext & ThreadPool::main() { return *(front()); }

void ThreadPool::wait(bool otherOnly) {
    Logging::LogIt(Logging::logInfo) << "Wait for workers to be ready";
    for (auto s : *this) { if (!otherOnly || !(*s).isMainThread()) (*s).wait(); }
    Logging::LogIt(Logging::logInfo) << "...ok";
}

Move ThreadPool::searchSync(const ThreadData & d){
    Logging::LogIt(Logging::logInfo) << "Search Sync" ;
    wait();
    ThreadContext::startLock.store(true);
    for (auto s : *this) (*s).setData(d); // this is a copy
    Logging::LogIt(Logging::logInfo) << "Calling main thread search" ;
    main().search(); ///@todo 1 thread for nothing here
    ThreadContext::stopFlag = true;
    wait();
    return main().getData().best;
}

void ThreadPool::searchASync(const ThreadData & d){} ///@todo for pondering

void ThreadPool::startOthers(){ for (auto s : *this) if (!(*s).isMainThread()) (*s).start();}

ThreadPool::ThreadPool():stop(false){ push_back(new ThreadContext(size()));} // this one will be called "Main" thread

Counter ThreadPool::counter(Stats::StatId id) const { Counter n = 0; for (auto it : *this ){ n += it->stats.counters[id];  } return n;}

bool apply(Position & p, const Move & m); //forward decl

namespace TT{
enum Bound{ B_exact = 0, B_alpha = 1, B_beta = 2};
struct Entry{
    Entry():m(INVALIDMOVE),score(0),eval(0),b(B_alpha),d(-1),h(0ull){}
    Entry(Move m, ScoreType s, ScoreType e, Bound b, DepthType d, Hash h) : m(m), score(s), eval(e), b(b), d(d), h(h){}
    Move m;
    ScoreType score, eval;
    Bound b;
    DepthType d;
    Hash h;
};

struct Bucket { static const int nbBucket = 2;  Entry e[nbBucket];};

unsigned long long int powerFloor(unsigned long long int x) {
    unsigned long long int power = 1;
    while (power < x) power *= 2;
    return power/2;
}

static unsigned long long int ttSize = 0;
static std::unique_ptr<Bucket[]> table;

void initTable(){
    assert(table==nullptr);
    Logging::LogIt(Logging::logInfo) << "Init TT" ;
    Logging::LogIt(Logging::logInfo) << "Bucket size " << sizeof(Bucket);
    ttSize = 1024 * powerFloor((DynamicConfig::ttSizeMb * 1024) / (unsigned long long int)sizeof(Bucket));
    table = std::unique_ptr<Bucket[]>(new Bucket[ttSize]);
    Logging::LogIt(Logging::logInfo) << "Size of TT " << ttSize * sizeof(Bucket) / 1024 / 1024 << "Mb" ;
}

void clearTT() {
    for (unsigned int k = 0; k < ttSize; ++k) for (unsigned int i = 0 ; i < Bucket::nbBucket ; ++i) table[k].e[i] = { INVALIDMOVE, 0, 0, B_alpha, 0, 0ull };
}

bool getEntry(ThreadContext & context, Hash h, DepthType d, Entry & e, int nbuck = 0) {
    assert(h > 0);
    if ( DynamicConfig::disableTT  ) return false;
    if ( nbuck >= Bucket::nbBucket ) return false; // no more bucket
    const Entry & _e = table[h&(ttSize-1)].e[nbuck];
    if ( _e.h == 0 ) return false; //early exist cause next ones are also empty ...
    if ( _e.h != h ) return getEntry(context,h,d,e,nbuck+1); // next one
    e = _e; // update entry only if no collision is detected !
    if ( _e.d >= d ){ ++context.stats.counters[Stats::sid_tthits]; return true; } // valid entry if depth is ok
    else return getEntry(context,h,d,e,nbuck+1); // next one
}

void setEntry(const Entry & e){
    assert(e.h > 0);
    if ( DynamicConfig::disableTT ) return;
    const size_t index = e.h&(ttSize-1);
    DepthType lowest = MAX_DEPTH;
    Entry & _eDepthLowest = table[index].e[0];
    for (unsigned int i = 0 ; i < Bucket::nbBucket ; ++i){ // we look for the lower depth
        const Entry & _eDepth = table[index].e[i];
        if ( _eDepth.h == 0 ) break; //early break cause next ones are also empty
        if ( _eDepth.d < lowest ) { _eDepthLowest = _eDepth; lowest = _eDepth.d; }
    }
    _eDepthLowest = e; // replace the one with the lowest depth
}

void getPV(const Position & p, ThreadContext & context, PVList & pv){
    TT::Entry e;
    if (TT::getEntry(context, computeHash(p), 0, e)) {
        if (e.h != 0) pv.insert(pv.begin(),e.m);
        Position p2 = p;
        if ( apply(p2,e.m) ) getPV(p2,context,pv);
    }
}

} // TT

template < bool display = false, bool safeMatEvaluator = true >
ScoreType eval(const Position & p, float & gp); // forward decl

std::string trim(const std::string& str, const std::string& whitespace = " \t"){
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) return ""; // no content
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

std::string GetFENShort(const Position &p ){ // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR"
    std::stringstream ss;
    int count = 0;
    for (int i = 7; i >= 0; --i) {
        for (int j = 0; j < 8; j++) {
            const Square k = 8 * i + j;
            if (p.b[k] == P_none) ++count;
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

std::string GetFENShort2(const Position &p) { // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d5
    std::stringstream ss;
    ss << GetFENShort(p) << " " << (p.c == Co_White ? "w" : "b") << " ";
    bool withCastling = false;
    if (p.castling & C_wks) { ss << "K"; withCastling = true; }
    if (p.castling & C_wqs) { ss << "Q"; withCastling = true; }
    if (p.castling & C_bks) { ss << "k"; withCastling = true; }
    if (p.castling & C_bqs) { ss << "q"; withCastling = true; }
    if (!withCastling) ss << "-";
    if (p.ep != INVALIDSQUARE) ss << " " << SquareNames[p.ep];
    else ss << " -";
    return ss.str();
}

std::string GetFEN(const Position &p) { // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d5 0 2"
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
    case T_bks: return "e8g8" + score;
    case T_wks: return "e1g1" + score;
    case T_bqs: return "e8c8" + score;
    case T_wqs: return "e1c1" + score;
    default:
        static const std::string promSuffixe[] = { "","","","","q","r","b","n","q","r","b","n" };
        prom = promSuffixe[Move2Type(m)];
        break;
    }
    ss << SquareNames[Move2From(m)] << SquareNames[Move2To(m)];
    return ss.str() + prom + score;
}

std::string ToString(const PVList & moves){
    std::stringstream ss;
    for (size_t k = 0; k < moves.size(); ++k) { if (moves[k] == INVALIDMOVE) break;  ss << ToString(moves[k]) << " "; }
    return ss.str();
}

std::string ToString(const Position::Material & mat) {
    std::stringstream str;
    str << "\n" << "#Q  :" << (int)mat[Co_White][M_q] << "\n" << "#R  :" << (int)mat[Co_White][M_r] << "\n" << "#B  :" << (int)mat[Co_White][M_b] << "\n" << "#L  :" << (int)mat[Co_White][M_bl] << "\n" << "#D  :" << (int)mat[Co_White][M_bd] << "\n" << "#N  :" << (int)mat[Co_White][M_n] << "\n" << "#P  :" << (int)mat[Co_White][M_p] << "\n" << "#Ma :" << (int)mat[Co_White][M_M] << "\n" << "#Mi :" << (int)mat[Co_White][M_m] << "\n" << "#T  :" << (int)mat[Co_White][M_t] << "\n";
    str << "\n" << "#q  :" << (int)mat[Co_Black][M_q] << "\n" << "#r  :" << (int)mat[Co_Black][M_r] << "\n" << "#b  :" << (int)mat[Co_Black][M_b] << "\n" << "#l  :" << (int)mat[Co_Black][M_bl] << "\n" << "#d  :" << (int)mat[Co_Black][M_bd] << "\n" << "#n  :" << (int)mat[Co_Black][M_n] << "\n" << "#p  :" << (int)mat[Co_Black][M_p] << "\n" << "#ma :" << (int)mat[Co_Black][M_M] << "\n" << "#mi :" << (int)mat[Co_Black][M_m] << "\n" << "#t  :" << (int)mat[Co_Black][M_t] << "\n";
    return str.str();
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
    if ( p.ep >=0 ) ss << "# ep " << SquareNames[p.ep] << std::endl;
    ss << "# wk " << (p.king[Co_White]!=INVALIDSQUARE?SquareNames[p.king[Co_White]]:"none")  << std::endl << "# bk " << (p.king[Co_Black]!=INVALIDSQUARE?SquareNames[p.king[Co_Black]]:"none") << std::endl;
    ss << "# Turn " << (p.c == Co_White ? "white" : "black") << std::endl;
    ScoreType sc = 0;
    if ( ! noEval ){
        float gp = 0;
        sc = eval(p, gp);
        ss << "# Phase " << gp  << std::endl << "# Static score " << sc << std::endl << "# Hash " << computeHash(p) << std::endl << "# FEN " << GetFEN(p) << std::endl;
    }
    //ss << ToString(p.mat);
    return ss.str();
}

template < typename T > T readFromString(const std::string & s){ std::stringstream ss(s); T tmp; ss >> tmp; return tmp;}

bool readFEN(const std::string & fen, Position & p, bool silent){
    std::vector<std::string> strList;
    std::stringstream iss(fen);
    std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));

    if ( !silent) Logging::LogIt(Logging::logInfo) << "Reading fen " << fen ;

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
        case 'k': p.b[k]= P_bk; p.king[Co_Black] = k; break;
        case 'P': p.b[k]= P_wp; break;
        case 'R': p.b[k]= P_wr; break;
        case 'N': p.b[k]= P_wn; break;
        case 'B': p.b[k]= P_wb; break;
        case 'Q': p.b[k]= P_wq; break;
        case 'K': p.b[k]= P_wk; p.king[Co_White] = k; break;
        case '/': j--; break;
        case '1': break;
        case '2': j++; break;
        case '3': j += 2; break;
        case '4': j += 3; break;
        case '5': j += 4; break;
        case '6': j += 5; break;
        case '7': j += 6; break;
        case '8': j += 7; break;
        default: Logging::LogIt(Logging::logFatal) << "FEN ERROR 0 : " << letter ;
        }
        j++;
    }

    p.c = Co_White; // set the turn; default is white
    if (strList.size() >= 2){
        if (strList[1] == "w")      p.c = Co_White;
        else if (strList[1] == "b") p.c = Co_Black;
        else { Logging::LogIt(Logging::logFatal) << "FEN ERROR 1" ; return false; }
    }

    // Initialize all castle possibilities (default is none)
    p.castling = C_none;
    if (strList.size() >= 3){
        bool found = false;
        if (strList[2].find('K') != std::string::npos){ p.castling |= C_wks; found = true; }
        if (strList[2].find('Q') != std::string::npos){ p.castling |= C_wqs; found = true; }
        if (strList[2].find('k') != std::string::npos){ p.castling |= C_bks; found = true; }
        if (strList[2].find('q') != std::string::npos){ p.castling |= C_bqs; found = true; }
        if ( ! found ){ if ( !silent) Logging::LogIt(Logging::logWarn) << "No castling right given" ; }
    }
    else if ( !silent) Logging::LogIt(Logging::logInfo) << "No castling right given" ;

    // read en passant and save it (default is invalid)
    p.ep = INVALIDSQUARE;
    if ((strList.size() >= 4) && strList[3] != "-" ){
        if (strList[3].length() >= 2){
            if ((strList[3].at(0) >= 'a') && (strList[3].at(0) <= 'h') && ((strList[3].at(1) == '3') || (strList[3].at(1) == '6'))) p.ep = stringToSquare(strList[3]);
            else { Logging::LogIt(Logging::logFatal) << "FEN ERROR 3-1 : bad en passant square : " << strList[3] ; return false; }
        }
        else{ Logging::LogIt(Logging::logFatal) << "FEN ERROR 3-2 : bad en passant square : " << strList[3] ; return false; }
    }
    else if ( !silent) Logging::LogIt(Logging::logInfo) << "No en passant square given" ;

    assert(p.ep == INVALIDSQUARE || (SQRANK(p.ep) == 2 || SQRANK(p.ep) == 5));

    // read 50 moves rules
    if (strList.size() >= 5) p.fifty = (unsigned char)readFromString<int>(strList[4]);
    else p.fifty = 0;

    // read number of move
    if (strList.size() >= 6) p.moves = (unsigned char)readFromString<int>(strList[5]);
    else p.moves = 1;

    p.halfmoves = (int(p.moves) - 1) * 2 + 1 + (p.c == Co_Black ? 1 : 0);

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

inline std::string SanitizeCastling(const Position & p, const std::string & str){
    // SAN castling notation
    if (( str == "e1g1" && p.b[Sq_e1] == P_wk ) || ( str == "e8g8" && p.b[Sq_e8] == P_bk )) return "0-0";
    if (( str == "e1c1" && p.b[Sq_e1] == P_wk ) || ( str == "e8c8" && p.b[Sq_e8] == P_bk )) return "0-0-0";
    return str;
}

inline Move SanitizeCastling(const Position & p, const Move & m){
    if ( m == INVALIDMOVE ) return m;
    // SAN castling notation
    const Square from = Move2From(m);
    const Square to   = Move2To(m);
    MType mtype = Move2Type(m);
    if ( (from == Sq_e1) && (p.b[Sq_e1] == P_wk) && (to == Sq_g1)) mtype=T_wks;
    if ( (from == Sq_e8) && (p.b[Sq_e8] == P_bk) && (to == Sq_g8)) mtype=T_bks;
    if ( (from == Sq_e1) && (p.b[Sq_e1] == P_wk) && (to == Sq_c1)) mtype=T_wqs;
    if ( (from == Sq_e8) && (p.b[Sq_e8] == P_bk) && (to == Sq_c8)) mtype=T_bqs;
    return ToMove(from,to,mtype);
}

bool readMove(const Position & p, const std::string & ss, Square & from, Square & to, MType & moveType ) {
    if ( ss.empty()){
        Logging::LogIt(Logging::logFatal) << "Trying to read empty move ! " ;
        moveType = T_std;
        return false;
    }
    std::string str(ss);
    str = SanitizeCastling(p,str);
    // add space to go to own internal notation
    if ( str != "0-0" && str != "0-0-0" && str != "O-O" && str != "O-O-O" ) str.insert(2," ");

    std::vector<std::string> strList;
    std::stringstream iss(str);
    std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));

    moveType = T_std;
    if ( strList.empty()){ Logging::LogIt(Logging::logError) << "Trying to read bad move, seems empty " << str ; return false; }

    // detect special move
    if (strList[0] == "0-0" || strList[0] == "O-O") {
        moveType = (p.c == Co_White) ? T_wks : T_bks;
        from = (p.c == Co_White) ? Sq_e1 : Sq_e8;
        to = (p.c == Co_White) ? Sq_g1 : Sq_g8;
    }
    else if (strList[0] == "0-0-0" || strList[0] == "O-O-O") {
        moveType = (p.c == Co_White) ? T_wqs : T_bqs;
        to = (p.c == Co_White) ? Sq_c1 : Sq_c8;
    }
    else{
        if ( strList.size() == 1 ){ Logging::LogIt(Logging::logError) << "Trying to read bad move, malformed (=1) " << str ; return false; }
        if ( strList.size() > 2 && strList[2] != "ep"){ Logging::LogIt(Logging::logError) << "Trying to read bad move, malformed (>2)" << str ; return false; }
        if (strList[0].size() == 2 && (strList[0].at(0) >= 'a') && (strList[0].at(0) <= 'h') && ((strList[0].at(1) >= 1) && (strList[0].at(1) <= '8'))) from = stringToSquare(strList[0]);
        else { Logging::LogIt(Logging::logError) << "Trying to read bad move, invalid from square " << str ; return false; }
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
                else{ Logging::LogIt(Logging::logError) << "Trying to read bad move, invalid to square " << str ; return false; }
            }
            else{
                to = stringToSquare(strList[1]);
                isCapture = p.b[to] != P_none;
                if(isCapture) moveType = T_capture;
            }
        }
        else { Logging::LogIt(Logging::logError) << "Trying to read bad move, invalid to square " << str ; return false; }
    }
    if (getPieceType(p,from) == P_wp && to == p.ep) moveType = T_ep;
    return true;
}

namespace TimeMan{
int msecPerMove, msecInTC, nbMoveInTC, msecInc, msecUntilNextTC, maxKNodes, moveToGo;
bool isDynamic;
std::chrono::time_point<Clock> startTime;

void init(){
    Logging::LogIt(Logging::logInfo) << "Init timeman" ;
    msecPerMove = 777;
    msecInTC    = nbMoveInTC = msecInc = msecUntilNextTC = maxKNodes = moveToGo = -1;
    isDynamic   = false;
}

int GetNextMSecPerMove(const Position & p){
    static const int msecMarginMin = 50;
    static const int msecMarginMax = 3000;
    static const float msecMarginCoef = 0.02f;
    int ms = -1;
    Logging::LogIt(Logging::logInfo) << "msecPerMove     " << msecPerMove;
    Logging::LogIt(Logging::logInfo) << "msecInTC        " << msecInTC   ;
    Logging::LogIt(Logging::logInfo) << "msecInc         " << msecInc    ;
    Logging::LogIt(Logging::logInfo) << "nbMoveInTC      " << nbMoveInTC ;
    Logging::LogIt(Logging::logInfo) << "msecUntilNextTC " << msecUntilNextTC;
    Logging::LogIt(Logging::logInfo) << "currentNbMoves  " << int(p.moves);
    Logging::LogIt(Logging::logInfo) << "moveToGo        " << moveToGo;
    int msecIncLoc = (msecInc > 0) ? msecInc : 0;
    if ( msecPerMove > 0 ) {
        Logging::LogIt(Logging::logInfo) << "Fixed time per move";
        ms =  msecPerMove;
    }
    else if ( nbMoveInTC > 0){ // mps is given (xboard style)
        Logging::LogIt(Logging::logInfo) << "Xboard style TC";
        assert(msecInTC > 0); assert(nbMoveInTC > 0);
        Logging::LogIt(Logging::logInfo) << "TC mode, xboard";
        const int msecMargin = std::max(std::min(msecMarginMax, int(msecMarginCoef*msecInTC)), msecMarginMin);
        if (!isDynamic) ms = int((msecInTC - msecMarginMin) / (float)nbMoveInTC) + msecIncLoc ;
        else { ms = std::min(msecUntilNextTC - msecMargin, int((msecUntilNextTC - msecMargin) /float(nbMoveInTC - ((p.moves - 1) % nbMoveInTC))) + msecIncLoc); }
    }
    else if (moveToGo > 0) { // moveToGo is given (uci style)
        assert(msecUntilNextTC > 0);
        Logging::LogIt(Logging::logInfo) << "UCI style TC";
        const int msecMargin = std::max(std::min(msecMarginMax, int(msecMarginCoef*msecUntilNextTC)), msecMarginMin);
        if (!isDynamic) Logging::LogIt(Logging::logFatal) << "bad timing configuration ...";
        else { ms = std::min(msecUntilNextTC - msecMargin, int((msecUntilNextTC - msecMargin) / float(moveToGo)) + msecIncLoc); }
    }
    else{ // mps is not given
        Logging::LogIt(Logging::logInfo) << "Suddendeath style";
        const int nmoves = 24; // always be able to play this more moves !
        Logging::LogIt(Logging::logInfo) << "nmoves    " << nmoves;
        Logging::LogIt(Logging::logInfo) << "p.moves   " << int(p.moves);
        assert(nmoves > 0); assert(msecInTC >= 0);
        const int msecMargin = std::max(std::min(msecMarginMax, int(msecMarginCoef*msecInTC)), msecMarginMin);
        if (!isDynamic) ms = int((msecInTC+msecIncLoc) / (float)(nmoves)) - msecMarginMin;
        else ms = std::min(msecUntilNextTC - msecMargin, int(msecUntilNextTC / (float)nmoves + 0.75*msecIncLoc) - msecMargin);
    }
    return std::max(ms, 20);// if not much time left, let's try that ...
}
} // TimeMan

inline void addMove(Square from, Square to, MType type, MoveList & moves){ assert( from >= 0 && from < 64); assert( to >=0 && to < 64); moves.push_back(ToMove(from,to,type,0));}

int ThreadContext::getCurrentMoveMs() {
    switch (moveDifficulty) {
    case MoveDifficultyUtil::MD_forced:      return (currentMoveMs >> 4);
    case MoveDifficultyUtil::MD_easy:        return (currentMoveMs >> 3);
    case MoveDifficultyUtil::MD_std:         return (currentMoveMs);
    case MoveDifficultyUtil::MD_hardDefense: return (std::min(int(TimeMan::msecUntilNextTC*MoveDifficultyUtil::maxStealFraction), currentMoveMs*MoveDifficultyUtil::emergencyFactor));
    case MoveDifficultyUtil::MD_hardAttack:  return (currentMoveMs);
    }
    return currentMoveMs;
}

inline Square kingSquare(const Position & p) { return p.king[p.c]; }
inline Square oppKingSquare(const Position & p) { return p.king[~p.c]; }
inline bool isAttacked(const Position & p, const Square k) { return k!=INVALIDSQUARE && BB::isAttackedBB(p, k, p.c) != 0ull;}
inline bool getAttackers(const Position & p, const Square k, SquareList & attakers) { return k!=INVALIDSQUARE && BB::getAttackers(p, k, attakers);}

enum GenPhase { GP_all = 0, GP_cap = 1, GP_quiet = 2 };

template < GenPhase phase = GP_all >
void generateSquare(const Position & p, MoveList & moves, Square from){
    assert(from != INVALIDSQUARE);
    const Color side = p.c;
    const BitBoard myPieceBB  = p.allPieces[side];
    const BitBoard oppPieceBB = p.allPieces[~side];
    const Piece piece = p.b[from];
    const Piece ptype = (Piece)std::abs(piece);
    assert ( ptype != P_none ) ;
    static BitBoard(*const pf[])(const Square, const BitBoard, const Color) = { &BB::coverage<P_wp>, &BB::coverage<P_wn>, &BB::coverage<P_wb>, &BB::coverage<P_wr>, &BB::coverage<P_wq>, &BB::coverage<P_wk> };
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
                if ( p.castling & (C_wqs|C_wks) ){
                   bool e1Attacked = isAttacked(p,Sq_e1);
                   if ( (p.castling & C_wqs) && p.b[Sq_b1] == P_none && p.b[Sq_c1] == P_none && p.b[Sq_d1] == P_none && !isAttacked(p,Sq_c1) && !isAttacked(p,Sq_d1) && !e1Attacked) addMove(from, Sq_c1, T_wqs, moves); // wqs
                   if ( (p.castling & C_wks) && p.b[Sq_f1] == P_none && p.b[Sq_g1] == P_none && !e1Attacked && !isAttacked(p,Sq_f1) && !isAttacked(p,Sq_g1)) addMove(from, Sq_g1, T_wks, moves); // wks
                }
            }
            else if ( p.castling & (C_bqs|C_bks)){
                const bool e8Attacked = isAttacked(p,Sq_e8);
                if ( (p.castling & C_bqs) && p.b[Sq_b8] == P_none && p.b[Sq_c8] == P_none && p.b[Sq_d8] == P_none && !isAttacked(p,Sq_c8) && !isAttacked(p,Sq_d8) && !e8Attacked) addMove(from, Sq_c8, T_bqs, moves); // bqs
                if ( (p.castling & C_bks) && p.b[Sq_f8] == P_none && p.b[Sq_g8] == P_none && !e8Attacked && !isAttacked(p,Sq_f8) && !isAttacked(p,Sq_g8)) addMove(from, Sq_g8, T_bks, moves); // bks
            }
        }
    }
    else {
        BitBoard pawnmoves = 0ull;
        static const BitBoard rank1_or_rank8 = rank1 | rank8;
        if ( phase != GP_quiet) pawnmoves = BB::mask[from].pawnAttack[p.c] & ~myPieceBB & oppPieceBB;
        while (pawnmoves) {
            const Square to = BB::popBit(pawnmoves);
            if ( SquareToBitboard(to) & rank1_or_rank8 ) {
                addMove(from, to, T_cappromq, moves); // pawn capture with promotion
                addMove(from, to, T_cappromr, moves); // pawn capture with promotion
                addMove(from, to, T_cappromb, moves); // pawn capture with promotion
                addMove(from, to, T_cappromn, moves); // pawn capture with promotion
            } else addMove(from,to,T_capture,moves);
        }
        if ( phase != GP_cap) pawnmoves |= BB::mask[from].push[p.c] & ~p.occupancy;
        if ((phase != GP_cap) && (BB::mask[from].push[p.c] & p.occupancy) == 0ull) pawnmoves |= BB::mask[from].dpush[p.c] & ~p.occupancy;
        while (pawnmoves) {
            const Square to = BB::popBit(pawnmoves);
            if ( SquareToBitboard(to) & rank1_or_rank8 ) {
                addMove(from, to, T_promq, moves); // promotion Q
                addMove(from, to, T_promr, moves); // promotion R
                addMove(from, to, T_promb, moves); // promotion B
                addMove(from, to, T_promn, moves); // promotion N
            } else addMove(from,to,T_std,moves);
        }
        if ( p.ep != INVALIDSQUARE && phase != GP_quiet ) pawnmoves = BB::mask[from].pawnAttack[p.c] & ~myPieceBB & SquareToBitboard(p.ep);
        while (pawnmoves) addMove(from,BB::popBit(pawnmoves),T_ep,moves);
    }
}

template < GenPhase phase = GP_all >
void generate(const Position & p, MoveList & moves, bool doNotClear = false){
    if ( !doNotClear) moves.clear();
    BitBoard myPieceBBiterator = ( (p.c == Co_White) ? p.allPieces[Co_White] : p.allPieces[Co_Black]);
    while (myPieceBBiterator) generateSquare<phase>(p,moves,BB::popBit(myPieceBBiterator));
}

inline void movePiece(Position & p, Square from, Square to, Piece fromP, Piece toP, bool isCapture = false, Piece prom = P_none) {
    const int fromId   = fromP + PieceShift;
    const int toId     = toP + PieceShift;
    const Piece toPnew = prom != P_none ? prom : fromP;
    const int toIdnew  = prom != P_none ? (prom + PieceShift) : fromId;
    assert(from>=0 && from<64);
    assert(to>=0 && to<64);
    p.b[from] = P_none;
    p.b[to]   = toPnew;
    unSetBit(p, from, fromP);
    unSetBit(p, to,   toP); // usefull only if move is a capture
    setBit  (p, to,   toPnew);
    p.h ^= Zobrist::ZT[from][fromId]; // remove fromP at from
    if (isCapture) p.h ^= Zobrist::ZT[to][toId]; // if capture remove toP at to
    p.h ^= Zobrist::ZT[to][toIdnew]; // add fromP (or prom) at to
}

void applyNull(Position & pN) {
    pN.c = ~pN.c;
    pN.h ^= Zobrist::ZT[3][13];
    pN.h ^= Zobrist::ZT[4][13];
    pN.lastMove = INVALIDMOVE;
    if (pN.ep != INVALIDSQUARE) pN.h ^= Zobrist::ZT[pN.ep][13];
    pN.ep = INVALIDSQUARE;
}

bool apply(Position & p, const Move & m){

    assert(m != INVALIDMOVE);

    const Square from  = Move2From(m);
    const Square to    = Move2To(m);
    const MType  type  = Move2Type(m);
    const Piece  fromP = p.b[from];
    const Piece  toP   = p.b[to];

    const int fromId   = fromP + PieceShift;
    //const int toId     = toP + PieceShift;

    switch(type){
    case T_std:
    case T_capture:
    case T_check:
        updateMaterialStd(p,to);
        movePiece(p, from, to, fromP, toP, type == T_capture);

        // update castling rigths and king position
        if ( fromP == P_wk ){
            p.king[Co_White] = to;
            if (p.castling & C_wks) p.h ^= Zobrist::ZT[7][13];
            if (p.castling & C_wqs) p.h ^= Zobrist::ZT[0][13];
            p.castling &= ~(C_wks | C_wqs);
        }
        else if ( fromP == P_bk ){
            p.king[Co_Black] = to;
            if (p.castling & C_bks) p.h ^= Zobrist::ZT[63][13];
            if (p.castling & C_bqs) p.h ^= Zobrist::ZT[56][13];
            p.castling &= ~(C_bks | C_bqs);
        }
        // king capture : is that necessary ???
        if      ( toP == P_wk ) p.king[Co_White] = INVALIDSQUARE;
        else if ( toP == P_bk ) p.king[Co_Black] = INVALIDSQUARE;

        if ( fromP == P_wr && from == Sq_a1 && (p.castling & C_wqs)){
            p.castling &= ~C_wqs;
            p.h ^= Zobrist::ZT[0][13];
        }
        else if ( fromP == P_wr && from == Sq_h1 && (p.castling & C_wks) ){
            p.castling &= ~C_wks;
            p.h ^= Zobrist::ZT[7][13];
        }
        else if ( fromP == P_br && from == Sq_a8 && (p.castling & C_bqs)){
            p.castling &= ~C_bqs;
            p.h ^= Zobrist::ZT[56][13];
        }
        else if ( fromP == P_br && from == Sq_h8 && (p.castling & C_bks) ){
            p.castling &= ~C_bks;
            p.h ^= Zobrist::ZT[63][13];
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
        p.h ^= Zobrist::ZT[from][fromId]; // remove fromP at from
        p.h ^= Zobrist::ZT[epCapSq][(p.c == Co_White ? P_bp : P_wp) + PieceShift]; // remove captured pawn
        p.h ^= Zobrist::ZT[to][fromId]; // add fromP at to
        updateMaterialEp(p);
    }
        break;

    case T_promq:
    case T_cappromq:
        updateMaterialProm(p,to,type);
        movePiece(p, from, to, fromP, toP, type == T_cappromq,(p.c == Co_White ? P_wq : P_bq));
        break;
    case T_promr:
    case T_cappromr:
        updateMaterialProm(p,to,type);
        movePiece(p, from, to, fromP, toP, type == T_cappromr, (p.c == Co_White ? P_wr : P_br));
        break;
    case T_promb:
    case T_cappromb:
        updateMaterialProm(p,to,type);
        movePiece(p, from, to, fromP, toP, type == T_cappromb, (p.c == Co_White ? P_wb : P_bb));
        break;
    case T_promn:
    case T_cappromn:
        updateMaterialProm(p,to,type);
        movePiece(p, from, to, fromP, toP, type == T_cappromn, (p.c == Co_White ? P_wn : P_bn));
        break;
    case T_wks:
        movePiece(p, Sq_e1, Sq_g1, P_wk, P_none);
        movePiece(p, Sq_h1, Sq_f1, P_wr, P_none);
        p.king[Co_White] = Sq_g1;
        if (p.castling & C_wqs) p.h ^= Zobrist::ZT[0][13];
        if (p.castling & C_wks) p.h ^= Zobrist::ZT[7][13];
        p.castling &= ~(C_wks | C_wqs);
        break;
    case T_wqs:
        movePiece(p, Sq_e1, Sq_c1, P_wk, P_none);
        movePiece(p, Sq_a1, Sq_d1, P_wr, P_none);
        p.king[Co_White] = Sq_c1;
        if (p.castling & C_wqs) p.h ^= Zobrist::ZT[0][13];
        if (p.castling & C_wks) p.h ^= Zobrist::ZT[7][13];
        p.castling &= ~(C_wks | C_wqs);
        break;
    case T_bks:
        movePiece(p, Sq_e8, Sq_g8, P_bk, P_none);
        movePiece(p, Sq_h8, Sq_f8, P_br, P_none);
        p.b[Sq_e8] = P_none; p.b[Sq_f8] = P_br; p.b[Sq_g8] = P_bk; p.b[Sq_h8] = P_none;
        p.king[Co_Black] = Sq_g8;
        if (p.castling & C_bqs) p.h ^= Zobrist::ZT[56][13];
        if (p.castling & C_bks) p.h ^= Zobrist::ZT[63][13];
        p.castling &= ~(C_bks | C_bqs);
        break;
    case T_bqs:
        movePiece(p, Sq_e8, Sq_c8, P_bk, P_none);
        movePiece(p, Sq_a8, Sq_d8, P_br, P_none);
        p.king[Co_Black] = Sq_c8;
        if (p.castling & C_bqs) p.h ^= Zobrist::ZT[56][13];
        if (p.castling & C_bks) p.h ^= Zobrist::ZT[63][13];
        p.castling &= ~(C_bks | C_bqs);
        break;
    }

    p.allPieces[Co_White] = p.whitePawn() | p.whiteKnight() | p.whiteBishop() | p.whiteRook() | p.whiteQueen() | p.whiteKing();
    p.allPieces[Co_Black] = p.blackPawn() | p.blackKnight() | p.blackBishop() | p.blackRook() | p.blackQueen() | p.blackKing();
    p.occupancy = p.allPieces[Co_White] | p.allPieces[Co_Black];

    if ( isAttacked(p,kingSquare(p)) ) return false; // this is the only legal move validation needed

    // Update castling right if rook captured
    if ( toP == P_wr && to == Sq_a1 && (p.castling & C_wqs) ){
        p.castling &= ~C_wqs;
        p.h ^= Zobrist::ZT[0][13];
    }
    else if ( toP == P_wr && to == Sq_h1 && (p.castling & C_wks) ){
        p.castling &= ~C_wks;
        p.h ^= Zobrist::ZT[7][13];
    }
    else if ( toP == P_br && to == Sq_a8 && (p.castling & C_bqs)){
        p.castling &= ~C_bqs;
        p.h ^= Zobrist::ZT[56][13];
    }
    else if ( toP == P_br && to == Sq_h8 && (p.castling & C_bks)){
        p.castling &= ~C_bks;
        p.h ^= Zobrist::ZT[63][13];
    }

    // update EP
    if (p.ep != INVALIDSQUARE) p.h ^= Zobrist::ZT[p.ep][13];
    p.ep = INVALIDSQUARE;
    if ( abs(fromP) == P_wp && abs(to-from) == 16 ) p.ep = (from + to)/2;
    assert(p.ep == INVALIDSQUARE || (SQRANK(p.ep) == 2 || SQRANK(p.ep) == 5));
    if (p.ep != INVALIDSQUARE) p.h ^= Zobrist::ZT[p.ep][13];

    p.c = ~p.c;
    p.h ^= Zobrist::ZT[3][13]; p.h ^= Zobrist::ZT[4][13];

    if ( toP != P_none || abs(fromP) == P_wp ) p.fifty = 0;
    else ++p.fifty;
    if ( p.c == Co_White ) ++p.moves;
    ++p.halfmoves;

    updateMaterialOther(p);
//#define DEBUG_MATERIAL
#ifdef DEBUG_MATERIAL
    Position::Material mat = p.mat;
    initMaterial(p);
    if ( p.mat != mat ){ Logging::LogIt(Logging::logFatal) << "Material update error";}
#endif
    p.lastMove = m;

    return true;
}

namespace Book {
template<typename T> struct bits_t { T t; };
template<typename T> bits_t<T&> bits(T &t) { return bits_t<T&>{t}; }
template<typename T> bits_t<const T&> bits(const T& t) { return bits_t<const T&>{t};}
template<typename S, typename T> S& operator<<(S &s, bits_t<T>  b) { s.write((char*)&b.t, sizeof(T)); return s;}
template<typename S, typename T> S& operator>>(S& s, bits_t<T&> b) { s.read ((char*)&b.t, sizeof(T)); return s;}

std::unordered_map<Hash, std::set<Move> > book;

bool fileExists(const std::string& name){ return std::ifstream(name.c_str()).good(); }

bool readBinaryBook(std::ifstream & stream) {
    Position ps;
    readFEN(startPosition,ps,true);
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
    Logging::LogIt(Logging::logInfo) << "Book hit";
    return *select_randomly(it->second.begin(),it->second.end());
}

void initBook() {
    if (DynamicConfig::book ) {
        if (DynamicConfig::bookFile.empty()) { Logging::LogIt(Logging::logWarn) << "json entry bookFile is empty, cannot load book"; }
        else {
            if (Book::fileExists(DynamicConfig::bookFile)) {
                Logging::LogIt(Logging::logInfo) << "Loading book ...";
                std::ifstream bbook(DynamicConfig::bookFile, std::ios::in | std::ios::binary);
                Book::readBinaryBook(bbook);
                Logging::LogIt(Logging::logInfo) << "... done";
            }
            else { Logging::LogIt(Logging::logWarn) << "book file " << DynamicConfig::bookFile << " not found, cannot load book"; }
        }
    }
}

#ifdef IMPORTBOOK
#include "Add-On/bookGenerationTools.cc"
#endif

} // Book

/*
struct SortThreatsFunctor {
    const Position & _p;
    SortThreatsFunctor(const Position & p):_p(p){}
    bool operator()(const Square s1,const Square s2) { return std::abs(getValue(_p,s1)) < std::abs(getValue(_p,s2));}
};
*/

// Static Exchange Evaluation (cutoff version algorithm from stockfish)
bool ThreadContext::SEE(const Position & p, const Move & m, ScoreType threshold) const{
    // Only deal with normal moves
    //if (! isCapture(m)) return true;
    const Square from = Move2From(m);
    const Square to   = Move2To(m);
    const bool promPossible = (SQRANK(to) == 0 || SQRANK(to) == 7);
    Piece nextVictim  = getPieceType(p,from);
    const Color us    = Colors[getPieceIndex(p,from)];
    ScoreType balance = std::abs(getValue(p,to)) - threshold; // The opponent may be able to recapture so this is the best result we can hope for.
    if (balance < 0) return false;
    balance -= Values[nextVictim+PieceShift]; // Now assume the worst possible result: that the opponent can capture our piece for free.
    if (balance >= 0) return true;
    if (getPieceType(p, to) == P_wk) return false; // capture king !
    Position p2 = p;
    if (!apply(p2, m)) return false;
    SquareList stmAttackers;
    bool endOfSEE = false;
    while (!endOfSEE){
        p2.c = ~p2.c;
        bool threatsFound = getAttackers(p2, to, stmAttackers);
        p2.c = ~p2.c;
        if (!threatsFound) break;
        //std::sort(stmAttackers.begin(),stmAttackers.end(),SortThreatsFunctor(p2)); // already sorted in getAttackers
        bool validThreatFound = false;
        unsigned int threatId = 0;
        while (!validThreatFound && threatId < stmAttackers.size()) {
            const Square att = stmAttackers[threatId];
            const bool prom = promPossible && getPieceType(p, att) == P_wp;
            const Move mm = ToMove(att, to, prom ? T_cappromq : T_capture);
            nextVictim = (Piece)(prom ? P_wq : getPieceType(p2,att)); // CAREFULL here :: we don't care black or white, always use abs(value) next !!!
            if (nextVictim == P_wk) return false; // capture king !
            ++threatId;
            if ( ! apply(p2,mm) ) continue;
            validThreatFound = true;
            balance = -balance - 1 - Values[nextVictim+PieceShift];
            if (balance >= 0) {
                if (nextVictim == P_wk) p2.c = ~p2.c;
                endOfSEE = true;
            }
        }
        if (!validThreatFound) endOfSEE = true;
    }
    return us != p2.c; // we break the above loop when stm loses
}

bool sameMove(const Move & a, const Move & b) { return (a & 0x0000FFFF) == (b & 0x0000FFFF);}

struct MoveSorter{

    MoveSorter(const ThreadContext & context, const Position & p, bool useSEE = true, bool isInCheck = false, const TT::Entry * e = NULL):context(context),p(p),useSEE(useSEE),isInCheck(isInCheck),e(e){ assert(e==0||e->h!=0||e->m==INVALIDMOVE); }

    void computeScore(Move & m)const{
        const MType  t    = Move2Type(m);
        const Square from = Move2From(m);
        const Square to   = Move2To(m);
        ScoreType s = MoveScoring[t];
        if (e && sameMove(e->m,m)) s += 10000;
        else if (isInCheck && getPieceType(p, from) == P_wk) s += 8500;
        if (isCapture(t)){
            s += StaticConfig::MvvLvaScores[getPieceType(p,to)-1][getPieceType(p,from)-1];
            if ( useSEE && !context.SEE(p,m,0)) s -= 2*MoveScoring[T_capture];
        }
        else if ( t == T_std){
            if      (sameMove(m, context.killerT.killers[0][p.halfmoves])) s += 1600;
            else if (sameMove(m, context.killerT.killers[1][p.halfmoves])) s += 1500;
            else if (p.halfmoves > 1 && sameMove(m, context.killerT.killers[0][p.halfmoves-2])) s += 1400;
            else if (p.lastMove!=INVALIDMOVE && sameMove(context.counterT.counter[Move2From(p.lastMove)][Move2To(p.lastMove)],m)) s+= 1300;
            else s += context.historyT.history[getPieceIndex(p, from)][to];
            const bool isWhite = (p.allPieces[Co_White] & SquareToBitboard(from)) != 0ull;
            s += EvalConfig::PST[getPieceType(p, from) - 1][isWhite ? (to ^ 56) : to] - EvalConfig::PST[getPieceType(p, from) - 1][isWhite ? (from ^ 56) : from];
        }
        m = ToMove(from, to, t, s);
    }

    bool operator()(const Move & a, const Move & b)const{ assert( a != INVALIDMOVE); assert( b != INVALIDMOVE); return Move2Score(a) > Move2Score(b); }

    const Position & p;
    const TT::Entry * e;
    const ThreadContext & context;
    const bool useSEE;
    const bool isInCheck;
};

void sort(const ThreadContext & context, MoveList & moves, const Position & p, bool useSEE = true, bool isInCheck = false, const TT::Entry * e = NULL){
    const MoveSorter ms(context,p,useSEE,isInCheck,e);
    for(auto it = moves.begin() ; it != moves.end() ; ++it){ ms.computeScore(*it); }
    std::sort(moves.begin(),moves.end(),ms);
}

inline bool ThreadContext::isRep(const Position & p, bool isPV)const{
    const int limit = isPV ? 3 : 1;
    if ( p.fifty < (2*limit-1) ) return false;
    int count = 0;
    const Hash h = computeHash(p);
    for (int k = p.halfmoves - 1; k >= 0; --k) {
        if (hashStack[k] == 0ull) break;
        if (hashStack[k] == h) ++count;
        if (count >= limit) return true;
    }
    return false;
}

template< bool withRep, bool isPV, bool INR>
MaterialHash::Terminaison ThreadContext::interiorNodeRecognizer(const Position & p)const{
    if (withRep && isRep(p,isPV)) return MaterialHash::Ter_Draw;
    if (p.fifty >= 100)           return MaterialHash::Ter_Draw;
    if ( INR && (p.mat[Co_White][M_p] + p.mat[Co_Black][M_p]) < 2 ) return MaterialHash::probeMaterialHashTable(p.mat);
    return MaterialHash::Ter_Unknown;
}

double sigmoid(double x, double m = 1.f, double trans = 0.f, double scale = 1.f, double offset = 0.f){ return m / (1 + exp((trans - x) / scale)) - offset;}
void initEval(){ for(int i = 0; i < 64; i++){ EvalConfig::kingAttTable[i] = (int) sigmoid(i,EvalConfig::kingAttMax,EvalConfig::kingAttTrans,EvalConfig::kingAttScale,EvalConfig::kingAttOffset); } } // idea taken from Topple

struct EvalScore{
    enum eScores : unsigned char{ sc_Mat = 0, sc_PST, sc_MOB, sc_ATT, sc_Pwn, sc_PwnShield, sc_PwnPassed, sc_PwnIsolated, sc_PwnDoubled, sc_PwnPush, sc_Blocked, sc_Adjust, sc_OpenFile, sc_EndGame, sc_Exchange, sc_Tempo, sc_max };
    float scalingFactor = 1;
    ScoreType scores[sc_max] = {0};  ScoreType scoresEG[sc_max] = {0};  ScoreType scoresScaled[sc_max] = {0};
    ScoreType Score(const Position &p, float gp){
        ScoreType sc = 0, scEG = 0, scScaled = 0;
        for(int k = 0 ; k < sc_max ; ++k){ sc += scores[k]; scEG += scoresEG[k]; scScaled += scoresScaled[k]; }
        return ScoreType((gp*sc + (1.f-gp)*scEG + scScaled)*scalingFactor*std::min(1.f,(110-p.fifty)/100.f));
    }
    void Display(const Position &p, float gp){
        static const std::string scNames[sc_max] = { "Mat", "PST", "MOB", "Att", "Pwn", "PwnShield", "PwnPassed", "PwnIsolated", "PwnDoubled", "PwnPush", "Blocked", "Adjust", "OpenFile", "EndGame", "Exchange", "Tempo"};
        ScoreType sc = 0, scEG = 0, scScaled = 0;
        for(int k = 0 ; k < sc_max ; ++k){
            Logging::LogIt(Logging::logInfo) << scNames[k] << "       " << scores[k];
            Logging::LogIt(Logging::logInfo) << scNames[k] << "EG     " << scoresEG[k];
            Logging::LogIt(Logging::logInfo) << scNames[k] << "Scaled " << scoresScaled[k];
            sc += scores[k];  scEG += scoresEG[k];  scScaled += scoresScaled[k];
        }
        Logging::LogIt(Logging::logInfo) << "Score  " << sc;
        Logging::LogIt(Logging::logInfo) << "EG     " << scEG;
        Logging::LogIt(Logging::logInfo) << "Scaled " << scScaled;
        Logging::LogIt(Logging::logInfo) << "Scaling factor " << scalingFactor;
        Logging::LogIt(Logging::logInfo) << "Game phase " << gp;
        Logging::LogIt(Logging::logInfo) << "Fifty  " << std::min(1.f,(110-p.fifty)/100.f);
        Logging::LogIt(Logging::logInfo) << "Total  " << (gp*sc + (1.f-gp)*scEG + scScaled)*scalingFactor*std::min(1.f,(110-p.fifty)/100.f);
    }
};

namespace{ // some Color / Piece helpers
   BitBoard(*const pf[])(const Square, const BitBoard, const  Color) =  { &BB::coverage<P_wp>, &BB::coverage<P_wn>, &BB::coverage<P_wb>, &BB::coverage<P_wr>, &BB::coverage<P_wq>, &BB::coverage<P_wk> };
   template<Color C> inline bool isPasser(const Position &p, Square k)  { return (BB::mask[k].passerSpan[C] & p.pieces<P_wp>(~C)) == 0ull;}
   template<Color C> inline Square ColorSquarePstHelper(Square k)       { return C==Co_White?(k^56):k;}
   template<Color C> inline constexpr Square ColorSignHelper()          { return C==Co_White?+1:-1;}
   template<Color C> inline const BitBoard forwardWest(const BitBoard b){ return C==Co_White? BB::shiftSouthWest(b) : BB::shiftNorthWest(b);}
   template<Color C> inline const BitBoard forwardEast(const BitBoard b){ return C==Co_White? BB::shiftSouthEast(b) : BB::shiftNorthEast(b);}
   template<Color C> inline const Square PromotionSquare(const Square k){ return C==Co_White? (SQFILE(k) + 56) : SQFILE(k);}
   template<Color C> inline const Rank ColorRank(const Square k)        { return Rank(C==Co_White? SQRANK(k) : (7-SQRANK(k)));}
}

template < Piece T , Color C>
inline void evalPiece(const Position & p, BitBoard pieceBBiterator, EvalScore & score, BitBoard & att, ScoreType (& danger)[2]){
    const BitBoard kingZone[2] = { BB::mask[p.king[Co_White]].kingZone, BB::mask[p.king[Co_Black]].kingZone};
    while (pieceBBiterator) {
        const Square k = BB::popBit(pieceBBiterator);
        const Square kk = ColorSquarePstHelper<C>(k);
        score.scores[EvalScore::sc_PST]   += ColorSignHelper<C>()*EvalConfig::PST  [T-1][kk];
        score.scoresEG[EvalScore::sc_PST] += ColorSignHelper<C>()*EvalConfig::PSTEG[T-1][kk];
        const BitBoard target = pf[T-1](k, p.occupancy ^ p.pieces<T>(C), p.c); // aligned threats also taken into account
        danger[C]  -= ScoreType(countBit(target & kingZone[C])  * EvalConfig::kingAttWeight[EvalConfig::katt_defence][T]);
        danger[~C] += ScoreType(countBit(target & kingZone[~C]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack][T]);
        const BitBoard curAtt = target & ~p.allPieces[C];
        att |= curAtt;
        const uint64_t n = countBit(curAtt);
        score.scores[EvalScore::sc_MOB]   += ColorSignHelper<C>()*EvalConfig::MOB  [T-1][n];
        score.scoresEG[EvalScore::sc_MOB] += ColorSignHelper<C>()*EvalConfig::MOBEG[T-1][n];
    }
}

template< Color C>
inline void evalPawn(const Position & p, BitBoard pieceBBiterator, EvalScore & score, BitBoard & att, BitBoard & safePPush, BitBoard & pawnTargets, BitBoard & passer, float gp, float gpCompl){
    while (pieceBBiterator) {
        const Square k = BB::popBit(pieceBBiterator);
        safePPush |= BB::mask[k].push[C] & ~p.occupancy;
        pawnTargets |= BB::mask[k].pawnAttack[C] /*& ~p.allPieces[C]*/;
        att |= BB::mask[k].pawnAttack[C] & ~p.allPieces[C];
        if (isPasser<C>(p,k)) {
            const BitBoard bbk = SquareToBitboard(k);
            passer |= bbk;
            const BitBoard fw = forwardWest<C>(bbk);
            const BitBoard fe = forwardEast<C>(bbk);
            const float factorProtected = 1+( (((fw&p.pieces<P_wp>(C))!=0ull)&&isPasser<C>(p, BB::SquareFromBitBoard(fw))) || (((fe&p.pieces<P_wp>(C))!=0ull)&&isPasser<C>(p, BB::SquareFromBitBoard(fe))) ) * EvalConfig::protectedPasserFactor/100.f;
            const float factorFree      = 1+((BB::mask[k].frontSpan[C] & p.allPieces[~C]) == 0ull) * EvalConfig::freePasserFactor/100.f;
            const float kingNearBonus   = EvalConfig::kingNearPassedPawnEG * gpCompl * (chebyshevDistance(p.king[~C], k) - chebyshevDistance(p.king[C], k));
            const bool unstoppable      = (p.mat[~C][M_t] == 0)&&((chebyshevDistance(p.king[~C],PromotionSquare<C>(k))-int(p.c!=C)) > std::min(5, chebyshevDistance(PromotionSquare<C>(k),k)));
            const ScoreType rookBehind  = ScoreType(countBit(p.pieces<P_wr>(C) & BB::mask[k].rearSpan[C]) - countBit(p.pieces<P_wr>(~C) & BB::mask[k].rearSpan[C])) * EvalConfig::rookBehindPassed;
            if (unstoppable) score.scoresScaled[EvalScore::sc_PwnPassed] += ColorSignHelper<C>()*(Values[P_wq+PieceShift] - Values[P_wp+PieceShift]);
            else             score.scoresScaled[EvalScore::sc_PwnPassed] += ColorSignHelper<C>()*ScoreType( factorProtected * factorFree * (gp*EvalConfig::passerBonus[ColorRank<C>(k)] + gpCompl*EvalConfig::passerBonusEG[ColorRank<C>(k)]) + kingNearBonus + gpCompl*rookBehind);
        }
    }
}

template< Color C>
inline void evalPawnDanger(const Position & p, const BitBoard (& att)[2], ScoreType (& danger)[2]){
    danger[C] -= int(countBit(BB::mask[p.king[C]].kingZone & p.pieces<P_wp>(C)))            * EvalConfig::kingAttWeight[EvalConfig::katt_defence][P_wp];
    danger[C] += int(countBit(BB::mask[p.king[C]].kingZone & p.pieces<P_wp>(~C) & att[~C])) * EvalConfig::kingAttWeight[EvalConfig::katt_attack] [P_wp];
}

///@todo reward safe checks
///@todo threat by protected pawn (and after push)
///@todo threat on the queen
///@todo storm

template < bool display, bool safeMatEvaluator >
ScoreType eval(const Position & p, float & gp ){
    EvalScore score;
    static const ScoreType dummyScore = 0;
    static const ScoreType *absValues[7]   = { &dummyScore, &Values  [P_wp + PieceShift], &Values  [P_wn + PieceShift], &Values  [P_wb + PieceShift], &Values  [P_wr + PieceShift], &Values  [P_wq + PieceShift], &Values  [P_wk + PieceShift] };
    static const ScoreType *absValuesEG[7] = { &dummyScore, &ValuesEG[P_wp + PieceShift], &ValuesEG[P_wn + PieceShift], &ValuesEG[P_wb + PieceShift], &ValuesEG[P_wr + PieceShift], &ValuesEG[P_wq + PieceShift], &ValuesEG[P_wk + PieceShift] };

    // game phase
    ///@todo tune gp computation, but care it seems to have a huge impact
    const float totalMatScore = 2.f * *absValues[P_wq] + 4.f * *absValues[P_wr] + 4.f * *absValues[P_wb] + 4.f * *absValues[P_wn] + 16.f * *absValues[P_wp];
    const ScoreType matPieceScoreW = p.mat[Co_White][M_q] * *absValues[P_wq] + p.mat[Co_White][M_r] * *absValues[P_wr] + p.mat[Co_White][M_b] * *absValues[P_wb] + p.mat[Co_White][M_n] * *absValues[P_wn];
    const ScoreType matPieceScoreB = p.mat[Co_Black][M_q] * *absValues[P_wq] + p.mat[Co_Black][M_r] * *absValues[P_wr] + p.mat[Co_Black][M_b] * *absValues[P_wb] + p.mat[Co_Black][M_n] * *absValues[P_wn];
    const ScoreType matPawnScoreW  = p.mat[Co_White][M_p] * *absValues[P_wp];
    const ScoreType matPawnScoreB  = p.mat[Co_Black][M_p] * *absValues[P_wp];
    const ScoreType matScoreW = matPieceScoreW + matPawnScoreW;
    const ScoreType matScoreB = matPieceScoreB + matPawnScoreB;
    //const ScoreType matPiece = matPieceScoreW - matPieceScoreB;
    //const ScoreType matPawn  = matPawnScoreW - matPawnScoreB;
    gp = ( matScoreW + matScoreB ) / totalMatScore;
    const float gpCompl = 1.f - gp;

    // king captured
    const bool white2Play = p.c == Co_White;
    if ( p.king[Co_White] == INVALIDSQUARE ) return (white2Play?-1:+1)* MATE;
    if ( p.king[Co_Black] == INVALIDSQUARE ) return (white2Play?+1:-1)* MATE;

    // EG material (symetric version)
    score.scoresEG[EvalScore::sc_Mat] += (p.mat[Co_White][M_q] - p.mat[Co_Black][M_q]) * *absValuesEG[P_wq] + (p.mat[Co_White][M_r] - p.mat[Co_Black][M_r]) * *absValuesEG[P_wr] + (p.mat[Co_White][M_b] - p.mat[Co_Black][M_b]) * *absValuesEG[P_wb] + (p.mat[Co_White][M_n] - p.mat[Co_Black][M_n]) * *absValuesEG[P_wn] + (p.mat[Co_White][M_p] - p.mat[Co_Black][M_p]) * *absValuesEG[P_wp];
    const Color winningSide = score.scoresEG[EvalScore::sc_Mat]>0?Co_White:Co_Black;

    // material (symetric version)
    score.scores[EvalScore::sc_Mat] += matScoreW - matScoreB;

    // pst & mobility & attack
    ScoreType danger[2]     = {0, 0};
    BitBoard att[2]         = {0ull, 0ull};
    BitBoard safePPush[2]   = {0ull, 0ull};
    BitBoard pawnTargets[2] = {0ull, 0ull};
    BitBoard passer[2]      = {0ull, 0ull};

    // PST, mobility, king zone attack, passer, for white
    evalPiece<P_wn,Co_White>(p,p.pieces<P_wn>(Co_White),score,att[Co_White],danger);
    evalPiece<P_wb,Co_White>(p,p.pieces<P_wb>(Co_White),score,att[Co_White],danger);
    evalPiece<P_wr,Co_White>(p,p.pieces<P_wr>(Co_White),score,att[Co_White],danger);
    evalPiece<P_wq,Co_White>(p,p.pieces<P_wq>(Co_White),score,att[Co_White],danger);
    evalPiece<P_wk,Co_White>(p,p.pieces<P_wk>(Co_White),score,att[Co_White],danger);
    evalPiece<P_wn,Co_Black>(p,p.pieces<P_wn>(Co_Black),score,att[Co_Black],danger);
    evalPiece<P_wb,Co_Black>(p,p.pieces<P_wb>(Co_Black),score,att[Co_Black],danger);
    evalPiece<P_wr,Co_Black>(p,p.pieces<P_wr>(Co_Black),score,att[Co_Black],danger);
    evalPiece<P_wq,Co_Black>(p,p.pieces<P_wq>(Co_Black),score,att[Co_Black],danger);
    evalPiece<P_wk,Co_Black>(p,p.pieces<P_wk>(Co_Black),score,att[Co_Black],danger);

    // end game knowledge (helper or scaling)
    if ( safeMatEvaluator && gp < 0.3f ){
       const Hash matHash = MaterialHash::getMaterialHash(p.mat);
       const MaterialHash::Terminaison ter = MaterialHash::materialHashTable[matHash];
       if ( ter == MaterialHash::Ter_WhiteWinWithHelper || ter == MaterialHash::Ter_BlackWinWithHelper ) return (white2Play?+1:-1)*(MaterialHash::helperTable[matHash](p,winningSide,score.scoresEG[EvalScore::sc_Mat]));
       else if ( ter == MaterialHash::Ter_WhiteWin || ter == MaterialHash::Ter_BlackWin) score.scalingFactor = 5 - 5*p.fifty/100.f;
       else if ( ter == MaterialHash::Ter_HardToWin)   score.scalingFactor = 0.5f - 0.5f*(p.fifty/100.f);
       else if ( ter == MaterialHash::Ter_LikelyDraw ) score.scalingFactor = 0.3f - 0.3f*(p.fifty/100.f);
       ///@todo next two seem to lose elo
       //else if ( ter == MaterialHash::Ter_Draw){         if ( !isAttacked(p,kingSquare(p)) ) return 0;}
       else if ( ter == MaterialHash::Ter_MaterialDraw){ if ( !isAttacked(p,kingSquare(p)) ) return 0;} ///@todo also verify stalemate ?
    }

    // pawn first pass
    evalPawn<Co_White>(p,p.pieces<P_wp>(Co_White),score,att[Co_White],safePPush[Co_White],pawnTargets[Co_White],passer[Co_White],gp,gpCompl);
    evalPawn<Co_Black>(p,p.pieces<P_wp>(Co_Black),score,att[Co_Black],safePPush[Co_Black],pawnTargets[Co_Black],passer[Co_Black],gp,gpCompl);
    // pawn second pass (needs att)
    evalPawnDanger<Co_White>(p,att,danger);
    evalPawnDanger<Co_Black>(p,att,danger);

    // count pawn per file
    ///@todo use a cache for that ?!
    const BitBoard pawns[2] = {p.whitePawn(), p.blackPawn()};
    uint64_t nbWP[10] = {0ull}, nbBP[10] = {0ull};
    for(int f = File_a; f <= File_h ; ++f){
        nbWP[f+1] = countBit(pawns[Co_White] & files[f]);
        nbBP[f+1] = countBit(pawns[Co_Black] & files[f]);
        const uint64_t nbWR = countBit(p.whiteRook() & files[f]);
        const uint64_t nbBR = countBit(p.blackRook() & files[f]);
        // double pawn malus
        score.scores  [EvalScore::sc_PwnDoubled] -= ScoreType(nbWP[f+1]>>1)*EvalConfig::doublePawnMalus;
        score.scoresEG[EvalScore::sc_PwnDoubled] -= ScoreType(nbWP[f+1]>>1)*EvalConfig::doublePawnMalusEG;
        score.scores  [EvalScore::sc_PwnDoubled] += ScoreType(nbBP[f+1]>>1)*EvalConfig::doublePawnMalus;
        score.scoresEG[EvalScore::sc_PwnDoubled] += ScoreType(nbBP[f+1]>>1)*EvalConfig::doublePawnMalusEG;

        // danger if open file near king and rook(s) on the board
        if (p.blackRook() && (BB::mask[p.king[Co_White]].kingZone & files[f]) ){
            if      (!nbWP[f+1]){ danger[Co_White] += (!nbBP[f+1]) ? EvalConfig::kingAttOpenfile : EvalConfig::kingAttSemiOpenfileOur; }
            else if (!nbBP[f+1]){ danger[Co_White] += EvalConfig::kingAttSemiOpenfileOpp; }
        }
        if (p.whiteRook() && (BB::mask[p.king[Co_Black]].kingZone & files[f]) ){
            if      (!nbBP[f+1]){ danger[Co_Black] += (!nbWP[f+1]) ? EvalConfig::kingAttOpenfile : EvalConfig::kingAttSemiOpenfileOur; }
            else if (!nbWP[f+1]){ danger[Co_Black] += EvalConfig::kingAttSemiOpenfileOpp; }
        }

        // rook on open file
        if ( nbWR ){
            if      ( nbWP[f+1] == 0 ) score.scores[EvalScore::sc_OpenFile] += ScoreType(nbWR*(nbBP[f+1] == 0 ? EvalConfig::rookOnOpenFile:EvalConfig::rookOnOpenSemiFileOur));
            else if ( nbBP[f+1] == 0 ) score.scores[EvalScore::sc_OpenFile] += ScoreType(nbWR*EvalConfig::rookOnOpenSemiFileOpp);
        }
        if ( nbBR ){
            if      ( nbBP[f+1] == 0 ) score.scores[EvalScore::sc_OpenFile] -= ScoreType(nbBR*(nbWP[f+1] == 0 ? EvalConfig::rookOnOpenFile:EvalConfig::rookOnOpenSemiFileOur));
            else if ( nbWP[f+1] == 0 ) score.scores[EvalScore::sc_OpenFile] -= ScoreType(nbBR*EvalConfig::rookOnOpenSemiFileOpp);
        }
    }

    // isolated pawn malus (second loop needed)
    for(int f = File_a; f <= File_h ; ++f){
        score.scores  [EvalScore::sc_PwnIsolated] -= (!nbWP[f] && nbWP[f+1] && !nbWP[f+2])*EvalConfig::isolatedPawnMalus;
        score.scoresEG[EvalScore::sc_PwnIsolated] -= (!nbWP[f] && nbWP[f+1] && !nbWP[f+2])*EvalConfig::isolatedPawnMalusEG;
        score.scores  [EvalScore::sc_PwnIsolated] += (!nbBP[f] && nbBP[f+1] && !nbBP[f+2])*EvalConfig::isolatedPawnMalus;
        score.scoresEG[EvalScore::sc_PwnIsolated] += (!nbBP[f] && nbBP[f+1] && !nbBP[f+2])*EvalConfig::isolatedPawnMalusEG;
    }

    // use king danger score. **DO NOT** apply this in end-game
    score.scores[EvalScore::sc_ATT] -=  EvalConfig::kingAttTable[std::min(std::max(danger[Co_White],ScoreType(0)),ScoreType(63))];
    score.scores[EvalScore::sc_ATT] +=  EvalConfig::kingAttTable[std::min(std::max(danger[Co_Black],ScoreType(0)),ScoreType(63))];

    // safe pawn push (protected once or not attacked)
    safePPush[Co_White] &= (att[Co_White] | ~att[Co_Black]);
    safePPush[Co_Black] &= (att[Co_Black] | ~att[Co_White]);
    score.scores[EvalScore::sc_PwnPush]   += ScoreType(countBit(safePPush[Co_White]) - countBit(safePPush[Co_Black])) * EvalConfig::pawnMobility[0];
    score.scoresEG[EvalScore::sc_PwnPush] += ScoreType(countBit(safePPush[Co_White]) - countBit(safePPush[Co_Black])) * EvalConfig::pawnMobility[1];

    // in very end game winning king must be near the other king ///@todo shall be removed if material helpers work ...
    if ((p.mat[Co_White][M_p] + p.mat[Co_Black][M_p] == 0) && p.king[Co_White] != INVALIDSQUARE && p.king[Co_Black] != INVALIDSQUARE) score.scoresEG[EvalScore::sc_EndGame] -= ScoreType((score.scoresEG[EvalScore::sc_EndGame]>0?+1:-1)*(chebyshevDistance(p.king[Co_White], p.king[Co_Black])-2)*15);

    /*
    // blocked piece ///@todo
    // white
    // bishop blocked by own pawn
    ScoreType scBlocked = 0;
    if ( (p.whiteBishop() & BBSq_c1) && (pawns[Co_White] & BBSq_d2) && (p.occupancy & BBSq_d3) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishopByPawn;
    if ( (p.whiteBishop() & BBSq_f1) && (pawns[Co_White] & BBSq_e2) && (p.occupancy & BBSq_e3) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishopByPawn;

    // trapped knight
    if ( (p.whiteKnight() & BBSq_a8) && ( (pawns[Co_Black] & BBSq_a7) || (pawns[Co_Black] & BBSq_c7) ) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedKnight;
    if ( (p.whiteKnight() & BBSq_h8) && ( (pawns[Co_Black] & BBSq_h7) || (pawns[Co_Black] & BBSq_f7) ) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedKnight;
    if ( (p.whiteKnight() & BBSq_a7) && ( (pawns[Co_Black] & BBSq_a6) || (pawns[Co_Black] & BBSq_b7) ) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedKnight2;
    if ( (p.whiteKnight() & BBSq_h7) && ( (pawns[Co_Black] & BBSq_h6) || (pawns[Co_Black] & BBSq_g7) ) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedKnight2;

    // trapped bishop
    if ( (p.whiteBishop() & BBSq_a7) && (pawns[Co_Black] & BBSq_b6) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishop;
    if ( (p.whiteBishop() & BBSq_h7) && (pawns[Co_Black] & BBSq_g6) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishop;
    if ( (p.whiteBishop() & BBSq_b8) && (pawns[Co_Black] & BBSq_c7) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishop2;
    if ( (p.whiteBishop() & BBSq_g8) && (pawns[Co_Black] & BBSq_f7) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishop2;
    if ( (p.whiteBishop() & BBSq_a6) && (pawns[Co_Black] & BBSq_b5) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishop3;
    if ( (p.whiteBishop() & BBSq_h6) && (pawns[Co_Black] & BBSq_g5) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishop3;

    // bishop near castled king (bonus)
    if ( (p.whiteBishop() & BBSq_f1) && (p.whiteKing() & BBSq_g1) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::returningBishopBonus;
    if ( (p.whiteBishop() & BBSq_c1) && (p.whiteKing() & BBSq_b1) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::returningBishopBonus;

    // king blocking rook
    if ( ( (p.whiteKing() & BBSq_f1) || (p.whiteKing() & BBSq_g1) ) && ( (p.whiteRook() & BBSq_h1) || (p.whiteRook() & BBSq_g1) ) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedRookByKing;
    if ( ( (p.whiteKing() & BBSq_c1) || (p.whiteKing() & BBSq_b1) ) && ( (p.whiteRook() & BBSq_a1) || (p.whiteRook() & BBSq_b1) ) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedRookByKing;

    // black
    // bishop blocked by own pawn
    if ( (p.blackBishop() & BBSq_c8) && (pawns[Co_Black] & BBSq_d7) && (p.occupancy & BBSq_d6) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishopByPawn;
    if ( (p.blackBishop() & BBSq_f8) && (pawns[Co_Black] & BBSq_e7) && (p.occupancy & BBSq_e6) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishopByPawn;

    // trapped knight
    if ( (p.blackKnight() & BBSq_a1) && ((pawns[Co_White] & BBSq_a2) || (pawns[Co_White] & BBSq_c2) ) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedKnight;
    if ( (p.blackKnight() & BBSq_h1) && ((pawns[Co_White] & BBSq_h2) || (pawns[Co_White] & BBSq_f2) ) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedKnight;
    if ( (p.blackKnight() & BBSq_a2) && ((pawns[Co_White] & BBSq_a3) || (pawns[Co_White] & BBSq_b2) ) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedKnight2;
    if ( (p.blackKnight() & BBSq_h2) && ((pawns[Co_White] & BBSq_h3) || (pawns[Co_White] & BBSq_g2) ) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedKnight2;

    // trapped bishop
    if ( (p.blackBishop() & BBSq_a2) && (pawns[Co_White] & BBSq_b3) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishop;
    if ( (p.blackBishop() & BBSq_h2) && (pawns[Co_White] & BBSq_g3) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishop;
    if ( (p.blackBishop() & BBSq_b1) && (pawns[Co_White] & BBSq_c2) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishop2;
    if ( (p.blackBishop() & BBSq_g1) && (pawns[Co_White] & BBSq_f2) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishop2;
    if ( (p.blackBishop() & BBSq_a3) && (pawns[Co_White] & BBSq_b4) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishop3;
    if ( (p.blackBishop() & BBSq_h3) && (pawns[Co_White] & BBSq_g4) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedBishop3;

    // bishop near castled king (bonus)
    if ( (p.blackBishop() & BBSq_f8) && (p.blackKing() & BBSq_g8) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::returningBishopBonus;
    if ( (p.blackBishop() & BBSq_c8) && (p.blackKing() & BBSq_b8) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::returningBishopBonus;

    // king blocking rook
    if ( ( (p.blackKing() & BBSq_f8) || (p.blackKing() & BBSq_g8) ) && ( (p.blackRook() & BBSq_h8) || (p.blackRook() & BBSq_g8) ) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedRookByKing;
    if ( ( (p.blackKing() & BBSq_c8) || (p.blackKing() & BBSq_b8) ) && ( (p.blackRook() & BBSq_a8) || (p.blackRook() & BBSq_b8) ) ) score.scores[EvalScore::sc_Blocked] += EvalConfig::blockedRookByKing;
    */

    // number of pawn and piece type value
    score.scoresScaled[EvalScore::sc_Adjust] += p.mat[Co_White][M_r] * EvalConfig::adjRook  [p.mat[Co_White][M_p]];
    score.scoresScaled[EvalScore::sc_Adjust] -= p.mat[Co_Black][M_r] * EvalConfig::adjRook  [p.mat[Co_Black][M_p]];
    score.scoresScaled[EvalScore::sc_Adjust] += p.mat[Co_White][M_n] * EvalConfig::adjKnight[p.mat[Co_White][M_p]];
    score.scoresScaled[EvalScore::sc_Adjust] -= p.mat[Co_Black][M_n] * EvalConfig::adjKnight[p.mat[Co_Black][M_p]];

    // adjust piece pair score
    score.scores[EvalScore::sc_Adjust] += ( (p.mat[Co_White][M_b] > 1 ? EvalConfig::bishopPairBonus : 0)-(p.mat[Co_Black][M_b] > 1 ? EvalConfig::bishopPairBonus : 0) );
    score.scores[EvalScore::sc_Adjust] += ( (p.mat[Co_White][M_n] > 1 ? EvalConfig::knightPairMalus : 0)-(p.mat[Co_Black][M_n] > 1 ? EvalConfig::knightPairMalus : 0) );
    score.scores[EvalScore::sc_Adjust] += ( (p.mat[Co_White][M_r] > 1 ? EvalConfig::rookPairMalus   : 0)-(p.mat[Co_Black][M_r] > 1 ? EvalConfig::rookPairMalus   : 0) );
    score.scoresEG[EvalScore::sc_Adjust] += ( (p.mat[Co_White][M_b] > 1 ? EvalConfig::bishopPairBonusEG : 0)-(p.mat[Co_Black][M_b] > 1 ? EvalConfig::bishopPairBonusEG : 0) );
    score.scoresEG[EvalScore::sc_Adjust] += ( (p.mat[Co_White][M_n] > 1 ? EvalConfig::knightPairMalusEG : 0)-(p.mat[Co_Black][M_n] > 1 ? EvalConfig::knightPairMalusEG : 0) );
    score.scoresEG[EvalScore::sc_Adjust] += ( (p.mat[Co_White][M_r] > 1 ? EvalConfig::rookPairMalusEG   : 0)-(p.mat[Co_Black][M_r] > 1 ? EvalConfig::rookPairMalusEG   : 0) );

    // pawn shield (PST and king troppism alone is not enough)
    score.scores[EvalScore::sc_PwnShield] += ScoreType(((p.whiteKing() & whiteKingQueenSide ) != 0ull)*countBit(pawns[Co_White] & whiteQueenSidePawnShield1)*EvalConfig::pawnShieldBonus    );
    score.scores[EvalScore::sc_PwnShield] += ScoreType(((p.whiteKing() & whiteKingQueenSide ) != 0ull)*countBit(pawns[Co_White] & whiteQueenSidePawnShield2)*EvalConfig::pawnShieldBonus / 2);
    score.scores[EvalScore::sc_PwnShield] += ScoreType(((p.whiteKing() & whiteKingKingSide  ) != 0ull)*countBit(pawns[Co_White] & whiteKingSidePawnShield1 )*EvalConfig::pawnShieldBonus    );
    score.scores[EvalScore::sc_PwnShield] += ScoreType(((p.whiteKing() & whiteKingKingSide  ) != 0ull)*countBit(pawns[Co_White] & whiteKingSidePawnShield2 )*EvalConfig::pawnShieldBonus / 2);
    score.scores[EvalScore::sc_PwnShield] -= ScoreType(((p.blackKing() & blackKingQueenSide ) != 0ull)*countBit(pawns[Co_Black] & blackQueenSidePawnShield1)*EvalConfig::pawnShieldBonus    );
    score.scores[EvalScore::sc_PwnShield] -= ScoreType(((p.blackKing() & blackKingQueenSide ) != 0ull)*countBit(pawns[Co_Black] & blackQueenSidePawnShield2)*EvalConfig::pawnShieldBonus / 2);
    score.scores[EvalScore::sc_PwnShield] -= ScoreType(((p.blackKing() & blackKingKingSide  ) != 0ull)*countBit(pawns[Co_Black] & blackKingSidePawnShield1 )*EvalConfig::pawnShieldBonus    );
    score.scores[EvalScore::sc_PwnShield] -= ScoreType(((p.blackKing() & blackKingKingSide  ) != 0ull)*countBit(pawns[Co_Black] & blackKingSidePawnShield2 )*EvalConfig::pawnShieldBonus / 2);

    // tempo
    //score.scores[EvalScore::sc_Tempo] += ScoreType(30);

    // scale phase and 50 moves rule
    if ( display ) score.Display(p,gp);
    return (white2Play?+1:-1)*score.Score(p,gp);
}

ScoreType createHashScore(ScoreType score, DepthType ply){
  if      (isMatingScore(score)) score += ply;
  else if (isMatedScore (score)) score -= ply;
  return score;
}

ScoreType adjustHashScore(ScoreType score, DepthType ply){
  if      (isMatingScore(score)) score -= ply;
  else if (isMatedScore (score)) score += ply;
  return score;
}

#ifdef WITH_SYZYGY
#include "Add-On/syzygyInterface.cc"
#endif

ScoreType ThreadContext::qsearchNoPruning(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth){
    float gp = 0;
    ++stats.counters[Stats::sid_qnodes];
    const ScoreType evalScore = eval(p,gp);

    if ( evalScore >= beta ) return evalScore;
    if ( evalScore > alpha ) alpha = evalScore;

    const bool isInCheck = isAttacked(p, kingSquare(p));
    ScoreType bestScore = isInCheck?-MATE+ply:evalScore;

    MoveList moves;
    generate<GP_cap>(p,moves);
    sort(*this,moves,p,true);

    for(auto it = moves.begin() ; it != moves.end() ; ++it){
        Position p2 = p;
        if ( ! apply(p2,*it) ) continue;
        if (p.c == Co_White && Move2To(*it) == p.king[Co_Black]) return MATE - ply + 1;
        if (p.c == Co_Black && Move2To(*it) == p.king[Co_White]) return MATE - ply + 1;
        const ScoreType score = -qsearchNoPruning(-beta,-alpha,p2,ply+1,seldepth);
        if ( score > bestScore){
           bestScore = score;
           if ( score > alpha ){
              if ( score >= beta ) return score;
              alpha = score;
           }
        }
    }
    return bestScore;
}

template < bool qRoot, bool pvnode >
ScoreType ThreadContext::qsearch(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth){
    if (stopFlag) return STOPSCORE; // no time check in qsearch, too slow
    ++stats.counters[Stats::sid_qnodes];

    alpha = std::max(alpha, (ScoreType)(-MATE + ply));
    beta  = std::min(beta , (ScoreType)( MATE - ply + 1));
    if (alpha >= beta) return alpha;

    if ((int)ply > seldepth) seldepth = ply;

    float gp = 0;
    if (ply >= MAX_PLY - 1) return eval(p, gp);
    Move bestMove = INVALIDMOVE;

    const bool isInCheck = isAttacked(p, kingSquare(p));
    DepthType hashDepth = (isInCheck||qRoot) ? 0 : -1;

    TT::Entry e;
    if (TT::getEntry(*this, computeHash(p), hashDepth, e)) {
        if (!pvnode && e.h != 0 && ((e.b == TT::B_alpha && e.score <= alpha) || (e.b == TT::B_beta  && e.score >= beta) || (e.b == TT::B_exact))) { return adjustHashScore(e.score, ply); }
        bestMove = e.m;
    }
    if ( qRoot && interiorNodeRecognizer<true,false,true>(p) == MaterialHash::Ter_Draw) return 0; ///@todo is that gain elo ???

    ScoreType evalScore = isInCheck ? -MATE+ply : (e.h!=0?e.eval:eval(p,gp));
    bool evalScoreIsHashScore = false;
    // use tt score if possible and not in check 
    if ( !isInCheck && e.h != 0 && ((e.b == TT::B_alpha && e.score <= evalScore) || (e.b == TT::B_beta  && e.score >= evalScore) || (e.b == TT::B_exact)) ) evalScore = e.score, evalScoreIsHashScore = true;

    if ( evalScore >= beta ) return evalScore;
    if ( evalScore > alpha) alpha = evalScore;

    TT::Bound b = TT::B_alpha;
    ScoreType bestScore = evalScore;

    MoveList moves;
    if ( isInCheck ) generate<GP_all>(p,moves);
    else             generate<GP_cap>(p,moves);
    sort(*this,moves,p,qRoot||isInCheck,isInCheck,qRoot?&e:0); ///@todo only mvv-lva seems to lose elo

    const ScoreType alphaInit = alpha;

    for(auto it = moves.begin() ; it != moves.end() ; ++it){
        if (!isInCheck) {
            if (isBadCap(*it)) continue; // see
            if (StaticConfig::doQFutility && evalScore + StaticConfig::qfutilityMargin[evalScoreIsHashScore] + getAbsValue(p, Move2To(*it)) <= alphaInit) continue;
        }
        Position p2 = p;
        if ( ! apply(p2,*it) ) continue;
        //bool isCheck = isAttacked(p2, kingSquare(p2));
        if (p.c == Co_White && Move2To(*it) == p.king[Co_Black]) return MATE - ply + 1;
        if (p.c == Co_Black && Move2To(*it) == p.king[Co_White]) return MATE - ply + 1;
        const ScoreType score = -qsearch<false,false>(-beta,-alpha,p2,ply+1,seldepth);
        if ( score > bestScore){
           bestMove = *it;
           bestScore = score;
           b = TT::B_exact;
           if ( score > alpha ){
               if (score >= beta) {
                   TT::setEntry({ bestMove,createHashScore(bestScore,ply),createHashScore(evalScore,ply),TT::B_beta,hashDepth,computeHash(p) });
                   return score;
               }
               alpha = score;
           }
        }
    }
    // store eval
    TT::setEntry({ bestMove,createHashScore(bestScore,ply),createHashScore(evalScore,ply),b,hashDepth,computeHash(p) });
    return bestScore;
}

inline void updatePV(PVList & pv, const Move & m, const PVList & childPV) {
    pv.clear();
    pv.push_back(m);
    std::copy(childPV.begin(), childPV.end(), std::back_inserter(pv));
}

inline void updateTables(ThreadContext & context, const Position & p, DepthType depth, const Move m, TT::Bound bound) {
    if (bound == TT::B_beta) {
        if (!sameMove(context.killerT.killers[0][p.halfmoves], m)) {
            context.killerT.killers[1][p.halfmoves] = context.killerT.killers[0][p.halfmoves];
            context.killerT.killers[0][p.halfmoves] = m;
        }
        context.historyT.update<1>(depth, m, p);
        context.counterT.update(m, p);
    }
    else if ( bound == TT::B_alpha) context.historyT.update<-1>(depth, m, p);
}

inline bool singularExtension(ThreadContext & context, const Position & p, DepthType depth, const TT::Entry & e, const Move m, bool rootnode, int ply, bool isInCheck) {
    if ( depth >= StaticConfig::singularExtensionDepth && sameMove(m, e.m) && !rootnode && !isMateScore(e.score) && e.b == TT::B_beta && e.d >= depth - 3) {
        const ScoreType betaC = e.score - depth;
        PVList sePV;
        DepthType seSeldetph = 0;
        const ScoreType score = context.pvs<false,false>(betaC - 1, betaC, p, depth/2, ply, sePV, seSeldetph, isInCheck, m); 
        if (!ThreadContext::stopFlag && score < betaC) return true;
    }
    return false;
}

// pvs inspired by Xiphos
template< bool pvnode, bool canPrune>
ScoreType ThreadContext::pvs(ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, unsigned int ply, PVList & pv, DepthType & seldepth, bool isInCheck, const Move skipMove, std::vector<RootScores> * rootScores){
    if (stopFlag) return STOPSCORE;
    if ( std::max(1, (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count()) > getCurrentMoveMs() ){ stopFlag = true; Logging::LogIt(Logging::logInfo) << "stopFlag triggered in thread " << id(); }

    float gp = 0;
    if (ply >= MAX_PLY - 1 || depth >= MAX_DEPTH - 1) return eval(p, gp);

    if ((int)ply > seldepth) seldepth = ply;

    if ( depth <= 0 ) return qsearch<true,pvnode>(alpha,beta,p,ply,seldepth);

    ++stats.counters[Stats::sid_nodes];

    alpha = std::max(alpha, (ScoreType)(-MATE + ply));
    beta  = std::min(beta,  (ScoreType)( MATE - ply + 1));
    if (alpha >= beta) return alpha;

    const bool rootnode = ply == 1;

    if (!rootnode && interiorNodeRecognizer<true, pvnode, true>(p) == MaterialHash::Ter_Draw) return 0;

    TT::Entry e;
    if (skipMove==INVALIDMOVE && TT::getEntry(*this,computeHash(p), depth, e)) { // if not skipmove
        if ( e.h != 0 && !rootnode && !pvnode && ( (e.b == TT::B_alpha && e.score <= alpha) || (e.b == TT::B_beta  && e.score >= beta) || (e.b == TT::B_exact) ) ) {
            if ( e.m != INVALIDMOVE) pv.push_back(e.m); // here e.m might be INVALIDMOVE if B_alpha without alphaUpdated/bestScoreUpdated (so don't try this at root node !)
            if ((Move2Type(e.m) == T_std /*|| isBadCap(e.m)*/) && !isInCheck) updateTables(*this, p, depth, e.m, e.b);
            return adjustHashScore(e.score, ply);
        }
    }

#ifdef WITH_SYZYGY
    ScoreType tbScore = 0;
    if ( !rootnode && int(countBit(p.allPieces[Co_White]|p.allPieces[Co_Black])) <= SyzygyTb::MAX_TB_MEN && SyzygyTb::probe_wdl(p, tbScore, false) > 0){
       TT::setEntry({INVALIDMOVE,createHashScore(tbScore,ply),eval(p, gp),TT::B_exact,DepthType(200),computeHash(p)});
       return tbScore;
    }
#endif

    ScoreType evalScore;
    if (isInCheck) evalScore = -MATE + ply;
    else evalScore = (e.h != 0)?e.eval:eval(p, gp);
    evalStack[p.halfmoves] = evalScore; // insert only static eval, never hash score !
    bool evalScoreIsHashScore = false;
    if ( (e.h != 0 && !isInCheck) && ((e.b == TT::B_alpha && e.score < evalScore) || (e.b == TT::B_beta && e.score > evalScore) || (e.b == TT::B_exact)) ) evalScore = adjustHashScore(e.score,ply), evalScoreIsHashScore=true;

    /* ///@todo THIS IS BUGGY : because pv is not filled (even at root) if bestScore starts too big.
    ScoreType bestScore = evalScore;
    */
    ScoreType bestScore = -MATE + ply;
    MoveList moves;
    bool moveGenerated = false;
    bool capMoveGenerated = false;
    bool futility = false, lmp = false, /*mateThreat = false,*/ historyPruning = false;
    const bool isNotEndGame = (p.mat[Co_White][M_t]+p.mat[Co_Black][M_t] > 0); ///@todo better ?

    const bool improving = (!isInCheck && ply > 1 && evalStack[p.halfmoves] >= evalStack[p.halfmoves - 2]);
    DepthType marginDepth = std::max(1,depth-(evalScoreIsHashScore?e.d:0));

    // prunings
    if ( !DynamicConfig::mateFinder && canPrune && !isInCheck && !isMateScore(beta) && !pvnode){ ///@todo removing the !isMateScore(beta) is not losing that much elo and allow for better check mate finding ...
        // static null move ///@todo use improving ?
        if (StaticConfig::doStaticNullMove && isNotEndGame && depth <= StaticConfig::staticNullMoveMaxDepth[evalScoreIsHashScore] && evalScore >= beta + StaticConfig::staticNullMoveDepthInit[evalScoreIsHashScore] + (StaticConfig::staticNullMoveDepthCoeff[evalScoreIsHashScore] /*- 50*improving*/) * depth) return ++stats.counters[Stats::sid_staticNullMove], evalScore;

        // razoring
        int rAlpha = alpha - StaticConfig::razoringMarginDepthInit[evalScoreIsHashScore] - StaticConfig::razoringMarginDepthCoeff[evalScoreIsHashScore]*marginDepth;
        if ( StaticConfig::doRazoring && depth <= StaticConfig::razoringMaxDepth[evalScoreIsHashScore] && evalScore <= rAlpha ){
            ++stats.counters[Stats::sid_razoringTry];
            const ScoreType qScore = qsearch<true,pvnode>(alpha,beta,p,ply,seldepth);
            if ( stopFlag ) return STOPSCORE;
            if ( qScore <= alpha ) return ++stats.counters[Stats::sid_razoring],qScore;
        }

        // null move
        if (isNotEndGame && StaticConfig::doNullMove && depth >= StaticConfig::nullMoveMinDepth) {
            PVList nullPV;
            ++stats.counters[Stats::sid_nullMoveTry];
            const int R = depth / 4 + 3 + std::min((evalScore - beta) / 80, 3); // adaptative
            const ScoreType nullIIDScore = evalScore; // pvs<false, false>(beta - 1, beta, p, std::max(depth/4,1), ply, nullPV, seldepth, isInCheck);
            if (nullIIDScore >= beta /*- 10 * depth*/) {
                TT::Entry nullE;
                TT::getEntry(*this, computeHash(p), depth - R, nullE);
                if (nullE.h == 0 || nullE.score >= beta) { // avoid null move search if TT gives a score < beta for the same depth
                    ++stats.counters[Stats::sid_nullMoveTry2];
                    Position pN = p;
                    applyNull(pN);
                    const ScoreType nullscore = -pvs<false, false>(-beta, -beta + 1, pN, depth - R, ply + 1, nullPV, seldepth, isInCheck);
                    if (stopFlag) return STOPSCORE;
                    if (nullscore >= beta) return ++stats.counters[Stats::sid_nullMove], isMateScore(nullscore) ? beta : nullscore;
                    //if (isMatedScore(nullscore)) mateThreat = true;
                }
            }
        }

        // ProbCut
        if ( StaticConfig::doProbcut && depth >= StaticConfig::probCutMinDepth /*&& !isMateScore(beta)*/){
          ++stats.counters[Stats::sid_probcutTry];
          int probCutCount = 0;
          const ScoreType betaPC = beta + StaticConfig::probCutMargin;
          generate<GP_cap>(p,moves);
          sort(*this,moves,p,true,isInCheck,&e);
          capMoveGenerated = true;
          for (auto it = moves.begin() ; it != moves.end() && probCutCount < StaticConfig::probCutMaxMoves; ++it){
            if ( (e.h != 0 && sameMove(e.m, *it)) || isBadCap(*it) ) continue; // skip TT move if quiet or bad captures
            Position p2 = p;
            if ( ! apply(p2,*it) ) continue;
            ++probCutCount;
            ScoreType scorePC = -qsearch<true,pvnode>(-betaPC, -betaPC + 1, p2, ply + 1, seldepth);
            PVList pcPV;
            if (stopFlag) return STOPSCORE;
            if (scorePC >= betaPC) ++stats.counters[Stats::sid_probcutTry2], scorePC = -pvs<pvnode,true>(-betaPC,-betaPC+1,p2,depth-StaticConfig::probCutMinDepth+1,ply+1,pcPV,seldepth, isAttacked(p2, kingSquare(p2)));
            if (stopFlag) return STOPSCORE;
            if (scorePC >= betaPC) return ++stats.counters[Stats::sid_probcut], scorePC;
          }
        }
    }

    // IID
    if ( (e.h == 0 /*|| e.d < depth/3*/) && ((pvnode && depth >= StaticConfig::iidMinDepth) || depth >= StaticConfig::iidMinDepth2)){
        ++stats.counters[Stats::sid_iid];
        PVList iidPV;
        pvs<pvnode,false>(alpha,beta,p,depth/2,ply,iidPV,seldepth,isInCheck);
        if (stopFlag) return STOPSCORE;
        TT::getEntry(*this,computeHash(p), depth, e);
    }

    // LMP
    if (!rootnode && StaticConfig::doLMP && depth <= StaticConfig::lmpMaxDepth) lmp = true;
    // futility
    if (!rootnode && StaticConfig::doFutility && depth <= StaticConfig::futilityMaxDepth[evalScoreIsHashScore] && evalScore <= alpha - StaticConfig::futilityDepthInit[evalScoreIsHashScore] - StaticConfig::futilityDepthCoeff[evalScoreIsHashScore]*depth) futility = true;
    // history pruning
    if (!rootnode && StaticConfig::doHistoryPruning && depth < StaticConfig::historyPruningMaxDepth) historyPruning = true;

    int validMoveCount = 0;
    //bool alphaUpdated = false;
    bool bestScoreUpdated = false;
    Move bestMove = INVALIDMOVE;
    TT::Bound hashBound = TT::B_alpha;
    bool ttMoveIsCapture = false;

    // try the tt move before move generation (if not skipped move)
    if (e.h != 0 && e.m != INVALIDMOVE && !sameMove(e.m,skipMove)) { // should be the case thanks to iid at pvnode
        Position p2 = p;
        if (apply(p2, e.m)) {
            const Square to = Move2To(e.m);
            if (p.c == Co_White && to == p.king[Co_Black]) return MATE - ply + 1;
            if (p.c == Co_Black && to == p.king[Co_White]) return MATE - ply + 1;
            validMoveCount++;
            PVList childPV;
            hashStack[p.halfmoves] = p.h;
            const bool isCheck = isAttacked(p2, kingSquare(p2));
            if ( isCapture(e.m) ) ttMoveIsCapture = true;
            //const bool isAdvancedPawnPush = getPieceType(p,Move2From(e.m)) == P_wp && (SQRANK(to) > 5 || SQRANK(to) < 2);
            // extensions
            int extension = 0;
            if (!extension && pvnode && isInCheck) ++stats.counters[Stats::sid_checkExtension],++extension;
            if (!extension && isCastling(e.m) ) ++stats.counters[Stats::sid_castlingExtension],++extension;
            //if (!extension && mateThreat) ++stats.counters[Stats::sid_mateThreadExtension],++extension;
            //if (!extension && p.lastMove != INVALIDMOVE && Move2Type(p.lastMove) == T_capture && Move2To(e.m) == Move2To(p.lastMove)) ++stats.counters[Stats::sid_recaptureExtension],++extension; // recapture
            //if (!extension && isCheck && !isBadCap(e.m)) ++stats.counters[Stats::sid_checkExtension2],++extension; // we give check with a non risky move
            //if (!extension && isAdvancedPawnPush && sameMove(e.m, killerT.killers[0][p.halfmoves])) ++stats.counters[Stats::sid_pawnPushExtension],++extension; // a pawn is near promotion ///@todo isPassed ?
            if (!extension && skipMove == INVALIDMOVE && singularExtension(*this, p, depth, e, e.m, rootnode, ply, isInCheck)) ++stats.counters[Stats::sid_singularExtension],++extension;
            const ScoreType ttScore = -pvs<pvnode,true>(-beta, -alpha, p2, depth - 1 + extension, ply + 1, childPV, seldepth, isCheck);
            if (stopFlag) return STOPSCORE;
            if ( ttScore > bestScore ){
                bestScore = ttScore;
                bestScoreUpdated = true;
                bestMove = e.m;
                if (rootnode && rootScores) rootScores->push_back({ e.m,ttScore });
                if (ttScore > alpha) {
                    hashBound = TT::B_exact;
                    if (pvnode) updatePV(pv, e.m, childPV);
                    if (ttScore >= beta) {
                        ++stats.counters[Stats::sid_ttbeta];
                        if ((Move2Type(e.m) == T_std /*|| isBadCap(e.m)*/) && !isInCheck) updateTables(*this, p, depth + (ttScore > (beta+80)), e.m, TT::B_beta); ///@todo badcap can be killers ?
                        if (skipMove == INVALIDMOVE /*&& ttScore != 0*/) TT::setEntry({ e.m,createHashScore(ttScore,ply),createHashScore(evalScore,ply),TT::B_beta,depth,computeHash(p) }); 
                        return ttScore;
                    }
                    ++stats.counters[Stats::sid_ttalpha];
                    //alphaUpdated = true;
                    alpha = ttScore;
                }
            }
        }
    }

#ifdef WITH_SYZYGY
    if (rootnode && int(countBit(p.allPieces[Co_White] | p.allPieces[Co_Black])) <= SyzygyTb::MAX_TB_MEN) {
        ScoreType tbScore = 0;
        if (SyzygyTb::probe_root(*this, p, tbScore, moves) < 0) { // only good moves if TB success
            if (capMoveGenerated) generate<GP_quiet>(p, moves, true);
            else                  generate<GP_all>  (p, moves, false);
            moveGenerated = true;
        }
    }
#endif

    ///@todo at root, maybe rootScore or node count can be used to sort moves ??
    if (!moveGenerated) {
        if (capMoveGenerated) generate<GP_quiet>(p, moves, true);
        else                  generate<GP_all>  (p, moves, false);
        if (moves.empty()) return isInCheck ? -MATE + ply : 0;
    /*if ( isMainThread() )*/ sort(*this, moves, p,true, isInCheck, &e);
    //else std::random_shuffle(moves.begin(),moves.end());
    }

    ScoreType score = -MATE + ply;

    for(auto it = moves.begin() ; it != moves.end() && !stopFlag ; ++it){
        if (sameMove(skipMove, *it)) continue; // skipmove
        if ( e.h != 0 && sameMove(e.m, *it)) continue; // already tried
        Position p2 = p;
        if ( ! apply(p2,*it) ) continue;
        const Square to = Move2To(*it);
        if (p.c == Co_White && to == p.king[Co_Black]) return MATE - ply + 1;
        if (p.c == Co_Black && to == p.king[Co_White]) return MATE - ply + 1;
        validMoveCount++;
        PVList childPV;
        hashStack[p.halfmoves] = p.h;
        const bool isCheck = isAttacked(p2, kingSquare(p2));
        const bool isAdvancedPawnPush = getPieceType(p,Move2From(*it)) == P_wp && (SQRANK(to) > 5 || SQRANK(to) < 2);
        // extensions
        int extension = 0;
        if (!extension && pvnode && isInCheck) ++stats.counters[Stats::sid_checkExtension],++extension; // we are in check (extension)
        //if (!extension && mateThreat && depth <= 4) ++stats.counters[Stats::sid_mateThreadExtension],++extension;
        //if (!extension && p.lastMove != INVALIDMOVE && !isBadCap(*it) && Move2Type(p.lastMove) == T_capture && Move2To(*it) == Move2To(p.lastMove)) ++stats.counters[Stats::sid_recaptureExtension],++extension; //recapture
        //if (!extension && isCheck && !isBadCap(*it)) ++stats.counters[Stats::sid_checkExtension2],++extension; // we give check with a non risky move
        //if (!extension && isAdvancedPawnPush && sameMove(*it, killerT.killers[0][p.halfmoves])) ++stats.counters[Stats::sid_pawnPushExtension],++extension; // a pawn is near promotion ///@todo and isPassed ?
        if (!extension && isCastling(*it) ) ++stats.counters[Stats::sid_castlingExtension],++extension;
        // pvs
        if (validMoveCount < 2 || !StaticConfig::doPVS) score = -pvs<pvnode,true>(-beta,-alpha,p2,depth-1+extension,ply+1,childPV,seldepth,isCheck);
        else{
            // reductions & prunings
            int reduction = 0;
            const bool isPrunable =  /*isNotEndGame &&*/ !isAdvancedPawnPush && !sameMove(*it, killerT.killers[0][p.halfmoves]) && !sameMove(*it, killerT.killers[1][p.halfmoves]) /*&& !isMateScore(alpha) */&& !isMateScore(beta);
            const bool noCheck = !isInCheck && !isCheck;
            const bool isPrunableStd = isPrunable && Move2Type(*it) == T_std;
            const bool isPrunableStdNoCheck = isPrunable && noCheck && Move2Type(*it) == T_std;
            // futility
            if (futility && isPrunableStdNoCheck) {++stats.counters[Stats::sid_futility]; continue;}
            // LMP
            if (lmp && isPrunableStdNoCheck && validMoveCount >= StaticConfig::lmpLimit[improving][depth] ) {++stats.counters[Stats::sid_lmp]; continue;}
            // History pruning
            if (historyPruning && isPrunableStdNoCheck && validMoveCount > StaticConfig::lmpLimit[improving][depth]/3 && Move2Score(*it) < StaticConfig::historyPruningThreshold) {++stats.counters[Stats::sid_historyPruning]; continue;}
            // SEE (cap)
            const bool isPrunableCap = isPrunable && noCheck && Move2Type(*it) == T_capture && isBadCap(*it);
            if ((futility||depth<=4) && isPrunableCap ) {++stats.counters[Stats::sid_see]; continue;}
            // SEE (quiet)
            if ((futility||depth<=4) && isPrunableStdNoCheck && !SEE(p,*it,-100*depth)) {++stats.counters[Stats::sid_seeQuiet]; continue;}
            // LMR
            if (StaticConfig::doLMR && !DynamicConfig::mateFinder && isPrunableStd && depth >= StaticConfig::lmrMinDepth ){
                reduction = StaticConfig::lmrReduction[std::min((int)depth,MAX_DEPTH-1)][std::min(validMoveCount,MAX_DEPTH)];
                if (!improving) ++reduction;
                if (ttMoveIsCapture) ++reduction;
                reduction -= 2*int(Move2Score(*it) / MAX_HISTORY); //history reduction/extension
                //if (!pvnode) reduction += std::min(2,(int)moves.size()/10 - 1); // if many moves => reduce more, if less move => reduce less
                if (pvnode && reduction > 0) --reduction;
                if (reduction < 0) reduction = 0;
                if (reduction >= depth - 1) reduction = depth - 1;
            }
            // PVS
            score = -pvs<false,true>(-alpha-1,-alpha,p2,depth-1-reduction+extension,ply+1,childPV,seldepth,isCheck);
            if ( reduction > 0 && score > alpha )                       { childPV.clear(); score = -pvs<false,true>(-alpha-1,-alpha,p2,depth-1+extension,ply+1,childPV,seldepth,isCheck); }
            if ( pvnode && score > alpha && (rootnode || score < beta) ){ childPV.clear(); score = -pvs<true ,true>(-beta   ,-alpha,p2,depth-1+extension,ply+1,childPV,seldepth,isCheck); } // potential new pv node
        }
        if (stopFlag) return STOPSCORE;
        if (rootnode && rootScores) rootScores->push_back({ *it,score });
        if ( score > bestScore ){
            bestScore = score;
            bestMove = *it;
            bestScoreUpdated = true;
            if ( score > alpha ){
                if (pvnode) updatePV(pv, *it, childPV);
                //alphaUpdated = true;
                alpha = score;
                hashBound = TT::B_exact;
                if ( score >= beta ){
                    if ( (Move2Type(*it) == T_std /*|| isBadCap(*it)*/) && !isInCheck){ ///@todo bad cap can be killers?
                        updateTables(*this, p, depth + (score>beta+80), *it, TT::B_beta);
                        for(auto it2 = moves.begin() ; it2 != moves.end() && !sameMove(*it2,*it); ++it2) if ( Move2Type(*it2) == T_std && !sameMove(*it2, killerT.killers[0][p.halfmoves])/*|| isBadCap(*it2)*/) historyT.update<-1>(depth + (score > (beta + 80)),*it2,p); ///@todo bad cap can be killers
                    }
                    hashBound = TT::B_beta;
                    break;
                }
            }
        }
    }

    if ( validMoveCount==0 ) return (isInCheck || skipMove!=INVALIDMOVE)?-MATE + ply : 0;
    ///@todo here not checking for alphaUpdated or bestScoreUpdated seems to LOSS elo !!!
    if ( skipMove==INVALIDMOVE && /*alphaUpdated*/ bestScoreUpdated ) TT::setEntry({bestMove,createHashScore(bestScore,ply),createHashScore(evalScore,ply),hashBound,depth,computeHash(p)});

    return bestScore;
}

PVList ThreadContext::search(const Position & p, Move & m, DepthType & d, ScoreType & sc, DepthType & seldepth){
    // a playing level feature for the poor ...
    ///@todo more things shall depend on level
    d=DynamicConfig::level==10?d:DynamicConfig::level;

    if ( isMainThread() ){
        Logging::LogIt(Logging::logInfo) << "Search called" ;
        Logging::LogIt(Logging::logInfo) << "requested time  " << getCurrentMoveMs() ;
        Logging::LogIt(Logging::logInfo) << "requested depth " << (int) d ;
        stopFlag = false;
        moveDifficulty = MoveDifficultyUtil::MD_std;
    }
    else{
        Logging::LogIt(Logging::logInfo) << "helper thread waiting ... " << id() ;
        while(startLock.load()){;}
        Logging::LogIt(Logging::logInfo) << "... go for id " << id() ;
    }
    stats.init();
    //TT::clearTT(); // to be used for reproductible results
    killerT.initKillers();
    historyT.initHistory();
    counterT.initCounter();

    TimeMan::startTime = Clock::now();

    DepthType reachedDepth = 0;
    PVList pv;
    ScoreType bestScore = 0;
    m = INVALIDMOVE;

    hashStack[p.halfmoves] = p.h;

    if ( isMainThread() ){
       const Move bookMove = SanitizeCastling(p,Book::Get(computeHash(p)));
       if ( bookMove != INVALIDMOVE){
           if ( isMainThread() ) startLock.store(false);
           pv.push_back(bookMove);
           m = pv[0];
           d = 0;
           sc = 0;
           return pv;
       }
    }

    ScoreType depthScores[MAX_DEPTH] = { 0 };
    const bool isInCheck = isAttacked(p, kingSquare(p));
    DepthType startDepth = 1;

    if ( isMainThread() ){
       // easy move detection (small open window depth 2 search)
       std::vector<ThreadContext::RootScores> rootScores;
       ScoreType easyScore = pvs<true,false>(-MATE, MATE, p, 2, 1, pv, seldepth, isInCheck,INVALIDMOVE,&rootScores);
       std::sort(rootScores.begin(), rootScores.end(), [](const ThreadContext::RootScores& r1, const ThreadContext::RootScores & r2) {return r1.s > r2.s; });
       if (stopFlag) { bestScore = easyScore; goto pvsout; }
       if (rootScores.size() == 1) moveDifficulty = MoveDifficultyUtil::MD_forced; // only one : check evasion or zugzwang
       else if (rootScores.size() > 1 && rootScores[0].s > rootScores[1].s + MoveDifficultyUtil::easyMoveMargin) moveDifficulty = MoveDifficultyUtil::MD_easy;
    }

    // ID loop
    for(DepthType depth = startDepth ; depth <= std::min(d,DepthType(MAX_DEPTH-6)) && !stopFlag ; ++depth ){ // -6 so that draw can be found for sure
        if (!isMainThread()){ // stockfish like thread management
            const int i = (id()-1)%20;
            if (((depth + ThreadPool::skipPhase[i]) / ThreadPool::skipSize[i]) % 2) continue;
        }
        else{ if ( depth >= 5) startLock.store(false);} // delayed other thread start
        Logging::LogIt(Logging::logInfo) << "Thread " << id() << " searching depth " << (int)depth;
        PVList pvLoc;
        ScoreType delta = (StaticConfig::doWindow && depth>4)?8:MATE; // MATE not INFSCORE in order to enter the loop below once
        ScoreType alpha = std::max(ScoreType(bestScore - delta), ScoreType (-INFSCORE));
        ScoreType beta  = std::min(ScoreType(bestScore + delta), INFSCORE);
        ScoreType score = 0;
        while( delta <= MATE ){
            pvLoc.clear();
            score = pvs<true,false>(alpha,beta,p,depth,1,pvLoc,seldepth, isInCheck);
            if ( stopFlag ) break;
            delta += 2 + delta/2; // from xiphos ...
            if      (score <= alpha) {beta = (alpha + beta) / 2; alpha = std::max(ScoreType(score - delta), ScoreType(-MATE) ); Logging::LogIt(Logging::logInfo) << "Increase window alpha " << alpha << ".." << beta;}
            else if (score >= beta ) {/*alpha= (alpha + beta) / 2;*/ beta  = std::min(ScoreType(score + delta), ScoreType( MATE) ); Logging::LogIt(Logging::logInfo) << "Increase window beta "  << alpha << ".." << beta;}
            else break;
        }
        if (stopFlag) break;
        pv = pvLoc;
        reachedDepth = depth;
        bestScore    = score;
        if ( isMainThread() ){
            const int ms = std::max(1,(int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count());
            std::stringstream str;
            Counter nodeCount = ThreadPool::instance().counter(Stats::sid_nodes) + ThreadPool::instance().counter(Stats::sid_qnodes);
            if (Logging::ct == Logging::CT_xboard) {
                str << int(depth) << " " << bestScore << " " << ms / 10 << " " << nodeCount << " ";
                if (DynamicConfig::fullXboardOutput) str << (int)seldepth << " " << int(nodeCount / (ms / 1000.f) / 1000.) << " " << ThreadPool::instance().counter(Stats::sid_tthits) / 1000;
                str << "\t" << ToString(pv);
            }
            else if (Logging::ct == Logging::CT_uci) { str << "info depth " << int(depth) << " score cp " << bestScore << " time " << ms << " nodes " << nodeCount << " nps " << int(nodeCount / (ms / 1000.f)) << " seldepth " << (int)seldepth << " tbhits " << ThreadPool::instance().counter(Stats::sid_tthits) << " pv " << ToString(pv); }
            Logging::LogIt(Logging::logGUI) << str.str();
            if (TimeMan::isDynamic && depth > MoveDifficultyUtil::emergencyMinDepth && bestScore < depthScores[depth - 1] - MoveDifficultyUtil::emergencyMargin) { moveDifficulty = MoveDifficultyUtil::MD_hardDefense; Logging::LogIt(Logging::logInfo) << "Emergency mode activated : " << bestScore << " < " << depthScores[depth - 1] - MoveDifficultyUtil::emergencyMargin; }
            if (TimeMan::isDynamic && std::max(1, int(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count()*1.8)) > getCurrentMoveMs()) { stopFlag = true; Logging::LogIt(Logging::logInfo) << "stopflag triggered, not enough time for next depth"; break; } // not enought time
            depthScores[depth] = bestScore;
        }
    }
pvsout:
    if ( isMainThread() ) startLock.store(false);
    if (pv.empty()){
        m = INVALIDMOVE;
        Logging::LogIt(Logging::logInfo) << "Empty pv, trying to use TT" ;
        TT::getPV(p, *this, pv);
    }
    if (pv.empty()) { Logging::LogIt(Logging::logWarn) << "Empty pv"; }// may occur in case of mate / stalemate... 
    else m = pv[0];
    d = reachedDepth;
    sc = bestScore;
    if (isMainThread()) ThreadPool::instance().DisplayStats();
    return pv;
}

namespace COM {
    bool pondering; // currently pondering
    enum Ponder : unsigned char { p_off = 0, p_on = 1 };
    Ponder ponder;
    std::string command;
    Position position;
    Move move;
    DepthType depth;
    enum Mode : unsigned char { m_play_white = 0, m_play_black = 1, m_force = 2, m_analyze = 3 };
    Mode mode;
    enum SideToMove : unsigned char { stm_white = 0, stm_black = 1 };
    SideToMove stm; ///@todo isn't this redundant with position.c ??

    void newgame() {
        mode = m_analyze;
        stm = stm_white;
        readFEN(startPosition, COM::position);
        TT::clearTT();
    }

    void init() {
        Logging::LogIt(Logging::logInfo) << "Init COM";
        ponder = p_off;
        pondering = false;
        depth = -1;
        newgame();
    }

    void readLine() {
        command.clear();
        std::getline(std::cin, command);
        Logging::LogIt(Logging::logInfo) << "Received command : " << command;
    }

    SideToMove opponent(SideToMove & s) { return s == stm_white ? stm_black : stm_white; }
    
    bool sideToMoveFromFEN(const std::string & fen) {
        const bool b = readFEN(fen, COM::position,true);
        stm = COM::position.c == Co_White ? stm_white : stm_black;
        return b;
    }

    Move thinkUntilTimeUp() {
        Logging::LogIt(Logging::logInfo) << "Thinking... ";
        ScoreType score = 0;
        Move m = INVALIDMOVE;
        if (depth < 0) depth = MAX_DEPTH;
        Logging::LogIt(Logging::logInfo) << "depth          " << (int)depth;
        ThreadContext::currentMoveMs = TimeMan::GetNextMSecPerMove(position);
        Logging::LogIt(Logging::logInfo) << "currentMoveMs  " << ThreadContext::currentMoveMs;
        Logging::LogIt(Logging::logInfo) << ToString(position);
        DepthType seldepth = 0;
        PVList pv;
        const ThreadData d = { depth,seldepth/*dummy*/,score/*dummy*/,position,m/*dummy*/,pv/*dummy*/ }; // only input coef
        ThreadPool::instance().searchSync(d);
        m = ThreadPool::instance().main().getData().best; // here output results
        Logging::LogIt(Logging::logInfo) << "...done returning move " << ToString(m);
        return m;
    }

    bool makeMove(Move m, bool disp, std::string tag) {
        if (disp && m != INVALIDMOVE) Logging::LogIt(Logging::logGUI) << tag << " " << ToString(m);
        Logging::LogIt(Logging::logInfo) << ToString(position);
        return apply(position, m);
    }

    void ponderUntilInput() {
        Logging::LogIt(Logging::logInfo) << "Pondering... ";
        ScoreType score = 0;
        Move m = INVALIDMOVE;
        depth = MAX_DEPTH;
        DepthType seldepth = 0;
        PVList pv;
        const ThreadData d = { depth,seldepth,score,position,m,pv };
        ThreadPool::instance().searchASync(d);
    }

    void stop() { ThreadContext::stopFlag = true; }

    void stopPonder() { if ((int)mode == (int)opponent(stm) && ponder == p_on && pondering) { pondering = false; stop(); } }

    Move moveFromCOM(std::string mstr) { // copy string on purpose
        mstr = trim(mstr);
        Square from = INVALIDSQUARE;
        Square to = INVALIDSQUARE;
        MType mtype = T_std;
        if (!readMove(COM::position, mstr, from, to, mtype)) return INVALIDMOVE;
        Move m = ToMove(from, to, mtype);
        bool whiteToMove = COM::position.c == Co_White;
        // convert castling input notation to internal castling style if needed
        if (mtype == T_std && from == (whiteToMove ? Sq_e1 : Sq_e8) && COM::position.b[from] == (whiteToMove ? P_wk : P_bk)) {
            if      (to == (whiteToMove ? Sq_c1 : Sq_c8)) m = ToMove(from, to, whiteToMove ? T_wqs : T_bqs);
            else if (to == (whiteToMove ? Sq_g1 : Sq_g8)) m = ToMove(from, to, whiteToMove ? T_wks : T_bks);
        }
        return m;
    }
}

namespace XBoard{
bool display;

void init(){
    Logging::ct = Logging::CT_xboard;
    Logging::LogIt(Logging::logInfo) << "Init xboard" ;
    COM::init();
    display = false;
}

void setFeature(){
    ///@todo more feature disable !!
    ///@todo use otime ?
    Logging::LogIt(Logging::logGUI) << "feature ping=1 setboard=1 colors=0 usermove=1 memory=0 sigint=0 sigterm=0 otime=0 time=1 nps=0 myname=\"Minic " << MinicVersion << "\"";
    Logging::LogIt(Logging::logGUI) << "feature done=1";
}

bool receiveMove(const std::string & command){
    std::string mstr(command);
    const size_t p = command.find("usermove");
    if (p != std::string::npos) mstr = mstr.substr(p + 8);
    Move m = COM::moveFromCOM(mstr);
    if ( m == INVALIDMOVE ) return false;
    COM::stopPonder();
    COM::stop();
    if(!COM::makeMove(m,false,"")){ // make move
        Logging::LogIt(Logging::logInfo) << "Bad opponent move ! " << mstr;
        Logging::LogIt(Logging::logInfo) << ToString(COM::position) ;
        COM::mode = COM::m_force;
        return false;
    }
    else COM::stm = COM::opponent(COM::stm);
    return true;
}

void xboard(){
    Logging::LogIt(Logging::logInfo) << "Starting XBoard main loop" ;
    setFeature(); ///@todo should not be here

    while(true) {
        Logging::LogIt(Logging::logInfo) << "XBoard: mode " << COM::mode ;
        Logging::LogIt(Logging::logInfo) << "XBoard: stm  " << COM::stm ;
        if(COM::mode == COM::m_analyze){
            //AnalyzeUntilInput(); ///@todo analyze mode
        }
        // move as computer if mode is equal to stm
        if((int)COM::mode == (int)COM::stm) { // mouarfff
            COM::move = COM::thinkUntilTimeUp();
            if(COM::move == INVALIDMOVE){ COM::mode = COM::m_force; } // game ends
            else{
                if ( !COM::makeMove(COM::move,true,"move") ){
                    Logging::LogIt(Logging::logInfo) << "Bad computer move !" ;
                    Logging::LogIt(Logging::logInfo) << ToString(COM::position) ;
                    COM::mode = COM::m_force;
                }
                COM::stm = COM::opponent(COM::stm);
            }
        }

        // if not our turn, and ponder is on, let's think ...
        if(COM::move != INVALIDMOVE && (int)COM::mode == (int)COM::opponent(COM::stm) && COM::ponder == COM::p_on && !COM::pondering) COM::ponderUntilInput();

        bool commandOK = true;
        int once = 0;

        while(once++ == 0 || !commandOK){ // loop until a good command is found
            commandOK = true;
            COM::readLine(); // read next command !

            if (COM::command == "force")        COM::mode = COM::m_force;
            else if (COM::command == "xboard")  Logging::LogIt(Logging::logInfo) << "This is minic!" ;
            else if (COM::command == "post")    display = true;
            else if (COM::command == "nopost")  display = false;
            else if (COM::command == "computer"){ }
            else if ( strncmp(COM::command.c_str(),"protover",8) == 0){ setFeature(); }
            else if ( strncmp(COM::command.c_str(),"accepted",8) == 0){ }
            else if ( strncmp(COM::command.c_str(),"rejected",8) == 0){ }
            else if ( strncmp(COM::command.c_str(),"ping",4) == 0){
                std::string str(COM::command);
                const size_t p = COM::command.find("ping");
                str = str.substr(p+4);
                str = trim(str);
                Logging::LogIt(Logging::logGUI) << "pong " << str ;
            }
            else if (COM::command == "new"){
                COM::stopPonder();
                COM::stop();
                COM::sideToMoveFromFEN(startPosition);
                COM::mode = (COM::Mode)((int)COM::stm); ///@todo should not be here !!! I thought start mode shall be analysis ...
                COM::move = INVALIDMOVE;
                if(COM::mode != COM::m_analyze){
                    COM::mode = COM::m_play_black;
                    COM::stm = COM::stm_white;
                }
            }
            else if (COM::command == "white"){ // deprecated
                COM::stopPonder();
                COM::stop();
                COM::mode = COM::m_play_black;
                COM::stm = COM::stm_white;
            }
            else if (COM::command == "black"){ // deprecated
                COM::stopPonder();
                COM::stop();
                COM::mode = COM::m_play_white;
                COM::stm = COM::stm_black;
            }
            else if (COM::command == "go"){
                COM::stopPonder();
                COM::stop();
                COM::mode = (COM::Mode)((int)COM::stm);
            }
            else if (COM::command == "playother"){
                COM::stopPonder();
                COM::stop();
                COM::mode = (COM::Mode)((int)COM::opponent(COM::stm));
            }
            else if ( strncmp(COM::command.c_str(),"usermove",8) == 0){
                if (!receiveMove(COM::command)) commandOK = false;
            }
            else if (  strncmp(COM::command.c_str(),"setboard",8) == 0){
                COM::stopPonder();
                COM::stop();
                std::string fen(COM::command);
                const size_t p = COM::command.find("setboard");
                fen = fen.substr(p+8);
                if (!COM::sideToMoveFromFEN(fen)){
                    Logging::LogIt(Logging::logFatal) << "Illegal FEN" ;
                    commandOK = false;
                }
            }
            else if ( strncmp(COM::command.c_str(),"result",6) == 0){
                COM::stopPonder();
                COM::stop();
                COM::mode = COM::m_force;
            }
            else if (COM::command == "analyze"){
                COM::stopPonder();
                COM::stop();
                COM::mode = COM::m_analyze;
            }
            else if (COM::command == "exit"){
                COM::stopPonder();
                COM::stop();
                COM::mode = COM::m_force;
            }
            else if (COM::command == "easy"){
                COM::stopPonder();
                COM::ponder = COM::p_off;
            }
            else if (COM::command == "hard"){COM::ponder = COM::p_off;} ///@todo pondering
            else if (COM::command == "quit"){_exit(0);}
            else if (COM::command == "pause"){
                COM::stopPonder();
                COM::readLine();
                while(COM::command != "resume"){
                    Logging::LogIt(Logging::logInfo) << "Error (paused): " << COM::command ;
                    COM::readLine();
                }
            }
            else if( strncmp(COM::command.c_str(), "time",4) == 0) {
                int centisec = 0;
                sscanf(COM::command.c_str(), "time %d", &centisec);
                // just updating remaining time in curren TC (shall be classic TC or sudden death)
                TimeMan::isDynamic        = true;
                TimeMan::msecUntilNextTC  = centisec*10;
            }
            else if ( strncmp(COM::command.c_str(), "st", 2) == 0) {
                int msecPerMove = 0;
                sscanf(COM::command.c_str(), "st %d", &msecPerMove);
                // forced move time
                TimeMan::isDynamic        = false;
                TimeMan::nbMoveInTC       = -1;
                TimeMan::msecPerMove      = msecPerMove*1000;
                TimeMan::msecInTC         = -1;
                TimeMan::msecInc          = -1;
                TimeMan::msecUntilNextTC  = -1;
                TimeMan::moveToGo         = -1;
                COM::depth                = MAX_DEPTH; // infinity
            }
            else if ( strncmp(COM::command.c_str(), "sd", 2) == 0) {
                int d = 0;
                sscanf(COM::command.c_str(), "sd %d", &d);
                COM::depth = d;
                if(COM::depth<0) COM::depth = 8;
                // forced move depth
                TimeMan::isDynamic        = false;
                TimeMan::nbMoveInTC       = -1;
                TimeMan::msecPerMove      = 60*60*1000*24; // 1 day == infinity ...
                TimeMan::msecInTC         = -1;
                TimeMan::msecInc          = -1;
                TimeMan::msecUntilNextTC  = -1;
                TimeMan::moveToGo         = -1;
            }
            else if(strncmp(COM::command.c_str(), "level",5) == 0) {
                int timeTC = 0, secTC = 0, inc = 0, mps = 0;
                if( sscanf(COM::command.c_str(), "level %d %d %d", &mps, &timeTC, &inc) != 3) sscanf(COM::command.c_str(), "level %d %d:%d %d", &mps, &timeTC, &secTC, &inc);
                // classic TC is mps>0, else sudden death
                TimeMan::isDynamic        = false;
                TimeMan::nbMoveInTC       = mps;
                TimeMan::msecPerMove      = -1;
                TimeMan::msecInTC         = timeTC*60000 + secTC * 1000;
                TimeMan::msecInc          = inc * 1000;
                TimeMan::msecUntilNextTC  = timeTC; // just an init here, will be managed using "time" command later
                TimeMan::moveToGo         = -1;
                COM::depth                = MAX_DEPTH; // infinity
            }
            else if ( COM::command == "edit"){ }
            else if ( COM::command == "?"){ COM::stop(); }
            else if ( COM::command == "draw")  { }
            else if ( COM::command == "undo")  { }
            else if ( COM::command == "remove"){ }
            //************ end of Xboard command ********//
            // let's try to read the unknown command as a move ... trying to fix a scid versus PC issue ...
            else if ( !receiveMove(COM::command)) Logging::LogIt(Logging::logInfo) << "Xboard does not know this command " << COM::command ;
        } // readline
    } // while true
}
} // XBoard

#ifdef WITH_UCI
#include "Add-On/uci.cc"
#endif

#ifdef DEBUG_TOOL
#include "Add-On/cli.cc"
#endif

#if defined(WITH_TEXEL_TUNING) || defined(WITH_TEST_SUITE)
#include "Add-On/extendedPosition.cc"
#endif

#ifdef WITH_TEXEL_TUNING
#include "Add-On/texelTuning.cc"
#endif

#ifdef WITH_TEST_SUITE
#include "Add-On/testSuite.cc"
#endif

void init(int argc, char ** argv) {
    Logging::hellooo();
    Options::initOptions(argc, argv);
    Logging::init(); // after reading options
    Zobrist::initHash();
    TT::initTable();
    StaticConfig::initLMR();
    StaticConfig::initMvvLva();
    BB::initMask();
    MaterialHash::KPK::init();
    MaterialHash::MaterialHashInitializer::init();
    initEval();
    ThreadPool::instance().setup(DynamicConfig::threads);
    Book::initBook();
#ifdef WITH_SYZYGY
    SyzygyTb::initTB("syzygy");
#endif
}

int main(int argc, char ** argv){
    init(argc, argv);
    
#ifdef WITH_TEST_SUITE
    if ( argc > 1 && test(argv[1])) return 0;
#endif

#ifdef WITH_TEXEL_TUNING
    if ( argc > 1 && std::string(argv[1]) == "-texel" ) { TexelTuning(argv[2]); return 0;}
#endif

#ifdef DEBUG_TOOL
    if ( argc < 2 ) Logging::LogIt(Logging::logFatal) << "Hint: You can use -xboard command line option to enter xboard mode";
    return cliManagement(argv[1],argc,argv);
#else
    ///@todo factorize with the one in cliManagement
    TimeMan::init();
#ifndef WITH_UCI
    XBoard::init();
    XBoard::xboard();
#else
    UCI::init();
    UCI::uci();
#endif
    return 0;
#endif
}
