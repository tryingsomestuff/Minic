#pragma once

#include "definition.hpp"

#include "attack.hpp"
#include "bitboardTools.hpp"
#include "hash.hpp"
#include "timers.hpp"

struct Searcher;

namespace MoveGen{

enum GenPhase { GP_all = 0, GP_cap = 1, GP_quiet = 2 };

void addMove(Square from, Square to, MType type, MoveList & moves);

template < GenPhase phase = GP_all >
void generateSquare(const Position & p, MoveList & moves, Square from){
    assert(from != INVALIDSQUARE);
    const Color side = p.c;
    const BitBoard myPieceBB  = p.allPieces[side];
    const BitBoard oppPieceBB = p.allPieces[~side];
    const Piece piece = p.b[from];
    const Piece ptype = (Piece)std::abs(piece);
    assert ( ptype != P_none );
    if (ptype != P_wp) {
        BitBoard bb = BBTools::pfCoverage[ptype-1](from, p.occupancy, p.c) & ~myPieceBB;
        if      (phase == GP_cap)   bb &= oppPieceBB;  // only target opponent piece
        else if (phase == GP_quiet) bb &= ~oppPieceBB; // do not target opponent piece
        while (bb) {
            const Square to = popBit(bb);
            const bool isCap = (phase == GP_cap) || ((oppPieceBB&SquareToBitboard(to)) != empty);
            if (isCap) addMove(from,to,T_capture,moves);
            else addMove(from,to,T_std,moves);
        }
        if ( phase != GP_cap && ptype == P_wk ){ // castling
            if ( side == Co_White) {
                if ( p.castling & (C_wqs|C_wks) ){
                   if ( (p.castling & C_wqs)
                        && ( ( (BBTools::mask[p.king[Co_White]].between[Sq_c1] | BBTools::mask[p.rooksInit[Co_White][CT_OOO]].between[Sq_d1]) & p.occupancy) == empty)
                        && !isAttacked(p,BBTools::mask[p.king[Co_White]].between[Sq_c1] | SquareToBitboard(p.king[Co_White])) ) addMove(from, Sq_c1, T_wqs, moves); // wqs
                   if ( (p.castling & C_wks)
                        && ( ( (BBTools::mask[p.king[Co_White]].between[Sq_g1] | BBTools::mask[p.rooksInit[Co_White][CT_OO ]].between[Sq_f1]) & p.occupancy) == empty)
                        && !isAttacked(p,BBTools::mask[p.king[Co_White]].between[Sq_g1] | SquareToBitboard(p.king[Co_White])) ) addMove(from, Sq_g1, T_wks, moves); // wks
                }
            }
            else if ( p.castling & (C_bqs|C_bks)){
                if ( (p.castling & C_bqs)
                     && ( ( (BBTools::mask[p.king[Co_Black]].between[Sq_c8] | BBTools::mask[p.rooksInit[Co_Black][CT_OOO]].between[Sq_d8]) & p.occupancy) == empty)
                     && !isAttacked(p,BBTools::mask[p.king[Co_Black]].between[Sq_c8] | SquareToBitboard(p.king[Co_Black])) ) addMove(from, Sq_c8, T_bqs, moves); // bqs
                if ( (p.castling & C_bks)
                     && ( ( (BBTools::mask[p.king[Co_Black]].between[Sq_g8] | BBTools::mask[p.rooksInit[Co_Black][CT_OO ]].between[Sq_f8]) & p.occupancy) == empty)
                     && !isAttacked(p,BBTools::mask[p.king[Co_Black]].between[Sq_g8] | SquareToBitboard(p.king[Co_Black])) ) addMove(from, Sq_g8, T_bks, moves); // bks
            }
        }
    }
    else {
        BitBoard pawnmoves = empty;
        static const BitBoard rank1_or_rank8 = rank1 | rank8;
        if ( phase != GP_quiet) pawnmoves = BBTools::mask[from].pawnAttack[p.c] & ~myPieceBB & oppPieceBB;
        while (pawnmoves) {
            const Square to = popBit(pawnmoves);
            if ( SquareToBitboard(to) & rank1_or_rank8 ) {
                addMove(from, to, T_cappromq, moves); // pawn capture with promotion
                addMove(from, to, T_cappromr, moves); // pawn capture with promotion
                addMove(from, to, T_cappromb, moves); // pawn capture with promotion
                addMove(from, to, T_cappromn, moves); // pawn capture with promotion
            } else addMove(from,to,T_capture,moves);
        }
        if ( phase != GP_cap) pawnmoves |= BBTools::mask[from].push[p.c] & ~p.occupancy;
        if ((phase != GP_cap) && (BBTools::mask[from].push[p.c] & p.occupancy) == empty) pawnmoves |= BBTools::mask[from].dpush[p.c] & ~p.occupancy;
        while (pawnmoves) {
            const Square to = popBit(pawnmoves);
            if ( SquareToBitboard(to) & rank1_or_rank8 ) {
                addMove(from, to, T_promq, moves); // promotion Q
                addMove(from, to, T_promr, moves); // promotion R
                addMove(from, to, T_promb, moves); // promotion B
                addMove(from, to, T_promn, moves); // promotion N
            } else addMove(from,to,T_std,moves);
        }
        if ( p.ep != INVALIDSQUARE && phase != GP_quiet ) pawnmoves = BBTools::mask[from].pawnAttack[p.c] & ~myPieceBB & SquareToBitboard(p.ep);
        while (pawnmoves) addMove(from,popBit(pawnmoves),T_ep,moves);
    }
}

template < GenPhase phase = GP_all >
void generate(const Position & p, MoveList & moves, bool doNotClear = false){
    START_TIMER
    if (!doNotClear) moves.clear();
    BitBoard myPieceBBiterator = ( (p.c == Co_White) ? p.allPieces[Co_White] : p.allPieces[Co_Black]);
    while (myPieceBBiterator) generateSquare<phase>(p,moves,popBit(myPieceBBiterator));
    STOP_AND_SUM_TIMER(Generate)
}

} // MoveGen

