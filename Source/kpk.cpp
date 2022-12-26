#include "kpk.hpp"

#include "attack.hpp"
#include "logging.hpp"
#include "position.hpp"

namespace KPK {

#if !defined(WITH_SMALL_MEMORY)
namespace {
array1d<uint32_t,KPK::KPKmaxIndex / 32> KPKBitbase;               // force 32bit uint
FORCE_FINLINE unsigned KPKindex(Color us, Square bksq, Square wksq, Square psq) {
   return wksq | (bksq << 6) | (us << 12) | (SQFILE(psq) << 13) | ((6 - SQRANK(psq)) << 15);
}
} // namespace

Square normalizeSquare(const Position& p, const Color strongSide, const Square sq) {
   assert(BB::countBit(p.pieces_const<P_wp>(strongSide)) == 1); // only for KPK !
   const Square nsq = SQFILE(BBTools::SquareFromBitBoard(p.pieces_const<P_wp>(strongSide))) >= File_e ? HFlip(sq) : sq;
   return strongSide == Co_White ? nsq : VFlip(nsq);
}

KPKPosition::KPKPosition(const unsigned idx) :
      ksq ({ static_cast<Square>(idx & 0x3F), static_cast<Square>((idx >> 6) & 0x3F) }),
      us  (static_cast<Color>((idx >> 12) & 0x01)),
      psq (MakeSquare(File((idx >> 13) & 0x3), Rank(6 - ((idx >> 15) & 0x7)))){ // first init
   if (chebyshevDistance(ksq[Co_White], ksq[Co_Black]) <= 1 || ksq[Co_White] == psq || ksq[Co_Black] == psq ||
       (us == Co_White && (BBTools::mask[psq].pawnAttack[Co_White] & SquareToBitboard(ksq[Co_Black]))))
      result = kpk_invalid;
   else if (us == Co_White && SQRANK(psq) == 6 && ksq[us] != psq + 8 &&
            (chebyshevDistance(ksq[~us], psq + 8) > 1 || (BBTools::mask[ksq[us]].king & SquareToBitboard(psq + 8))))
      result = kpk_win;
   else if (us == Co_Black && (!(BBTools::mask[ksq[us]].king & ~(BBTools::mask[ksq[~us]].king | BBTools::mask[psq].pawnAttack[~us])) ||
                               (BBTools::mask[ksq[us]].king & SquareToBitboard(psq) & ~BBTools::mask[ksq[~us]].king)))
      result = kpk_draw;
   else
      result = kpk_unknown; // done later
}

kpk_result KPKPosition::preCompute(const array1d<KPKPosition, KPKmaxIndex>& db) {
   return us == Co_White ? preCompute<Co_White>(db) : preCompute<Co_Black>(db);
}

template<Color Us> kpk_result KPKPosition::preCompute(const array1d<KPKPosition, KPKmaxIndex>& db) {
   constexpr Color      Them = (Us == Co_White ? Co_Black : Co_White);
   constexpr kpk_result good = (Us == Co_White ? kpk_win : kpk_draw);
   constexpr kpk_result bad  = (Us == Co_White ? kpk_draw : kpk_win);
   kpk_result r = kpk_invalid;
   BB::applyOn(BBTools::mask[ksq[us]].king, [&](const Square & k){ r |= (Us == Co_White ? db[KPKindex(Them, ksq[Them], k, psq)] : db[KPKindex(Them, k, ksq[Them], psq)]); });
   if (Us == Co_White) {
      if (SQRANK(psq) < 6) r |= db[KPKindex(Them, ksq[Them], ksq[Us], psq + 8)];
      if (SQRANK(psq) == 1 && psq + 8 != ksq[Us] && psq + 8 != ksq[Them]) r |= db[KPKindex(Them, ksq[Them], ksq[Us], psq + 8 + 8)];
   }
   return result = r & good ? good : r & kpk_unknown ? kpk_unknown : bad;
}

bool probe(Square wksq, const Square wpsq, const Square bksq, const Color us) {
   assert(isValidSquare(wksq));
   assert(isValidSquare(wpsq));
   assert(isValidSquare(bksq));
   assert(SQFILE(wpsq) <= 4);
   const unsigned idx = KPKindex(us, bksq, wksq, wpsq);
   assert(idx < KPKmaxIndex);
   return KPKBitbase[idx / 32] & (1 << (idx & 0x1F));
}
#endif

void init() {
#if !defined(WITH_SMALL_MEMORY)
   Logging::LogIt(Logging::logInfo) << "KPK init";
   Logging::LogIt(Logging::logInfo) << "KPK table size : " << KPKmaxIndex / 32 * sizeof(uint32_t) / 1024 << "Kb";
   array1d<KPKPosition, KPKmaxIndex> db;
   unsigned idx = 0;
   bool repeat = true;
   for (idx = 0; idx < KPKmaxIndex; ++idx) db[idx] = KPKPosition(idx); // init
   // loop until all the dababase is filled
   while (repeat)
      for (repeat = false, idx = 0; idx < KPKmaxIndex; ++idx) repeat |= (db[idx] == kpk_unknown && db[idx].preCompute(db) != kpk_unknown);
   for (idx = 0; idx < KPKmaxIndex; ++idx) {
      if (db[idx] == kpk_win) { KPKBitbase[idx / 32] |= 1 << (idx & 0x1F); }
   } // compress
#endif
}

} // namespace KPK
