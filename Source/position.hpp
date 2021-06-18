#pragma once

#include "bitboard.hpp"
#include "definition.hpp"

#ifdef WITH_NNUE
#include "nnue.hpp"
#endif

struct Position; // forward decl

bool readFEN(const std::string& fen, Position& p, bool silent = false, bool withMoveount = false); // forward decl

/*!
 * The main position structure
 * Storing
 *  - board
 *  - all bitboards
 *  - material
 *  - hash (full position and just K+P)
 *  - some game status info
 *
 *  Contains also some usefull accessor
 *
 * Minic is a copy/make engine, so that this structure is copied a lot !
 */
struct Position {
   Position();
   Position(const std::string& fen, bool withMoveCount = true);
   ~Position();

   std::array<Piece, 64>   _b {{P_none}}; // works because P_none is in fact 0 ...
   std::array<BitBoard, 6> _allB {{emptyBitBoard}};
   std::array<BitBoard, 2> allPieces {{emptyBitBoard}};

   // t p n b r q k bl bd M n  (total is first so that pawn to king is same a Piece)
   typedef std::array<std::array<char, 11>, 2> Material;
   Material                                    mat = {{{{0}}}}; // such a nice syntax ...

   mutable Hash h = nullHash, ph = nullHash;
   MiniMove     lastMove = INVALIDMINIMOVE;
   uint16_t     moves = 0, halfmoves = 0;
   std::array<Square, 2>                king      = {INVALIDSQUARE, INVALIDSQUARE};
   std::array<std::array<Square, 2>, 2> rooksInit = {{{INVALIDSQUARE, INVALIDSQUARE}, {INVALIDSQUARE, INVALIDSQUARE}}};
   std::array<Square, 2>                kingInit  = {INVALIDSQUARE, INVALIDSQUARE};
   Square          ep        = INVALIDSQUARE;
   uint8_t         fifty     = 0;
   CastlingRights  castling  = C_none;
   Color           c         = Co_White;

   [[nodiscard]] inline const Piece& board_const(Square k) const { return _b[k]; }
   [[nodiscard]] inline Piece&       board(Square k) { return _b[k]; }

   [[nodiscard]] inline BitBoard occupancy() const { return allPieces[Co_White] | allPieces[Co_Black]; }

   [[nodiscard]] inline BitBoard allKing() const { return _allB[5]; }
   [[nodiscard]] inline BitBoard allQueen() const { return _allB[4]; }
   [[nodiscard]] inline BitBoard allRook() const { return _allB[3]; }
   [[nodiscard]] inline BitBoard allBishop() const { return _allB[2]; }
   [[nodiscard]] inline BitBoard allKnight() const { return _allB[1]; }
   [[nodiscard]] inline BitBoard allPawn() const { return _allB[0]; }

   [[nodiscard]] inline BitBoard blackKing() const { return _allB[5] & allPieces[Co_Black]; }
   [[nodiscard]] inline BitBoard blackQueen() const { return _allB[4] & allPieces[Co_Black]; }
   [[nodiscard]] inline BitBoard blackRook() const { return _allB[3] & allPieces[Co_Black]; }
   [[nodiscard]] inline BitBoard blackBishop() const { return _allB[2] & allPieces[Co_Black]; }
   [[nodiscard]] inline BitBoard blackKnight() const { return _allB[1] & allPieces[Co_Black]; }
   [[nodiscard]] inline BitBoard blackPawn() const { return _allB[0] & allPieces[Co_Black]; }

   [[nodiscard]] inline BitBoard whitePawn() const { return _allB[0] & allPieces[Co_White]; }
   [[nodiscard]] inline BitBoard whiteKnight() const { return _allB[1] & allPieces[Co_White]; }
   [[nodiscard]] inline BitBoard whiteBishop() const { return _allB[2] & allPieces[Co_White]; }
   [[nodiscard]] inline BitBoard whiteRook() const { return _allB[3] & allPieces[Co_White]; }
   [[nodiscard]] inline BitBoard whiteQueen() const { return _allB[4] & allPieces[Co_White]; }
   [[nodiscard]] inline BitBoard whiteKing() const { return _allB[5] & allPieces[Co_White]; }

   [[nodiscard]] inline BitBoard whiteLightBishop() const { return whiteBishop() & BB::whiteSquare; }
   [[nodiscard]] inline BitBoard whiteDarkBishop() const { return whiteBishop() & BB::blackSquare; }
   [[nodiscard]] inline BitBoard blackLightBishop() const { return blackBishop() & BB::whiteSquare; }
   [[nodiscard]] inline BitBoard blackDarkBishop() const { return blackBishop() & BB::blackSquare; }

   template<Piece pp> [[nodiscard]] inline BitBoard pieces_const(Color cc) const {
      assert(pp != P_none);
      return _allB[pp - 1] & allPieces[cc];
   }
   [[nodiscard]] inline BitBoard pieces_const(Color cc, Piece pp) const {
      assert(pp != P_none);
      return _allB[pp - 1] & allPieces[cc];
   }
   [[nodiscard]] inline BitBoard pieces_const(Piece pp) const {
      assert(pp != P_none);
      return _allB[std::abs(pp) - 1] & allPieces[pp > 0 ? Co_White : Co_Black];
   }
   // next one is kinda "private"
   [[nodiscard]] inline BitBoard& _pieces(Piece pp) {
      assert(pp != P_none);
      return _allB[std::abs(pp) - 1];
   }

#ifdef WITH_NNUE

