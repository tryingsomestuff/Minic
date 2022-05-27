#include "hash.hpp"

#include "bitboardTools.hpp"
#include "logging.hpp"
#include "position.hpp"
#include "tools.hpp"

namespace Zobrist {
Hash ZT[NbSquare][14];
Hash ZTCastling[16];
Hash ZTMove[std::numeric_limits<uint16_t>::max()];

void initHash() {
   Logging::LogIt(Logging::logInfo) << "Init hash";
   Logging::LogIt(Logging::logInfo) << "Size of zobrist table " << (sizeof(ZT)+sizeof(ZTCastling)) / 1024 << "Kb";
   Logging::LogIt(Logging::logInfo) << "Size of zobrist table for moves " << sizeof(ZTMove) / 1024 << "Kb";
   for (int k = 0; k < NbSquare; ++k)
      for (int j = 0; j < 14; ++j) ZT[k][j] = randomInt<Hash, 42>(std::numeric_limits<Hash>::min(), std::numeric_limits<Hash>::max());
   for (int k = 0; k < 16; ++k) ZTCastling[k] = randomInt<Hash, 42>(std::numeric_limits<Hash>::min(), std::numeric_limits<Hash>::max());
   for (int k = 0; k < std::numeric_limits<uint16_t>::max(); ++k) ZTMove[k] = randomInt<Hash, 42>(std::numeric_limits<Hash>::min(), std::numeric_limits<Hash>::max());
}
} // namespace Zobrist

Hash computeHash(const Position &p) {
#ifdef DEBUG_HASH
   Hash h = p.h;
   p.h    = nullHash;
#endif
   if (p.h != nullHash) return p.h;
   //++ThreadPool::instance().main().stats.counters[Stats::sid_hashComputed]; // shall of course never happend !
   for (Square k = 0; k < NbSquare; ++k) { ///todo try if BB is faster here ?
      const Piece pp = p.board_const(k);
      if (pp != P_none) p.h ^= Zobrist::ZT[k][PieceIdx(pp)];
   }
   if (p.ep != INVALIDSQUARE) p.h ^= Zobrist::ZT[p.ep][NbPiece];
   p.h ^= Zobrist::ZTCastling[p.castling];
   if (p.c == Co_White) p.h ^= Zobrist::ZT[3][NbPiece];
   if (p.c == Co_Black) p.h ^= Zobrist::ZT[4][NbPiece];
#ifdef DEBUG_HASH
   if (h != nullHash && h != p.h) { Logging::LogIt(Logging::logFatal) << "Hash error " << ToString(p.lastMove) << ToString(p, true); }
#endif
   return p.h;
}

Hash computePHash(const Position &p) {
#ifdef DEBUG_PHASH
   Hash h = p.ph;
   p.ph   = nullHash;
#endif
   if (p.ph != nullHash) return p.ph;
   //++ThreadPool::instance().main().stats.counters[Stats::sid_hashComputed]; // shall of course never happend !
   BitBoard bb = p.whitePawn();
   while (bb) { p.ph ^= Zobrist::ZT[BB::popBit(bb)][PieceIdx(P_wp)]; }
   bb = p.blackPawn();
   while (bb) { p.ph ^= Zobrist::ZT[BB::popBit(bb)][PieceIdx(P_bp)]; }
   bb = p.whiteKing();
   while (bb) { p.ph ^= Zobrist::ZT[BB::popBit(bb)][PieceIdx(P_wk)]; }
   bb = p.blackKing();
   while (bb) { p.ph ^= Zobrist::ZT[BB::popBit(bb)][PieceIdx(P_bk)]; }
#ifdef DEBUG_PHASH
   if (h != nullHash && h != p.ph) {
      Logging::LogIt(Logging::logFatal) << "Pawn Hash error " << ToString(p.lastMove) << ToString(p, true) << p.ph << " != " << h;
   }
#endif
   return p.ph;
}
