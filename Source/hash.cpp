#include "hash.hpp"

#include "bitboardTools.hpp"
#include "logging.hpp"
#include "position.hpp"
#include "tools.hpp"

namespace Zobrist {
array2d<Hash,NbSquare,14> ZT;
array1d<Hash,16> ZTCastling;

inline constexpr int hash_seed = 42;

namespace {
   // Test bit distribution across all Zobrist hash values
   void testBitDistribution() {
      std::array<uint64_t, 64> bitCounts = {0};
      uint64_t totalHashes = 0;
      
      // Count bits across ZT table
      for (int k = 0; k < NbSquare; ++k) {
         for (int j = 0; j < 14; ++j) {
            Hash h = ZT[k][j];
            for (int bit = 0; bit < 64; ++bit) {
               if (h & (1ULL << bit)) bitCounts[bit]++;
            }
            totalHashes++;
         }
      }
      
      // Count bits in castling table
      for (int k = 0; k < 16; ++k) {
         Hash h = ZTCastling[k];
         for (int bit = 0; bit < 64; ++bit) {
            if (h & (1ULL << bit)) bitCounts[bit]++;
         }
         totalHashes++;
      }
      
      Logging::LogIt(Logging::logInfo) << "Zobrist bit distribution test (should be ~0.5 for each bit):";
      bool allGood = true;
      for (int bit = 0; bit < 64; ++bit) {
         double ratio = bitCounts[bit] / static_cast<double>(totalHashes);
         if (ratio < 0.45 || ratio > 0.55) {
            Logging::LogIt(Logging::logWarn) << "  Bit " << bit << " has poor distribution: " << ratio;
            allGood = false;
         }
      }
      if (allGood) {
         Logging::LogIt(Logging::logInfo) << "  All bits have good distribution (between 0.45 and 0.55)";
      }
   }
   
   // Calculate Hamming distance between two hashes
   int hammingDistance(Hash a, Hash b) {
      return __builtin_popcountll(a ^ b);
   }
   
   // Test average Hamming distance (should be around 32 for random 64-bit values)
   void testHammingDistance() {
      const int sampleSize = 1000;
      uint64_t totalDistance = 0;
      int comparisons = 0;
      
      // Sample random pairs from ZT table
      for (int i = 0; i < sampleSize && i < NbSquare * 14; ++i) {
         int k1 = (i * 7) % NbSquare;
         int j1 = (i * 13) % 14;
         int k2 = ((i + 1) * 11) % NbSquare;
         int j2 = ((i + 1) * 17) % 14;
         
         if (k1 != k2 || j1 != j2) {
            totalDistance += hammingDistance(ZT[k1][j1], ZT[k2][j2]);
            comparisons++;
         }
      }
      
      double avgDistance = static_cast<double>(totalDistance) / static_cast<double>(comparisons);
      Logging::LogIt(Logging::logInfo) << "Zobrist Hamming distance test:";
      Logging::LogIt(Logging::logInfo) << "  Average Hamming distance: " << avgDistance << " (ideal: ~32)";
      
      if (avgDistance < 28 || avgDistance > 36) {
         Logging::LogIt(Logging::logWarn) << "  Hamming distance outside expected range [28, 36]";
      }
   }
   
   // Test for unwanted correlations (adjacent squares)
   void testCorrelations() {
      Logging::LogIt(Logging::logInfo) << "Zobrist correlation test (adjacent squares):";
      
      // Check adjacent squares for the same piece type
      int poorDistances = 0;
      int totalComparisons = 0;
      
      for (int k = 0; k < NbSquare - 1; ++k) {
         for (int j = 0; j < 14; ++j) {
            int dist = hammingDistance(ZT[k][j], ZT[k+1][j]);
            totalComparisons++;
            // Count outliers (very low or very high Hamming distance)
            if (dist < 20 || dist > 44) {
               poorDistances++;
            }
         }
      }
      
      double poorRatio = poorDistances / static_cast<double>(totalComparisons);
      Logging::LogIt(Logging::logInfo) << "  Outliers: " << poorDistances << "/" << totalComparisons 
                                        << " (" << (poorRatio * 100) << "%)";
      
      // More than 10% outliers would indicate a real problem
      if (poorRatio > 0.10) {
         Logging::LogIt(Logging::logWarn) << "  WARNING: High correlation detected (>10% outliers)";
      } else {
         Logging::LogIt(Logging::logInfo) << "  No significant correlations detected";
      }
   }
}

void initHash() {
   Logging::LogIt(Logging::logInfo) << "Init hash";
   Logging::LogIt(Logging::logInfo) << "Size of zobrist table " << (sizeof(ZT)+sizeof(ZTCastling)) / 1024 << "Kb";
   for (int k = 0; k < NbSquare; ++k)
      for (int j = 0; j < 14; ++j) ZT[k][j] = randomInt<Hash, hash_seed>(std::numeric_limits<Hash>::min(), std::numeric_limits<Hash>::max());
   for (int k = 0; k < 16; ++k) ZTCastling[k] = randomInt<Hash, hash_seed>(std::numeric_limits<Hash>::min(), std::numeric_limits<Hash>::max());
   
   // Run quality tests on generated Zobrist values
   testBitDistribution();
   testHammingDistance();
   testCorrelations();
}
} // namespace Zobrist

