#include "searcher.hpp"

#include "logging.hpp"

ScoreType Searcher::SEE(const Position & p, const Move & m) const {
    if ( ! VALIDMOVE(m) ) return 0;

    START_TIMER

    Square from = Move2From(m); assert(squareOK(from));
    const Square to = Move2To(m); assert(squareOK(to));
    const MType mtype = Move2Type(m); assert(moveTypeOK(mtype));
    BitBoard attackers = BBTools::allAttackedBB(p, to, p.c) | BBTools::allAttackedBB(p, to, ~p.c);
    BitBoard occupation_mask = 0xFFFFFFFFFFFFFFFF;
    ScoreType current_target_val = 0;
    const bool promPossible = PROMOTION_RANK(to);
    Color c = p.c;

    int nCapt = 0;
    ScoreType swapList[32]; // max 32 caps ... shall be ok

    Piece pp = PieceTools::getPieceType(p, from);
    if ( mtype == T_ep ){
        swapList[nCapt] = Values[P_wp+PieceShift];
        current_target_val = Values[pp+PieceShift];
        occupation_mask &= ~SquareToBitboard(p.ep);
    }
    else{
        swapList[nCapt] = PieceTools::getAbsValue(p, to);
        if (promPossible && pp == P_wp) {
            swapList[nCapt] += Values[promShift(mtype)+PieceShift] - Values[P_wp+PieceShift];
            current_target_val = Values[promShift(mtype)+PieceShift];
        }
        else current_target_val = Values[pp+PieceShift];
    }
    nCapt++;

    attackers &= ~SquareToBitboard(from);
    occupation_mask &= ~SquareToBitboard(from);
    const BitBoard occupancy = p.occupancy();

    attackers |= BBTools::attack<P_wr>(to, p.whiteQueen() | p.blackQueen() | p.whiteRook()   | p.blackRook(),   occupancy & occupation_mask, c) |
                 BBTools::attack<P_wb>(to, p.whiteQueen() | p.blackQueen() | p.whiteBishop() | p.blackBishop(), occupancy & occupation_mask, c) ;
    attackers &= occupation_mask;
    c = ~c;

    while (attackers) {
        if (!promPossible && attackers & p.pieces_const<P_wp>(c))        from = BBTools::SquareFromBitBoard(attackers & p.pieces_const<P_wp>(c)),pp=P_wp;
        else if (attackers & p.pieces_const<P_wn>(c))                    from = BBTools::SquareFromBitBoard(attackers & p.pieces_const<P_wn>(c)),pp=P_wn;
        else if (attackers & p.pieces_const<P_wb>(c))                    from = BBTools::SquareFromBitBoard(attackers & p.pieces_const<P_wb>(c)),pp=P_wb;
        else if (attackers & p.pieces_const<P_wr>(c))                    from = BBTools::SquareFromBitBoard(attackers & p.pieces_const<P_wr>(c)),pp=P_wr;
        else if (promPossible && (attackers & p.pieces_const<P_wp>(c)))  from = BBTools::SquareFromBitBoard(attackers & p.pieces_const<P_wp>(c)),pp=P_wp;
        else if (attackers & p.pieces_const<P_wq>(c))                    from = BBTools::SquareFromBitBoard(attackers & p.pieces_const<P_wq>(c)),pp=P_wq;
        else if ((attackers & p.pieces_const<P_wk>(c)) && !(attackers & p.allPieces[~c])) from = BBTools::SquareFromBitBoard(attackers &  p.pieces_const<P_wk>(c)),pp=P_wk;
        else break;

        swapList[nCapt] = -swapList[nCapt - 1] + current_target_val;
        if (promPossible && pp == P_wp) {
            swapList[nCapt] += Values[P_wq+PieceShift] - Values[P_wp+PieceShift];
            current_target_val = Values[P_wq+PieceShift];
        }
        else current_target_val = Values[pp+PieceShift];

        nCapt++;
        attackers &= ~SquareToBitboard(from);
        occupation_mask &= ~SquareToBitboard(from);

        attackers |= BBTools::attack<P_wr>(to, p.whiteQueen() | p.blackQueen() | p.whiteRook()   | p.blackRook(),   occupancy & occupation_mask, c) |
                     BBTools::attack<P_wb>(to, p.whiteQueen() | p.blackQueen() | p.whiteBishop() | p.blackBishop(), occupancy & occupation_mask, c) ;
        attackers &= occupation_mask;

        c = ~c;
    }

    while (--nCapt) if (swapList[nCapt] > -swapList[nCapt - 1])  swapList[nCapt - 1] = -swapList[nCapt];
    STOP_AND_SUM_TIMER(See)
    return swapList[0];
}

bool Searcher::SEE_GE(const Position & p, const Move & m, ScoreType threshold) const{
   return SEE(p,m) >= threshold;
}

/*
// Static Exchange Evaluation (cutoff version algorithm from Stockfish)
bool Searcher::SEE_GE(const Position & p, const Move & m, ScoreType threshold) const{
    assert(VALIDMOVE(m));
    START_TIMER
    const Square from = Move2From(m);
    const Square to   = Move2To(m);
    const MType type  = Move2Type(m);
    if (PieceTools::getPieceType(p, to) == P_wk) return true; // capture king !
    const bool promPossible = PROMOTION_RANK_C(to,p.c);
    if (promPossible) return true; // never treat possible prom case !
    Piece pp = PieceTools::getPieceType(p,from);
    bool prom = promPossible && pp == P_wp;
    Piece nextVictim  = prom ? P_wq : pp; ///@todo other prom
    const Color us    = p.c;
    ScoreType balance = (type==T_ep ? Values[P_wp+PieceShift] : PieceTools::getAbsValue(p,to)) - threshold + (prom?(Values[P_wq+PieceShift]-Values[P_wp+PieceShift]):0); // The opponent may be able to recapture so this is the best result we can hope for.
    if (balance < 0) return false;
    balance -= Values[nextVictim+PieceShift]; // Now assume the worst possible result: that the opponent can capture our piece for free.
    if (balance >= 0) return true;
    Position p2 = p;
    if (!apply(p2, m, true)) return false;
    bool endOfSEE = false;
    while (!endOfSEE){
        bool validThreatFound = false;
        for ( pp = P_wp ; pp <= P_wk && !validThreatFound ; ++pp){
           BitBoard att = BBTools::pfAttack[pp-1](to, p2.pieces_const(p2.c,pp), p2.occupancy(), ~p2.c);
           if ( !att ) continue; // next piece type
           Square sqAtt = INVALIDSQUARE;
           while (!validThreatFound && att && (sqAtt = popBit(att))) {
              if (PieceTools::getPieceType(p2,to) == P_wk) return us == p2.c; // capture king !
              Position p3 = p2;
              prom = promPossible && pp == P_wp;
              const Move mm = ToMove(sqAtt, to, prom ? T_cappromq : T_capture);
              if (!apply(p3,mm,true)) continue;
              validThreatFound = true;
              nextVictim = prom ? P_wq : pp; // CAREFULL here :: we don't care black or white, always use abs(value) next !!!
              if (prom) balance -= Values[P_wp + PieceShift];
              balance = -balance - 1 - Values[nextVictim+PieceShift]; ///@todo other prom ?
              if (balance >= 0 && nextVictim != P_wk) endOfSEE = true;
              p2 = p3;
           }
        }
        if (!validThreatFound) endOfSEE = true;
    }
    STOP_AND_SUM_TIMER(See)
    return us != p2.c; // we break the above loop when stm loses
}
*/
