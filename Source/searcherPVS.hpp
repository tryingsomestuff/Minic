#pragma once

#include "definition.hpp"

#include "dynamicConfig.hpp"
#include "egt.hpp"
#include "evalConfig.hpp"
#include "moveGen.hpp"
#include "moveSort.hpp"
#include "positionTools.hpp"
#include "searchConfig.hpp"
#include "timeMan.hpp"
#include "tools.hpp"
#include "transposition.hpp"

// pvs inspired by Xiphos
template< bool pvnode, bool canPrune>
ScoreType Searcher::pvs(ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, unsigned int ply, PVList & pv, DepthType & seldepth, bool isInCheck, bool cutNode, const std::vector<MiniMove>* skipMoves){
    if (stopFlag) return STOPSCORE;
    //if ( TimeMan::maxKNodes>0 && (stats.counters[Stats::sid_nodes] + stats.counters[Stats::sid_qnodes])/1000 > TimeMan::maxKNodes) { stopFlag = true; Logging::LogIt(Logging::logInfo) << "stopFlag triggered (nodes limits) in thread " << id(); } ///@todo nodes per move
    if ( (TimeType)std::max(1, (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count()) > getCurrentMoveMs() ){ stopFlag = true; Logging::LogIt(Logging::logInfo) << "stopFlag triggered in thread " << id(); }

    EvalData data;
    if (ply >= MAX_DEPTH - 1 || depth >= MAX_DEPTH - 1) return eval(p, data, *this);

    if ((int)ply > seldepth) seldepth = ply;

    if ( depth <= 0 ) return qsearch<true,pvnode>(alpha,beta,p,ply,seldepth);

    debug_king_cap(p);

    ++stats.counters[Stats::sid_nodes];

    alpha = std::max(alpha, (ScoreType)(-MATE + ply));
    beta  = std::min(beta,  (ScoreType)( MATE - ply + 1));
    if (alpha >= beta) return alpha;

    const bool rootnode = ply == 0;

    if (!rootnode && interiorNodeRecognizer<true, pvnode, true>(p) == MaterialHash::Ter_Draw) return drawScore();

    CMHPtrArray cmhPtr;
    getCMHPtr(p.halfmoves,cmhPtr);

    const bool withoutSkipMove = skipMoves == nullptr;
    Hash pHash = computeHash(p);
    if ( skipMoves ) for (auto it = skipMoves->begin() ; it != skipMoves->end() ; ++it ){ pHash ^= (*it); }
    bool validTTmove = false;
    //bool ttPv = false;
    TT::Entry e;
    if ( TT::getEntry(*this, p, pHash, depth, e)) {
        if ( e.h != 0 && !rootnode && !pvnode && 
             ( (e.b == TT::B_alpha && e.s <= alpha) || (e.b == TT::B_beta  && e.s >= beta) || (e.b == TT::B_exact) ) ) {
            if (!isInCheck && e.m != INVALIDMINIMOVE && Move2Type(e.m) == T_std ) updateTables(*this, p, depth, ply, e.m, e.b, cmhPtr);
            return adjustHashScore(e.s, ply);
        }
    }
    //ttPv = pvnode ;//| e.isPv; ///@todo
    validTTmove = e.h != 0 && e.m != INVALIDMINIMOVE;

#ifdef WITH_SYZYGY
    ScoreType tbScore = 0;
    if ( !rootnode && withoutSkipMove 
         && (countBit(p.allPieces[Co_White]|p.allPieces[Co_Black])) <= SyzygyTb::MAX_TB_MEN && SyzygyTb::probe_wdl(p, tbScore, false) > 0){
       ++stats.counters[Stats::sid_tbHit1];
       if ( abs(tbScore) == SyzygyTb::TB_WIN_SCORE) tbScore += eval(p, data, *this);
       TT::setEntry(*this,pHash,INVALIDMOVE,createHashScore(tbScore,ply),createHashScore(tbScore,ply),TT::B_exact,DepthType(MAX_DEPTH));
       return tbScore;
    }
#endif

    ScoreType evalScore;
    if (isInCheck) evalScore = -MATE + ply;
    else if ( p.lastMove == NULLMOVE && ply > 0 ) evalScore = ScaleScore(EvalConfig::tempo,stack[p.halfmoves-1].data.gp) - stack[p.halfmoves-1].eval; // skip eval if nullmove just applied ///@todo wrong ! gp is 0 here
    else{
        if (e.h != 0){
            ++stats.counters[Stats::sid_ttschits];
            evalScore = e.e;
            const Hash matHash = MaterialHash::getMaterialHash(p.mat);
            if ( matHash ){
               ++stats.counters[Stats::sid_materialTableHits];
               const MaterialHash::MaterialHashEntry & MEntry = MaterialHash::materialHashTable[matHash];
               data.gp = MEntry.gp;
            }
            else{
               ScoreType matScoreW = 0;
               ScoreType matScoreB = 0;
               data.gp = gamePhase(p,matScoreW,matScoreB);
               ++stats.counters[Stats::sid_materialTableMiss];
            }
            ///@todo data.danger is not filled here !!
        }
        else {
            ++stats.counters[Stats::sid_ttscmiss];
            evalScore = eval(p, data, *this);
        }
    }
    stack[p.halfmoves].eval = evalScore; // insert only static eval, never hash score !
    stack[p.halfmoves].data = data;
    bool evalScoreIsHashScore = false;
    if ( !validTTmove) TT::setEntry(*this,pHash,INVALIDMOVE,createHashScore(evalScore,ply),createHashScore(evalScore,ply),TT::B_none,-2); // already insert an eval here in case of pruning ...
    if ( (e.h != 0 && !isInCheck) && ((e.b == TT::B_alpha && e.s < evalScore) || (e.b == TT::B_beta && e.s > evalScore) || (e.b == TT::B_exact)) ) evalScore = adjustHashScore(e.s,ply), evalScoreIsHashScore=true;

    ScoreType bestScore = -MATE + ply;
    MoveList moves;
    bool moveGenerated = false;
    bool capMoveGenerated = false;
    bool futility = false, lmp = false, /*mateThreat = false,*/ historyPruning = false, CMHPruning = false;
    const bool isNotEndGame = p.mat[p.c][M_t]> 0; ///@todo better ?
    const bool improving = (!isInCheck && ply > 1 && stack[p.halfmoves].eval >= stack[p.halfmoves-2].eval);
    DepthType marginDepth = std::max(1,depth-(evalScoreIsHashScore?e.d:0));

    Move refutation = INVALIDMOVE;

    // forward prunings
    if ( !DynamicConfig::mateFinder && canPrune && !isInCheck /*&& !isMateScore(beta)*/ && !pvnode){ ///@todo removing the !isMateScore(beta) is not losing that much elo and allow for better check mate finding ...
        // static null move
        if (SearchConfig::doStaticNullMove && !isMateScore(evalScore) && isNotEndGame && depth <= SearchConfig::staticNullMoveMaxDepth[evalScoreIsHashScore] && evalScore >= beta + SearchConfig::staticNullMoveDepthInit[evalScoreIsHashScore] + SearchConfig::staticNullMoveDepthCoeff[evalScoreIsHashScore] * depth) return ++stats.counters[Stats::sid_staticNullMove], evalScore;

        // razoring
        ScoreType rAlpha = alpha - SearchConfig::razoringMarginDepthInit[evalScoreIsHashScore] - SearchConfig::razoringMarginDepthCoeff[evalScoreIsHashScore]*marginDepth;
        if ( SearchConfig::doRazoring && depth <= SearchConfig::razoringMaxDepth[evalScoreIsHashScore] && evalScore <= rAlpha ){
            ++stats.counters[Stats::sid_razoringTry];
            const ScoreType qScore = qsearch<true,pvnode>(alpha,beta,p,ply,seldepth);
            if ( stopFlag ) return STOPSCORE;
            if ( qScore <= alpha || (depth < 2 && evalScoreIsHashScore) ) return ++stats.counters[Stats::sid_razoring],qScore;
        }

        // null move
        if (SearchConfig::doNullMove && isNotEndGame && withoutSkipMove /*&& evalScore >= beta*/ && evalScore >= stack[p.halfmoves].eval /*&& stack[p.halfmoves].eval >= beta - 32*depth - 30*improving */ &&  ply >= (unsigned int)nullMoveMinPly && depth >= SearchConfig::nullMoveMinDepth) {
            PVList nullPV;
            ++stats.counters[Stats::sid_nullMoveTry];
            const DepthType R = depth / 4 + 3 + std::min((evalScore - beta) / 80, 3); // adaptative
            const ScoreType nullIIDScore = evalScore; // pvs<false, false>(beta - 1, beta, p, std::max(depth/4,1), ply, nullPV, seldepth, isInCheck, !cutNode);
            if (nullIIDScore >= beta /*&& stack[p.halfmoves].eval >= beta + 10 * (depth-improving)*/ ) { ///@todo try to minimize sid_nullMoveTry2 versus sid_nullMove
                TT::Entry nullE;
                const DepthType nullDepth = depth-R;
                TT::getEntry(*this, p, pHash, nullDepth, nullE);
                if (nullE.h == nullHash || nullE.s >= beta ) { // avoid null move search if TT gives a score < beta for the same depth
                    ++stats.counters[Stats::sid_nullMoveTry2];
                    Position pN = p;
                    applyNull(*this,pN);
                    stack[pN.halfmoves].h = pN.h;
                    stack[pN.halfmoves].p = pN;
                    ScoreType nullscore = -pvs<false, false>(-beta, -beta + 1, pN, nullDepth, ply + 1, nullPV, seldepth, isInCheck, !cutNode);
                    if (stopFlag) return STOPSCORE;
                    TT::Entry nullEThreat;
                    TT::getEntry(*this, pN, computeHash(pN), 0, nullEThreat);
                    if ( nullEThreat.h != nullHash ) refutation = nullEThreat.m;
                    //if (isMatedScore(nullscore)) mateThreat = true;
                    if (nullscore >= beta){
                       if (depth <= SearchConfig::nullMoveVerifDepth || nullMoveMinPly>0) return ++stats.counters[Stats::sid_nullMove], isMateScore(nullscore) ? beta : nullscore;
                       ++stats.counters[Stats::sid_nullMoveTry3];
                       nullMoveMinPly = ply + 3*nullDepth/4;
                       nullscore = pvs<false, false>(beta - 1, beta, p, nullDepth, ply+1, nullPV, seldepth, isInCheck, !cutNode);
                       nullMoveMinPly = 0;
                       if (stopFlag) return STOPSCORE;
                       if (nullscore >= beta ) return ++stats.counters[Stats::sid_nullMove2], nullscore;
                    }
                }
            }
        }

        // ProbCut
        if ( SearchConfig::doProbcut && depth >= SearchConfig::probCutMinDepth && !isMateScore(beta)){
          ++stats.counters[Stats::sid_probcutTry];
          int probCutCount = 0;
          const ScoreType betaPC = beta + SearchConfig::probCutMargin;
          MoveGen::generate<MoveGen::GP_cap>(p,moves);
          MoveSorter::sort(*this,moves,p,data.gp,ply,cmhPtr,true,isInCheck,e.h?&e:NULL);
          capMoveGenerated = true;
          for (auto it = moves.begin() ; it != moves.end() && probCutCount < SearchConfig::probCutMaxMoves /*+ 2*cutNode*/; ++it){
            if ( (validTTmove && sameMove(e.m, *it)) || isBadCap(*it) ) continue; // skip TT move if quiet or bad captures
            Position p2 = p;
            if ( ! apply(p2,*it) ) continue;
            ++probCutCount;
            ScoreType scorePC = -qsearch<true,pvnode>(-betaPC, -betaPC + 1, p2, ply + 1, seldepth);
            PVList pcPV;
            if (stopFlag) return STOPSCORE;
            if (scorePC >= betaPC) ++stats.counters[Stats::sid_probcutTry2], scorePC = -pvs<false,true>(-betaPC,-betaPC+1,p2,depth-SearchConfig::probCutMinDepth+1,ply+1,pcPV,seldepth, isAttacked(p2, kingSquare(p2)), !cutNode);
            if (stopFlag) return STOPSCORE;
            if (scorePC >= betaPC) return ++stats.counters[Stats::sid_probcut], scorePC;
          }
        }
    }

    // IID
    if ( (e.h == nullHash /*|| e.d < depth/4*/) && ((pvnode && depth >= SearchConfig::iidMinDepth) || (cutNode && depth >= SearchConfig::iidMinDepth2)) ){ ///@todo try with cutNode only ?
        ++stats.counters[Stats::sid_iid];
        PVList iidPV;
        pvs<pvnode,false>(alpha,beta,p,/*pvnode?depth-2:*/depth/2,ply,iidPV,seldepth,isInCheck,cutNode,skipMoves);
        if (stopFlag) return STOPSCORE;
        validTTmove = TT::getEntry(*this, p, pHash, depth, e) && e.h != 0 && e.m != INVALIDMINIMOVE;
    }

    killerT.killers[ply+1][0] = killerT.killers[ply+1][1] = 0;

    // LMP
    if (!rootnode && SearchConfig::doLMP && depth <= SearchConfig::lmpMaxDepth) lmp = true;
    // futility
    const ScoreType futilityScore = alpha - SearchConfig::futilityDepthInit[evalScoreIsHashScore] - SearchConfig::futilityDepthCoeff[evalScoreIsHashScore]*depth;
    if (!rootnode && SearchConfig::doFutility && depth <= SearchConfig::futilityMaxDepth[evalScoreIsHashScore] && evalScore <= futilityScore) futility = true;
    // history pruning
    if (!rootnode && SearchConfig::doHistoryPruning && isNotEndGame && depth < SearchConfig::historyPruningMaxDepth) historyPruning = true;
    // CMH pruning
    if (!rootnode && SearchConfig::doCMHPruning && isNotEndGame && depth < SearchConfig::CMHMaxDepth) CMHPruning = true;

    int validMoveCount = 0;
    Move bestMove = INVALIDMOVE;
    TT::Bound hashBound = TT::B_alpha;
    bool ttMoveIsCapture = false;
    //bool ttMoveSingularExt = false;

    stack[p.halfmoves].threat = refutation;

    // try the tt move before move generation (if not skipped move)
    if ( e.h != 0 && validTTmove && !isSkipMove(e.m,skipMoves)) { // should be the case thanks to iid at pvnode
        bestMove = e.m; // in order to preserve tt move for alpha bound entry
        Position p2 = p;
        if ( apply(p2, e.m)) {
            TT::prefetch(computeHash(p2));
            const Square to = Move2To(e.m);
            validMoveCount++;
            PVList childPV;
            stack[p2.halfmoves].h = p2.h;
            stack[p2.halfmoves].p = p2; ///@todo another expensive copy !!!!
            const bool isCheck = isAttacked(p2, kingSquare(p2));
            if ( isCapture(e.m) ) ttMoveIsCapture = true;
            const bool isQuiet = Move2Type(e.m) == T_std;
            const bool isAdvancedPawnPush = PieceTools::getPieceType(p,Move2From(e.m)) == P_wp && (SQRANK(to) > 5 || SQRANK(to) < 2);
            // extensions
            DepthType extension = 0;
            if ( DynamicConfig::level>80){
               if (!extension && pvnode && isInCheck) ++stats.counters[Stats::sid_checkExtension],++extension;
               if (!extension && isCastling(e.m) ) ++stats.counters[Stats::sid_castlingExtension],++extension;
               if (!extension && ply > 1 && VALIDMOVE(stack[p.halfmoves].threat) && VALIDMOVE(stack[p.halfmoves - 2].threat) && (sameMove(stack[p.halfmoves].threat, stack[p.halfmoves - 2].threat) || (Move2To(stack[p.halfmoves].threat) == Move2To(stack[p.halfmoves - 2].threat) && isCapture(stack[p.halfmoves].threat)))) ++stats.counters[Stats::sid_BMExtension], ++extension;
               //if (!extension && mateThreat) ++stats.counters[Stats::sid_mateThreatExtension],++extension;
               //if (!extension && VALIDMOVE(p.lastMove) && Move2Type(p.lastMove) == T_capture && Move2To(e.m) == Move2To(p.lastMove)) ++stats.counters[Stats::sid_recaptureExtension],++extension; // recapture
               //if (!extension && isCheck ) ++stats.counters[Stats::sid_checkExtension2],++extension; // we give check with a non risky move
               /*
               if (!extension && isQuiet) {
               const int pp = (p.b[Move2From(e.m)] + PieceShift) * 64 + Move2To(e.m);
               if (cmhPtr[0] && cmhPtr[1] && cmhPtr[0][pp] >= MAX_HISTORY / 2 && cmhPtr[1][pp] >= MAX_HISTORY / 2) ++stats.counters[Stats::sid_CMHExtension], ++extension;
               }
               */
               if (!extension && isAdvancedPawnPush ) {
                   const BitBoard pawns[2] = { p2.pieces_const<P_wp>(Co_White), p2.pieces_const<P_wp>(Co_Black) };
                   const BitBoard passed[2] = { BBTools::pawnPassed<Co_White>(pawns[Co_White], pawns[Co_Black]), BBTools::pawnPassed<Co_Black>(pawns[Co_Black], pawns[Co_White]) };
                   if ( SquareToBitboard(to) & passed[p.c] ) ++stats.counters[Stats::sid_pawnPushExtension], ++extension;
               }
               if (!extension && pvnode && (p.pieces_const<P_wq>(p.c) && isQuiet && PieceTools::getPieceType(p, Move2From(e.m)) == P_wq && isAttacked(p, BBTools::SquareFromBitBoard(p.pieces_const<P_wq>(p.c)))) && SEE_GE(p, e.m, 0)) ++stats.counters[Stats::sid_queenThreatExtension], ++extension;
               if (!extension && withoutSkipMove && depth >= SearchConfig::singularExtensionDepth && !rootnode && !isMateScore(e.s) && e.b == TT::B_beta && e.d >= depth - 3){
                   const ScoreType betaC = e.s - 2*depth;
                   PVList sePV;
                   DepthType seSeldetph = 0;
                   std::vector<MiniMove> skip({Move2MiniMove(e.m)});
                   const ScoreType score = pvs<false,false>(betaC - 1, betaC, p, depth/2, ply, sePV, seSeldetph, isInCheck, cutNode, &skip);
                   if (stopFlag) return STOPSCORE;
                   if (score < betaC) {
                       ++stats.counters[Stats::sid_singularExtension],++extension;
                       if ( score < betaC - std::min(4*depth,36)) ++stats.counters[Stats::sid_singularExtension2],/*ttMoveSingularExt=true,*/++extension;
                   }
                   else if ( score >= beta && betaC >= beta){
                       return ++stats.counters[Stats::sid_singularExtension3],score;
                   }
               }
            }
            const ScoreType ttScore = -pvs<pvnode,true>(-beta, -alpha, p2, depth - 1 + extension, ply + 1, childPV, seldepth, isCheck, !cutNode);
            if (stopFlag) return STOPSCORE;
            if (rootnode) rootScores.push_back({e.m,ttScore});
            if (rootnode) previousBest = e.m;
            if ( ttScore > bestScore ){
                bestScore = ttScore;
                bestMove = e.m;
                if (ttScore > alpha) {
                    hashBound = TT::B_exact;
                    if (pvnode) updatePV(pv, e.m, childPV);
                    if (ttScore >= beta) {
                        ++stats.counters[Stats::sid_ttbeta];
                        if ( !isInCheck && isQuiet ) updateTables(*this, p, depth + (ttScore > (beta+80)), ply, e.m, TT::B_beta, cmhPtr);
                        TT::setEntry(*this,pHash,e.m,createHashScore(ttScore,ply),createHashScore(evalScore,ply),TT::B_beta,depth);
                        return ttScore;
                    }
                    ++stats.counters[Stats::sid_ttalpha];
                    alpha = ttScore;
                }
            }
            else if ( rootnode && !isInCheck && ttScore < alpha - SearchConfig::failLowRootMargin){
                return alpha - SearchConfig::failLowRootMargin;
            }
        }
    }

#ifdef WITH_SYZYGY
    if (rootnode && withoutSkipMove && (countBit(p.allPieces[Co_White] | p.allPieces[Co_Black])) <= SyzygyTb::MAX_TB_MEN) {
        ScoreType tbScore = 0;
        if (SyzygyTb::probe_root(*this, p, tbScore, moves) < 0) { // only good moves if TB success
            if (capMoveGenerated) MoveGen::generate<MoveGen::GP_quiet>(p, moves, true);
            else                  MoveGen::generate<MoveGen::GP_all>  (p, moves, false);
        }
        else ++stats.counters[Stats::sid_tbHit2];
        moveGenerated = true;
    }
#endif

    ScoreType score = -MATE + ply;

    if (!moveGenerated) {
        if (capMoveGenerated) MoveGen::generate<MoveGen::GP_quiet>(p, moves, true);
        else                  MoveGen::generate<MoveGen::GP_all>  (p, moves, false);
    }
    if (moves.empty()) return isInCheck ? -MATE + ply : 0;
    MoveSorter::sort(*this, moves, p, data.gp, ply, cmhPtr, true, isInCheck, e.h?&e:NULL, refutation != INVALIDMOVE && isCapture(Move2Type(refutation)) ? refutation : INVALIDMOVE);

    for(auto it = moves.begin() ; it != moves.end() && !stopFlag ; ++it){
        if (isSkipMove(*it,skipMoves)) continue; // skipmoves
        if (validTTmove && sameMove(e.m, *it)) continue; // already tried
        Position p2 = p;
        if ( ! apply(p2,*it) ) continue;
        TT::prefetch(computeHash(p2));
        const Square to = Move2To(*it);
        if (p.c == Co_White && to == p.king[Co_Black]) return MATE - ply + 1;
        if (p.c == Co_Black && to == p.king[Co_White]) return MATE - ply + 1;
        validMoveCount++;
        const bool firstMove = validMoveCount == 1;
        PVList childPV;
        stack[p2.halfmoves].h = p2.h;
        stack[p2.halfmoves].p = p2; ///@todo another expensive copy !!!!
        const bool isCheck = isAttacked(p2, kingSquare(p2));
        bool isAdvancedPawnPush = PieceTools::getPieceType(p,Move2From(*it)) == P_wp && (SQRANK(to) > 5 || SQRANK(to) < 2);
        // extensions
        DepthType extension = 0;
        const bool isQuiet = Move2Type(*it) == T_std;
        if ( DynamicConfig::level>80){
           if (!extension && pvnode && isInCheck) ++stats.counters[Stats::sid_checkExtension],++extension; // we are in check (extension)
           if (!extension && isCastling(*it) ) ++stats.counters[Stats::sid_castlingExtension],++extension;
           if (!extension && ply > 1 && stack[p.halfmoves].threat != INVALIDMOVE && stack[p.halfmoves - 2].threat != INVALIDMOVE && (sameMove(stack[p.halfmoves].threat, stack[p.halfmoves - 2].threat) || (Move2To(stack[p.halfmoves].threat) == Move2To(stack[p.halfmoves - 2].threat) && isCapture(stack[p.halfmoves].threat)))) ++stats.counters[Stats::sid_BMExtension], ++extension;
           //if (!extension && mateThreat && depth <= 4) ++stats.counters[Stats::sid_mateThreatExtension],++extension;
           //if (!extension && VALIDMOVE(p.lastMove) && !isBadCap(*it) && Move2Type(p.lastMove) == T_capture && Move2To(*it) == Move2To(p.lastMove)) ++stats.counters[Stats::sid_recaptureExtension],++extension; //recapture
           //if (!extension && isCheck && !isBadCap(*it)) ++stats.counters[Stats::sid_checkExtension2],++extension; // we give check with a non risky move
           if (!extension && !firstMove && isQuiet) {
               const int pp = (p.board_const(Move2From(*it)) + PieceShift) * 64 + Move2To(*it);
               if (cmhPtr[0] && cmhPtr[1] && cmhPtr[0][pp] >= MAX_HISTORY / 2 && cmhPtr[1][pp] >= MAX_HISTORY / 2) ++stats.counters[Stats::sid_CMHExtension], ++extension;
           }
           if (!extension && isAdvancedPawnPush /*&& (killerT.isKiller(*it, ply) || !isBadCap(*it))*/){
               const BitBoard pawns[2] = { p2.pieces_const<P_wp>(Co_White), p2.pieces_const<P_wp>(Co_Black) };
               const BitBoard passed[2] = { BBTools::pawnPassed<Co_White>(pawns[Co_White], pawns[Co_Black]), BBTools::pawnPassed<Co_Black>(pawns[Co_Black], pawns[Co_White]) };
               if ( (SquareToBitboard(to) & passed[p.c]) ) ++stats.counters[Stats::sid_pawnPushExtension], ++extension;
           }
           if (!extension && pvnode && firstMove && (p.pieces_const<P_wq>(p.c) && isQuiet && Move2Type(*it) == T_std && PieceTools::getPieceType(p, Move2From(*it)) == P_wq && isAttacked(p, BBTools::SquareFromBitBoard(p.pieces_const<P_wq>(p.c)))) && SEE_GE(p, *it, 0)) ++stats.counters[Stats::sid_queenThreatExtension], ++extension;
        }
        // pvs
        if (validMoveCount < (2/*+2*rootnode*/) || !SearchConfig::doPVS ) score = -pvs<pvnode,true>(-beta,-alpha,p2,depth-1+extension,ply+1,childPV,seldepth,isCheck,!cutNode);
        else{
            // reductions & prunings
            DepthType reduction = 0;
            const bool isPrunable           = /*isNotEndGame &&*/ !isAdvancedPawnPush && !isMateScore(alpha) && !DynamicConfig::mateFinder && !killerT.isKiller(*it,ply);
            const bool isReductible         = /*isNotEndGame &&*/ !isAdvancedPawnPush && !DynamicConfig::mateFinder;
            const bool noCheck              = !isInCheck && !isCheck;
            const bool isPrunableStd        = isPrunable && isQuiet;
            const bool isPrunableStdNoCheck = isPrunableStd && noCheck;
            const bool isEmergencyDefence   = false; //moveDifficulty == MoveDifficultyUtil::MD_hardDefense; ///@todo use this
            //const bool isEmergencyAttack    = moveDifficulty == MoveDifficultyUtil::MD_hardAttack; ///@todo use this
            const bool isPrunableCap        = isPrunable && Move2Type(*it) == T_capture && isBadCap(*it) && noCheck ;
            const bool isDangerPrune        = data.danger[p.c] > SearchConfig::dangerLimitPruning[0] || data.danger[~p.c] > SearchConfig::dangerLimitPruning[1];
            const bool isDangerRed          = data.danger[p.c] > SearchConfig::dangerLimitReduction[0] || data.danger[~p.c] > SearchConfig::dangerLimitReduction[1];
            const float dangerPruneFactor   = ((1.f+data.danger[p.c])/SearchConfig::dangerLimitPruning[0] + (1.f+data.danger[~p.c])/SearchConfig::dangerLimitPruning[1])/2;
            if ( isDangerPrune) ++stats.counters[Stats::sid_dangerPrune];
            if ( isDangerRed)   ++stats.counters[Stats::sid_dangerReduce];
            // futility
            if (futility && isPrunableStdNoCheck) {++stats.counters[Stats::sid_futility]; continue;}
            // LMP
            if (lmp && isPrunableStdNoCheck && validMoveCount > (1/*+dangerPruneFactor*/)*SearchConfig::lmpLimit[improving][depth] + 2*isEmergencyDefence ) {++stats.counters[Stats::sid_lmp]; continue;}
            // History pruning (with CMH)
            if (historyPruning && isPrunableStdNoCheck && !isEmergencyDefence && Move2Score(*it) < SearchConfig::historyPruningThresholdInit + depth*SearchConfig::historyPruningThresholdDepth) {++stats.counters[Stats::sid_historyPruning]; continue;}
            // CMH pruning alone
            if (CMHPruning && isPrunableStdNoCheck && !isEmergencyDefence){
              const int pp = (p.board_const(Move2From(*it))+PieceShift) * 64 + Move2To(*it);
              if ((!cmhPtr[0] || cmhPtr[0][pp] < 0) && (!cmhPtr[1] || cmhPtr[1][pp] < 0)) { ++stats.counters[Stats::sid_CMHPruning]; continue;}
            }
            // SEE (capture)
            if (isPrunableCap){
               if (futility) {++stats.counters[Stats::sid_see]; continue;}
               else if ( !rootnode && badCapScore(*it) < -(1+dangerPruneFactor*dangerPruneFactor)*100*(depth+isEmergencyDefence) /*!SEE_GE(p,*it,-100*depth)*/) {++stats.counters[Stats::sid_see2]; continue;}
            }
            // LMR
            if (SearchConfig::doLMR && (isReductible && isQuiet ) && depth >= SearchConfig::lmrMinDepth ){
                ++stats.counters[Stats::sid_lmr];
                reduction = SearchConfig::lmrReduction[std::min((int)depth,MAX_DEPTH-1)][std::min(validMoveCount,MAX_DEPTH)];
                reduction += !improving;
                reduction += ttMoveIsCapture;
                //reduction += (cutNode && evalScore - SearchConfig::failHighReductionThreshold[evalScoreIsHashScore] > beta);
                reduction -= (2 * Move2Score(*it)) / MAX_HISTORY; //history reduction/extension (beware killers and counter are scored above history max, so reduced less
                if ( reduction > 0){
                    if      ( pvnode           ) --reduction;
                    else if ( isDangerRed      ) --reduction;
                    else if ( !noCheck         ) --reduction;
                    //else if ( ttMoveSingularExt) --reduction;
                    if ( reduction>1 && isEmergencyDefence ) --reduction;
                }
                // never extend more than reduce
                if ( extension - reduction > 0 ) reduction = extension;
                if ( reduction >= depth - 1 + extension ) reduction = depth - 1 + extension - 1;
            }
            const DepthType nextDepth = depth-1-reduction+extension;
            // SEE (quiet)
            if ( isPrunableStdNoCheck && /*!rootnode &&*/ !SEE_GE(p,*it,-15*(1/*+isDangerPrune*/)*(nextDepth+isEmergencyDefence)*nextDepth)) {
                ++stats.counters[Stats::sid_seeQuiet]; 
                continue;
            }
            // PVS
            score = -pvs<false,true>(-alpha-1,-alpha,p2,nextDepth,ply+1,childPV,seldepth,isCheck,true);
            if ( reduction > 0 && score > alpha ){ 
                ++stats.counters[Stats::sid_lmrFail]; childPV.clear(); 
                score = -pvs<false,true>(-alpha-1,-alpha,p2,depth-1+extension,ply+1,childPV,seldepth,isCheck,!cutNode); 
            }
            if ( pvnode && score > alpha && (rootnode || score < beta) ){ 
                ++stats.counters[Stats::sid_pvsFail]; childPV.clear(); 
                score = -pvs<true ,true>(-beta   ,-alpha,p2,depth-1+extension,ply+1,childPV,seldepth,isCheck,false); 
            } // potential new pv node
        }
        if (stopFlag) return STOPSCORE;
        if (rootnode) rootScores.push_back({*it,score});
        if (rootnode) previousBest = *it;
        if ( score > bestScore ){
            bestScore = score;
            bestMove = *it;
            //bestScoreUpdated = true;
            if ( score > alpha ){
                if (pvnode) updatePV(pv, *it, childPV);
                //alphaUpdated = true;
                alpha = score;
                hashBound = TT::B_exact;
                if ( score >= beta ){
                    if ( !isInCheck && isQuiet ){
                        updateTables(*this, p, depth + (score>beta+80), ply, *it, TT::B_beta, cmhPtr);
                        for(auto it2 = moves.begin() ; it2 != moves.end() && !sameMove(*it2,*it); ++it2) {
                            if ( Move2Type(*it2) == T_std ) historyT.update<-1>(depth + (score > (beta + 80)), *it2, p, cmhPtr);
                        }
                    }
                    hashBound = TT::B_beta;
                    break;
                }
            }
        }
        else if ( rootnode && !isInCheck && firstMove && score < alpha - SearchConfig::failLowRootMargin){
            return alpha - SearchConfig::failLowRootMargin;
        }
    }

    if ( validMoveCount==0 ) return (isInCheck || !withoutSkipMove)?-MATE + ply : 0;
    TT::setEntry(*this,pHash,bestMove,createHashScore(bestScore,ply),createHashScore(evalScore,ply),hashBound,depth);
    return bestScore;
}
