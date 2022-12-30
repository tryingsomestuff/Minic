#pragma once

#include "bitboard.hpp"
#include "definition.hpp"
#include "timers.hpp"

#ifdef WITH_NNUE
#include "nnue.hpp"

[[nodiscard]] static constexpr size_t NNUEIndiceUs(const Square ksq, const Square s, const Piece p) {
   return FeatureIdx::major * HFlip(ksq) + HFlip(s) + FeatureIdx::usOffset(p);
}

[[nodiscard]] static constexpr size_t NNUEIndiceThem(const Square ksq, const Square s, const Piece p) {
   return FeatureIdx::major * HFlip(ksq) + HFlip(s) + FeatureIdx::themOffset(p);
}
#endif // WITH_NNUE

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
   array1d<CastlingRights, NbSquare> castlePermHashTable = {CastlingRights::C_none}; // C_none is 0 so ok
   array2d<Square, 2, 2> rooksInit = {{{INVALIDSQUARE, INVALIDSQUARE}, {INVALIDSQUARE, INVALIDSQUARE}}};
   colored<Square> kingInit  = {INVALIDSQUARE, INVALIDSQUARE};
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
struct alignas(32) Position {
   Position();
   virtual ~Position();

   array1d<Piece, NbSquare> _b {{P_none}}; // works because P_none is in fact 0
   array1d<BitBoard, 6>     _allB {{emptyBitBoard}}; // works because emptyBitBoard is in fact 0
   array1d<BitBoard, 2>     allPieces {{emptyBitBoard}}; // works because emptyBitBoard is in fact 0

   // t p n b r q k bl bd M n x x x x x (total is first so that pawn to king is same index as Piece type)
   using Material = colored<array1d<char, 16>>;
   Material mat = {{{{0}}}}; // such a nice syntax ...

   // shared by all "child" of a same "root" position
   // Assumed rule of 3 disrespect
   // the default copy CTOR and copy operator will copy that
   // but only a RootPosition will delete it
   // (this is way faster than a shared_ptr)
   mutable RootInformation* root = nullptr;

#ifdef WITH_NNUE
   mutable NNUEEvaluator* associatedEvaluator = nullptr;
#endif
   
   mutable Hash    h         = nullHash;
   mutable Hash    ph        = nullHash;
   MiniMove        lastMove  = INVALIDMINIMOVE;
   uint16_t        moves     = 0;
   uint16_t        halfmoves = 0;
   colored<Square> king      = {INVALIDSQUARE, INVALIDSQUARE};
   Square          ep        = INVALIDSQUARE;
   uint8_t         fifty     = 0;
   CastlingRights  castling  = C_none;
   Color           c         = Co_White;

   inline void clear(){
      _b        = {{P_none}};
      _allB     = {{emptyBitBoard}};
      allPieces = {{emptyBitBoard}};
      mat       = {{{{0}}}};
      h         = nullHash;
      ph        = nullHash;
      lastMove  = INVALIDMINIMOVE;
      moves     = 0;
      halfmoves = 0;
      king      = {INVALIDSQUARE, INVALIDSQUARE};
      //root     = nullptr;
      ep        = INVALIDSQUARE;
      fifty     = 0;
      castling  = C_none;
      c         = Co_White;
   }

   //Position & operator=(const Position & p2);
   //Position(const Position& p);

#ifdef DEBUG_FIFTY_COLLISION
   [[nodiscard]] bool operator== (const Position &p)const{
      return _allB == p._allB && ep == p.ep && castling == p.castling && c == p.c;
   }
   [[nodiscard]] bool operator!= (const Position &p)const{
      return _allB != p._allB || ep != p.ep || castling != p.castling || c != p.c;
   }
#endif

   [[nodiscard]] FORCE_FINLINE RootInformation& rootInfo() {
      assert(root);
      return *root;
   }

   [[nodiscard]] FORCE_FINLINE const RootInformation& rootInfo() const {
      assert(root);
      return *root;
   }

   [[nodiscard]] FORCE_FINLINE const Piece& board_const(const Square k) const { return _b[k]; }
   [[nodiscard]] FORCE_FINLINE Piece&       board      (const Square k)       { return _b[k]; }

   [[nodiscard]] FORCE_FINLINE BitBoard occupancy() const { return allPieces[Co_White] | allPieces[Co_Black]; }

