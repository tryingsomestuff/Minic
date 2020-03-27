#include "moveGen.hpp"

#include "logging.hpp"
#include "material.hpp"
#include "positionTools.hpp"
#include "tools.hpp"

namespace MoveGen{

void addMove(Square from, Square to, MType type, MoveList & moves) {
  assert(from >= 0 && from < 64);
  assert(to >= 0 && to < 64);
  moves.push_back(ToMove(from, to, type, 0));
}

} // MoveGen

void movePiece(Position & p, Square from, Square to, Piece fromP, Piece toP, bool isCapture, Piece prom) {
    START_TIMER
    const int fromId   = fromP + PieceShift;
    const int toId     = toP + PieceShift;
    const Piece toPnew = prom != P_none ? prom : fromP;
    const int toIdnew  = prom != P_none ? (prom + PieceShift) : fromId;
    assert(from>=0 && from<64);
    assert(to>=0 && to<64);
    assert(fromP != P_none);
    // update board
    p.b[from] = P_none;
    p.b[to]   = toPnew;
    // update bitboard
    BBTools::unSetBit(p, from, fromP);
    BBTools::unSetBit(p, to,   toP); // usefull only if move is a capture
    BBTools::setBit  (p, to,   toPnew);
    // update Zobrist hash
    p.h ^= Zobrist::ZT[from][fromId]; // remove fromP at from
    p.h ^= Zobrist::ZT[to][toIdnew]; // add fromP (or prom) at to
    if ( abs(fromP) == P_wp || abs(fromP) == P_wk ){
       p.ph ^= Zobrist::ZT[from][fromId]; // remove fromP at from
       if ( prom == P_none) p.ph ^= Zobrist::ZT[to][toIdnew]; // add fromP (if not prom) at to
    }
    if (isCapture) { // if capture remove toP at to
        p.h ^= Zobrist::ZT[to][toId];
        if ( (abs(toP) == P_wp || abs(toP) == P_wk) ) p.ph ^= Zobrist::ZT[to][toId];
    }
    STOP_AND_SUM_TIMER(MovePiece)
}

void applyNull(Searcher & context, Position & pN) {
    pN.c = ~pN.c;
    pN.h ^= Zobrist::ZT[3][13];
    pN.h ^= Zobrist::ZT[4][13];
    if (pN.ep != INVALIDSQUARE) pN.h ^= Zobrist::ZT[pN.ep][13];
    pN.ep = INVALIDSQUARE;
    pN.lastMove = NULLMOVE;
    if ( pN.c == Co_White ) ++pN.moves;
    ++pN.halfmoves;
}

