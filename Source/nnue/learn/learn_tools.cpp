#include "learn_tools.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "definition.hpp"
#include "logging.hpp"
#include "material.hpp"
#include "moveGen.hpp"
#include "pieceTools.hpp"
#include "position.hpp"
#include "positionTools.hpp"

#if defined(WITH_DATA2BIN)

// Mostly copy/paste from nodchip Stockfish repository and adapted to Minic
// Tools for handling various learning data format

namespace { // anonymous

// Class that handles bitstream
// useful when doing aspect encoding
struct BitStream {
   // Set the memory to store the data in advance.
   // Assume that memory is cleared to 0.
   void set_data(uint8_t* data_) {
      data = data_;
      reset();
   }

   // Get the pointer passed in set_data().
   uint8_t* get_data() const { return data; }

   // Get the cursor.
   int get_cursor() const { return bit_cursor; }

   // reset the cursor
   void reset() { bit_cursor = 0; }

   // Write 1bit to the stream.
   // If b is non-zero, write out 1. If 0, write 0.
   void write_one_bit(int b) {
      if (b) {
         //std::cout << bit_cursor << std::endl;
         data[bit_cursor / 8] |= static_cast<uint8_t>(1 << (bit_cursor & 7));
      }
      ++bit_cursor;
      assert(bit_cursor <= 256);
   }

   // Get 1 bit from the stream.
   int read_one_bit() {
      int b = (data[bit_cursor / 8] >> (bit_cursor & 7)) & 1;
      ++bit_cursor;
      assert(bit_cursor <= 256);
      return b;
   }

   // write n bits of data
   // Data shall be written out from the lower order of d.
   void write_n_bit(int d, int n) {
      for (int i = 0; i < n; ++i) write_one_bit(d & (1 << i));
   }

   // read n bits of data
   // Reverse conversion of write_n_bit().
   int read_n_bit(int n) {
      int result = 0;
      for (int i = 0; i < n; ++i) result |= read_one_bit() ? (1 << i) : 0;

      return result;
   }

  private:
   // Next bit position to read/write.
   int bit_cursor = 0;

   // data entity
   mutable uint8_t* data = nullptr;
};

struct HuffmanedPiece {
   int code; // how it will be coded
   int bits; // How many bits do you have
};

HuffmanedPiece huffman_table[] = {
    {0b0000, 1}, // NO_PIECE
    {0b0001, 4}, // PAWN
    {0b0011, 4}, // KNIGHT
    {0b0101, 4}, // BISHOP
    {0b0111, 4}, // ROOK
    {0b1001, 4}, // QUEEN
};

// Class for compressing/decompressing sfen
struct SfenPacker {
   // Pack sfen and store in data[32].
   void pack(const Position& pos) {
      //std::cout << GetFEN(pos) << std::endl;

      memset(data, 0, 32 /* 256bit */);
      stream.set_data(data);

      // Side to move.
      stream.write_one_bit(static_cast<int>(pos.c));

      // 7-bit positions for leading and trailing balls
      // White king and black king, 6 bits for each.
      for (auto c : {Co_White, Co_Black}) stream.write_n_bit(pos.king[c], 6);

      // Write the pieces on the board other than the kings.
      for (Rank r = Rank_8;; --r) {
         for (File f = File_a; f <= File_h; ++f) {
            Piece pc = pos.board_const(MakeSquare(f, r));
            //std::cout << static_cast<int>(pc) << std::endl;
            if (std::abs(pc) == P_wk) continue;
            write_board_piece_to_stream(pc);
         }
         if (r == Rank_1) break; // because rank is unsigned !!
      }

      ///@todo: Support chess960.
      stream.write_one_bit(pos.castling & C_wks);
      stream.write_one_bit(pos.castling & C_wqs);
      stream.write_one_bit(pos.castling & C_bks);
      stream.write_one_bit(pos.castling & C_bqs);

      if (pos.ep == INVALIDSQUARE) { stream.write_one_bit(0); }
      else {
         stream.write_one_bit(1);
         stream.write_n_bit(static_cast<int>(pos.ep), 6);
      }

      stream.write_n_bit(pos.fifty, 6);
      stream.write_n_bit(pos.moves, 8);
      assert(stream.get_cursor() <= 256);
   }

