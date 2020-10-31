#pragma once

#include "definition.hpp"
#include "bitboard.hpp"

#ifdef WITH_NNUE
#include "nnue.hpp"
#endif

struct Position; // forward decl

bool readFEN(const std::string & fen, Position & p, bool silent = false, bool withMoveount = false); // forward decl

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
struct Position{
    Position();
    Position(const std::string & fen, bool withMoveCount = true);
    ~Position();

    std::array<Piece,64>    _b           {{P_none}}; // works because P_none is in fact 0 ...
    std::array<BitBoard,6>  _allB        {{emptyBitBoard}};
    std::array<BitBoard,2>  allPieces    {{emptyBitBoard}};

    // t p n b r q k bl bd M n  (total is first so that pawn to king is same a Piece)
    typedef std::array<std::array<char,11>,2> Material;
    Material mat = {{{{0}}}}; // such a nice syntax ...

    mutable Hash h = nullHash, ph = nullHash;
    MiniMove lastMove = INVALIDMINIMOVE;
    unsigned short int moves = 0, halfmoves = 0;
    std::array<Square,2> king = { INVALIDSQUARE, INVALIDSQUARE };
    std::array<std::array<Square,2>,2> rooksInit = {{ {INVALIDSQUARE, INVALIDSQUARE}, {INVALIDSQUARE, INVALIDSQUARE} }};
    std::array<Square,2> kingInit = {INVALIDSQUARE, INVALIDSQUARE};
    Square ep = INVALIDSQUARE;
    unsigned char fifty = 0;
    CastlingRights castling = C_none;
    Color c = Co_White;

    inline const Piece & board_const(Square k)const{ return _b[k];}
    inline Piece & board(Square k){ return _b[k];}

    inline BitBoard occupancy()const { return allPieces[Co_White] | allPieces[Co_Black];}

    inline BitBoard allKing  ()const {return _allB[5];}
    inline BitBoard allQueen ()const {return _allB[4];}
    inline BitBoard allRook  ()const {return _allB[3];}
    inline BitBoard allBishop()const {return _allB[2];}
    inline BitBoard allKnight()const {return _allB[1];}
    inline BitBoard allPawn  ()const {return _allB[0];}

    inline BitBoard blackKing  ()const {return _allB[5] & allPieces[Co_Black];}
    inline BitBoard blackQueen ()const {return _allB[4] & allPieces[Co_Black];}
    inline BitBoard blackRook  ()const {return _allB[3] & allPieces[Co_Black];}
    inline BitBoard blackBishop()const {return _allB[2] & allPieces[Co_Black];}
    inline BitBoard blackKnight()const {return _allB[1] & allPieces[Co_Black];}
    inline BitBoard blackPawn  ()const {return _allB[0] & allPieces[Co_Black];}

    inline BitBoard whitePawn  ()const {return _allB[0] & allPieces[Co_White];}
    inline BitBoard whiteKnight()const {return _allB[1] & allPieces[Co_White];}
    inline BitBoard whiteBishop()const {return _allB[2] & allPieces[Co_White];}
    inline BitBoard whiteRook  ()const {return _allB[3] & allPieces[Co_White];}
    inline BitBoard whiteQueen ()const {return _allB[4] & allPieces[Co_White];}
    inline BitBoard whiteKing  ()const {return _allB[5] & allPieces[Co_White];}

    template<Piece pp>
    inline BitBoard pieces_const(Color cc)const          { assert(pp!=P_none); return _allB[pp-1] & allPieces[cc]; }
    inline BitBoard pieces_const(Color cc, Piece pp)const{ assert(pp!=P_none); return _allB[pp-1] & allPieces[cc]; }
    inline BitBoard pieces_const(Piece pp)const          { assert(pp!=P_none); return _allB[std::abs(pp)-1] & allPieces[pp>0?Co_White:Co_Black]; }
    // next one is kinda "private"
    inline BitBoard & _pieces(Piece pp)                  { assert(pp!=P_none); return _allB[std::abs(pp)-1]; }

#ifdef WITH_NNUE

  using NNUEEvaluator = nnue::half_kp_eval<NNUEWrapper::nnueType>;

  mutable NNUEEvaluator * associatedEvaluator = nullptr;
  void associateEvaluator(NNUEEvaluator & evaluator){ associatedEvaluator = &evaluator; }

  NNUEEvaluator & Evaluator(){ assert(associatedEvaluator); return *associatedEvaluator; }
  const NNUEEvaluator & Evaluator()const{ assert(associatedEvaluator); return *associatedEvaluator; }

// Taken from Seer implementation.
// see https://github.com/connormcmonigle/seer-nnue