   [[nodiscard]] FORCE_FINLINE BitBoard allKing()   const { return _allB[5]; }
   [[nodiscard]] FORCE_FINLINE BitBoard allQueen()  const { return _allB[4]; }
   [[nodiscard]] FORCE_FINLINE BitBoard allRook()   const { return _allB[3]; }
   [[nodiscard]] FORCE_FINLINE BitBoard allBishop() const { return _allB[2]; }
   [[nodiscard]] FORCE_FINLINE BitBoard allKnight() const { return _allB[1]; }
   [[nodiscard]] FORCE_FINLINE BitBoard allPawn()   const { return _allB[0]; }

   [[nodiscard]] FORCE_FINLINE BitBoard blackKing()   const { return _allB[5] & allPieces[Co_Black]; }
   [[nodiscard]] FORCE_FINLINE BitBoard blackQueen()  const { return _allB[4] & allPieces[Co_Black]; }
   [[nodiscard]] FORCE_FINLINE BitBoard blackRook()   const { return _allB[3] & allPieces[Co_Black]; }
   [[nodiscard]] FORCE_FINLINE BitBoard blackBishop() const { return _allB[2] & allPieces[Co_Black]; }
   [[nodiscard]] FORCE_FINLINE BitBoard blackKnight() const { return _allB[1] & allPieces[Co_Black]; }
   [[nodiscard]] FORCE_FINLINE BitBoard blackPawn()   const { return _allB[0] & allPieces[Co_Black]; }

   [[nodiscard]] FORCE_FINLINE BitBoard whitePawn()   const { return _allB[0] & allPieces[Co_White]; }
   [[nodiscard]] FORCE_FINLINE BitBoard whiteKnight() const { return _allB[1] & allPieces[Co_White]; }
   [[nodiscard]] FORCE_FINLINE BitBoard whiteBishop() const { return _allB[2] & allPieces[Co_White]; }
   [[nodiscard]] FORCE_FINLINE BitBoard whiteRook()   const { return _allB[3] & allPieces[Co_White]; }
   [[nodiscard]] FORCE_FINLINE BitBoard whiteQueen()  const { return _allB[4] & allPieces[Co_White]; }
   [[nodiscard]] FORCE_FINLINE BitBoard whiteKing()   const { return _allB[5] & allPieces[Co_White]; }

   [[nodiscard]] FORCE_FINLINE BitBoard whiteLightBishop() const { return whiteBishop() & BB::whiteSquare; }
   [[nodiscard]] FORCE_FINLINE BitBoard whiteDarkBishop()  const { return whiteBishop() & BB::blackSquare; }
   [[nodiscard]] FORCE_FINLINE BitBoard blackLightBishop() const { return blackBishop() & BB::whiteSquare; }
   [[nodiscard]] FORCE_FINLINE BitBoard blackDarkBishop()  const { return blackBishop() & BB::blackSquare; }

   template<Piece pp> [[nodiscard]] FORCE_FINLINE BitBoard pieces_const(const Color cc) const {
      assert(pp > P_none);
      assert(pp < P_end);
      assert(cc == Co_White || cc == Co_Black);
      return _allB[pp - 1] & allPieces[cc];
   }
   [[nodiscard]] FORCE_FINLINE BitBoard pieces_const(const Color cc, const Piece pp) const {
      assert(pp > P_none);
      assert(pp < P_end);
      assert(cc == Co_White || cc == Co_Black);
      return _allB[pp - 1] & allPieces[cc];
   }
   [[nodiscard]] FORCE_FINLINE BitBoard pieces_const(const Piece pp) const {
      assert(pp != P_none);
      assert(pp < P_end);
      assert(pp >= P_bk);
      return _allB[Abs(pp) - 1] & allPieces[pp > 0 ? Co_White : Co_Black];
   }
   // next one is kinda "private"
   [[nodiscard]] FORCE_FINLINE BitBoard& _pieces(const Piece pp) {
      assert(pp != P_none);
      assert(pp < P_end);
      assert(pp >= P_bk);
      return _allB[Abs(pp) - 1];
   }

