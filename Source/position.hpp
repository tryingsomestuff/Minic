#pragma once

#include "bitboard.hpp"
#include "definition.hpp"
#include "timers.hpp"

#ifdef WITH_NNUE
#include "nnue.hpp"
#endif

struct Position; // forward decl
struct RootPosition; // forward decl

/*!
 * Main Position initialisation function
 * - silent will forbid logging (for info, not for error !)
 * - withMoveCount shall be set in order to read last part of the FEN string
 */
bool readFEN(const std::string& fen, RootPosition& p, bool silent = false, bool withMoveCount = false); // forward decl

/*!
 * Informations associated with an initially given Position (from readFEN)
 * There will not change inside the search tree
 */
struct RootInformation {
   std::array<CastlingRights, NbSquare> castlePermHashTable = {CastlingRights::C_none}; // C_none is 0 so ok
   std::array<std::array<Square, 2>, 2> rooksInit = {{{INVALIDSQUARE, INVALIDSQUARE}, {INVALIDSQUARE, INVALIDSQUARE}}};
   std::array<Square, 2>                kingInit  = {INVALIDSQUARE, INVALIDSQUARE};
   void initCaslingPermHashTable();
};

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
   virtual ~Position();

   std::array<Piece, NbSquare> _b {{P_none}}; // works because P_none is in fact 0
   std::array<BitBoard, 6>     _allB {{emptyBitBoard}}; // works because emptyBitBoard is in fact 0
   std::array<BitBoard, 2>     allPieces {{emptyBitBoard}}; // works because emptyBitBoard is in fact 0
   
   // t p n b r q k bl bd M n  (total is first so that pawn to king is same a Piece)
   typedef std::array<std::array<char, 11>, 2> Material;
   Material mat = {{{{0}}}}; // such a nice syntax ...

   mutable Hash h = nullHash, ph = nullHash;
   MiniMove lastMove = INVALIDMINIMOVE;
   uint16_t moves = 0, halfmoves = 0;
   std::array<Square, 2> king = {INVALIDSQUARE, INVALIDSQUARE};

   // shared by all "child" of a same "root" position
   mutable RootInformation* root = nullptr; 

   Square         ep       = INVALIDSQUARE;
   uint8_t        fifty    = 0;
   CastlingRights castling = C_none;
   Color          c        = Co_White;

   inline void clear(){
      _b = {{P_none}};
      _allB = {{emptyBitBoard}};
      allPieces = {{emptyBitBoard}};
      mat = {{{{0}}}};
      h = nullHash; 
      ph = nullHash;
      lastMove = INVALIDMINIMOVE;
      moves = 0;
      halfmoves = 0;
      king = {INVALIDSQUARE, INVALIDSQUARE};
      //root = nullptr; 
      ep = INVALIDSQUARE;
      fifty = 0;
      castling = C_none;
      c = Co_White;
   }

   [[nodiscard]] inline RootInformation& rootInfo() {
      assert(root);
      return *root;
   }

   [[nodiscard]] inline const RootInformation& rootInfo() const {
      assert(root);
      return *root;
   }

   [[nodiscard]] inline const Piece& board_const(const Square k) const { return _b[k]; }
   [[nodiscard]] inline Piece&       board      (const Square k)       { return _b[k]; }

   [[nodiscard]] inline BitBoard occupancy() const { return allPieces[Co_White] | allPieces[Co_Black]; }

   [[nodiscard]] inline BitBoard allKing()   const { return _allB[5]; }
   [[nodiscard]] inline BitBoard allQueen()  const { return _allB[4]; }
   [[nodiscard]] inline BitBoard allRook()   const { return _allB[3]; }
   [[nodiscard]] inline BitBoard allBishop() const { return _allB[2]; }
   [[nodiscard]] inline BitBoard allKnight() const { return _allB[1]; }
   [[nodiscard]] inline BitBoard allPawn()   const { return _allB[0]; }

   [[nodiscard]] inline BitBoard blackKing()   const { return _allB[5] & allPieces[Co_Black]; }
   [[nodiscard]] inline BitBoard blackQueen()  const { return _allB[4] & allPieces[Co_Black]; }
   [[nodiscard]] inline BitBoard blackRook()   const { return _allB[3] & allPieces[Co_Black]; }
   [[nodiscard]] inline BitBoard blackBishop() const { return _allB[2] & allPieces[Co_Black]; }
   [[nodiscard]] inline BitBoard blackKnight() const { return _allB[1] & allPieces[Co_Black]; }
   [[nodiscard]] inline BitBoard blackPawn()   const { return _allB[0] & allPieces[Co_Black]; }

   [[nodiscard]] inline BitBoard whitePawn()   const { return _allB[0] & allPieces[Co_White]; }
   [[nodiscard]] inline BitBoard whiteKnight() const { return _allB[1] & allPieces[Co_White]; }
   [[nodiscard]] inline BitBoard whiteBishop() const { return _allB[2] & allPieces[Co_White]; }
   [[nodiscard]] inline BitBoard whiteRook()   const { return _allB[3] & allPieces[Co_White]; }
   [[nodiscard]] inline BitBoard whiteQueen()  const { return _allB[4] & allPieces[Co_White]; }
   [[nodiscard]] inline BitBoard whiteKing()   const { return _allB[5] & allPieces[Co_White]; }

   [[nodiscard]] inline BitBoard whiteLightBishop() const { return whiteBishop() & BB::whiteSquare; }
   [[nodiscard]] inline BitBoard whiteDarkBishop()  const { return whiteBishop() & BB::blackSquare; }
   [[nodiscard]] inline BitBoard blackLightBishop() const { return blackBishop() & BB::whiteSquare; }
   [[nodiscard]] inline BitBoard blackDarkBishop()  const { return blackBishop() & BB::blackSquare; }

   template<Piece pp> [[nodiscard]] inline BitBoard pieces_const(const Color cc) const {
      assert(pp != P_none);
      return _allB[pp - 1] & allPieces[cc];
   }
   [[nodiscard]] inline BitBoard pieces_const(const Color cc, const Piece pp) const {
      assert(pp != P_none);
      return _allB[pp - 1] & allPieces[cc];
   }
   [[nodiscard]] inline BitBoard pieces_const(const Piece pp) const {
      assert(pp != P_none);
      return _allB[std::abs(pp) - 1] & allPieces[pp > 0 ? Co_White : Co_Black];
   }
   // next one is kinda "private"
   [[nodiscard]] inline BitBoard& _pieces(const Piece pp) {
      assert(pp != P_none);
      return _allB[std::abs(pp) - 1];
   }

   struct MoveInfo {
      MoveInfo(const Position& p, const Move m):
          from(Move2From(m)),
          to(correctedMove2ToKingDest(m)),
          type(Move2Type(m)),
          fromP(p.board_const(from)),
          toP(p.board_const(to)),
          fromId(PieceIdx(fromP)),
          isCapNoEP(toP != P_none) {
         assert(isValidSquare(from));
         assert(isValidSquare(to));
         assert(isValidMoveType(type));
      }
      const Square from      = INVALIDSQUARE;
      const Square to        = INVALIDSQUARE;
      const MType  type      = T_std;
      const Piece  fromP     = P_none;
      const Piece  toP       = P_none;
      const int    fromId    = 0;
      const bool   isCapNoEP = false;
   };