  template<Color c>
  void resetNNUEIndices_(NNUEEvaluator & nnueEvaluator)const {
    using namespace feature_idx;
    const size_t king_idx = HFlip(king[c]);
    //us
    BitBoard us_pawn   = pieces_const<P_wp>(c); while(us_pawn)  { nnueEvaluator.template us<c>().insert(major*king_idx + us_pawn_offset   + HFlip(popBit(us_pawn))); }
    BitBoard us_knigt  = pieces_const<P_wn>(c); while(us_knigt) { nnueEvaluator.template us<c>().insert(major*king_idx + us_knight_offset + HFlip(popBit(us_knigt))); }
    BitBoard us_bishop = pieces_const<P_wb>(c); while(us_bishop){ nnueEvaluator.template us<c>().insert(major*king_idx + us_bishop_offset + HFlip(popBit(us_bishop))); }
    BitBoard us_rook   = pieces_const<P_wr>(c); while(us_rook)  { nnueEvaluator.template us<c>().insert(major*king_idx + us_rook_offset   + HFlip(popBit(us_rook))); }
    BitBoard us_queen  = pieces_const<P_wq>(c); while(us_queen) { nnueEvaluator.template us<c>().insert(major*king_idx + us_queen_offset  + HFlip(popBit(us_queen))); }
    BitBoard us_king   = pieces_const<P_wk>(c); while(us_king)  { nnueEvaluator.template us<c>().insert(major*king_idx + us_king_offset   + HFlip(popBit(us_king))); }
    //them
    BitBoard them_pawn   = pieces_const<P_wp>(~c); while(them_pawn)  { nnueEvaluator.template us<c>().insert(major*king_idx + them_pawn_offset   + HFlip(popBit(them_pawn))); }
    BitBoard them_knigt  = pieces_const<P_wn>(~c); while(them_knigt) { nnueEvaluator.template us<c>().insert(major*king_idx + them_knight_offset + HFlip(popBit(them_knigt))); }
    BitBoard them_bishop = pieces_const<P_wb>(~c); while(them_bishop){ nnueEvaluator.template us<c>().insert(major*king_idx + them_bishop_offset + HFlip(popBit(them_bishop))); }
    BitBoard them_rook   = pieces_const<P_wr>(~c); while(them_rook)  { nnueEvaluator.template us<c>().insert(major*king_idx + them_rook_offset   + HFlip(popBit(them_rook))); }
    BitBoard them_queen  = pieces_const<P_wq>(~c); while(them_queen) { nnueEvaluator.template us<c>().insert(major*king_idx + them_queen_offset  + HFlip(popBit(them_queen))); }
    BitBoard them_king   = pieces_const<P_wk>(~c); while(them_king)  { nnueEvaluator.template us<c>().insert(major*king_idx + them_king_offset   + HFlip(popBit(them_king))); }
  }

  void resetNNUEEvaluator(NNUEEvaluator & nnueEvaluator)const {
    nnueEvaluator.white.clear();
    nnueEvaluator.black.clear();
    resetNNUEIndices_<Co_White>(nnueEvaluator);
    resetNNUEIndices_<Co_Black>(nnueEvaluator);
  }

  template<Color c>
  void updateNNUEEvaluator(NNUEEvaluator & nnueEvaluator, const Move & m)const{
    const Square from = Move2From(m);
    const Square to   = Move2To(m);
    const MType type  = Move2Type(m);
    const Piece fromP = board_const(from);
    const Piece toP   = board_const(to);
    const Piece pTypeFrom = (Piece)std::abs(fromP);
    const Piece pTypeTo   = (Piece)std::abs(toP);      
    const Square our_king_idx   = HFlip(king[c]);
    const Square their_king_idx = HFlip(king[~c]);
    nnueEvaluator.template us<c>().erase(feature_idx::major * our_king_idx + HFlip(from) + feature_idx::us_offset(pTypeFrom));
    nnueEvaluator.template them<c>().erase(feature_idx::major * their_king_idx + HFlip(from) + feature_idx::them_offset(pTypeFrom));
    if(isPromotion(m)){
        const Piece promPieceType = (Piece)std::abs(promShift(type));
        nnueEvaluator.template us<c>().insert(feature_idx::major * our_king_idx + HFlip(to) + feature_idx::us_offset(promPieceType));
        nnueEvaluator.template them<c>().insert(feature_idx::major * their_king_idx + HFlip(to) + feature_idx::them_offset(promPieceType));
    }
    else{
        nnueEvaluator.template us<c>().insert(feature_idx::major * our_king_idx + HFlip(to) + feature_idx::us_offset(pTypeFrom));
        nnueEvaluator.template them<c>().insert(feature_idx::major * their_king_idx + HFlip(to) + feature_idx::them_offset(pTypeFrom));
    }
    if(type == T_ep){
        const Square epSq = ep + (c == Co_White ? -8 : +8);
        nnueEvaluator.template them<c>().erase(feature_idx::major * their_king_idx + HFlip(epSq) + feature_idx::us_pawn_offset);
        nnueEvaluator.template us<c>().erase(feature_idx::major * our_king_idx + HFlip(epSq) + feature_idx::them_pawn_offset);
    }
    else if(isCapture(m)){
        nnueEvaluator.template them<c>().erase(feature_idx::major * their_king_idx + HFlip(to) + feature_idx::us_offset(pTypeTo));
        nnueEvaluator.template us<c>().erase(feature_idx::major * our_king_idx + HFlip(to) + feature_idx::them_offset(pTypeTo));
    }
  }

#endif

};