void movePiece(Position & p, Square from, Square to, Piece fromP, Piece toP, bool isCapture = false, Piece prom = P_none);

template < Color c>
inline void movePieceCastle(Position & p, CastlingTypes ct, Square kingDest, Square rookDest){
    START_TIMER
    const Piece pk = c==Co_White?P_wk:P_bk;
    const Piece pr = c==Co_White?P_wr:P_br;
    const CastlingRights ks = c==Co_White?C_wks:C_bks;
    const CastlingRights qs = c==Co_White?C_wqs:C_bqs;
    const Square sks = c==Co_White?7:63;
    const Square sqs = c==Co_White?0:56;
    BBTools::unSetBit(p, p.king[c]);
    BBTools::unSetBit(p, p.rooksInit[c][ct]);
    BBTools::setBit(p, kingDest, pk);
    BBTools::setBit(p, rookDest, pr);
    p.b[p.king[c]] = P_none;
    p.b[p.rooksInit[c][ct]] = P_none;
    p.b[kingDest] = pk;
    p.b[rookDest] = pr;
    p.h ^= Zobrist::ZT[p.king[c]][pk+PieceShift];
    p.ph ^= Zobrist::ZT[p.king[c]][pk+PieceShift];
    p.h ^= Zobrist::ZT[p.rooksInit[c][ct]][pr+PieceShift];
    p.h ^= Zobrist::ZT[kingDest][pk+PieceShift];
    p.ph ^= Zobrist::ZT[kingDest][pk+PieceShift];
    p.h ^= Zobrist::ZT[rookDest][pr+PieceShift];
    p.king[c] = kingDest;
    if (p.castling & qs) p.h ^= Zobrist::ZT[sqs][13];
    if (p.castling & ks) p.h ^= Zobrist::ZT[sks][13];
    p.castling &= ~(ks | qs);
    STOP_AND_SUM_TIMER(MovePiece)
}

void applyNull(Searcher & context, Position & pN);

bool apply(Position & p, const Move & m, bool noValidation = false);

ScoreType randomMover(const Position & p, PVList & pv, bool isInCheck);

bool isPseudoLegal(const Position & p, Move m);