bool apply(Position & p, const Move & m, bool noValidation){
    START_TIMER
    assert(VALIDMOVE(m));
#ifdef DEBUG_MATERIAL
    Position previous = p;
#endif
    const Square from  = Move2From(m);
    const Square to    = Move2To(m);
    const MType  type  = Move2Type(m);
    const Piece  fromP = p.b[from];
    const Piece  toP   = p.b[to];
    const int fromId   = fromP + PieceShift;
#ifdef DEBUG_APPLY
    if (!isPseudoLegal(p, m)) {
        Logging::LogIt(Logging::logError) << "Apply error, not legal " << ToString(p) << ToString(m);
        assert(false);
    }
#endif
    switch(type){
    case T_std:
    case T_capture:
    case T_reserved:
        p.mat[~p.c][std::abs(toP)]--;
        movePiece(p, from, to, fromP, toP, type == T_capture);

        // update castling rigths and king position
        if ( fromP == P_wk ){
            p.king[Co_White] = to;
            if (p.castling & C_wks) p.h ^= Zobrist::ZT[7][13];
            if (p.castling & C_wqs) p.h ^= Zobrist::ZT[0][13];
            p.castling &= ~(C_wks | C_wqs);
        }
        else if ( fromP == P_bk ){
            p.king[Co_Black] = to;
            if (p.castling & C_bks) p.h ^= Zobrist::ZT[63][13];
            if (p.castling & C_bqs) p.h ^= Zobrist::ZT[56][13];
            p.castling &= ~(C_bks | C_bqs);
        }
        // king capture : is that necessary ???
        if      ( toP == P_wk ) p.king[Co_White] = INVALIDSQUARE;
        else if ( toP == P_bk ) p.king[Co_Black] = INVALIDSQUARE;

        if ( p.castling != C_none ){
           if ( (p.castling & C_wqs) && from == p.rooksInit[Co_White][CT_OOO] && fromP == P_wr ){
               p.castling &= ~C_wqs;
               p.h ^= Zobrist::ZT[0][13];
           }
           else if ( (p.castling & C_wks) && from == p.rooksInit[Co_White][CT_OO] && fromP == P_wr ){
               p.castling &= ~C_wks;
               p.h ^= Zobrist::ZT[7][13];
           }
           else if ( (p.castling & C_bqs) && from == p.rooksInit[Co_Black][CT_OOO] && fromP == P_br){
               p.castling &= ~C_bqs;
               p.h ^= Zobrist::ZT[56][13];
           }
           else if ( (p.castling & C_bks) && from == p.rooksInit[Co_Black][CT_OO] && fromP == P_br ){
               p.castling &= ~C_bks;
               p.h ^= Zobrist::ZT[63][13];
           }
        }
        break;

    case T_ep: {
        assert(p.ep != INVALIDSQUARE);
        assert(SQRANK(p.ep) == EPRank[p.c]);
        const Square epCapSq = p.ep + (p.c == Co_White ? -8 : +8);
        assert(epCapSq>=0 && epCapSq<64);
        BBTools::unSetBit(p, epCapSq); // BEFORE setting p.b new shape !!!
        BBTools::unSetBit(p, from);
        BBTools::setBit(p, to, fromP);
        p.b[from] = P_none;
        p.b[to] = fromP;
        p.b[epCapSq] = P_none;

        p.h ^= Zobrist::ZT[from][fromId]; // remove fromP at from
        p.h ^= Zobrist::ZT[epCapSq][(p.c == Co_White ? P_bp : P_wp) + PieceShift]; // remove captured pawn
        p.h ^= Zobrist::ZT[to][fromId]; // add fromP at to

        p.ph ^= Zobrist::ZT[from][fromId]; // remove fromP at from
        p.ph ^= Zobrist::ZT[epCapSq][(p.c == Co_White ? P_bp : P_wp) + PieceShift]; // remove captured pawn
        p.ph ^= Zobrist::ZT[to][fromId]; // add fromP at to

        p.mat[~p.c][M_p]--;
    }
        break;

    case T_promq:
    case T_cappromq:
        MaterialHash::updateMaterialProm(p,to,type);
        movePiece(p, from, to, fromP, toP, type == T_cappromq,(p.c == Co_White ? P_wq : P_bq));
        break;
    case T_promr:
    case T_cappromr:
        MaterialHash::updateMaterialProm(p,to,type);
        movePiece(p, from, to, fromP, toP, type == T_cappromr, (p.c == Co_White ? P_wr : P_br));
        break;
    case T_promb:
    case T_cappromb:
        MaterialHash::updateMaterialProm(p,to,type);
        movePiece(p, from, to, fromP, toP, type == T_cappromb, (p.c == Co_White ? P_wb : P_bb));
        break;
    case T_promn:
    case T_cappromn:
        MaterialHash::updateMaterialProm(p,to,type);
        movePiece(p, from, to, fromP, toP, type == T_cappromn, (p.c == Co_White ? P_wn : P_bn));
        break;
    case T_wks:
        movePieceCastle<Co_White>(p,CT_OO,Sq_g1,Sq_f1);
        break;
    case T_wqs:
        movePieceCastle<Co_White>(p,CT_OOO,Sq_c1,Sq_d1);
        break;
    case T_bks:
        movePieceCastle<Co_Black>(p,CT_OO,Sq_g8,Sq_f8);
        break;
    case T_bqs:
        movePieceCastle<Co_Black>(p,CT_OOO,Sq_c8,Sq_d8);
        break;
    }

    p.allPieces[Co_White] = p.whitePawn() | p.whiteKnight() | p.whiteBishop() | p.whiteRook() | p.whiteQueen() | p.whiteKing();
    p.allPieces[Co_Black] = p.blackPawn() | p.blackKnight() | p.blackBishop() | p.blackRook() | p.blackQueen() | p.blackKing();
    p.occupancy = p.allPieces[Co_White] | p.allPieces[Co_Black];

    if ( !noValidation && isAttacked(p,kingSquare(p)) ) return false; // this is the only legal move validation needed

    // Update castling right if rook captured
    if ( p.castling != C_none ){
       if ( toP == P_wr && to == p.rooksInit[Co_White][CT_OOO] && (p.castling & C_wqs) ){
           p.castling &= ~C_wqs;
           p.h ^= Zobrist::ZT[0][13];
       }
       else if ( toP == P_wr && to == p.rooksInit[Co_White][CT_OO] && (p.castling & C_wks) ){
           p.castling &= ~C_wks;
           p.h ^= Zobrist::ZT[7][13];
       }
       else if ( toP == P_br && to == p.rooksInit[Co_Black][CT_OOO] && (p.castling & C_bqs)){
           p.castling &= ~C_bqs;
           p.h ^= Zobrist::ZT[56][13];
       }
       else if ( toP == P_br && to == p.rooksInit[Co_Black][CT_OO] && (p.castling & C_bks)){
           p.castling &= ~C_bks;
           p.h ^= Zobrist::ZT[63][13];
       }
    }

    // update EP
    if (p.ep != INVALIDSQUARE) p.h  ^= Zobrist::ZT[p.ep][13];
    p.ep = INVALIDSQUARE;
    if ( abs(fromP) == P_wp && abs(to-from) == 16 ) p.ep = (from + to)/2;
    assert(p.ep == INVALIDSQUARE || SQRANK(p.ep) == EPRank[~p.c]);
    if (p.ep != INVALIDSQUARE) p.h  ^= Zobrist::ZT[p.ep][13];

    // update color
    p.c = ~p.c;
    p.h  ^= Zobrist::ZT[3][13] ; p.h  ^= Zobrist::ZT[4][13];

    // update game state
    if ( toP != P_none || abs(fromP) == P_wp ) p.fifty = 0;
    else ++p.fifty;
    if ( p.c == Co_White ) ++p.moves;
    ++p.halfmoves;

    MaterialHash::updateMaterialOther(p);
#ifdef DEBUG_MATERIAL
    Position::Material mat = p.mat;
    MaterialHash::initMaterial(p);
    if ( p.mat != mat ){ Logging::LogIt(Logging::logFatal) << "Material update error" << ToString(previous) << ToString(previous.mat) << ToString(p) << ToString(p.lastMove) << ToString(m) << ToString(mat) << ToString(p.mat); }
#endif
    p.lastMove = m;
    STOP_AND_SUM_TIMER(Apply)
    return true;
}