   // sfen packed by pack() (256bit = 32bytes)
   // Or sfen to decode with unpack()
   uint8_t* data = nullptr; // uint8_t[32];

   BitStream stream;

   // Output the board pieces to stream.
   void write_board_piece_to_stream(Piece pc) {
      // piece type
      auto c = huffman_table[std::abs(pc)];
      stream.write_n_bit(c.code, c.bits);
      if (pc == P_none) return;
      stream.write_one_bit(pc > 0 ? Co_White : Co_Black);
   }

   // Read one board piece from stream
   Piece read_board_piece_from_stream() {
      Piece pr   = P_none;
      int code = 0;
      int bits = 0;
      while (true) {
         code |= stream.read_one_bit() << bits;
         ++bits;
         assert(bits <= 6);
         for (pr = P_none; pr < P_wk; ++pr) {
            if (huffman_table[pr].code == code && huffman_table[pr].bits == bits) goto Found;
         }
      }
   Found:;
      assert(pr != P_wk);
      if (pr == P_none) return P_none;

      const Color c = static_cast<Color>(stream.read_one_bit());
      return c == Co_White ? pr : ~pr;
   }

}; // SfenPacker

} // namespace

MiniMove ToSFMove(const Position& p, const Square from, const Square to, const MType type) {
   assert(from >= 0 && from < 64);
   assert(to >= 0 && to < 64);
   if (isPromotion(type)) return FromSF::MakeMove<FromSF::PROMOTION>(from, to, promShift(type));
   else if (type == T_ep)
      return FromSF::MakeMove<FromSF::ENPASSANT>(from, p.ep);
   else if (isCastling(type)) {
      switch (type) {
         case T_wks: return FromSF::MakeMove<FromSF::CASTLING>(from, p.rootInfo().rooksInit[Co_White][CT_OO]);
         case T_wqs: return FromSF::MakeMove<FromSF::CASTLING>(from, p.rootInfo().rooksInit[Co_White][CT_OOO]);
         case T_bks: return FromSF::MakeMove<FromSF::CASTLING>(from, p.rootInfo().rooksInit[Co_Black][CT_OO]);
         case T_bqs: return FromSF::MakeMove<FromSF::CASTLING>(from, p.rootInfo().rooksInit[Co_Black][CT_OOO]);
         default: return INVALIDMINIMOVE;
      }
   }
   else
      return FromSF::MakeMoveStd(from, to);
}

MiniMove FromSFMove(const Position& p, const MiniMove sfmove) {
   const Square           from   = FromSF::from_sq(sfmove);
   const Square           to     = FromSF::to_sq(sfmove);
   const FromSF::MoveType SFtype = FromSF::type_of(sfmove);
   MType                  type   = T_std;
   switch (SFtype) {
      case FromSF::NORMAL:
         if (p.board_const(to) != P_none) type = T_capture;
         break;
      case FromSF::PROMOTION:
         {
            const Piece pp = FromSF::promotion_type(sfmove);
            if (pp == P_wq) { type = p.board_const(to) == P_none ? T_promq : T_cappromq; }
            else if (pp == P_wr) {
               type = p.board_const(to) == P_none ? T_promr : T_cappromr;
            }
            else if (pp == P_wb) {
               type = p.board_const(to) == P_none ? T_promb : T_cappromb;
            }
            else if (pp == P_wn) {
               type = p.board_const(to) == P_none ? T_promn : T_cappromn;
            }
         }
         break;
      case FromSF::ENPASSANT: type = T_ep; break;
      case FromSF::CASTLING:
         if (to == p.rootInfo().rooksInit[Co_White][CT_OO]) type = T_wks;
         if (to == p.rootInfo().rooksInit[Co_White][CT_OOO]) type = T_wqs;
         if (to == p.rootInfo().rooksInit[Co_Black][CT_OO]) type = T_bks;
         if (to == p.rootInfo().rooksInit[Co_Black][CT_OOO]) type = T_bqs;
         break;
      default: assert(false);
   }
   return ToMove(from, to, type);
}