   struct MoveInfo {
      MoveInfo(const Position& p, const Move _m):
          m(_m),
          from(Move2From(_m)),
          to(correctedMove2ToKingDest(_m)),
          king{p.king[Co_White], p.king[Co_Black]},
          ep(p.ep),
          type(Move2Type(_m)),
          fromP(p.board_const(from)),
          toP(p.board_const(to)), // won't be used for EP
          fromId(PieceIdx(fromP)),
          isCapNoEP(toP != P_none) {
         assert(isValidSquare(from));
         assert(isValidSquare(to));
         assert(isValidMoveType(type));
      }
      const Move   m         = INVALIDMOVE;
      const Square from      = INVALIDSQUARE;
      const Square to        = INVALIDSQUARE;
      const colored<Square> king = {INVALIDSQUARE, INVALIDSQUARE};
      const Square ep        = INVALIDSQUARE;
      const MType  type      = T_std;
      const Piece  fromP     = P_none;
      const Piece  toP       = P_none;
      const int    fromId    = 0;
      const bool   isCapNoEP = false;
   };

#ifdef WITH_NNUE
   void associateEvaluator(NNUEEvaluator& evaluator, bool dirty = true) { 
      associatedEvaluator = &evaluator; 
      evaluator.dirty = dirty;
   }

   void dissociateEvaluator() { 
      associatedEvaluator = nullptr; 
   }

   [[nodiscard]] NNUEEvaluator& evaluator() {
      assert(associatedEvaluator);
      return *associatedEvaluator;
   }
   [[nodiscard]] const NNUEEvaluator& evaluator() const {
      assert(associatedEvaluator);
      return *associatedEvaluator;
   }
   [[nodiscard]] bool hasEvaluator() const {
      return associatedEvaluator != nullptr;
   }

   // Vastly taken from Seer implementation.
   // see https://github.com/connormcmonigle/seer-nnue

   template<Color c> void resetNNUEIndices_(NNUEEvaluator& nnueEvaluator) const {
      using namespace FeatureIdx;
      //us
      BB::applyOn(pieces_const<P_wp>(c), [&](const Square & k) {
         nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], k, P_wp)); 
      });
      BB::applyOn(pieces_const<P_wn>(c), [&](const Square & k) {
         nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], k, P_wn)); 
      });
      BB::applyOn(pieces_const<P_wb>(c), [&](const Square & k) {
         nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], k, P_wb)); 
      });
      BB::applyOn(pieces_const<P_wr>(c), [&](const Square & k) {
         nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], k, P_wr));
      });
      BB::applyOn(pieces_const<P_wq>(c), [&](const Square & k) {
         nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], k, P_wq)); 
      });
      BB::applyOn(pieces_const<P_wk>(c), [&](const Square & k) {
         nnueEvaluator.template us<c>().insert(NNUEIndiceUs(king[c], k, P_wk));
      });
      //them
      BB::applyOn(pieces_const<P_wp>(~c), [&](const Square & k) {
         nnueEvaluator.template us<c>().insert(NNUEIndiceThem(king[c], k, P_wp)); 
      });
      BB::applyOn(pieces_const<P_wn>(~c), [&](const Square & k) {
         nnueEvaluator.template us<c>().insert(NNUEIndiceThem(king[c], k, P_wn)); 
      });
      BB::applyOn(pieces_const<P_wb>(~c), [&](const Square & k) {
         nnueEvaluator.template us<c>().insert(NNUEIndiceThem(king[c], k, P_wb)); 
      });
      BB::applyOn(pieces_const<P_wr>(~c), [&](const Square & k) {
         nnueEvaluator.template us<c>().insert(NNUEIndiceThem(king[c], k, P_wr));
      });
      BB::applyOn(pieces_const<P_wq>(~c), [&](const Square & k) {
         nnueEvaluator.template us<c>().insert(NNUEIndiceThem(king[c], k, P_wq)); 
      });
      BB::applyOn(pieces_const<P_wk>(~c), [&](const Square & k) {
         nnueEvaluator.template us<c>().insert(NNUEIndiceThem(king[c], k, P_wk));
      });
   }

   void resetNNUEEvaluator(NNUEEvaluator& nnueEvaluator) const {
      START_TIMER
      nnueEvaluator.clear();
      resetNNUEIndices_<Co_White>(nnueEvaluator);
      resetNNUEIndices_<Co_Black>(nnueEvaluator);
      nnueEvaluator.dirty = false;
      STOP_AND_SUM_TIMER(ResetNNUE)
   }

   void resetNNUEEvaluator(NNUEEvaluator& nnueEvaluator, Color color) const {
      START_TIMER
      nnueEvaluator.clear(color);
      if (color == Co_White)
         resetNNUEIndices_<Co_White>(nnueEvaluator);
      else
         resetNNUEIndices_<Co_Black>(nnueEvaluator);
      nnueEvaluator.dirty = false;
      STOP_AND_SUM_TIMER(ResetNNUE)
   }

