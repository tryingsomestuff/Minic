const std::string MinicVersion = "3.42";

#if defined(ARDUINO) || defined(ESP32)
#   define WITH_SMALL_MEMORY
#endif

// *** options
#define WITH_UCI    // include or not UCI protocol support
#define WITH_XBOARD // include or not XBOARB protocol support
#if !defined(WITH_SMALL_MEMORY)
#define WITH_MAGIC  // use magic bitboard or HB ones
#define WITH_NNUE   // include or not NNUE support
#define WITH_STATS  // produce or not search statistics
//#define WITH_BETACUTSTATS // activate beta cutoff statistics
#define WITH_MATERIAL_TABLE // use (and build ! cost 27Mb) or not the material table
//#define WITH_MPI    // support "distributed" version or not
#define WITH_ASYNC_ANALYZE // support ASYNC call for CLI analysis
#ifndef _MSC_VER
#define WITH_SYZYGY // include or not syzgy ETG support
#endif // _MSC_VER
#endif // WITH_SMALL_MEMORY
//#define WITH_FMTLIB

//#define WITHOUT_FILESYSTEM              // some compiler don't support whole std:filesystem
//#define LIMIT_THREADS_TO_PHYSICAL_CORES // in order to restrict thread to the number of physical core
//#define REPRODUCTIBLE_RESULTS           // clear state table betwwen all new search (not only all new games)
#define WITH_NNUE_CLIPPED_RELU            // use clipped relu instead of relu for NNUE
#ifndef __ANDROID__
#define USE_SIMD_INTRIN                   // on simd architectures, use a hand written dot product
#endif

// *** Optim (?)
#define USE_PARTIAL_SORT        // do not sort every move in move list
//#define WITH_EVALSCORE_AS_INT // in fact just as slow as my basic impl ...

// *** Add-ons
#if !defined(WITH_SMALL_MEMORY)
#ifndef DEBUG_TOOL
#define DEBUG_TOOL // forced
#endif
#define WITH_TEST_SUITE
//#define WITH_PGN_PARSER
#endif // WITH_SMALL_MEMORY

// *** NNUE learning things
#ifndef WITHOUT_FILESYSTEM
#define WITH_GENFILE
#endif

#ifdef WITH_NNUE
#ifndef WITHOUT_FILESYSTEM
#define WITH_DATA2BIN
#endif
#endif

// *** Tuning
//#define WITH_TIMER
//#define WITH_SEARCH_TUNING
//#define WITH_EVAL_TUNING
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
//#define DEBUG_TT_CHECK
//#define DEBUG_FIFTY_COLLISION
