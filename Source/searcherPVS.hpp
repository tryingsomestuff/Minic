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

#define PERIODICCHECK 1024ull

// pvs inspired by Xiphos
template< bool pvnode>
ScoreType Searcher::pvs(ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, unsigned int ply, PVList & pv, DepthType & seldepth, bool isInCheck, bool cutNode, bool canPrune, const std::vector<MiniMove>* skipMoves){
    if (stopFlag) return STOPSCORE;
    if ( isMainThread() ){
        static int periodicCheck = 0;
        if ( periodicCheck == 0 ){
            periodicCheck = (TimeMan::maxNodes > 0) ? std::min(TimeMan::maxNodes/PERIODICCHECK,PERIODICCHECK) : PERIODICCHECK;
            const Counter nodeCount = ThreadPool::instance().counter(Stats::sid_nodes) + ThreadPool::instance().counter(Stats::sid_qnodes);
            if ( TimeMan::maxNodes > 0 && nodeCount > TimeMan::maxNodes) { 
                stopFlag = true; 
                Logging::LogIt(Logging::logInfo) << "stopFlag triggered (nodes limits) in thread " << id(); 
                return STOPSCORE;
            } 
            if ( (TimeType)std::max(1, (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count()) > getCurrentMoveMs() ){ 
                stopFlag = true; 
                Logging::LogIt(Logging::logInfo) << "stopFlag triggered in thread " << id(); 
                return STOPSCORE;
            }
        }
        --periodicCheck;
    }

    EvalData data;
    if (ply >= MAX_DEPTH - 1 || depth >= MAX_DEPTH - 1) return eval(p, data, *this);

    if ( depth <= 0 ) return qsearch(alpha,beta,p,ply,seldepth,0,true,pvnode,isInCheck);

    seldepth = std::max((DepthType)ply,seldepth);
    ++stats.counters[Stats::sid_nodes];

    debug_king_cap(p);

    alpha = std::max(alpha, (ScoreType)(-MATE + ply));
    beta  = std::min(beta,  (ScoreType)( MATE - ply + 1));
    if (alpha >= beta) return alpha;

    const bool rootnode = ply == 0;

    if (!rootnode && interiorNodeRecognizer<true, pvnode, true>(p) == MaterialHash::Ter_Draw) return drawScore();

    CMHPtrArray cmhPtr;
    getCMHPtr(p.halfmoves,cmhPtr);

    const bool withoutSkipMove = skipMoves == nullptr;
    Hash pHash = computeHash(p);
    // consider skipmove(s) in position hash
    if ( skipMoves ) for (auto it = skipMoves->begin() ; it != skipMoves->end() ; ++it ){ pHash ^= (*it); }

    // probe TT
    TT::Entry e;
    bool ttDepthOk = TT::getEntry(*this, p, pHash, depth, e);
    TT::Bound bound = TT::Bound(e.b & ~TT::B_allFlags);
    if (ttDepthOk) { // if depth of TT entry is enough
        if ( !rootnode && !pvnode && 
             ( (bound == TT::B_alpha && e.s <= alpha) || (bound == TT::B_beta  && e.s >= beta) || (bound == TT::B_exact) ) ) {
            // increase history bonus of this move
            if (!isInCheck && e.m != INVALIDMINIMOVE && Move2Type(e.m) == T_std ) updateTables(*this, p, depth, ply, e.m, bound, cmhPtr);
            return adjustHashScore(e.s, ply);
        }
    }
    // if entry hash is not null and entry move is valid, this is a valid TT move (we don't care about depth here !)
    bool ttHit = e.h != nullHash;
    bool validTTmove = ttHit && e.m != INVALIDMINIMOVE;
    bool ttPV = pvnode || (validTTmove && (e.b&TT::B_ttFlag)); ///@todo store more things in TT bound ...
    bool ttIsCheck = validTTmove && (e.b&TT::B_isCheckFlag);
    //bool formerPV = ttPV && !pvnode;
    
#ifdef WITH_SYZYGY
    ScoreType tbScore = 0;
    if ( !rootnode && withoutSkipMove 
         && (countBit(p.allPieces[Co_White]|p.allPieces[Co_Black])) <= SyzygyTb::MAX_TB_MEN && SyzygyTb::probe_wdl(p, tbScore, false) > 0){
       ++stats.counters[Stats::sid_tbHit1];
       if ( abs(tbScore) == SyzygyTb::TB_WIN_SCORE) tbScore += eval(p, data, *this);
       // store TB hits into TT (without associated move, but with max depth)
       TT::setEntry(*this,pHash,INVALIDMOVE,createHashScore(tbScore,ply),createHashScore(tbScore,ply),TT::B_none,DepthType(MAX_DEPTH));
       return tbScore;
    }
#endif

    // get a static score for the position.
    ScoreType evalScore;
    if (isInCheck) evalScore = -MATE + ply;
    else if ( p.lastMove == NULLMOVE && ply > 0 ) evalScore = 2*ScaleScore(EvalConfig::tempo,stack[p.halfmoves-1].data.gp) - stack[p.halfmoves-1].eval; // skip eval if nullmove just applied ///@todo wrong ! gp is 0 here so tempoMG must be == tempoEG
    else{
        if (ttHit){ // if we had a TT hit (with or without associated move), we can use its eval instead of calling eval()
            ++stats.counters[Stats::sid_ttschits];
            evalScore = e.e;
            // look for a material match (to get game phase)
            const Hash matHash = MaterialHash::getMaterialHash(p.mat);
            if ( matHash ){
               ++stats.counters[Stats::sid_materialTableHits];
               const MaterialHash::MaterialHashEntry & MEntry = MaterialHash::materialHashTable[matHash];
               data.gp = MEntry.gp;
            }
            else{ // if no match, compute game phase (this does not happend very often ...)
               ScoreType matScoreW = 0;
               ScoreType matScoreB = 0;
               data.gp = gamePhase(p,matScoreW,matScoreB);
               ++stats.counters[Stats::sid_materialTableMiss];
            }
            ///@todo data.danger, data.mob are not filled in case of TT hit !!
        }
        else { // if no TT hit call evaluation !
            ++stats.counters[Stats::sid_ttscmiss];
            evalScore = eval(p, data, *this);
        }
    }
    stack[p.halfmoves].eval = evalScore; // insert only static eval, never hash score !
    stack[p.halfmoves].data = data;

    bool evalScoreIsHashScore = false;
    // if no TT hit yet, we insert an eval without a move here in case of forward pruning (depth is negative, bound is none) ...
    if ( !ttHit ) TT::setEntry(*this,pHash,INVALIDMOVE,createHashScore(evalScore,ply),createHashScore(evalScore,ply),TT::B_none,-2); 
    // if TT hit, we can use its score as a best draft
    if ( ttHit && !isInCheck && ((bound == TT::B_alpha && e.s < evalScore) || (bound == TT::B_beta && e.s > evalScore) || (bound == TT::B_exact)) ) evalScore = adjustHashScore(e.s,ply), evalScoreIsHashScore=true;

    ScoreType bestScore = -MATE + ply;
    MoveList moves;
    bool moveGenerated = false;
    bool capMoveGenerated = false;
    bool futility = false, lmp = false, /*mateThreat = false,*/ historyPruning = false, CMHPruning = false;
    const bool isNotEndGame = p.mat[p.c][M_t]> 0; ///@todo better ?
    const bool improving = (!isInCheck && ply > 1 && stack[p.halfmoves].eval >= stack[p.halfmoves-2].eval);
    DepthType marginDepth = std::max(1,depth-(evalScoreIsHashScore?e.d:0)); // a depth that take TT depth into account
    Move refutation = INVALIDMOVE;

    // forward prunings
    if ( !DynamicConfig::mateFinder && canPrune && !isInCheck /*&& !isMateScore(beta)*/ && !pvnode){ ///@todo removing the !isMateScore(beta) is not losing that much elo and allow for better check mate finding ...
        // static null move
        if (SearchConfig::doStaticNullMove && !isMateScore(evalScore) && isNotEndGame && depth <= SearchConfig::staticNullMoveMaxDepth[evalScoreIsHashScore] && evalScore >= beta + SearchConfig::staticNullMoveDepthInit[evalScoreIsHashScore] + SearchConfig::staticNullMoveDepthCoeff[evalScoreIsHashScore] * depth) return ++stats.counters[Stats::sid_staticNullMove], evalScore;

        // razoring
        ScoreType rAlpha = alpha - SearchConfig::razoringMarginDepthInit[evalScoreIsHashScore] - SearchConfig::razoringMarginDepthCoeff[evalScoreIsHashScore]*marginDepth;
        if ( SearchConfig::doRazoring && depth <= SearchConfig::razoringMaxDepth[evalScoreIsHashScore] && evalScore <= rAlpha ){
            ++stats.counters[Stats::sid_razoringTry];
            const ScoreType qScore = qsearch(alpha,beta,p,ply,seldepth,0,true,pvnode,isInCheck);
            if ( stopFlag ) return STOPSCORE;
            if ( qScore <= alpha || (depth < 2 && evalScoreIsHashScore) ) return ++stats.counters[Stats::sid_razoring],qScore;
        }

        // null move (warning, mobility info is only available if no TT hit)
        if (SearchConfig::doNullMove && (isNotEndGame || data.mobility[p.c] > 4) && withoutSkipMove && 
            depth >= SearchConfig::nullMoveMinDepth && 
            evalScore >= beta && 
            evalScore >= stack[p.halfmoves].eval && 
            stack[p.halfmoves].p.lastMove != NULLMOVE && 
            //stack[p.halfmoves-1].p.lastMove != NULLMOVE &&
            /*stack[p.halfmoves].eval >= beta - 32*depth - 30*improving && */
            ply >= (unsigned int)nullMoveMinPly ) {
            PVList nullPV;
            ++stats.counters[Stats::sid_nullMoveTry];
            const DepthType R = depth / 4 + 3 + std::min((evalScore - beta) / 150, 5); // adaptative
            const ScoreType nullIIDScore = evalScore; // pvs<false, false>(beta - 1, beta, p, std::max(depth/4,1), ply, nullPV, seldepth, isInCheck, !cutNode);
            if (nullIIDScore >= beta /*&& stack[p.halfmoves].eval >= beta + 10 * (depth-improving)*/ ) { ///@todo try to minimize sid_nullMoveTry2 versus sid_nullMove
                TT::Entry nullE;
                const DepthType nullDepth = depth-R;
                TT::getEntry(*this, p, pHash, nullDepth, nullE);
                if (nullE.h == nullHash || nullE.s >= beta ) { // avoid null move search if TT gives a score < beta for the same depth ///@todo check this again !
                    ++stats.counters[Stats::sid_nullMoveTry2];
                    Position pN = p;
                    applyNull(*this,pN);
                    stack[pN.halfmoves].h = pN.h;
                    stack[pN.halfmoves].p = pN;
                    ScoreType nullscore = -pvs<false>(-beta, -beta + 1, pN, nullDepth, ply + 1, nullPV, seldepth, isInCheck, !cutNode, false);
                    if (stopFlag) return STOPSCORE;
                    TT::Entry nullEThreat;
                    TT::getEntry(*this, pN, computeHash(pN), 0, nullEThreat);
                    if ( nullEThreat.h != nullHash && nullEThreat.m != INVALIDMINIMOVE ) refutation = nullEThreat.m;
                    //if (isMatedScore(nullscore)) mateThreat = true;
                    if (nullscore >= beta){
                        /*
                       if ( (!isNotEndGame || depth > SearchConfig::nullMoveVerifDepth) && nullMoveMinPly == 0){
                          ++stats.counters[Stats::sid_nullMoveTry3];
                          nullMoveMinPly = ply + 3*nullDepth/4;
                          nullscore = pvs<false, false>(beta - 1, beta, p, nullDepth, ply+1, nullPV, seldepth, isInCheck, !cutNode);
                          nullMoveMinPly = 0;
                          if (stopFlag) return STOPSCORE;
                          if (nullscore >= beta ) return ++stats.counters[Stats::sid_nullMove2], nullscore;
                       }
                       else{
                           */
                          return ++stats.counters[Stats::sid_nullMove], (isMateScore(nullscore) ? beta : nullscore);
                       //}
                    }
                }
            }
        }

        // ProbCut
        if ( SearchConfig::doProbcut && depth >= SearchConfig::probCutMinDepth && !isMateScore(beta)){
          ++stats.counters[Stats::sid_probcutTry];
          int probCutCount = 0;
          const ScoreType betaPC = beta + SearchConfig::probCutMargin;
          capMoveGenerated = true;
          MoveGen::generate<MoveGen::GP_cap>(p,moves);
#ifdef USE_PARTIAL_SORT
          MoveSorter::score(*this,moves,p,data.gp,ply,cmhPtr,true,isInCheck,validTTmove?&e:NULL);
          size_t offset = 0;
          const Move * it = nullptr;
          while( (it = MoveSorter::pickNext(moves,offset)) && probCutCount < SearchConfig::probCutMaxMoves /*+ 2*cutNode*/){
#else
          MoveSorter::scoreAndSort(*this,moves,p,data.gp,ply,cmhPtr,true,isInCheck,validTTmove?&e:NULL);
          for (auto it = moves.begin() ; it != moves.end() && probCutCount < SearchConfig::probCutMaxMoves /*+ 2*cutNode*/; ++it){
#endif
            if ( (validTTmove && sameMove(e.m, *it)) || isBadCap(*it) ) continue; // skip TT move if quiet or bad captures
            Position p2 = p;
            if ( ! apply(p2,*it) ) continue;
            ++probCutCount;
            ScoreType scorePC = -qsearch(-betaPC, -betaPC + 1, p2, ply + 1, seldepth,0,true,pvnode);
            PVList pcPV;
            if (stopFlag) return STOPSCORE;
            if (scorePC >= betaPC) ++stats.counters[Stats::sid_probcutTry2], scorePC = -pvs<false>(-betaPC,-betaPC+1,p2,depth-SearchConfig::probCutMinDepth+1,ply+1,pcPV,seldepth, isAttacked(p2, kingSquare(p2)), !cutNode, true);
            if (stopFlag) return STOPSCORE;
            if (scorePC >= betaPC) return ++stats.counters[Stats::sid_probcut], scorePC;
          }
        }
    }

    // IID
    if ( (!validTTmove /*|| e.d < depth-4*/) && ((pvnode && depth >= SearchConfig::iidMinDepth) || (cutNode && depth >= SearchConfig::iidMinDepth2)) ){ ///@todo try with cutNode only ?
        ++stats.counters[Stats::sid_iid];
        PVList iidPV;
        pvs<pvnode>(alpha,beta,p,depth/2,ply,iidPV,seldepth,isInCheck,cutNode,false,skipMoves);
        if (stopFlag) return STOPSCORE;
        TT::getEntry(*this, p, pHash, 0, e);
        ttHit = e.h != nullHash;
        validTTmove = ttHit && e.m != INVALIDMINIMOVE;
        bound = TT::Bound(e.b & ~TT::B_allFlags);
        ttPV = pvnode || (ttHit && (e.b&TT::B_ttFlag));
        ttIsCheck = validTTmove && (e.b&TT::B_isCheckFlag);
        //formerPV = ttPV && !pvnode;
        if ( ttHit && !isInCheck && ((bound == TT::B_alpha && e.s < evalScore) || (bound == TT::B_beta && e.s > evalScore) || (bound == TT::B_exact)) ){
            evalScore = adjustHashScore(e.s,ply);
            evalScoreIsHashScore=true;
            marginDepth = std::max(1,depth-(evalScoreIsHashScore?e.d:0)); // a depth that take TT depth into account
        }
    }

    killerT.killers[ply+1][0] = killerT.killers[ply+1][1] = 0;

    if (!rootnode){
        // LMP
        lmp = (SearchConfig::doLMP && depth <= SearchConfig::lmpMaxDepth);
        // futility
        const ScoreType futilityScore = alpha - SearchConfig::futilityDepthInit[evalScoreIsHashScore] - SearchConfig::futilityDepthCoeff[evalScoreIsHashScore]*marginDepth;
        futility = (SearchConfig::doFutility && depth <= SearchConfig::futilityMaxDepth[evalScoreIsHashScore] && evalScore <= futilityScore);
        // history pruning
        historyPruning = (SearchConfig::doHistoryPruning && isNotEndGame && depth < SearchConfig::historyPruningMaxDepth);
        // CMH pruning
        CMHPruning = (SearchConfig::doCMHPruning && isNotEndGame && depth < SearchConfig::CMHMaxDepth);
    }

    int validMoveCount = 0;
    int validQuietMoveCount = 0;
    Move bestMove = INVALIDMOVE;
    TT::Bound hashBound = TT::B_alpha;
    bool ttMoveIsCapture = false;
    //bool ttMoveSingularExt = false;

    stack[p.halfmoves].threat = refutation;

    bool bestMoveIsCheck = false;

    // try the tt move before move generation (if not skipped move)
    if ( validTTmove && !isSkipMove(e.m,skipMoves)) { // should be the case thanks to iid at pvnode
        bestMove = e.m; // in order to preserve tt move for alpha bound entry
#ifdef DEBUG_APPLY
        // to debug race condition in entry affectation !
        if ( !isPseudoLegal(p, e.m) ){
            Logging::LogIt(Logging::logFatal) << "invalide TT move !";
        }
#endif
        Position p2 = p;
        if ( apply(p2, e.m)) {
            TT::prefetch(computeHash(p2));
            //const Square to = Move2To(e.m);
            validMoveCount++;
            const bool isQuiet = Move2Type(e.m) == T_std;
            if ( isQuiet ) validQuietMoveCount++;
            PVList childPV;
            stack[p2.halfmoves].h = p2.h;
            stack[p2.halfmoves].p = p2; ///@todo another expensive copy !!!!
            const bool isCheck = ttIsCheck || isAttacked(p2, kingSquare(p2));
            if ( isCapture(e.m) ) ttMoveIsCapture = true;
            //const bool isAdvancedPawnPush = PieceTools::getPieceType(p,Move2From(e.m)) == P_wp && (SQRANK(to) > 5 || SQRANK(to) < 2);
            // extensions
            DepthType extension = 0;
            if ( DynamicConfig::level>80){
               // is in check extension if pvnode
               //if (EXTENDMORE(extension) && pvnode && isInCheck) ++stats.counters[Stats::sid_checkExtension],++extension;
               // castling extension
               //if (EXTENDMORE(extension) && isCastling(e.m) ) ++stats.counters[Stats::sid_castlingExtension],++extension;
               // Botvinnik-Markoff Extension
               //if (EXTENDMORE(extension) && ply > 1 && VALIDMOVE(stack[p.halfmoves].threat) && VALIDMOVE(stack[p.halfmoves - 2].threat) && (sameMove(stack[p.halfmoves].threat, stack[p.halfmoves - 2].threat) || (Move2To(stack[p.halfmoves].threat) == Move2To(stack[p.halfmoves - 2].threat) && isCapture(stack[p.halfmoves].threat)))) ++stats.counters[Stats::sid_BMExtension], ++extension;
               // mate threat extension (from null move)
               //if (EXTENDMORE(extension) && mateThreat) ++stats.counters[Stats::sid_mateThreatExtension],++extension;
               // simple recapture extension
               //if (EXTENDMORE(extension) && VALIDMOVE(p.lastMove) && Move2Type(p.lastMove) == T_capture && to == Move2To(p.lastMove)) ++stats.counters[Stats::sid_recaptureExtension],++extension; // recapture
               // gives check extension
               //if (EXTENDMORE(extension) && isCheck ) ++stats.counters[Stats::sid_checkExtension2],++extension; // we give check with a non risky move
               /*
               // CMH extension
               if (EXTENDMORE(extension) && isQuiet) {
                   const int pp = (p.board_const(Move2From(e.m)) + PieceShift) * 64 + to;
                   if (cmhPtr[0] && cmhPtr[1] && cmhPtr[0][pp] >= HISTORY_MAX / 2 && cmhPtr[1][pp] >= HISTORY_MAX / 2) ++stats.counters[Stats::sid_CMHExtension], ++extension;
               }
               */
               // advanced pawn push extension
               /*
               if (EXTENDMORE(extension) && isAdvancedPawnPush ) {
                   const BitBoard pawns[2] = { p2.pieces_const<P_wp>(Co_White), p2.pieces_const<P_wp>(Co_Black) };
                   const BitBoard passed[2] = { BBTools::pawnPassed<Co_White>(pawns[Co_White], pawns[Co_Black]), BBTools::pawnPassed<Co_Black>(pawns[Co_Black], pawns[Co_White]) };
                   if ( SquareToBitboard(to) & passed[p.c] ) ++stats.counters[Stats::sid_pawnPushExtension], ++extension;
               }
               */
               // threat on queen extension
               //if (EXTENDMORE(extension) && pvnode && (p.pieces_const<P_wq>(p.c) && isQuiet && PieceTools::getPieceType(p, Move2From(e.m)) == P_wq && isAttacked(p, BBTools::SquareFromBitBoard(p.pieces_const<P_wq>(p.c)))) && SEE_GE(p, e.m, 0)) ++stats.counters[Stats::sid_queenThreatExtension], ++extension;
               // singular move extension
               if (EXTENDMORE(extension) && withoutSkipMove && depth >= SearchConfig::singularExtensionDepth && !rootnode && !isMateScore(e.s) && bound == TT::B_beta && e.d >= depth - 3){
                   const ScoreType betaC = e.s - 2*depth;
                   PVList sePV;
                   DepthType seSeldetph = 0;
                   std::vector<MiniMove> skip({e.m});
                   const ScoreType score = pvs<false>(betaC - 1, betaC, p, depth/2, ply, sePV, seSeldetph, isInCheck, cutNode, false, &skip);
                   if (stopFlag) return STOPSCORE;
                   if (score < betaC) { // TT move is singular
                       ++stats.counters[Stats::sid_singularExtension],/*ttMoveSingularExt=!ttMoveIsCapture,*/++extension;
                       // TT move is "very singular" : kind of single reply extension
                       if ( score < betaC - 4*depth) ++stats.counters[Stats::sid_singularExtension2],++extension;
                   }
                   // multi cut (at least the TT move and another move are above beta)
                   else if ( betaC >= beta){
                       return ++stats.counters[Stats::sid_singularExtension3],betaC; // fail-soft
                   }
                   // if TT move is above beta, we try a reduce search early to see if another move is above beta (from SF)
                   else if ( e.s >= beta ){
                       const ScoreType score2 = pvs<false>(beta - 1, beta, p, depth-4, ply, sePV, seSeldetph, isInCheck, cutNode, false, &skip);
                       if ( score2 > beta ) return ++stats.counters[Stats::sid_singularExtension4],beta; // fail-hard
                   }
               }
            }
            const ScoreType ttScore = -pvs<pvnode>(-beta, -alpha, p2, depth - 1 + extension, ply + 1, childPV, seldepth, isCheck, !cutNode, true);
            if (stopFlag) return STOPSCORE;
            if (rootnode){
                rootScores.push_back({e.m,ttScore});
                previousBest = e.m;
            }            
            if ( ttScore > bestScore ){
                bestScore = ttScore;
                bestMove = e.m;
                bestMoveIsCheck = isCheck;
                if (ttScore > alpha) {
                    hashBound = TT::B_exact;
                    if (pvnode) updatePV(pv, e.m, childPV);
                    if (ttScore >= beta) {
                        ++stats.counters[Stats::sid_ttbeta];
                        // increase history bonus of this move
                        if ( !isInCheck && isQuiet /*&& depth > 1*/) updateTables(*this, p, depth + (ttScore > (beta+80)), ply, e.m, TT::B_beta, cmhPtr);
                        TT::setEntry(*this,pHash,e.m,createHashScore(ttScore,ply),createHashScore(evalScore,ply),TT::Bound(TT::B_beta|(ttPV?TT::B_ttFlag:TT::B_none)|(bestMoveIsCheck?TT::B_isCheckFlag:TT::B_none)|(isInCheck?TT::B_isInCheckFlag:TT::B_none)),depth);
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
        tbScore = 0;
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

#ifdef USE_PARTIAL_SORT
    MoveSorter::score(*this, moves, p, data.gp, ply, cmhPtr, true, isInCheck, validTTmove?&e:NULL, refutation != INVALIDMOVE && isCapture(Move2Type(refutation)) ? refutation : INVALIDMOVE);
    size_t offset = 0;
    const Move * it = nullptr;
    while( (it = MoveSorter::pickNext(moves,offset)) && !stopFlag){
#else
    MoveSorter::scoreAndSort(*this, moves, p, data.gp, ply, cmhPtr, true, isInCheck, validTTmove?&e:NULL, refutation != INVALIDMOVE && isCapture(Move2Type(refutation)) ? refutation : INVALIDMOVE);
    for(auto it = moves.begin() ; it != moves.end() && !stopFlag ; ++it){
#endif
        if (isSkipMove(*it,skipMoves)) continue; // skipmoves
        if (validTTmove && sameMove(e.m, *it)) continue; // already tried
        Position p2 = p;
        if ( ! apply(p2,*it) ) continue;
        TT::prefetch(computeHash(p2));
        const Square to = Move2To(*it);
        if (p.c == Co_White && to == p.king[Co_Black]) return MATE - ply + 1;
        if (p.c == Co_Black && to == p.king[Co_White]) return MATE - ply + 1;
        validMoveCount++;
        const bool isQuiet = Move2Type(*it) == T_std;
        if ( isQuiet ) validQuietMoveCount++;
        const bool firstMove = validMoveCount == 1;
        PVList childPV;
        stack[p2.halfmoves].h = p2.h;
        stack[p2.halfmoves].p = p2; ///@todo another expensive copy !!!!
        const bool isCheck = isAttacked(p2, kingSquare(p2));
        bool isAdvancedPawnPush = PieceTools::getPieceType(p,Move2From(*it)) == P_wp && (SQRANK(to) > 5 || SQRANK(to) < 2);
        // extensions
        DepthType extension = 0;
        if ( DynamicConfig::level>80){ 
           // is in check extension if pvnode
           //if (EXTENDMORE(extension) && pvnode && isInCheck) ++stats.counters[Stats::sid_checkExtension],++extension; // we are in check (extension)
           // castling extension
           //if (EXTENDMORE(extension) && isCastling(*it) ) ++stats.counters[Stats::sid_castlingExtension],++extension;
           // Botvinnik-Markoff Extension
           //if (EXTENDMORE(extension) && ply > 1 && stack[p.halfmoves].threat != INVALIDMOVE && stack[p.halfmoves - 2].threat != INVALIDMOVE && (sameMove(stack[p.halfmoves].threat, stack[p.halfmoves - 2].threat) || (Move2To(stack[p.halfmoves].threat) == Move2To(stack[p.halfmoves - 2].threat) && isCapture(stack[p.halfmoves].threat)))) ++stats.counters[Stats::sid_BMExtension], ++extension;
           // mate threat extension (from null move)
           //if (EXTENDMORE(extension) && mateThreat && depth <= 4) ++stats.counters[Stats::sid_mateThreatExtension],++extension;
           // simple recapture extension
           //if (EXTENDMORE(extension) && VALIDMOVE(p.lastMove) && !isBadCap(*it) && Move2Type(p.lastMove) == T_capture && to == Move2To(p.lastMove)) ++stats.counters[Stats::sid_recaptureExtension],++extension; //recapture
           // gives check extension
           //if (EXTENDMORE(extension) && isCheck && !isBadCap(*it)) ++stats.counters[Stats::sid_checkExtension2],++extension; // we give check with a non risky move
           // CMH extension
           /*
           if (EXTENDMORE(extension) && isQuiet) {
               const int pp = (p.board_const(Move2From(*it)) + PieceShift) * 64 + to;
               if (cmhPtr[0] && cmhPtr[1] && cmhPtr[0][pp] >= HISTORY_MAX/2 && cmhPtr[1][pp] >= HISTORY_MAX/2) ++stats.counters[Stats::sid_CMHExtension], ++extension;
           }
           */
           // advanced pawn push extension
           /*
           if (EXTENDMORE(extension) && isAdvancedPawnPush && killerT.isKiller(*it, ply) ){
               const BitBoard pawns[2] = { p2.pieces_const<P_wp>(Co_White), p2.pieces_const<P_wp>(Co_Black) };
               const BitBoard passed[2] = { BBTools::pawnPassed<Co_White>(pawns[Co_White], pawns[Co_Black]), BBTools::pawnPassed<Co_Black>(pawns[Co_Black], pawns[Co_White]) };
               if ( (SquareToBitboard(to) & passed[p.c]) ) ++stats.counters[Stats::sid_pawnPushExtension], ++extension;
           }
           */
           // threat on queen extension
           //if (EXTENDMORE(extension) && pvnode && firstMove && (p.pieces_const<P_wq>(p.c) && isQuiet && Move2Type(*it) == T_std && PieceTools::getPieceType(p, Move2From(*it)) == P_wq && isAttacked(p, BBTools::SquareFromBitBoard(p.pieces_const<P_wq>(p.c)))) && SEE_GE(p, *it, 0)) ++stats.counters[Stats::sid_queenThreatExtension], ++extension;
           // move that lead to endgame
           /*
           if ( EXTENDMORE(extension) && isNotEndGame && (p2.mat[p.c][M_t]+p2.mat[~p.c][M_t] == 0)){
              ++extension;
              ++stats.counters[Stats::sid_endGameExtension];
           }
           */
           // extend if quiet with good history 
           /*
           if ( EXTENDMORE(extension) && isQuiet && Move2Score(*it) > SearchConfig::historyExtensionThreshold){
              ++extension; 
              ++stats.counters[Stats::sid_goodHistoryExtension];
           }
           */
        }
        // pvs
        if (validMoveCount < (2/*+2*rootnode*/) || !SearchConfig::doPVS ) score = -pvs<pvnode>(-beta,-alpha,p2,depth-1+extension,ply+1,childPV,seldepth,isCheck,!cutNode,true);
        else{
            // reductions & prunings
            const bool isPrunable           = /*isNotEndGame &&*/ !isAdvancedPawnPush && !isMateScore(alpha) && !DynamicConfig::mateFinder && !killerT.isKiller(*it,ply);
            const bool isReductible         = /*isNotEndGame &&*/ !isAdvancedPawnPush && !DynamicConfig::mateFinder;
            const bool noCheck              = !isInCheck && !isCheck;
            const bool isPrunableStd        = isPrunable && isQuiet;
            const bool isPrunableStdNoCheck = isPrunableStd && noCheck;
            const bool isPrunableCap        = isPrunable && Move2Type(*it) == T_capture && isBadCap(*it) && noCheck ;

            // take initial position situation into account ///@todo use this
            const bool isEmergencyDefence   = false; //moveDifficulty == MoveDifficultyUtil::MD_hardDefense; 
            const bool isEmergencyAttack    = false; //moveDifficulty == MoveDifficultyUtil::MD_hardAttack;
            
            // take current danger level into account
            // Warning : danger is only available if no TT hit !
            const int dangerFactor          = (data.danger[p.c]+data.danger[~p.c])/SearchConfig::dangerDivisor;
            const bool isDangerPrune        = dangerFactor >= SearchConfig::dangerLimitPruning;
            const bool isDangerRed          = dangerFactor >= SearchConfig::dangerLimitReduction;
            if ( isDangerPrune) ++stats.counters[Stats::sid_dangerPrune];
            if ( isDangerRed)   ++stats.counters[Stats::sid_dangerReduce];

            // futility
            if (futility && isPrunableStdNoCheck) {++stats.counters[Stats::sid_futility]; continue;}
            // LMP
            const bool moveCountPruning = validMoveCount > /*(1+(2*dangerFactor)/SearchConfig::dangerLimitPruning)**/SearchConfig::lmpLimit[improving][depth] + 2*isEmergencyDefence;
            if (lmp && isPrunableStdNoCheck && moveCountPruning) {++stats.counters[Stats::sid_lmp]; continue;}
            // History pruning (with CMH)
            if (historyPruning && isPrunableStdNoCheck && Move2Score(*it) < SearchConfig::historyPruningThresholdInit + marginDepth*SearchConfig::historyPruningThresholdDepth) {++stats.counters[Stats::sid_historyPruning]; continue;}
            // CMH pruning alone
            if (CMHPruning && isPrunableStdNoCheck){
              const int pp = (p.board_const(Move2From(*it))+PieceShift) * 64 + Move2To(*it);
              if ((!cmhPtr[0] || cmhPtr[0][pp] < 0) && (!cmhPtr[1] || cmhPtr[1][pp] < 0)) { ++stats.counters[Stats::sid_CMHPruning]; continue;}
            }
            // SEE (capture)
            if (isPrunableCap){
               if (futility) {++stats.counters[Stats::sid_see]; continue;}
               else if ( !rootnode && badCapScore(*it) < -(1+(6*dangerFactor)/SearchConfig::dangerLimitPruning)*100*(depth+isEmergencyDefence+isEmergencyAttack)) {++stats.counters[Stats::sid_see2]; continue;}
            }
            // LMR
            DepthType reduction = 0;
            if (SearchConfig::doLMR && depth >= SearchConfig::lmrMinDepth 
               && ( (isReductible && isQuiet) /*|| isPrunableCap*/ /*|| stack[p.halfmoves].eval <= alpha*/ /*|| cutNode*/ ) 
               && validMoveCount > 1 + 2*rootnode ){
                ++stats.counters[Stats::sid_lmr];
                reduction += SearchConfig::lmrReduction[std::min((int)depth,MAX_DEPTH-1)][std::min(validMoveCount,MAX_DEPTH)];
                reduction += !improving + ttMoveIsCapture;
                reduction += (cutNode && evalScore - SearchConfig::failHighReductionThresholdInit[evalScoreIsHashScore] - marginDepth*SearchConfig::failHighReductionThresholdDepth[evalScoreIsHashScore] > beta); ///@todo try without
                //reduction += moveCountPruning && !formerPV;
                reduction -= /*std::min(2,*/HISTORY_DIV(2 * Move2Score(*it))/*)*/; //history reduction/extension (beware killers and counter are scored above history max)
                //if ( !isInCheck) reduction += std::min(2,(data.mobility[p.c]-data.mobility[~p.c])/8);
                reduction -= (ttPV || isDangerRed || !noCheck /*|| ttMoveSingularExt*//*|| isEmergencyDefence*/);
                
                // never extend more than reduce
                if ( extension - reduction > 0 ) reduction = extension;
                if ( reduction >= depth - 1 + extension ) reduction = depth - 1 + extension - 1;
            }
            const DepthType nextDepth = depth-1-reduction+extension;
            // SEE (quiet)
            if ( isPrunableStdNoCheck && /*!rootnode &&*/ !SEE_GE(p,*it,-15*(nextDepth+isEmergencyDefence+isEmergencyAttack)*nextDepth)) {
                ++stats.counters[Stats::sid_seeQuiet]; 
                continue;
            }

            // PVS
            score = -pvs<false>(-alpha-1,-alpha,p2,nextDepth,ply+1,childPV,seldepth,isCheck,true,true);
            if ( reduction > 0 && score > alpha ){ 
                ++stats.counters[Stats::sid_lmrFail]; childPV.clear(); 
                score = -pvs<false>(-alpha-1,-alpha,p2,depth-1+extension,ply+1,childPV,seldepth,isCheck,!cutNode,true); 
            }
            if ( pvnode && score > alpha && (rootnode || score < beta) ){ 
                ++stats.counters[Stats::sid_pvsFail]; childPV.clear(); 
                score = -pvs<true>(-beta   ,-alpha,p2,depth-1+extension,ply+1,childPV,seldepth,isCheck,false,true); 
            } // potential new pv node

        }
        if (stopFlag) return STOPSCORE;
        if (rootnode) rootScores.push_back({*it,score});
        if (rootnode) previousBest = *it;
        if ( score > bestScore ){
            bestScore = score;
            bestMove = *it;
            bestMoveIsCheck = isCheck;
            //bestScoreUpdated = true;
            if ( score > alpha ){
                if (pvnode) updatePV(pv, *it, childPV);
                //alphaUpdated = true;
                alpha = score;
                hashBound = TT::B_exact;
                if ( score >= beta ){
                    if ( !isInCheck && isQuiet /*&& depth > 1*/ /*&& !( depth == 1 && validQuietMoveCount == 1 )*/){ // last cond from Alayan in Ethereal)
                        // increase history bonus of this move
                        updateTables(*this, p, depth + (score>beta+80), ply, *it, TT::B_beta, cmhPtr);
                        // reduce history bonus of all previous
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
    TT::setEntry(*this,pHash,bestMove,createHashScore(bestScore,ply),createHashScore(evalScore,ply),TT::Bound(hashBound|(ttPV?TT::B_ttFlag:TT::B_none)|(bestMoveIsCheck?TT::B_isCheckFlag:TT::B_none)|(isInCheck?TT::B_isInCheckFlag:TT::B_none)),depth);
    return bestScore;
}
