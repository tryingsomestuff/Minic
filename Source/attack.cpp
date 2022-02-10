#include "attack.hpp"

#include "bitboard.hpp"
#include "logging.hpp"
#include "timers.hpp"

#ifndef WITH_MAGIC
namespace {
int _ranks[512] = {0};
}
#endif

namespace BBTools {

Mask mask[NbSquare];

// This initialisation function is taken from Dumb chess engine by Richard Delorme
void initMask() {
   Logging::LogIt(Logging::logInfo) << "Init mask";
   int d[NbSquare][NbSquare] = {{0}};
   for (Square x = 0; x < NbSquare; ++x) {
      mask[x].bbsquare = SquareToBitboard(x);
      for (int i = -1; i <= 1; ++i) {
         for (int j = -1; j <= 1; ++j) {
            if (i == 0 && j == 0) continue;
            for (int r = SQRANK(x) + i, f = SQFILE(x) + j; 0 <= r && r < 8 && 0 <= f && f < 8; r += i, f += j) {
               const int y = 8 * r + f;
               d[x][y]     = (8 * i + j);
               for (int z = x + d[x][y]; z != y; z += d[x][y]) mask[x].between[y] |= SquareToBitboard(z);
            }
            const int r = SQRANK(x);
            const int f = SQFILE(x);
            if (0 <= r + i && r + i < 8 && 0 <= f + j && f + j < 8) mask[x].kingZone |= SquareToBitboard((SQRANK(x) + i) * 8 + SQFILE(x) + j);
         }
      }

      for (int y = x - 9; y >= 0 && d[x][y] == -9; y -= 9) mask[x].diagonal |= SquareToBitboard(y);
      for (int y = x + 9; y < NbSquare && d[x][y] == 9; y += 9) mask[x].diagonal |= SquareToBitboard(y);

      for (int y = x - 7; y >= 0 && d[x][y] == -7; y -= 7) mask[x].antidiagonal |= SquareToBitboard(y);
      for (int y = x + 7; y < NbSquare && d[x][y] == 7; y += 7) mask[x].antidiagonal |= SquareToBitboard(y);

      for (int y = x - 8; y >= 0; y -= 8) mask[x].file |= SquareToBitboard(y);
      for (int y = x + 8; y < NbSquare; y += 8) mask[x].file |= SquareToBitboard(y);

      int f = SQFILE(x);
      int r = SQRANK(x);
      for (int i = -1, c = 1, dp = 6; i <= 1; i += 2, c = 0, dp = 1) {
         for (int j = -1; j <= 1; j += 2)
            if (0 <= r + i && r + i < 8 && 0 <= f + j && f + j < 8) { mask[x].pawnAttack[c] |= SquareToBitboard((r + i) * 8 + (f + j)); }
         if (0 <= r + i && r + i < 8) {
            mask[x].push[c] = SquareToBitboard((r + i) * 8 + f);
            if (r == dp) mask[x].dpush[c] = SquareToBitboard((r + 2 * i) * 8 + f); // double push
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
      wspan = _shiftNorth(wspan);
      BitBoard bspan = SquareToBitboardTable(x);
      bspan |= bspan >> 8;
      bspan |= bspan >> 16;
      bspan |= bspan >> 32;
      bspan = _shiftSouth(bspan);

      mask[x].frontSpan[Co_White] = mask[x].rearSpan[Co_Black] = wspan;
      mask[x].frontSpan[Co_Black] = mask[x].rearSpan[Co_White] = bspan;

      mask[x].passerSpan[Co_White] = wspan;
      mask[x].passerSpan[Co_White] |= _shiftWest(wspan);
      mask[x].passerSpan[Co_White] |= _shiftEast(wspan);
      mask[x].passerSpan[Co_Black] = bspan;
      mask[x].passerSpan[Co_Black] |= _shiftWest(bspan);
      mask[x].passerSpan[Co_Black] |= _shiftEast(bspan);

      mask[x].attackFrontSpan[Co_White] = _shiftWest(wspan);
      mask[x].attackFrontSpan[Co_White] |= _shiftEast(wspan);
      mask[x].attackFrontSpan[Co_Black] = _shiftWest(bspan);
      mask[x].attackFrontSpan[Co_Black] |= _shiftEast(bspan);
   }

#ifndef WITH_MAGIC
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
         _ranks[o * 8 + k] = y;
      }
   }
#endif
}

#ifndef WITH_MAGIC // then use HQBB

BitBoard attack(const BitBoard occupancy, const Square s, const BitBoard m) {
   assert(isValidSquare(s));
   START_TIMER
   BitBoard forward = occupancy & m;
   BitBoard reverse = swapbits(forward);
   forward -= SquareToBitboard(s);
   reverse -= SquareToBitboard(s ^ 63);
   forward ^= swapbits(reverse);
   forward &= m;
   STOP_AND_SUM_TIMER(Attack)
   return forward;
}

BitBoard rankAttack(const BitBoard occupancy, const Square s) {
   assert(isValidSquare(s));
   const int f = SQFILE(x);
   const int r = s & 56;
   return static_cast<BitBoard>(_ranks[((occupancy >> r) & 126) * 4 + f]) << r;
}

BitBoard fileAttack(const BitBoard occupancy, const Square s) { 
   assert(isValidSquare(s));
   return attack(occupancy, s, mask[s].file); 
}

BitBoard diagonalAttack(const BitBoard occupancy, const Square s) { 
   assert(isValidSquare(s));
   return attack(occupancy, s, mask[s].diagonal); 
}

BitBoard antidiagonalAttack(const BitBoard occupancy, const Square s) { 
   assert(isValidSquare(s));
   return attack(occupancy, s, mask[s].antidiagonal); 
}

#else // MAGIC

namespace MagicBB {

SMagic bishopMagic[NbSquare];
SMagic rookMagic[NbSquare];

BitBoard bishopAttacks[NbSquare][1 << BISHOP_INDEX_BITS];
BitBoard rookAttacks[NbSquare][1 << ROOK_INDEX_BITS];

const BitBoard bishopMagics[] = {
    0x1002004102008200, 0x1002004102008200, 0x4310002248214800, 0x402010c110014208, 0xa000a06240114001, 0xa000a06240114001, 0x402010c110014208, 0xa000a06240114001,
    0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x100c009840001000, 0x4310002248214800, 0xa000a06240114001, 0x4310002248214800,
    0x4310002248214800, 0x822143005020a148, 0x0001901c00420040, 0x0880504024308060, 0x0100201004200002, 0xa000a06240114001, 0x822143005020a148, 0x1002004102008200,
    0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x2008080100820102, 0x1481010004104010, 0x0002052000100024, 0xc880221002060081, 0xc880221002060081,
    0x4310002248214800, 0xc880221002060081, 0x0001901c00420040, 0x8400208020080201, 0x000e008400060020, 0x00449210e3902028, 0x402010c110014208, 0xc880221002060081,
    0x100c009840001000, 0xc880221002060081, 0x1000820800c00060, 0x2803101084008800, 0x2200608200100080, 0x0040900130840090, 0x0024010008800a00, 0x0400110410804810,
    0x402010c110014208, 0xa000a06240114001, 0xa000a06240114001, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200,
    0xa000a06240114001, 0x4310002248214800, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200
};

const BitBoard rookMagics[] = {
    0x8200108041020020, 0x8200108041020020, 0xc880221002060081, 0x0009100804021000, 0x0500010004107800, 0x0024010008800a00, 0x0400110410804810, 0x8300038100004222,
    0x004a800182c00020, 0x0009100804021000, 0x3002200010c40021, 0x0020100104000208, 0x01021001a0080020, 0x0884020010082100, 0x1000820800c00060, 0x8020480110020020,
    0x0002052000100024, 0x0200190040088100, 0x0030802001a00800, 0x8010002004000202, 0x0040010100080010, 0x2200608200100080, 0x0001901c00420040, 0x0001400a24008010,
    0x1400a22008001042, 0x8200108041020020, 0x2004500023002400, 0x8105100028001048, 0x8010024d00014802, 0x8000820028030004, 0x402010c110014208, 0x8300038100004222,
    0x0001804002800124, 0x0084022014041400, 0x0030802001a00800, 0x0110a01001080008, 0x0b10080850081100, 0x000010040049020c, 0x0024010008800a00, 0x014c800040100426,
    0x1100400010208000, 0x0009100804021000, 0x0010024871202002, 0x8014001028c80801, 0x1201082010a00200, 0x0002008004102009, 0x8300038100004222, 0x0000401001a00408,
    0x4520920010210200, 0x0400110410804810, 0x8105100028001048, 0x8105100028001048, 0x0802801009083002, 0x8200108041020020, 0x8200108041020020, 0x4000a12400848110,
    0x2000804026001102, 0x2000804026001102, 0x800040a010040901, 0x80001802002c0422, 0x0010b018200c0122, 0x200204802a080401, 0x8880604201100844, 0x80000cc281092402
};

BitBoard computeAttacks(int index, BitBoard occ, int delta) {
   BitBoard attacks = emptyBitBoard, blocked = emptyBitBoard;
   for (int shift = index + delta; ISNEIGHBOUR(shift, shift - delta); shift += delta) {
      if (!blocked) attacks |= SquareToBitboard(shift);
      blocked |= ((1ULL << shift) & occ);
   }
   return attacks;
}

BitBoard occupiedFromIndex(int j, BitBoard mask) {
   BitBoard occ = emptyBitBoard;
   int      i   = 0;
   while (mask) {
      const int k = BB::popBit(mask);
      if (j & SquareToBitboard(i)) occ |= (1ULL << k);
      i++;
   }
   return occ;
}

void initMagic() {
   Logging::LogIt(Logging::logInfo) << "Init magic";
   for (Square from = 0; from < 64; from++) {
      bishopMagic[from].mask = emptyBitBoard;
      rookMagic[from].mask   = emptyBitBoard;
      for (Square j = 0; j < 64; j++) {
         if (from == j) continue;
         if (SQRANK(from) == SQRANK(j) && !ISOUTERFILE(j)) rookMagic[from].mask |= SquareToBitboard(j);
         if (SQFILE(from) == SQFILE(j) && !PROMOTION_RANK(j)) rookMagic[from].mask |= SquareToBitboard(j);
         if (abs(SQRANK(from) - SQRANK(j)) == abs(SQFILE(from) - SQFILE(j)) && !ISOUTERFILE(j) && !PROMOTION_RANK(j))
            bishopMagic[from].mask |= SquareToBitboard(j);
      }
      bishopMagic[from].magic = bishopMagics[from];
      for (int j = 0; j < (1 << BISHOP_INDEX_BITS); j++) {
         const BitBoard occ = occupiedFromIndex(j, bishopMagic[from].mask);
         bishopAttacks[from][MAGICBISHOPINDEX(occ, from)] =
             (computeAttacks(from, occ, -7) | computeAttacks(from, occ, 7) | computeAttacks(from, occ, -9) | computeAttacks(from, occ, 9));
      }
      rookMagic[from].magic = rookMagics[from];
      for (int j = 0; j < (1 << ROOK_INDEX_BITS); j++) {
         const BitBoard occ = occupiedFromIndex(j, rookMagic[from].mask);
         rookAttacks[from][MAGICROOKINDEX(occ, from)] =
             (computeAttacks(from, occ, -1) | computeAttacks(from, occ, 1) | computeAttacks(from, occ, -8) | computeAttacks(from, occ, 8));
      }
   }
}

} // namespace MagicBB

#endif // MAGIC

bool isAttackedBB(const Position &p, const Square s, Color c) { ///@todo try to optimize order better ?
   assert(isValidSquare(s));
   const BitBoard occupancy = p.occupancy();
   if (c == Co_White)
      return attack<P_wb>(s, p.blackBishop() | p.blackQueen(), occupancy) || attack<P_wr>(s, p.blackRook() | p.blackQueen(), occupancy) ||
             attack<P_wp>(s, p.blackPawn(), occupancy, Co_White) || attack<P_wn>(s, p.blackKnight()) || attack<P_wk>(s, p.blackKing());
   else
      return attack<P_wb>(s, p.whiteBishop() | p.whiteQueen(), occupancy) || attack<P_wr>(s, p.whiteRook() | p.whiteQueen(), occupancy) ||
             attack<P_wp>(s, p.whitePawn(), occupancy, Co_Black) || attack<P_wn>(s, p.whiteKnight()) || attack<P_wk>(s, p.whiteKing());
}

BitBoard allAttackedBB(const Position &p, const Square s, Color c) {
   assert(isValidSquare(s));
   const BitBoard occupancy = p.occupancy();
   if (c == Co_White)
      return attack<P_wb>(s, p.blackBishop() | p.blackQueen(), occupancy) | attack<P_wr>(s, p.blackRook() | p.blackQueen(), occupancy) |
             attack<P_wn>(s, p.blackKnight()) | attack<P_wp>(s, p.blackPawn(), occupancy, Co_White) | attack<P_wk>(s, p.blackKing());
   else
      return attack<P_wb>(s, p.whiteBishop() | p.whiteQueen(), occupancy) | attack<P_wr>(s, p.whiteRook() | p.whiteQueen(), occupancy) |
             attack<P_wn>(s, p.whiteKnight()) | attack<P_wp>(s, p.whitePawn(), occupancy, Co_Black) | attack<P_wk>(s, p.whiteKing());
}

BitBoard allAttackedBB(const Position &p, const Square s) {
   assert(isValidSquare(s));
   const BitBoard occupancy = p.occupancy();
   return attack<P_wb>(s, p.allBishop() | p.allQueen(), occupancy) | attack<P_wr>(s, p.allRook() | p.allQueen(), occupancy) |
          attack<P_wn>(s, p.allKnight()) | attack<P_wp>(s, p.blackPawn(), occupancy, Co_White) | attack<P_wp>(s, p.whitePawn(), occupancy, Co_Black) |
          attack<P_wk>(s, p.allKing());
}

} // namespace BBTools

bool isAttacked(const Position &p, const Square s) {
   //assert(isValidSquare(s)); ///@todo ?
   START_TIMER
   const bool b = s != INVALIDSQUARE && BBTools::isAttackedBB(p, s, p.c);
   STOP_AND_SUM_TIMER(IsAttacked);
   return b;
}

bool isAttacked(const Position &p, BitBoard bb) { // copy ///@todo should be done without iterate over Square !
   while (bb)
      if (isAttacked(p, Square(BB::popBit(bb)))) return true;
   return false;
}
