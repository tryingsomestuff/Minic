#include "hash.hpp"

#include "bitboardTools.hpp"
#include "logging.hpp"
#include "position.hpp"
#include "tools.hpp"

namespace Zobrist {
    Hash ZT[64][14];
    void initHash() {
        Logging::LogIt(Logging::logInfo) << "Init hash";
        for (int k = 0; k < 64; ++k) for (int j = 0; j < 14; ++j) ZT[k][j] = randomInt(Hash(0),Hash(UINT64_MAX));
    }
}

Hash computeHash(const Position &p){
#ifdef DEBUG_HASH
    Hash h = p.h;
    p.h = nullHash;
#endif
    if (p.h != nullHash) return p.h;
    //++ThreadPool::instance().main().stats.counters[Stats::sid_hashComputed]; // shall of course never happend !
    for (Square k = 0; k < 64; ++k){ ///todo try if BB is faster here ?
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
    if ( h != nullHash && h != p.h ){ Logging::LogIt(Logging::logFatal) << "Hash error " << ToString(p.lastMove) << ToString(p,true); }
#endif
    return p.h;
}

Hash computePHash(const Position &p){
#ifdef DEBUG_PHASH
    Hash h = p.ph;
    p.ph = nullHash;
#endif
    if (p.ph != nullHash) return p.ph;
    //++ThreadPool::instance().main().stats.counters[Stats::sid_hashComputed]; // shall of course never happend !
    BitBoard bb = p.whitePawn();
    while (bb) { p.ph ^= Zobrist::ZT[popBit(bb)][P_wp + PieceShift]; }
    bb = p.blackPawn();
    while (bb) { p.ph ^= Zobrist::ZT[popBit(bb)][P_bp + PieceShift]; }
    bb = p.whiteKing();
    while (bb) { p.ph ^= Zobrist::ZT[popBit(bb)][P_wk + PieceShift]; }
    bb = p.blackKing();
    while (bb) { p.ph ^= Zobrist::ZT[popBit(bb)][P_bk + PieceShift]; }
#ifdef DEBUG_PHASH
    if ( h != nullHash && h != p.ph ){ Logging::LogIt(Logging::logFatal) << "Pawn Hash error " << ToString(p.lastMove) << ToString(p,true) << p.ph << " != " << h; }
#endif
    return p.ph;
}