#ifdef WITH_NNUE

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

   [[nodiscard]] static inline constexpr size_t NNUEIndiceUs(const Square ksq, const Square s, const Piece p) {
      return FeatureIdx::major * HFlip(ksq) + HFlip(s) + FeatureIdx::usOffset(p);
   }

   [[nodiscard]] static inline constexpr size_t NNUEIndiceThem(const Square ksq, const Square s, const Piece p) {
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
      START_TIMER
      nnueEvaluator.clear();
      resetNNUEIndices_<Co_White>(nnueEvaluator);
      resetNNUEIndices_<Co_Black>(nnueEvaluator);
      STOP_AND_SUM_TIMER(ResetNNUE)
   }

   template<Color c> void updateNNUEEvaluator(NNUEEvaluator& nnueEvaluator, const MoveInfo& moveInfo) const {
      START_TIMER
      const Piece fromType = (Piece)std::abs(moveInfo.fromP);
      const Piece toType = (Piece)std::abs(moveInfo.toP);
      nnueEvaluator.template us<c>().erase(NNUEIndiceUs(king[c], moveInfo.from, fromType));
      nnueEvaluator.template them<c>().erase(NNUEIndiceThem(king[~c], moveInfo.from, fromType));
      if (isPromotion(moveInfo.type)) {
         const Piece promPieceType = promShift(moveInfo.type);
         nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], moveInfo.to, promPieceType));
         nnueEvaluator.template them<c>().insert(NNUEIndiceThem(king[~c], moveInfo.to, promPieceType));
      }
      else {
         nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], moveInfo.to, fromType));
         nnueEvaluator.template them<c>().insert(NNUEIndiceThem(king[~c], moveInfo.to, fromType));
      }
      if (moveInfo.type == T_ep) {
         const Square epSq = ep + (c == Co_White ? -8 : +8);
         nnueEvaluator.template them<c>().erase(NNUEIndiceUs(king[~c], epSq, P_wp));
         nnueEvaluator.template us<c>().erase(NNUEIndiceThem(king[c], epSq, P_wp));
      }
      else if (toType != P_none) {
         nnueEvaluator.template them<c>().erase(NNUEIndiceUs(king[~c], moveInfo.to, toType));
         nnueEvaluator.template us<c>().erase(NNUEIndiceThem(king[c], moveInfo.to, toType));
      }
      STOP_AND_SUM_TIMER(UpdateNNUE)
   }

#endif
};

/*!
 * RootPosition only specific responsability is to 
 * allocate and delete root pointer
 */
struct RootPosition : public Position {
   RootPosition(){ root = new RootInformation; }
   RootPosition(const std::string& fen, bool withMoveCount = true);
   ~RootPosition() {
      delete root;
   }
   RootPosition(const RootPosition& p): Position(p) {
      if (p.root) root = new RootInformation(*p.root);
      else root = new RootInformation;
   };
   RootPosition & operator=(const RootPosition &) = delete;
};