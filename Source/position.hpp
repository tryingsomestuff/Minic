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
    std::array<Piece,64>    b    {{ P_none }}; // works because P_none is in fact 0 ...

    std::array<BitBoard,13> allB {{ empty }};

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

    inline const BitBoard & blackKing  ()const {return allB[0];}
    inline const BitBoard & blackQueen ()const {return allB[1];}
    inline const BitBoard & blackRook  ()const {return allB[2];}
    inline const BitBoard & blackBishop()const {return allB[3];}
    inline const BitBoard & blackKnight()const {return allB[4];}
    inline const BitBoard & blackPawn  ()const {return allB[5];}
    inline const BitBoard & whitePawn  ()const {return allB[7];}
    inline const BitBoard & whiteKnight()const {return allB[8];}
    inline const BitBoard & whiteBishop()const {return allB[9];}
    inline const BitBoard & whiteRook  ()const {return allB[10];}
    inline const BitBoard & whiteQueen ()const {return allB[11];}
    inline const BitBoard & whiteKing  ()const {return allB[12];}

    template<Piece pp>
    inline const BitBoard & pieces(Color c)const{ return allB[(1-2*c)*pp+PieceShift]; }
    inline const BitBoard & pieces(Color c, Piece pp)const{ return allB[(1-2*c)*pp+PieceShift]; }

    Position(){}
    Position(const std::string & fen, bool withMoveCount = true){readFEN(fen,*this,true,withMoveCount);}
};