ScoreType randomMover(const Position & p, PVList & pv, bool isInCheck) {
    MoveList moves;
    MoveGen::generate<MoveGen::GP_all>(p, moves, false);
    if (moves.empty()) return isInCheck ? -MATE : 0;
    static std::random_device rd;
    static std::mt19937 g(rd());
    std::shuffle(moves.begin(), moves.end(),g);
    for (auto it = moves.begin(); it != moves.end(); ++it) {
        Position p2 = p;
        if (!apply(p2, *it)) continue;
        PVList childPV;
        updatePV(pv, *it, childPV);
        const Square to = Move2To(*it);
        if (p.c == Co_White && to == p.king[Co_Black]) return MATE + 1;
        if (p.c == Co_Black && to == p.king[Co_White]) return MATE + 1;
        return 0;
    }
    return isInCheck ? -MATE : 0;
}

#ifdef DEBUG_PSEUDO_LEGAL
bool isPseudoLegal2(const Position & p, Move m); // forward decl
bool isPseudoLegal(const Position & p, Move m) {
    const bool b = isPseudoLegal2(p,m);
    if (b) {
        MoveList moves;
        MoveGen::generate<MoveGen::GP_all>(p, moves);
        bool found = false;
        for (auto it = moves.begin(); it != moves.end() && !found; ++it) if (sameMove(*it, m)) found = true;
        if (!found){
            std::cout << ToString(p) << "\n"  << ToString(m) << "\t" << m << std::endl;
            std::cout << SquareNames[Move2From(m)] << std::endl;
            std::cout << SquareNames[Move2To(m)] << std::endl;
            std::cout << (int)Move2Type(m) << std::endl;
            std::cout << Move2Score(m) << std::endl;
            std::cout << int(m & 0x0000FFFF) << std::endl;
            for (auto it = moves.begin(); it != moves.end(); ++it) std::cout << ToString(*it) <<"\t" << *it << "\t";
            std::cout << std::endl;
            std::cout << "Not a generated move !" << std::endl;
        }
    }
    return b;
}
bool isPseudoLegal2(const Position & p, Move m) { // validate TT move
#else
 bool isPseudoLegal(const Position & p, Move m) { // validate TT move
#endif
    if (!VALIDMOVE(m)) { return false; }
    const Square from = Move2From(m);
    const Piece fromP = p.b[from];
    if (fromP == P_none || (fromP > 0 && p.c == Co_Black) || (fromP < 0 && p.c == Co_White)) { return false; }
    const Square to = Move2To(m);
    const Piece toP = p.b[to];
    if ((toP > 0 && p.c == Co_White) || (toP < 0 && p.c == Co_Black)) { return false; }
    if ((Piece)std::abs(toP) == P_wk) { return false; }
    const Piece fromPieceType = (Piece)std::abs(fromP);
    const MType t = Move2Type(m);
    if ( t == T_reserved ) {return false;}
    if (toP == P_none && (isCapture(t) && t!=T_ep)) { return false; }
    if (toP != P_none && !isCapture(t)) { return false; }
    if (t == T_ep && (p.ep == INVALIDSQUARE || fromPieceType != P_wp)) { return false;}
    if (t == T_ep && p.b[p.ep + (p.c==Co_White?-8:+8)] != (p.c==Co_White?P_bp:P_wp)) { return false;}
    if (isPromotion(m) && fromPieceType != P_wp) { return false; }
    if (isCastling(m)) {
        if (p.c == Co_White) {
            if (t == T_wqs && (p.castling & C_wqs) && from == p.kingInit[Co_White] && fromP == P_wk && to == Sq_c1 && toP == P_none
                && (((BBTools::mask[p.king[Co_White]].between[Sq_c1] | BBTools::mask[p.rooksInit[Co_White][CT_OOO]].between[Sq_d1]) & p.occupancy) == empty)
                && !isAttacked(p, BBTools::mask[p.king[Co_White]].between[Sq_c1] | SquareToBitboard(p.king[Co_White]))) return true;
            if (t == T_wks && (p.castling & C_wks) && from == p.kingInit[Co_White] && fromP == P_wk && to == Sq_g1 && toP == P_none
                && (((BBTools::mask[p.king[Co_White]].between[Sq_g1] | BBTools::mask[p.rooksInit[Co_White][CT_OO]].between[Sq_f1]) & p.occupancy) == empty)
                && !isAttacked(p, BBTools::mask[p.king[Co_White]].between[Sq_g1] | SquareToBitboard(p.king[Co_White]))) return true;
            return false;
        }
        else {
            if (t == T_bqs && (p.castling & C_bqs) && from == p.kingInit[Co_Black] && fromP == P_bk && to == Sq_c8 && toP == P_none
                && (((BBTools::mask[p.king[Co_Black]].between[Sq_c8] | BBTools::mask[p.rooksInit[Co_Black][CT_OOO]].between[Sq_d8]) & p.occupancy) == empty)
                && !isAttacked(p, BBTools::mask[p.king[Co_Black]].between[Sq_c8] | SquareToBitboard(p.king[Co_Black]))) return true;
            if (t == T_bks && (p.castling & C_bks) && from == p.kingInit[Co_Black] && fromP == P_bk && to == Sq_g8 && toP == P_none
                && (((BBTools::mask[p.king[Co_Black]].between[Sq_g8] | BBTools::mask[p.rooksInit[Co_Black][CT_OO]].between[Sq_f8]) & p.occupancy) == empty)
                && !isAttacked(p, BBTools::mask[p.king[Co_Black]].between[Sq_g8] | SquareToBitboard(p.king[Co_Black]))) return true;
            return false;
        }
    }
    if (fromPieceType == P_wp) {
        if (t == T_ep && to != p.ep) { return false; }
        if (t != T_ep && p.ep != INVALIDSQUARE && to == p.ep) { return false; }
        if (!isPromotion(m) && SQRANK(to) == PromRank[p.c]) { return false; }
        if (isPromotion(m) && SQRANK(to) != PromRank[p.c]) { return false; }
        BitBoard validPush = BBTools::mask[from].push[p.c] & ~p.occupancy;
        if ((BBTools::mask[from].push[p.c] & p.occupancy) == empty) validPush |= BBTools::mask[from].dpush[p.c] & ~p.occupancy;
        if (validPush & SquareToBitboard(to)) return true;
        const BitBoard validCap = BBTools::mask[from].pawnAttack[p.c] & ~p.allPieces[p.c];
        if ((validCap & SquareToBitboard(to)) && (( t != T_ep && toP != P_none) || (t == T_ep && to == p.ep && toP == P_none))) return true;
        return false;
    }
    if (fromPieceType != P_wk) {
        if ((BBTools::pfCoverage[fromPieceType - 1](from, p.occupancy, p.c) & SquareToBitboard(to)) != empty) { return true; }
        return false;
    }
    if ((BBTools::mask[p.king[p.c]].kingZone & SquareToBitboard(to)) != empty) { return true; } // only king is not verified yet
    return false;
}