Hash computeHash(const Position &p) {
#ifdef DEBUG_HASH
   Hash h = p.h;
   p.h    = nullHash;
#endif
   if (p.h != nullHash) return p.h;
   //++ThreadPool::instance().main().stats.counters[Stats::sid_hashComputed]; // shall of course never happend !
   BB::applyOn(p.whitePawn(),   [&](const Square & k){ p.h ^= Zobrist::ZT[k][PieceIdx(P_wp)]; });
   BB::applyOn(p.whiteKnight(), [&](const Square & k){ p.h ^= Zobrist::ZT[k][PieceIdx(P_wn)]; });
   BB::applyOn(p.whiteBishop(), [&](const Square & k){ p.h ^= Zobrist::ZT[k][PieceIdx(P_wb)]; });
   BB::applyOn(p.whiteRook(),   [&](const Square & k){ p.h ^= Zobrist::ZT[k][PieceIdx(P_wr)]; });
   BB::applyOn(p.whiteQueen(),  [&](const Square & k){ p.h ^= Zobrist::ZT[k][PieceIdx(P_wq)]; });
   BB::applyOn(p.whiteKing(),   [&](const Square & k){ p.h ^= Zobrist::ZT[k][PieceIdx(P_wk)]; });
   BB::applyOn(p.blackPawn(),   [&](const Square & k){ p.h ^= Zobrist::ZT[k][PieceIdx(P_bp)]; });
   BB::applyOn(p.blackKnight(), [&](const Square & k){ p.h ^= Zobrist::ZT[k][PieceIdx(P_bn)]; });
   BB::applyOn(p.blackBishop(), [&](const Square & k){ p.h ^= Zobrist::ZT[k][PieceIdx(P_bb)]; });
   BB::applyOn(p.blackRook(),   [&](const Square & k){ p.h ^= Zobrist::ZT[k][PieceIdx(P_br)]; });
   BB::applyOn(p.blackQueen(),  [&](const Square & k){ p.h ^= Zobrist::ZT[k][PieceIdx(P_bq)]; });
   BB::applyOn(p.blackKing(),   [&](const Square & k){ p.h ^= Zobrist::ZT[k][PieceIdx(P_bk)]; });
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
   BB::applyOn(p.whitePawn(), [&](const Square & k){ p.ph ^= Zobrist::ZT[k][PieceIdx(P_wp)]; });
   BB::applyOn(p.blackPawn(), [&](const Square & k){ p.ph ^= Zobrist::ZT[k][PieceIdx(P_bp)]; });
   BB::applyOn(p.whiteKing(), [&](const Square & k){ p.ph ^= Zobrist::ZT[k][PieceIdx(P_wk)]; });
   BB::applyOn(p.blackKing(), [&](const Square & k){ p.ph ^= Zobrist::ZT[k][PieceIdx(P_bk)]; });
#ifdef DEBUG_PHASH
   if (h != nullHash && h != p.ph) {
      Logging::LogIt(Logging::logFatal) << "Pawn Hash error " << ToString(p.lastMove) << ToString(p, true) << p.ph << " != " << h;
   }
#endif
   return p.ph;
}