void sfen_pack(const Position& p, PackedSfen& sfen) {
   SfenPacker sp;
   sp.data = reinterpret_cast<uint8_t*>(&sfen);
   sp.pack(p);
}

// If there is a problem with the passed phase and there is an error, non-zero is returned.
int set_from_packed_sfen(Position& p, PackedSfen& sfen) {
   SfenPacker packer;
   auto&      stream = packer.stream;
   stream.set_data(reinterpret_cast<uint8_t*>(&sfen));

   // Active color
   p.c = static_cast<Color>(stream.read_one_bit());

   for (auto c : {Co_White, Co_Black}) {
      const Square sq = static_cast<Square>(stream.read_n_bit(6));
      p.board(sq)     = (c == Co_White ? P_wk : P_bk);
      p.king[c]       = sq;
   }

   p.rootInfo().kingInit[Co_White] = p.king[Co_White];
   p.rootInfo().kingInit[Co_Black] = p.king[Co_Black];

   // Piece placement
   for (Rank r = Rank_8;; --r) {
      for (File f = File_a; f <= File_h; ++f) {
         auto  sq = MakeSquare(f, r);
         Piece pc = P_none;

         // skip already given kings
         if (PieceTools::getPieceType(p, sq) != P_wk) {
            assert(p.board_const(sq) == P_none);
            pc = packer.read_board_piece_from_stream();
         }

         // There may be no pieces, so skip in that case (also cover king case).
         if (pc == P_none) continue;

         p.board(sq) = pc;

         if (stream.get_cursor() > 256) return 1;
      }
      if (r == Rank_1) break; // because rank is unsigned !!
   }

   // Castling availability.
   p.castling = C_none;
   if (stream.read_one_bit()) {
      Square rsq;
      for (rsq = relative_square(Co_White, Sq_h1); p.board_const(rsq) != P_wr; --rsq) {}
      p.rootInfo().rooksInit[Co_White][CT_OO] = rsq;
      p.castling |= C_wks;
   }
   if (stream.read_one_bit()) {
      Square rsq;
      for (rsq = relative_square(Co_White, Sq_a1); p.board_const(rsq) != P_wr; ++rsq) {}
      p.rootInfo().rooksInit[Co_White][CT_OOO] = rsq;
      p.castling |= C_wqs;
   }
   if (stream.read_one_bit()) {
      Square rsq;
      for (rsq = relative_square(Co_Black, Sq_h1); p.board_const(rsq) != P_br; --rsq) {}
      p.rootInfo().rooksInit[Co_Black][CT_OO] = rsq;
      p.castling |= C_bks;
   }
   if (stream.read_one_bit()) {
      Square rsq;
      for (rsq = relative_square(Co_Black, Sq_a1); p.board_const(rsq) != P_br; ++rsq) {}
      p.rootInfo().rooksInit[Co_Black][CT_OOO] = rsq;
      p.castling |= C_bqs;
   }

   // En passant square.
   if (stream.read_one_bit()) {
      Square ep_square = static_cast<Square>(stream.read_n_bit(6));
      p.ep             = ep_square;
      ///@todo verify en passant square
   }
   else
      p.ep = INVALIDSQUARE;

   // Halfmove clock
   p.fifty = static_cast<decltype(p.fifty)>(stream.read_n_bit(6));

   // Fullmove number
   p.moves = static_cast<decltype(p.moves)>(stream.read_n_bit(8));
   if (p.moves < 1) { // fix a LittleBlitzer bug here ...
      Logging::LogIt(Logging::logDebug) << "Wrong move counter " << static_cast<int>(p.moves) << " using 1 instead";
      p.moves = 1;
   }
   assert(stream.get_cursor() <= 256);

   p.halfmoves = static_cast<decltype(p.halfmoves)>((p.moves - 1) * 2 + 1 + (p.c == Co_Black ? 1 : 0));

   p.rootInfo().initCaslingPermHashTable();

   BBTools::setBitBoards(p);
   MaterialHash::initMaterial(p);
   p.h  = computeHash(p);
   p.ph = computePHash(p);

   return 0;
}

#endif // WITH_DATA2BIN