#endif
};

/*!
 * RootPosition only specific responsability is to
 * allocate and delete root pointer
 */
struct RootPosition : public Position {
   RootPosition(){
      // a shared pointer here will be too slow
      root = new RootInformation;
   }

   RootPosition(const std::string& fen, bool withMoveCount = true);

   RootPosition(const RootPosition& p): Position(p) {
      assert(p.root);
      // we take a copy of the RootInformation of the copied RootPosition
      root = new RootInformation(*p.root);
   };

   // Root position cannot be copied
   RootPosition & operator=(const RootPosition &) = delete;

   ~RootPosition() {
      delete root;
   }

};

#ifdef WITH_NNUE
template<Color c> void updateNNUEEvaluator(NNUEEvaluator& nnueEvaluator, const Position::MoveInfo& moveInfo) {
   START_TIMER
   const Piece fromType = Abs(moveInfo.fromP);
   const Piece toType = Abs(moveInfo.toP);
   nnueEvaluator.template us<c>().erase(NNUEIndiceUs(moveInfo.king[c], moveInfo.from, fromType));
   nnueEvaluator.template them<c>().erase(NNUEIndiceThem(moveInfo.king[~c], moveInfo.from, fromType));
   if (isPromotion(moveInfo.type)) {
      const Piece promPieceType = promShift(moveInfo.type);
      nnueEvaluator.template us<c>().insert(NNUEIndiceUs(moveInfo.king[c], moveInfo.to, promPieceType));
      nnueEvaluator.template them<c>().insert(NNUEIndiceThem(moveInfo.king[~c], moveInfo.to, promPieceType));
   }
   else {
      nnueEvaluator.template us<c>().insert(NNUEIndiceUs(moveInfo.king[c], moveInfo.to, fromType));
      nnueEvaluator.template them<c>().insert(NNUEIndiceThem(moveInfo.king[~c], moveInfo.to, fromType));
   }
   if (moveInfo.type == T_ep) {
      const Square epSq = moveInfo.ep + (c == Co_White ? -8 : +8);
      nnueEvaluator.template us<c>().erase(NNUEIndiceThem(moveInfo.king[c], epSq, P_wp));
      nnueEvaluator.template them<c>().erase(NNUEIndiceUs(moveInfo.king[~c], epSq, P_wp));
   }
   else if (toType != P_none) {
      nnueEvaluator.template us<c>().erase(NNUEIndiceThem(moveInfo.king[c], moveInfo.to, toType));
      nnueEvaluator.template them<c>().erase(NNUEIndiceUs(moveInfo.king[~c], moveInfo.to, toType));
   }
   nnueEvaluator.dirty = false;
   STOP_AND_SUM_TIMER(UpdateNNUE)
}

template<Color c> void updateNNUEEvaluatorThemOnly(NNUEEvaluator& nnueEvaluator, const Position::MoveInfo& moveInfo) {
   START_TIMER
   const Piece fromType = Abs(moveInfo.fromP);
   const Piece toType = Abs(moveInfo.toP);
   nnueEvaluator.template them<c>().erase(NNUEIndiceThem(moveInfo.king[~c], moveInfo.from, fromType));
   if (isPromotion(moveInfo.type)) {
      const Piece promPieceType = promShift(moveInfo.type);
      nnueEvaluator.template them<c>().insert(NNUEIndiceThem(moveInfo.king[~c], moveInfo.to, promPieceType));
   }
   else {
      nnueEvaluator.template them<c>().insert(NNUEIndiceThem(moveInfo.king[~c], moveInfo.to, fromType));
   }
   if (moveInfo.type == T_ep) {
      const Square epSq = moveInfo.ep + (c == Co_White ? -8 : +8);
      nnueEvaluator.template them<c>().erase(NNUEIndiceUs(moveInfo.king[~c], epSq, P_wp));
   }
   else if (toType != P_none) {
      nnueEvaluator.template them<c>().erase(NNUEIndiceUs(moveInfo.king[~c], moveInfo.to, toType));
   }
   nnueEvaluator.dirty = false;
   STOP_AND_SUM_TIMER(UpdateNNUE)
}
#endif