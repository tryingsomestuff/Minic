#pragma once

#include "definition.hpp"

struct Position; // forward decl
bool readFEN(const std::string & fen, Position & p, bool silent = false, bool withMoveount = false); // forward decl

/* The main position structure
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
    inline const Piece & board_const(Square k)const{ return _b[k];}
    inline Piece & board(Square k){ return _b[k];}
    
    std::array<Piece,64>    _b   {{ P_none }}; // works because P_none is in fact 0 ...
    std::array<BitBoard,6>  _allB {{ empty }};

    BitBoard allPieces[2] = {empty};
    BitBoard occupancy    = empty;

    // t p n b r q k bl bd M n  (total is first so that pawn to king is same a Piece)
    typedef std::array<std::array<char,11>,2> Material;
    Material mat = {{{{0}}}}; // such a nice syntax ...

    mutable Hash h = nullHash, ph = nullHash;
    Move lastMove = INVALIDMOVE;
    Square ep = INVALIDSQUARE, king[2] = { INVALIDSQUARE, INVALIDSQUARE }, rooksInit[2][2] = { {INVALIDSQUARE, INVALIDSQUARE}, {INVALIDSQUARE, INVALIDSQUARE}}, kingInit[2] = {INVALIDSQUARE, INVALIDSQUARE};
    unsigned char fifty = 0;
    unsigned short int moves = 0, halfmoves = 0;
    CastlingRights castling = C_none;
    Color c = Co_White;

    inline const BitBoard blackKing  ()const {return _allB[5] & allPieces[Co_Black];}
    inline const BitBoard blackQueen ()const {return _allB[4] & allPieces[Co_Black];}
    inline const BitBoard blackRook  ()const {return _allB[3] & allPieces[Co_Black];}
    inline const BitBoard blackBishop()const {return _allB[2] & allPieces[Co_Black];}
    inline const BitBoard blackKnight()const {return _allB[1] & allPieces[Co_Black];}
    inline const BitBoard blackPawn  ()const {return _allB[0] & allPieces[Co_Black];}

    inline const BitBoard whitePawn  ()const {return _allB[0] & allPieces[Co_White];}
    inline const BitBoard whiteKnight()const {return _allB[1] & allPieces[Co_White];}
    inline const BitBoard whiteBishop()const {return _allB[2] & allPieces[Co_White];}
    inline const BitBoard whiteRook  ()const {return _allB[3] & allPieces[Co_White];}
    inline const BitBoard whiteQueen ()const {return _allB[4] & allPieces[Co_White];}
    inline const BitBoard whiteKing  ()const {return _allB[5] & allPieces[Co_White];}

    template<Piece pp>
    inline const BitBoard pieces_const(Color c)const          { assert(pp!=P_none); return _allB[pp-1] & allPieces[c]; }
    inline const BitBoard pieces_const(Color c, Piece pp)const{ assert(pp!=P_none); return _allB[pp-1] & allPieces[c]; }

    inline BitBoard & pieces(Piece pp)         { assert(pp!=P_none); return _allB[std::abs(pp)-1]; }

    Position(){}
    Position(const std::string & fen, bool withMoveCount = true){readFEN(fen,*this,true,withMoveCount);}
};