#ifdef WITH_NONPAWN_CORRHIST
// king excluded on purpose
Hash computeNonPawnHash(const Position &p, Color c) {
#ifdef DEBUG_NONPAWNHASH
   Hash h = p.nph[c];
   p.nph[c] = nullHash;
#endif
   if (p.nph[c] != nullHash) return p.nph[c];
   //++ThreadPool::instance().main().stats.counters[Stats::sid_nonPawnHashComputed]; // shall of course never happend !
   if (c == Co_White) {
      BB::applyOn(p.whiteKnight(), [&](const Square & k){ p.nph[c] ^= Zobrist::ZT[k][PieceIdx(P_wn)]; });
      BB::applyOn(p.whiteBishop(), [&](const Square & k){ p.nph[c] ^= Zobrist::ZT[k][PieceIdx(P_wb)]; });
      BB::applyOn(p.whiteRook(),   [&](const Square & k){ p.nph[c] ^= Zobrist::ZT[k][PieceIdx(P_wr)]; });
      BB::applyOn(p.whiteQueen(),  [&](const Square & k){ p.nph[c] ^= Zobrist::ZT[k][PieceIdx(P_wq)]; });
   }
   else {
      BB::applyOn(p.blackKnight(), [&](const Square & k){ p.nph[c] ^= Zobrist::ZT[k][PieceIdx(P_bn)]; });
      BB::applyOn(p.blackBishop(), [&](const Square & k){ p.nph[c] ^= Zobrist::ZT[k][PieceIdx(P_bb)]; });
      BB::applyOn(p.blackRook(),   [&](const Square & k){ p.nph[c] ^= Zobrist::ZT[k][PieceIdx(P_br)]; });
      BB::applyOn(p.blackQueen(),  [&](const Square & k){ p.nph[c] ^= Zobrist::ZT[k][PieceIdx(P_bq)]; });
   }
#ifdef DEBUG_NONPAWNHASH
   if (h != nullHash && h != p.nph[c]) {
      Logging::LogIt(Logging::logFatal) << "NonPawn Hash error " << ToString(p.lastMove) << ToString(p, true) << p.nph[c] << " != " << h;
   }
#endif
   return p.nph[c];
}

Hash nonPawnKey(const Position &p, Color c) {
   return p.nph[c] ^ Zobrist::ZT[p.king[c]][PieceIdx(c == Co_White ? P_wk : P_bk)];
}
#endif // WITH_NONPAWN_CORRHIST

#ifdef WITH_MINOR_CORRHIST
Hash computeMinorHash(const Position &p) {
#ifdef DEBUG_MINORHASH
   Hash h = p.mnh;
   p.mnh   = nullHash;
#endif
   if (p.mnh != nullHash) return p.mnh;
   //++ThreadPool::instance().main().stats.counters[Stats::sid_minorHashComputed]; // shall of course never happend !
   BB::applyOn(p.whiteKnight(), [&](const Square & k){ p.mnh ^= Zobrist::ZT[k][PieceIdx(P_wn)]; });
   BB::applyOn(p.whiteBishop(), [&](const Square & k){ p.mnh ^= Zobrist::ZT[k][PieceIdx(P_wb)]; });
   BB::applyOn(p.blackKnight(), [&](const Square & k){ p.mnh ^= Zobrist::ZT[k][PieceIdx(P_bn)]; });
   BB::applyOn(p.blackBishop(), [&](const Square & k){ p.mnh ^= Zobrist::ZT[k][PieceIdx(P_bb)]; });
#ifdef DEBUG_MINORHASH
   if (h != nullHash && h != p.mnh) {
      Logging::LogIt(Logging::logFatal) << "Minor Hash error " << ToString(p.lastMove) << ToString(p, true) << p.mnh << " != " << h;
   }
#endif
   return p.mnh;
}

Hash minorKey(const Position &p) {
   return p.mnh ^ Zobrist::ZT[p.king[Co_White]][PieceIdx(P_wk)] ^ Zobrist::ZT[p.king[Co_Black]][PieceIdx(P_bk)];
}
#endif // WITH_MINOR_CORRHIST

#ifdef WITH_MAJOR_CORRHIST
Hash computeMajorHash(const Position &p) {
#ifdef DEBUG_MAJORHASH
   Hash h = p.mjh;
   p.mjh   = nullHash;
#endif
   if (p.mjh != nullHash) return p.mjh;
   //++ThreadPool::instance().main().stats.counters[Stats::sid_majorHashComputed]; // shall of course never happend !
   BB::applyOn(p.whiteRook(),  [&](const Square & k){ p.mjh ^= Zobrist::ZT[k][PieceIdx(P_wr)]; });
   BB::applyOn(p.whiteQueen(), [&](const Square & k){ p.mjh ^= Zobrist::ZT[k][PieceIdx(P_wq)]; });
   BB::applyOn(p.blackRook(),  [&](const Square & k){ p.mjh ^= Zobrist::ZT[k][PieceIdx(P_br)]; });
   BB::applyOn(p.blackQueen(), [&](const Square & k){ p.mjh ^= Zobrist::ZT[k][PieceIdx(P_bq)]; });
#ifdef DEBUG_MAJORHASH
   if (h != nullHash && h != p.mjh) {
      Logging::LogIt(Logging::logFatal) << "Major Hash error " << ToString(p.lastMove) << ToString(p, true) << p.mjh << " != " << h;
   }
#endif
   return p.mjh;
}

Hash majorKey(const Position &p) {
   return p.mjh ^ Zobrist::ZT[p.king[Co_White]][PieceIdx(P_wk)] ^ Zobrist::ZT[p.king[Co_Black]][PieceIdx(P_bk)];
}
#endif // WITH_MAJOR_CORRHIST