   using NNUEEvaluator = nnue::NNUEEval<NNUEWrapper::nnueNType, NNUEWrapper::quantization>;

   mutable NNUEEvaluator* associatedEvaluator = nullptr;
   void                   associateEvaluator(NNUEEvaluator& evaluator) { associatedEvaluator = &evaluator; }

   [[nodiscard]] NNUEEvaluator& Evaluator() {
      assert(associatedEvaluator);
      return *associatedEvaluator;
   }
   [[nodiscard]] const NNUEEvaluator& Evaluator() const {
      assert(associatedEvaluator);
      return *associatedEvaluator;
   }

   // Vastly taken from Seer implementation.
   // see https://github.com/connormcmonigle/seer-nnue

   [[nodiscard]] static inline constexpr int NNUEIndiceUs(Square ksq, Square s, Piece p) {
      return FeatureIdx::major * HFlip(ksq) + HFlip(s) + FeatureIdx::usOffset(p);
   }

   [[nodiscard]] static inline constexpr int NNUEIndiceThem(Square ksq, Square s, Piece p) {
      return FeatureIdx::major * HFlip(ksq) + HFlip(s) + FeatureIdx::themOffset(p);
   }

   template<Color c> void resetNNUEIndices_(NNUEEvaluator& nnueEvaluator) const {
      using namespace FeatureIdx;
      //us
      BitBoard usPawn = pieces_const<P_wp>(c);
      while (usPawn) { nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], BB::popBit(usPawn), P_wp)); }
      BitBoard usKnight = pieces_const<P_wn>(c);
      while (usKnight) { nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], BB::popBit(usKnight), P_wn)); }
      BitBoard usBishop = pieces_const<P_wb>(c);
      while (usBishop) { nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], BB::popBit(usBishop), P_wb)); }
      BitBoard usRook = pieces_const<P_wr>(c);
      while (usRook) { nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], BB::popBit(usRook), P_wr)); }
      BitBoard usQueen = pieces_const<P_wq>(c);
      while (usQueen) { nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], BB::popBit(usQueen), P_wq)); }
      BitBoard usKing = pieces_const<P_wk>(c);
      while (usKing) { nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], BB::popBit(usKing), P_wk)); }
      //them
      BitBoard themPawn = pieces_const<P_wp>(~c);
      while (themPawn) { nnueEvaluator.template us<c>().insert(NNUEIndiceThem(king[c], BB::popBit(themPawn), P_wp)); }
      BitBoard themKnight = pieces_const<P_wn>(~c);
      while (themKnight) { nnueEvaluator.template us<c>().insert(NNUEIndiceThem(king[c], BB::popBit(themKnight), P_wn)); }
      BitBoard themBishop = pieces_const<P_wb>(~c);
      while (themBishop) { nnueEvaluator.template us<c>().insert(NNUEIndiceThem(king[c], BB::popBit(themBishop), P_wb)); }
      BitBoard themRook = pieces_const<P_wr>(~c);
      while (themRook) { nnueEvaluator.template us<c>().insert(NNUEIndiceThem(king[c], BB::popBit(themRook), P_wr)); }
      BitBoard themQueen = pieces_const<P_wq>(~c);
      while (themQueen) { nnueEvaluator.template us<c>().insert(NNUEIndiceThem(king[c], BB::popBit(themQueen), P_wq)); }
      BitBoard themKing = pieces_const<P_wk>(~c);
      while (themKing) { nnueEvaluator.template us<c>().insert(NNUEIndiceThem(king[c], BB::popBit(themKing), P_wk)); }
   }

   void resetNNUEEvaluator(NNUEEvaluator& nnueEvaluator) const {
      nnueEvaluator.clear();
      resetNNUEIndices_<Co_White>(nnueEvaluator);
      resetNNUEIndices_<Co_Black>(nnueEvaluator);
   }

   template<Color c> void updateNNUEEvaluator(NNUEEvaluator& nnueEvaluator, const Move& m) const {
      ///@todo to optimize, available in parent function !
      const Square from      = Move2From(m);
      const Square to        = Move2To(m);
      const MType  type      = Move2Type(m);
      const Piece  pTypeFrom = (Piece)std::abs(board_const(from));
      const Piece  pTypeTo   = (Piece)std::abs(board_const(to));
      nnueEvaluator.template us<c>().erase(NNUEIndiceUs(king[c], from, pTypeFrom));
      nnueEvaluator.template them<c>().erase(NNUEIndiceThem(king[~c], from, pTypeFrom));
      if (isPromotion(m)) {
         const Piece promPieceType = promShift(type);
         nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], to, promPieceType));
         nnueEvaluator.template them<c>().insert(NNUEIndiceThem(king[~c], to, promPieceType));
      }
      else {
         nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], to, pTypeFrom));
         nnueEvaluator.template them<c>().insert(NNUEIndiceThem(king[~c], to, pTypeFrom));
      }
      if (type == T_ep) {
         const Square epSq = ep + (c == Co_White ? -8 : +8);
         nnueEvaluator.template them<c>().erase(NNUEIndiceUs(king[~c], epSq, P_wp));
         nnueEvaluator.template us<c>().erase(NNUEIndiceThem(king[c], epSq, P_wp));
      }
      else if (isCapture(m)) {
         nnueEvaluator.template them<c>().erase(NNUEIndiceUs(king[~c], to, pTypeTo));
         nnueEvaluator.template us<c>().erase(NNUEIndiceThem(king[c], to, pTypeTo));
      }
   }

#endif
};
