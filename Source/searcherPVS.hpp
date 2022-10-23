#pragma once

#include "definition.hpp"
#include "distributed.h"
#include "dynamicConfig.hpp"
#include "egt.hpp"
#include "evalConfig.hpp"
#include "evalTools.hpp"
#include "moveGen.hpp"
#include "moveSort.hpp"
#include "positionTools.hpp"
#include "searchConfig.hpp"
#include "timeMan.hpp"
#include "tools.hpp"
#include "transposition.hpp"

#define PERIODICCHECK uint64_t(1024)

FORCE_FINLINE DepthType Searcher::getExtension(const Position & /*p*/,
                                               DepthType /*depth*/,
                                               Move /*m*/,
                                               const PVSData & /*pvsData*/,
                                               const EvalData & /*evalData*/,
                                               ScoreType /*evalScore*/,
                                               DepthType extension) const{

   // -----------------------
   // win seeking (I think there might be something good here if I look a little further.)
   // -----------------------

   // gives check extension
   //if (EXTENDMORE && pvsData.isCheck ) stats.incr(Stats::sid_checkExtension2),++extension;

   // -----------------------
   // loss seeking (I think I'm in trouble.)
   // -----------------------
   
   // is in check extension
   //if (EXTENDMORE && pvsData.isTTMove && pvsData.pvnode && pvsData.isInCheck) stats.incr(Stats::sid_checkExtension), ++extension;
   // Botvinnik-Markoff Extension
   //if (EXTENDMORE && !pvsData.earlyMove && pvsData.BMextension) stats.incr(Stats::sid_BMExtension), ++extension;
   // mate threat extension (from null move)
   //if (EXTENDMORE && pvsData.mateThreat) stats.incr(Stats::sid_mateThreatExtension), ++extension;
   // threat on queen extension
   //if (EXTENDMORE && (p.pieces_const<P_wq>(p.c) && pvsData.isQuiet && PieceTools::getPieceType(p, Move2From(m)) == P_wq && isAttacked(p, BBTools::SquareFromBitBoard(p.pieces_const<P_wq>(p.c)))) && SEE_GE(p, m, 0)) stats.incr(Stats::sid_queenThreatExtension), ++extension;   


   // -----------------------
   // horizon (This is a forcing sequence, and if I stop searching now I won't know how it ends.)
   // -----------------------

   // castling extension
   //if (EXTENDMORE && isCastling(m)) stats.incr(Stats::sid_castlingExtension), ++extension;
   // simple recapture extension
   //if (EXTENDMORE && isValidMove(p.lastMove) && Move2Type(p.lastMove) == T_capture && !isBadCap(m) && to == Move2To(p.lastMove)) stats.incr(Stats::sid_recaptureExtension), ++extension; // recapture
   // advanced pawn push extension
   /*
   if (EXTENDMORE && pvsData.isAdvancedPawnPush ) {
         const colored<BitBoard> pawns = { p2.pieces_const<P_wp>(Co_White), p2.pieces_const<P_wp>(Co_Black) };
         const colored<BitBoard> passed = { BBTools::pawnPassed<Co_White>(pawns[Co_White], pawns[Co_Black]), BBTools::pawnPassed<Co_Black>(pawns[Co_Black], pawns[Co_White]) };
         if ( SquareToBitboard(to) & passed[p.c] ) stats.incr(Stats::sid_pawnPushExtension), ++extension;
   }
   */
   // move that lead to endgame
   /*
   if ( EXTENDMORE && lessZugzwangRisk && (p2.mat[p.c][M_t]+p2.mat[~p.c][M_t] == 0)){
      ++extension;
      stats.incr(Stats::sid_endGameExtension);
   }
   */

   // extend if quiet with good history
   /*
   if ( EXTENDMORE && pvsData.isQuiet && Move2Score(m) > SearchConfig::historyExtensionThreshold){
      ++extension;
      stats.incr(Stats::sid_goodHistoryExtension);
   }
   */
   // CMH extension
   /*
   if (EXTENDMORE && pvsData.isQuiet) {
         if (isCMHGood(p, Move2From(m), to, pvsData.cmhPtr, HISTORY_MAX / 2)) stats.incr(Stats::sid_CMHExtension), ++extension;
   }
   */  

   return extension;
}

FORCE_FINLINE DepthType Searcher::getReduction( const Position & p,
                                                DepthType depth,
                                                Move m,
                                                const PVSData & pvsData,
                                                const EvalData & evalData,
                                                ScoreType evalScore,
                                                DepthType extension) const{
   // LMR
   DepthType reduction = 0;

   if (Move2Type(m) == T_std){
      stats.incr(Stats::sid_lmr);
      // base reduction
      const int threadReductionIdx = 0; //static_cast<int>(id()) / std::max(1,static_cast<int>(ThreadPool::instance().size()/8));
      reduction += SearchConfig::lmrReduction[std::min(static_cast<int>(depth), MAX_DEPTH - 1)][std::min(pvsData.validMoveCount + threadReductionIdx, MAX_MOVE - 1)];

      // more reduction
      reduction += !pvsData.improving;
      reduction += pvsData.ttMoveIsCapture;
      reduction += (pvsData.cutNode && evalScore - SearchConfig::failHighReductionCoeff.threshold(pvsData.marginDepth, evalData.gp, pvsData.evalScoreIsHashScore, pvsData.improving) > pvsData.beta);
      //reduction += moveCountPruning && !formerPV;
      /*
      if (!pvsData.isInCheck){
         const uint16_t mobilityBalance = evalData.mobility[p.c]-evalData.mobility[~p.c];
         reduction += std::max(-1, std::min(1, mobilityBalance/8));
      }
      */

      //reduction += (id()%2);

/*
      // aggressive random reduction
      const ScoreType randomShot = (stats.counters[Stats::sid_nodes] + stats.counters[Stats::sid_qnodes]) % 128;
      if ( randomShot > 128 + SearchConfig::lmpLimit[pvsData.improving][depth + depthCorrection] - SearchConfig::randomAggressiveReductionFactor * validQuietMoveCount) {
         stats.incr(Stats::sid_lmrAR);
         ++reduction;
      }
*/

      // history reduction/extension
      // beware killers and counter are scored above history max
      reduction -= std::min(3, HISTORY_DIV(2 * Move2Score(m)));

      // less reduction
      //reduction -= !noCheck;
      //reduction -= isCheck;
      reduction -= pvsData.formerPV || pvsData.ttPV || pvsData.pvnode;
      //reduction -= pvsData.ttMoveSingularExt;
      //reduction -= pvsData.theirTurn; ///@todo use in capture also ?
      //reduction -= isDangerRed || pvsData.isEmergencyDefence;
   }
   else if (Move2Type(m) == T_capture){
      stats.incr(Stats::sid_lmrcap);
      // base reduction
      reduction += SearchConfig::lmrReduction[std::min(static_cast<int>(depth), MAX_DEPTH - 1)][std::min(pvsData.validMoveCount, MAX_MOVE - 1)];
      //reduction += isBadCap(m);
      reduction -= pvsData.improving;
      reduction -= pvsData.pvnode;
      // capture history reduction
      const Square to = Move2To(m);
      reduction -= std::max(-2,std::min(2, HISTORY_DIV(SearchConfig::lmrCapHistoryFactor * historyT.historyCap[PieceIdx(p.board_const(Move2From(m)))][to][Abs(p.board_const(to))-1])));
   }

   // never extend more than reduce (to avoid search explosion)
   if (extension - reduction > 0) reduction = extension;
   // clamp to depth = 0
   if (reduction >= depth - 1 + extension) reduction = depth - 1 + extension;

   return reduction;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"

FORCE_FINLINE void evalFeatures(const Position & p,
                         array2d<BitBoard,2,6> & attFromPiece,
                         colored<BitBoard>     & att,
                         colored<BitBoard>     & att2,
                         array2d<BitBoard,2,6> & checkers,
                         colored<ScoreType>    & kdanger,
                         EvalScore             & mobilityScore,
                         colored<uint16_t>     & mobility){

   const bool kingIsMandatory = DynamicConfig::isKingMandatory();
   const colored<BitBoard> pawns = {p.whitePawn(), p.blackPawn()};
   const colored<BitBoard> nonPawnMat = {p.allPieces[Co_White] & ~pawns[Co_White], 
                                         p.allPieces[Co_Black] & ~pawns[Co_Black]};
   const colored<BitBoard> kingZone = {isValidSquare(p.king[Co_White]) ? BBTools::mask[p.king[Co_White]].kingZone : emptyBitBoard, 
                                       isValidSquare(p.king[Co_Black]) ? BBTools::mask[p.king[Co_Black]].kingZone : emptyBitBoard};
   const BitBoard occupancy = p.occupancy();

   for (Color c = Co_White ; c <= Co_Black ; ++c){
      for (Piece pp = P_wp ; pp <= P_wk ; ++pp){
         BB::applyOn(p.pieces_const(c,pp), [&](const Square & k) {
            // aligned threats removing own piece (not pawn) in occupancy
            const BitBoard shadowTarget = BBTools::pfCoverage[pp - 1](k, occupancy ^ nonPawnMat[c], c);
            if (shadowTarget) {
               kdanger[~c] += BB::countBit(shadowTarget & kingZone[~c]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack][pp - 1];
               const BitBoard target = BBTools::pfCoverage[pp - 1](k, occupancy, c); // real targets
               if (target) {
                  attFromPiece[c][pp - 1] |= target;
                  att2[c] |= att[c] & target;
                  att[c] |= target;
                  if (target & p.pieces_const<P_wk>(~c)) checkers[c][pp - 1] |= SquareToBitboard(k);
                  kdanger[c] -= BB::countBit(target & kingZone[c]) * EvalConfig::kingAttWeight[EvalConfig::katt_defence][pp - 1];
               }
            }
         });
      }
   }

   if(kingIsMandatory){
     const File wkf = (File)SQFILE(p.king[Co_White]);
     const File bkf = (File)SQFILE(p.king[Co_Black]);
 
     const colored<BitBoard> semiOpenFiles = {BBTools::fillFile(pawns[Co_White]) & ~BBTools::fillFile(pawns[Co_Black]),
                                               BBTools::fillFile(pawns[Co_Black]) & ~BBTools::fillFile(pawns[Co_White])};
     const BitBoard openFiles = BBTools::openFiles(pawns[Co_White], pawns[Co_Black]);
 
     kdanger[Co_White] += EvalConfig::kingAttOpenfile * BB::countBit(BB::kingFlank[wkf] & openFiles) / 8;
     kdanger[Co_White] += EvalConfig::kingAttSemiOpenfileOpp * BB::countBit(BB::kingFlank[wkf] & semiOpenFiles[Co_White]) / 8;
     kdanger[Co_White] += EvalConfig::kingAttSemiOpenfileOur * BB::countBit(BB::kingFlank[wkf] & semiOpenFiles[Co_Black]) / 8;
     kdanger[Co_Black] += EvalConfig::kingAttOpenfile * BB::countBit(BB::kingFlank[bkf] & openFiles) / 8;
     kdanger[Co_Black] += EvalConfig::kingAttSemiOpenfileOpp * BB::countBit(BB::kingFlank[bkf] & semiOpenFiles[Co_Black]) / 8;
     kdanger[Co_Black] += EvalConfig::kingAttSemiOpenfileOur * BB::countBit(BB::kingFlank[bkf] & semiOpenFiles[Co_White]) / 8;
   }

   const colored<BitBoard> weakSquare = { att[Co_Black] & ~att2[Co_White] & (~att[Co_White] | attFromPiece[Co_White][P_wk - 1] | attFromPiece[Co_White][P_wq - 1]),
                                          att[Co_White] & ~att2[Co_Black] & (~att[Co_Black] | attFromPiece[Co_Black][P_wk - 1] | attFromPiece[Co_Black][P_wq - 1])};

   const colored<BitBoard> safeSquare = { ~att[Co_Black] | (weakSquare[Co_Black] & att2[Co_White]),
                                          ~att[Co_White] | (weakSquare[Co_White] & att2[Co_Black])};

   // reward safe checks
   kdanger[Co_White] += EvalConfig::kingAttSafeCheck[0] * BB::countBit(checkers[Co_Black][0] & att[Co_Black]);
   kdanger[Co_Black] += EvalConfig::kingAttSafeCheck[0] * BB::countBit(checkers[Co_White][0] & att[Co_White]);
   for (Piece pp = P_wn; pp < P_wk; ++pp) {
      kdanger[Co_White] += EvalConfig::kingAttSafeCheck[pp - 1] * BB::countBit(checkers[Co_Black][pp - 1] & safeSquare[Co_Black]);
      kdanger[Co_Black] += EvalConfig::kingAttSafeCheck[pp - 1] * BB::countBit(checkers[Co_White][pp - 1] & safeSquare[Co_White]);
   }

   // pieces mobility (second attack loop needed, knowing safeSquare ...)
   const colored<BitBoard> knights = {p.whiteKnight(), p.blackKnight()};
   const colored<BitBoard> bishops = {p.whiteBishop(), p.blackBishop()};
   const colored<BitBoard> rooks   = {p.whiteRook(), p.blackRook()};
   const colored<BitBoard> queens  = {p.whiteQueen(), p.blackQueen()};
   const colored<BitBoard> kings   = {p.whiteKing(), p.blackKing()};

   evalMob<P_wn, Co_White>(p, knights[Co_White], mobilityScore, safeSquare[Co_White], occupancy, mobility[Co_White]);
   evalMob<P_wb, Co_White>(p, bishops[Co_White], mobilityScore, safeSquare[Co_White], occupancy, mobility[Co_White]);
   evalMob<P_wr, Co_White>(p, rooks  [Co_White], mobilityScore, safeSquare[Co_White], occupancy, mobility[Co_White]);
   evalMobQ<     Co_White>(p, queens [Co_White], mobilityScore, safeSquare[Co_White], occupancy, mobility[Co_White]);
   evalMobK<     Co_White>(p, kings  [Co_White], mobilityScore, ~att[Co_Black]      , occupancy, mobility[Co_White]);
   evalMob<P_wn, Co_Black>(p, knights[Co_Black], mobilityScore, safeSquare[Co_Black], occupancy, mobility[Co_Black]);
   evalMob<P_wb, Co_Black>(p, bishops[Co_Black], mobilityScore, safeSquare[Co_Black], occupancy, mobility[Co_Black]);
   evalMob<P_wr, Co_Black>(p, rooks  [Co_Black], mobilityScore, safeSquare[Co_Black], occupancy, mobility[Co_Black]);
   evalMobQ<     Co_Black>(p, queens [Co_Black], mobilityScore, safeSquare[Co_Black], occupancy, mobility[Co_Black]);
   evalMobK<     Co_Black>(p, kings  [Co_Black], mobilityScore, ~att[Co_White]      , occupancy, mobility[Co_Black]);   
}

#pragma GCC diagnostic pop

// this function shall be used on the initial position state before move is applied
[[nodiscard]] FORCE_FINLINE bool isNoisy(const Position & /*p*/, const Move & m){
   return Move2Type(m) != T_std;
   /*
   if ( Move2Type(m) != T_std ) return true;
   const Square from = Move2From(m);
   const Square to = Move2To(m);
   const Piece pp = p.board_const(from);
   if (pp == P_wp){
      const BitBoard pAtt = p.c == Co_White ? BBTools::pawnAttacks<Co_White>(SquareToBitboard(to)) & p.allPieces[~p.c]:
                                              BBTools::pawnAttacks<Co_Black>(SquareToBitboard(to)) & p.allPieces[~p.c];
      if ( BB::countBit(pAtt) > 1 ) return true; ///@todo verify if the pawn is protected and/or not attacked ?
   }
   else if (pp == P_wn){
      const BitBoard nAtt = BBTools::coverage<P_wn>(to, p.occupancy(), p.c) & p.allPieces[~p.c];
      if ( BB::countBit(nAtt) > 1 ) return true; ///@todo verify if the knight is protected and/or not attacked ?
   }
   return false;
   */
}

FORCE_FINLINE void Searcher::timeCheck(){
   static uint64_t periodicCheck = 0ull; ///@todo this is slow because of guard variable
   if (periodicCheck == 0ull) {
      periodicCheck = (TimeMan::maxNodes > 0) ? std::min(TimeMan::maxNodes, PERIODICCHECK) : PERIODICCHECK;
      const Counter nodeCount = isStoppableCoSearcher ? stats.counters[Stats::sid_nodes] + stats.counters[Stats::sid_qnodes]
                                : ThreadPool::instance().counter(Stats::sid_nodes) + ThreadPool::instance().counter(Stats::sid_qnodes);
      if (TimeMan::maxNodes > 0 && nodeCount > TimeMan::maxNodes) {
         stopFlag = true;
         Logging::LogIt(Logging::logInfo) << "stopFlag triggered (nodes limits) in thread " << id();
      }
      if (getTimeDiff(startTime) > getCurrentMoveMs()) {
         stopFlag = true;
         Logging::LogIt(Logging::logInfo) << "stopFlag triggered in thread " << id();
      }
      Distributed::pollStat();

      if (!Distributed::isMainProcess()) {
         bool masterStopFlag;
         Distributed::get(&masterStopFlag, 1, Distributed::_winStopFromR0, 0, Distributed::_requestStopFromR0);
         Distributed::waitRequest(Distributed::_requestStopFromR0);
         ThreadPool::instance().main().stopFlag = masterStopFlag;
      }
   }
   --periodicCheck;
}

// pvs inspired by Xiphos
template<bool pvnode>
ScoreType Searcher::pvs(ScoreType                    alpha,
                        ScoreType                    beta,
                        const Position&              p,
                        DepthType                    depth,
                        DepthType                    height,
                        PVList&                      pv,
                        DepthType&                   seldepth,
                        DepthType                    extensions,
                        bool                         isInCheck_,
                        bool                         cutNode_,
                        const std::vector<MiniMove>* skipMoves) {

   // stopFlag management and time check. Only on main thread and not at each node (see PERIODICCHECK)
   if (isMainThread() || isStoppableCoSearcher) timeCheck();
   if (stopFlag) return STOPSCORE;

   debug_king_cap(p);

   // initiate the big pvsData structure with already given parameters
   PVSData pvsData;
   pvsData.cutNode = cutNode_;
   pvsData.isInCheck = isInCheck_;
   pvsData.alpha = alpha;
   pvsData.beta = beta;
   pvsData.pvnode = pvnode;
   pvsData.withoutSkipMove = skipMoves == nullptr;

   // some evaluation are used inside search
   EvalData evalData;

   // height is updated recursively in pvs and qsearch calls 
   // but also given to Searcher itself in order to be available inside eval (which have "context").
   // @todo simplify this using only one solution !
   height_ = height;
   pvsData.theirTurn = height%2;
   pvsData.rootnode = height == 0;

   // we cannot search deeper than MAX_DEPTH, if so just return static evaluation in this case
   if (height >= MAX_DEPTH - 1 || depth >= MAX_DEPTH - 1) return eval(p, evalData, *this);

   // if not at root we check for draws (3rep, fifty and material)
   if (!pvsData.rootnode){
      if (auto INRscore = interiorNodeRecognizer<pvnode>(p, height); INRscore.has_value()){
         return INRscore.value();
      }
   }

   // on pvs leaf node, call a quiet search
   if (depth <= 0) {
      // don't enter qsearch when in check
      //if (pvsData.isInCheck) depth = 1;
      //else
         return qsearch(alpha, beta, p, height, seldepth, 0, true, pvnode, pvsData.isInCheck);
   }

   // update selective depth as soon as we "really enter" a node
   seldepth = std::max(seldepth,height);

   // update nodes count as soon as we "really enter" a node
   ++stats.counters[Stats::sid_nodes];

   if (pvsData.rootnode){
      // all threads clear rootScore, this is usefull for helper like in genfen or rescore.
      rootScores.clear();
   }

   if(!pvsData.rootnode){
      // mate distance pruning
      alpha = std::max(alpha, matedScore(height));
      beta  = std::min(beta, matingScore(height + 1));
      if (alpha >= beta) return alpha;
   }

   // get the CMH pointer, can be needed to update tables
   getCMHPtr(p.halfmoves, pvsData.cmhPtr);

   // the position hash will be used quite often
   Hash pHash = computeHash(p);
   // let's consider skipmove(s) in position hashing
   if (skipMoves){
      for (auto it = skipMoves->begin(); it != skipMoves->end(); ++it) { 
          assert( isValidMove(*it) );
          const auto moveHashIndex = static_cast<int32_t>(*it) + std::abs(std::numeric_limits<int16_t>::min());
          assert(moveHashIndex >= 0 && moveHashIndex < std::numeric_limits<uint16_t>::max());
          pHash ^= Zobrist::ZTMove[moveHashIndex];
      }
   }

   // probe TT
   TT::Entry e;
   const bool ttDepthOk = TT::getEntry(*this, p, pHash, depth, e);
   pvsData.bound = static_cast<TT::Bound>(e.b & ~TT::B_allFlags);
   // if depth of TT entry is enough, and not at root, then we consider to early return
   if (ttDepthOk && !pvsData.rootnode) { 
      if ((pvsData.bound == TT::B_alpha && e.s <= alpha) 
       || (pvsData.bound == TT::B_beta  && e.s >= beta)  
       || (pvsData.bound == TT::B_exact) ) {
         if constexpr(!pvnode) {
            // increase history bonus of this move
            if (e.m != INVALIDMINIMOVE){
               // quiet move history
               if (Move2Type(e.m) == T_std){ 
                  updateTables(*this, p, depth, height, e.m, pvsData.bound, pvsData.cmhPtr);
               }
               // capture history
               else if ( isCapture(e.m) ){ 
                  if (pvsData.bound == TT::B_beta) historyT.updateCap<1>(depth, e.m, p);
                  //else if (bound == TT::B_alpha) historyT.updateCap<-1>(depth, e.m, p);
               }
            }
            if (p.fifty < SearchConfig::ttMaxFiftyValideDepth){
               if(pvsData.bound == TT::B_alpha) stats.incr(Stats::sid_ttAlphaCut);
               if(pvsData.bound == TT::B_exact) stats.incr(Stats::sid_ttExactCut);
               if(pvsData.bound == TT::B_beta)  stats.incr(Stats::sid_ttBetaCut);
               return TT::adjustHashScore(e.s, height);
            }
         }
         ///@todo try returning also at pv node again (but this cuts pv ...)
         /*
         else { // in "good" condition, also return a score at pvnode
            if ( bound == TT::B_exact && e.d > 3*depth/2 && p.fifty < SearchConfig::ttMaxFiftyValideDepth){
               return TT::adjustHashScore(e.s, height);
            }
         }
         */
      }
   }

   // if entry hash is not null and entry move is valid, 
   // then this is a valid TT move (we don't care about depth of the entry here)
   ///@todo try to store more things in TT bound ...
   pvsData.ttHit       = e.h != nullHash;
   pvsData.validTTmove = pvsData.ttHit && e.m != INVALIDMINIMOVE;
   pvsData.ttPV        = pvnode || (pvsData.validTTmove && (e.b & TT::B_ttPVFlag)); 
   pvsData.ttIsCheck   = pvsData.validTTmove && (e.b & TT::B_isCheckFlag);
   pvsData.formerPV    = pvsData.ttPV && !pvnode;

   // an idea from Ethereal : 
   // near TT hits in terms of depth can trigger early returns
   // if cutoff is validated by a margin

   if constexpr(!pvnode){
      if ( !pvsData.rootnode 
         && pvsData.ttHit
         && (pvsData.bound == TT::B_alpha)
         &&  e.d >= depth - SearchConfig::ttAlphaCutDepth
         &&  e.s + SearchConfig::ttAlphaCutMargin * (depth - SearchConfig::ttAlphaCutDepth) <= alpha){
            stats.incr(Stats::sid_ttAlphaLateCut);
            return alpha;
      }

      // and it seems we can do the same with beta bound
      if ( !pvsData.rootnode 
         && pvsData.ttHit
         && (pvsData.bound == TT::B_beta)
         &&  e.d >= depth - SearchConfig::ttBetaCutDepth
         &&  e.s - SearchConfig::ttBetaCutMargin * (depth - SearchConfig::ttBetaCutDepth) >= beta){
            stats.incr(Stats::sid_ttBetaLateCut);
            return beta;
      }
   }

#ifdef WITH_SYZYGY
   // probe TB
   ScoreType tbScore = 0;
   if (!pvsData.rootnode 
     && pvsData.withoutSkipMove 
     && BB::countBit(p.allPieces[Co_White] | p.allPieces[Co_Black]) <= SyzygyTb::MAX_TB_MEN){
      if (SyzygyTb::probe_wdl(p, tbScore, false) > 0) {
         ++stats.counters[Stats::sid_tbHit1];
         // if this is a winning/losing EGT position, we add up static evaluation
         // this allow to go for mate better or to defend longer
         if (abs(tbScore) == SyzygyTb::TB_WIN_SCORE) {
            tbScore += eval(p, evalData, *this);
            tbScore = clampScore(tbScore);
         }
         else if (abs(tbScore) == SyzygyTb::TB_CURSED_SCORE) {
            tbScore = drawScore(p, height);
         }
         // store TB hits into TT (without associated move, but with max depth)
         TT::setEntry(*this, pHash, INVALIDMOVE, TT::createHashScore(tbScore, height), TT::createHashScore(tbScore, height), TT::B_none, DepthType(MAX_DEPTH));
         return tbScore;
      }
      else {
         ++stats.counters[Stats::sid_tbFail];
      }
   }
#endif

   assert(p.halfmoves - 1 < MAX_PLY && p.halfmoves - 1 >= 0);

   pvsData.previousMoveIsNullMove = p.lastMove == NULLMOVE;

   // now, let's get a static score for the position.
   ScoreType evalScore;
   // do not eval position when in check, we won't use it (as we won't forward prune)
   if (pvsData.isInCheck){
      evalScore = matedScore(height);
   }
   // skip eval: if nullmove just applied we can hack
   // but this won't work with asymetric HalfKA NNUE
   else if (!DynamicConfig::useNNUE && pvsData.previousMoveIsNullMove && height > 0){
      evalScore = 2 * EvalConfig::tempo - stack[p.halfmoves - 1].eval;
#ifdef DEBUG_STATICEVAL
      checkEval(p,evalScore,*this,"null move trick (pvs)");
#endif
   }
   else {
      // if we had a TT hit (with or without associated move), 
      // we can use its eval instead of calling eval()
      if (pvsData.ttHit) {
         stats.incr(Stats::sid_ttschits);
         evalScore = e.e;
         // but we are missing game phase in this case
         // so let's look for a material match
         const Hash matHash = MaterialHash::getMaterialHash(p.mat);
         if (matHash != nullHash) {
            stats.incr(Stats::sid_materialTableHitsSearch);
            const MaterialHash::MaterialHashEntry& MEntry = MaterialHash::materialHashTable[matHash];
            evalData.gp = MEntry.gamePhase();
         }
         else { 
            // if no match, compute game phase 
            // this does not happend very often ...
            ScoreType matScoreW = 0;
            ScoreType matScoreB = 0;
            evalData.gp = gamePhase(p.mat, matScoreW, matScoreB);
            stats.incr(Stats::sid_materialTableMissSearch);
         }
#ifdef DEBUG_STATICEVAL
         checkEval(p,evalScore,*this,"from TT (pvs)");
#endif
      }
      // if no TT hit call evaluation
      else {
         stats.incr(Stats::sid_ttscmiss);
         evalScore = eval(p, evalData, *this);
#ifdef DEBUG_STATICEVAL
         checkEval(p,evalScore,*this,"from eval (pvs)");
 #endif
      }
   }
   
   // insert only static eval in stack data, never hash score for consistancy!
   stack[p.halfmoves].eval = evalScore; 

   // backup the static evaluation score as we will try to get a better evalScore approximation
   const ScoreType staticScore = evalScore;

   // if no TT hit yet, we insert an eval without a move here in case of forward pruning (depth is negative, bound is none) ...
   // Be carefull here, _data2 in Entry is always (INVALIDMOVE,B_none,-2) here, so that collisions are a lot more likely
   if (!pvsData.ttHit) TT::setEntry(*this, pHash, INVALIDMOVE, TT::createHashScore(evalScore, height), TT::createHashScore(staticScore, height), TT::B_none, -2);

   // if TT hit, we can use entry score as a best draft 
   // but we set evalScoreIsHashScore to be aware of that !
   if (pvsData.ttHit && !pvsData.isInCheck 
     && ((pvsData.bound == TT::B_alpha && e.s < evalScore) || (pvsData.bound == TT::B_beta && e.s > evalScore) || (pvsData.bound == TT::B_exact))){
     evalScore = TT::adjustHashScore(e.s, height);
     pvsData.evalScoreIsHashScore = true;
     stats.incr(Stats::sid_staticScoreIsFromSearch);
   }

   // take **initial** position situation (from game history) into account for pruning ? 
   ///@todo retry this
   pvsData.isBoomingAttack = positionEvolution == MoveDifficultyUtil::PE_boomingAttack;
   pvsData.isBoomingDefend = positionEvolution == MoveDifficultyUtil::PE_boomingDefence;
   pvsData.isMoobingAttack = positionEvolution == MoveDifficultyUtil::PE_moobingAttack;
   pvsData.isMoobingDefend = positionEvolution == MoveDifficultyUtil::PE_moobingDefence;

   // take **initial** position situation (from IID variability) into account for pruning ? 
   ///@todo retry this
   pvsData.isEmergencyDefence = moveDifficulty == MoveDifficultyUtil::MD_moobDefenceIID;
   pvsData.isEmergencyAttack  = moveDifficulty == MoveDifficultyUtil::MD_moobAttackIID;

   // take **current** position danger level into account for purning
   // no HCE has been done, we need to work a little to get data from evaluation of the position
   if (!evalData.evalDone) {
      EvalScore mobilityScore = {0, 0};
      array2d<BitBoard,2,6> checkers     = {{emptyBitBoard}};
      array2d<BitBoard,2,6> attFromPiece = {{emptyBitBoard}};
      colored<BitBoard> att              = {emptyBitBoard, emptyBitBoard};
      colored<BitBoard> att2             = {emptyBitBoard, emptyBitBoard};

      evalFeatures(p, attFromPiece, att, att2, checkers, evalData.danger, mobilityScore, evalData.mobility);

      evalData.haveThreats[Co_White] = (att[Co_White] & p.allPieces[Co_Black]) != emptyBitBoard;
      evalData.haveThreats[Co_Black] = (att[Co_Black] & p.allPieces[Co_White]) != emptyBitBoard;

      evalData.goodThreats[Co_White] = ((attFromPiece[Co_White][P_wp - 1] & p.allPieces[Co_Black] & ~p.blackPawn()) |
                                    (attFromPiece[Co_White][P_wn - 1] & (p.blackQueen() | p.blackRook())) |
                                    (attFromPiece[Co_White][P_wb - 1] & (p.blackQueen() | p.blackRook())) |
                                    (attFromPiece[Co_White][P_wr - 1] & p.blackQueen())) != emptyBitBoard;
      evalData.goodThreats[Co_Black] = ((attFromPiece[Co_Black][P_wp - 1] & p.allPieces[Co_White] & ~p.whitePawn()) |
                                    (attFromPiece[Co_Black][P_wn - 1] & (p.whiteQueen() | p.whiteRook())) |
                                    (attFromPiece[Co_Black][P_wb - 1] & (p.whiteQueen() | p.whiteRook())) |
                                    (attFromPiece[Co_Black][P_wr - 1] & p.whiteQueen())) != emptyBitBoard;
   }

   // if the position is really intense, eventually for both side (thus very sharp), will try to prune/reduce a little less
   const int dangerFactor          = (evalData.danger[Co_White] + evalData.danger[Co_Black]) / SearchConfig::dangerDivisor;
   const bool isDangerPrune        = dangerFactor >= SearchConfig::dangerLimitPruning;
   const bool isDangerForwardPrune = dangerFactor >= SearchConfig::dangerLimitForwardPruning;
   const bool isDangerRed          = dangerFactor >= SearchConfig::dangerLimitReduction;
   ///@todo this is not used for now
   if (isDangerPrune)        stats.incr(Stats::sid_dangerPrune);
   if (isDangerForwardPrune) stats.incr(Stats::sid_dangerForwardPrune);
   if (isDangerRed)          stats.incr(Stats::sid_dangerReduce);

   // if we are doing a big attack, we will look for tactics involving sacrifices
   const int dangerGoodAttack  = evalData.danger[~p.c] / SearchConfig::dangerDivisor;
   // unless if we are ourself under attack
   const int dangerUnderAttack = evalData.danger[p.c]  / SearchConfig::dangerDivisor;

   // when score is fluctuating a lot in the main ID search, let's prune a bit less
   const DepthType emergencyDepthCorrection = pvsData.isEmergencyDefence || pvsData.isEmergencyAttack; 
   // take asymetry into account, prune less when it is not our turn
   const DepthType asymetryDepthCorrection = 0; //theirTurn; ///@todo
   // when score is fluctuating a lot in the current game, let's prune a bit less
   //const DepthType situationDepthCorrection = isBoomingAttack + isMoobingAttack;
   
   // take current situation and asymetry into account.
   const DepthType depthCorrection = emergencyDepthCorrection + asymetryDepthCorrection;

   // some heuristic will depend on not-updated initial alpha
   const ScoreType alphaInit = alpha;

   // moves loop data
   MoveList moves;
   bool     moveGenerated    = false;
   bool     capMoveGenerated = false;

   // nmp things
   pvsData.mateThreat = false;
   MiniMove refutation = INVALIDMINIMOVE;

   // a depth that take TT depth into account
   pvsData.marginDepth = std::max(1, depth - (pvsData.evalScoreIsHashScore ? e.d : 0)); 

   // is the reported static eval better than a move before in the search tree ?
   pvsData.improving = (!pvsData.isInCheck && height > 1 && stack[p.halfmoves].eval >= stack[p.halfmoves - 2].eval);
   
   // in end game situations, some prudence is required with pruning
   pvsData.isNotPawnEndGame = p.mat[p.c][M_t] > 0;
   pvsData.lessZugzwangRisk = pvsData.isNotPawnEndGame || evalData.mobility[p.c] > 4;

   // forward prunings (only at not pvnode)
   if constexpr(!pvnode){
      // removing the !isMateScore(beta) is not losing that much elo and allow for better check mate finding ...
      if (!DynamicConfig::mateFinder && !pvsData.rootnode && !pvsData.isInCheck /*&& !isMateScore(beta)*/) {

         // static null move
         if (SearchConfig::doStaticNullMove && !isMateScore(evalScore) && pvsData.lessZugzwangRisk && SearchConfig::staticNullMoveCoeff.isActive(depth, pvsData.evalScoreIsHashScore) ) {
            const ScoreType margin = SearchConfig::staticNullMoveCoeff.threshold(depth, evalData.gp, pvsData.evalScoreIsHashScore, pvsData.improving);
            if (evalScore >= beta + margin) return stats.incr(Stats::sid_staticNullMove), evalScore - margin;
         }

         // (absence of) opponent threats pruning (idea origin from Koivisto)
         if ( SearchConfig::doThreatsPruning && !isMateScore(evalScore) && pvsData.lessZugzwangRisk && SearchConfig::threatCoeff.isActive(depth, pvsData.evalScoreIsHashScore) &&
            !evalData.goodThreats[~p.c] && evalScore > beta + SearchConfig::threatCoeff.threshold(depth, evalData.gp, pvsData.evalScoreIsHashScore, pvsData.improving) ){
            return stats.incr(Stats::sid_threatsPruning), beta;
         }
/*
         // own threats pruning
         if ( SearchConfig::doThreatsPruning && !isMateScore(evalScore) && pvsData.lessZugzwangRisk && SearchConfig::ownThreatCoeff.isActive(depth, pvsData.evalScoreIsHashScore) &&
            evalData.goodThreats[p.c] && !evalData.goodThreats[~p.c] && evalScore > beta + SearchConfig::ownThreatCoeff.threshold(depth, evalData.gp, pvsData.evalScoreIsHashScore, pvsData.improving) ){
            return stats.incr(Stats::sid_threatsPruning), beta;
         }
*/
         // razoring
         const ScoreType rAlpha = alpha - SearchConfig::razoringCoeff.threshold(pvsData.marginDepth, evalData.gp, pvsData.evalScoreIsHashScore, pvsData.improving);
         if (SearchConfig::doRazoring && SearchConfig::razoringCoeff.isActive(depth, pvsData.evalScoreIsHashScore) && evalScore <= rAlpha) {
            stats.incr(Stats::sid_razoringTry);
            const ScoreType qScore = qsearch(alpha, beta, p, height, seldepth, 0, true, pvnode, pvsData.isInCheck);
            if (stopFlag) return STOPSCORE;
            if (depth < 2 && pvsData.evalScoreIsHashScore) return stats.incr(Stats::sid_razoringNoQ), qScore;
            if (qScore <= alpha) return stats.incr(Stats::sid_razoring), qScore;
         }

         // null move
         if (SearchConfig::doNullMove && !subSearch && 
            depth >= SearchConfig::nullMoveMinDepth &&
            pvsData.lessZugzwangRisk && 
            pvsData.withoutSkipMove &&
            evalScore >= beta + SearchConfig::nullMoveMargin && 
            evalScore >= stack[p.halfmoves].eval &&
            stack[p.halfmoves].p.lastMove != NULLMOVE && 
            (height >= nullMoveMinPly || nullMoveVerifColor != p.c)) {
            PVList nullPV;
            stats.incr(Stats::sid_nullMoveTry);
            const DepthType R = SearchConfig::nullMoveReductionInit +
                              depth / SearchConfig::nullMoveReductionDepthDivisor + 
                              std::min((evalScore - beta) / SearchConfig::nullMoveDynamicDivisor, 5); // adaptative
            ///@todo try to minimize sid_nullMoveTry2 versus sid_nullMove
            const ScoreType nullIIDScore = evalScore; // pvs<false, false>(beta - 1, beta, p, std::max(depth/4,1), evaluator, height, nullPV, seldepth, isInCheck, !pvsData.cutNode);
            if (nullIIDScore >= beta + SearchConfig::nullMoveMargin2) {
               TT::Entry nullE;
               const DepthType nullDepth = depth - R;
               TT::getEntry(*this, p, pHash, nullDepth, nullE);
               if (nullE.h == nullHash ||
                  nullE.s >= beta) { // avoid null move search if TT gives a score < beta for the same depth ///@todo check this again !
                  stats.incr(Stats::sid_nullMoveTry2);
                  Position pN = p;
                  applyNull(*this, pN);
                  assert(pN.halfmoves < MAX_PLY && pN.halfmoves >= 0);
                  stack[pN.halfmoves].p = pN; ///@todo another expensive copy !!!!
                  stack[pN.halfmoves].h = pN.h;
                  ScoreType nullscore   = -pvs<false>(-beta, -beta + 1, pN, nullDepth, height + 1, nullPV, seldepth, extensions, pvsData.isInCheck, !pvsData.cutNode);
                  if (stopFlag) return STOPSCORE;
                  TT::Entry nullEThreat;
                  TT::getEntry(*this, pN, computeHash(pN), 0, nullEThreat);
                  if (nullEThreat.h != nullHash && nullEThreat.m != INVALIDMINIMOVE) refutation = nullEThreat.m;
                  if (isMatedScore(nullscore)) pvsData.mateThreat = true;
                  if (nullscore >= beta) { // verification search
                     if ( (!pvsData.lessZugzwangRisk || depth > SearchConfig::nullMoveVerifDepth) && nullMoveMinPly == 0){
                        stats.incr(Stats::sid_nullMoveTry3);
                        nullMoveMinPly = height + 3*nullDepth/4;
                        nullMoveVerifColor = p.c;
                        nullscore = pvs<false>(beta - 1, beta, p, nullDepth, height+1, nullPV, seldepth, extensions, pvsData.isInCheck, false);
                        nullMoveMinPly = 0;
                        nullMoveVerifColor = Co_None;
                        if (stopFlag) return STOPSCORE;
                        if (nullscore >= beta ) return stats.incr(Stats::sid_nullMove2), nullscore;
                     }
                     else {
                        return stats.incr(Stats::sid_nullMove), (isMateScore(nullscore) ? beta : nullscore);
                     }
                  }
               }
            }
         }

         // ProbCut
         if (SearchConfig::doProbcut && depth >= SearchConfig::probCutMinDepth && !isMateScore(beta) && evalData.haveThreats[p.c]) {
            stats.incr(Stats::sid_probcutTry);
            int probCutCount = 0;
            const ScoreType betaPC = beta + SearchConfig::probCutMargin;
            capMoveGenerated = true;
            MoveGen::generate<MoveGen::GP_cap>(p, moves);
   #ifdef USE_PARTIAL_SORT
            MoveSorter::score(*this, moves, p, evalData.gp, height, pvsData.cmhPtr, true, pvsData.isInCheck, pvsData.validTTmove ? &e : NULL);
            size_t offset = 0;
            const Move* it = nullptr;
            while ((it = MoveSorter::pickNext(moves, offset)) && probCutCount < SearchConfig::probCutMaxMoves /*+ 2*pvsData.cutNode*/) {
   #else
            MoveSorter::scoreAndSort(*this, moves, p, evalData.gp, height, pvsData.cmhPtr, true, pvsData.isInCheck, pvsData.validTTmove ? &e : NULL);
            for (auto it = moves.begin(); it != moves.end() && probCutCount < SearchConfig::probCutMaxMoves /*+ 2*pvsData.cutNode*/; ++it) {
   #endif
               if ((pvsData.validTTmove && sameMove(e.m, *it)) || isBadCap(*it)) continue; // skip TT move if quiet or bad captures
               Position p2 = p;
               const Position::MoveInfo moveInfo(p2,*it);
               if (!applyMove(p2, moveInfo, true)) continue;
   #ifdef WITH_NNUE
               NNUEEvaluator newEvaluator = p.evaluator();
               p2.associateEvaluator(newEvaluator);
               applyMoveNNUEUpdate(p2, moveInfo);
   #endif            
               ++probCutCount;
               ScoreType scorePC = -qsearch(-betaPC, -betaPC + 1, p2, height + 1, seldepth, 0, true, pvnode);
               PVList pcPV;
               if (stopFlag) return STOPSCORE;
               if (scorePC >= betaPC) {
                  stats.incr(Stats::sid_probcutTry2);
                  scorePC = -pvs<false>(-betaPC, -betaPC + 1, p2, depth - SearchConfig::probCutMinDepth + 1, height + 1, pcPV, seldepth, extensions, 
                                       isPosInCheck(p2), !pvsData.cutNode);
               }
               if (stopFlag) return STOPSCORE;
               if (scorePC >= betaPC) return stats.incr(Stats::sid_probcut), scorePC;
            }
         }
      }
   }

   // Ed Schroder style IIR 
   if ( SearchConfig::doIIR && depth >= SearchConfig::iirMinDepth && !pvsData.ttHit){
      stats.incr(Stats::sid_iid);
      depth -= SearchConfig::iirReduction;
   }

   // Classic IID
   if (SearchConfig::doIID && !pvsData.validTTmove /*|| e.d < depth-4*/) {
      if (((pvnode && depth >= SearchConfig::iidMinDepth) || (pvsData.cutNode && depth >= SearchConfig::iidMinDepth2))) { ///@todo try with cutNode only ?
         stats.incr(Stats::sid_iid);
         PVList iidPV;
         DISCARD pvs<pvnode>(alpha, beta, p, depth / 2, height, iidPV, seldepth, extensions, pvsData.isInCheck, pvsData.cutNode, skipMoves);
         if (stopFlag) return STOPSCORE;
         TT::getEntry(*this, p, pHash, 0, e);
         pvsData.ttHit       = e.h != nullHash;
         pvsData.validTTmove = pvsData.ttHit && e.m != INVALIDMINIMOVE;
         pvsData.bound       = static_cast<TT::Bound>(e.b & ~TT::B_allFlags);
         pvsData.ttPV        = pvnode || (pvsData.ttHit && (e.b & TT::B_ttPVFlag));
         pvsData.ttIsCheck   = pvsData.validTTmove && (e.b & TT::B_isCheckFlag);
         pvsData.formerPV    = pvsData.ttPV && !pvnode;
         // note that e.d >= -1 here is quite redondant with bound != TT::B_None, but anyway ...
         if (pvsData.ttHit && !pvsData.isInCheck && /*e.d >= -1 &&*/
             ((pvsData.bound == TT::B_alpha && e.s < evalScore) || (pvsData.bound == TT::B_beta && e.s > evalScore) || (pvsData.bound == TT::B_exact))) {
            evalScore = TT::adjustHashScore(e.s, height);
            pvsData.evalScoreIsHashScore = true;
            pvsData.marginDepth = std::max(1, depth - (pvsData.evalScoreIsHashScore ? e.d : 0)); // a depth that take TT depth into account
         }
      }
   }

   // reset killer
   killerT.killers[height + 1][0] = killerT.killers[height + 1][1] = INVALIDMOVE;

   // backup refutation from null move heuristic
   stack[p.halfmoves].threat = refutation;

   // verify threat evolution for Botvinnik-Markoff extension 
   const MiniMove formerRefutation = height > 1 ? stack[p.halfmoves - 2].threat : INVALIDMINIMOVE;
   pvsData.BMextension = DynamicConfig::useNNUE && height > 1 && 
                         isValidMove(refutation) && isValidMove(formerRefutation) &&
                         (sameMove(refutation, formerRefutation) ||
                          (correctedMove2ToKingDest(refutation) == correctedMove2ToKingDest(formerRefutation) && isCapture(refutation)));

   // verify situation for various pruning heuristics
   if (!pvsData.rootnode) {
      // LMP
      pvsData.lmp = SearchConfig::doLMP && depth <= SearchConfig::lmpMaxDepth;
      // futility
      const ScoreType futilityScore = alpha - SearchConfig::futilityPruningCoeff.threshold(pvsData.marginDepth, evalData.gp, pvsData.evalScoreIsHashScore, pvsData.improving);
      pvsData.futility = SearchConfig::doFutility && SearchConfig::futilityPruningCoeff.isActive(depth, pvsData.evalScoreIsHashScore) && evalScore <= futilityScore;
      // history pruning
      pvsData.historyPruning = SearchConfig::doHistoryPruning && pvsData.isNotPawnEndGame && SearchConfig::historyPruningCoeff.isActive(depth, pvsData.improving);
      // CMH pruning
      pvsData.CMHPruning = SearchConfig::doCMHPruning && pvsData.isNotPawnEndGame && depth < SearchConfig::CMHMaxDepth[pvsData.improving];
      // capture history pruning
      pvsData.capHistoryPruning = SearchConfig::doCapHistoryPruning && pvsData.isNotPawnEndGame && SearchConfig::captureHistoryPruningCoeff.isActive(depth, pvsData.improving);
   }

#ifdef WITH_SYZYGY
   if (pvsData.rootnode && pvsData.withoutSkipMove && (BB::countBit(p.allPieces[Co_White] | p.allPieces[Co_Black])) <= SyzygyTb::MAX_TB_MEN) {
      tbScore = 0;
      MoveList movesTB;
      if (SyzygyTb::probe_root(*this, p, tbScore, movesTB) < 0) { // only good moves if TB success
         ++stats.counters[Stats::sid_tbFail];
         if (capMoveGenerated) MoveGen::generate<MoveGen::GP_quiet>(p, moves, true);
         else
            if ( pvsData.isInCheck ) MoveGen::generate<MoveGen::GP_evasion>(p, moves, false);
            else MoveGen::generate<MoveGen::GP_all>(p, moves, false);
      }
      else {
         moves = movesTB;
         ++stats.counters[Stats::sid_tbHit2];
      }
      moveGenerated = true;
   }
#endif

   // bestScore / bestMove will be updated in move loop and returned at the end and inserted in TT
   ScoreType bestScore = matedScore(height);
   Move bestMove = INVALIDMOVE;

   TT::Bound hashBound = TT::B_alpha;
   
   // try the tt move before move generation (if not skipped move)
   if (pvsData.validTTmove && 
       (moves.empty() || !pvsData.rootnode) && // avoid trying TT move at root if coming from a TB probe hit that filled moves
       !isSkipMove(e.m, skipMoves)) {

#ifdef DEBUG_APPLY
      // to debug race condition in entry affectation !
      if (!isPseudoLegal(p, e.m)) { Logging::LogIt(Logging::logFatal) << "invalide TT move !"; }
#endif

      pvsData.ttMoveTried = true;
      pvsData.isTTMove = true;
      bestMove = e.m; // in order to preserve tt move for alpha bound entry

      Position p2 = p;
      const Position::MoveInfo moveInfo(p2,e.m);
      if (applyMove(p2, moveInfo, true)) {
         // prefetch as soon as possible
         TT::prefetch(computeHash(p2));

#ifdef WITH_NNUE
         NNUEEvaluator newEvaluator = p.evaluator();
         p2.associateEvaluator(newEvaluator);
         applyMoveNNUEUpdate(p2, moveInfo);
#endif

         ++pvsData.validMoveCount;
         ++pvsData.validNonPrunedCount;

         pvsData.earlyMove = true;
         pvsData.isQuiet = Move2Type(e.m) == T_std && !isNoisy(p,e.m);
         if (pvsData.isQuiet) ++pvsData.validQuietMoveCount;

         if (isCapture(e.m)) pvsData.ttMoveIsCapture = true;
         pvsData.isCheck = pvsData.ttIsCheck || isPosInCheck(p2);

         assert(p2.halfmoves < MAX_PLY && p2.halfmoves >= 0);
         stack[p2.halfmoves].p = p2; ///@todo another expensive copy !!!!
         stack[p2.halfmoves].h = p2.h;
         
#ifdef DEBUG_TT_CHECK
         if (pvsData.ttIsCheck && !isPosInCheck(p2)){
            std::cout << "Error ttIsCheck" << std::endl;
            std::cout << ToString(p2) << std::endl;
         }
#endif

         // used for extensions heuristics
         const Square to = Move2To(e.m);
         pvsData.isAdvancedPawnPush = PieceTools::getPieceType(p,Move2From(e.m)) == P_wp && (SQRANK(to) > 5 || SQRANK(to) < 2);

         // extensions
         DepthType extension = 0;
         if (DynamicConfig::level > 80) {
            // singular move extension
            if (pvsData.withoutSkipMove 
              && depth >= SearchConfig::singularExtensionDepth 
              && !pvsData.rootnode 
              && !isMateScore(e.s) 
              && (pvsData.bound == TT::B_exact || pvsData.bound == TT::B_beta) 
              && e.d >= depth - 3) {
               const ScoreType betaC = e.s - 2 * depth;
               PVList          sePV;
               DepthType       seSeldetph = 0;
               std::vector<MiniMove> skip({e.m});
               const ScoreType       score = pvs<false>(betaC - 1, betaC, p, depth / 2, height, sePV, seSeldetph, extensions, pvsData.isInCheck, pvsData.cutNode, &skip);
               if (stopFlag) return STOPSCORE;
               if (score < betaC) { // TT move is singular
                  stats.incr(Stats::sid_singularExtension), pvsData.ttMoveSingularExt=!pvsData.ttMoveIsCapture, ++extension;
                  // TT move is "very singular" : kind of single reply extension
                  if (score < betaC - 4 * depth) {
                     stats.incr(Stats::sid_singularExtension2);
                     ++extension;
                  }
               }
               // multi cut (at least the TT move and another move are above beta)
               else if (betaC >= beta) {
                  return stats.incr(Stats::sid_singularExtension3), betaC; // fail-soft
               }
               // if TT move is above beta, we try a reduce search early to see if another move is above beta (from SF)
               else if (e.s >= beta) {
                  const ScoreType score2 = pvs<false>(beta - 1, beta, p, depth - 4, height, sePV, seSeldetph, extensions, pvsData.isInCheck, pvsData.cutNode, &skip);
                  if (score2 > beta) return stats.incr(Stats::sid_singularExtension4), beta; // fail-hard
               }
            }
            
            // other extensions
            extension = getExtension(p, depth, e.m, pvsData, evalData, evalScore, extension);
         }

         PVList childPV;
         ScoreType ttScore;
         ttScore = -pvs<pvnode>(-beta, -alpha, p2, depth - 1 + extension, height + 1, childPV, seldepth, static_cast<DepthType>(extensions + extension), pvsData.isCheck, !pvsData.cutNode);

         if (stopFlag) return STOPSCORE;

         if (pvsData.rootnode) { 
            rootScores.push_back({e.m, ttScore}); 
         }

         if (ttScore > bestScore) {
            if (pvsData.rootnode) previousBest = e.m;
            bestScore = ttScore;
            bestMove  = e.m;
            pvsData.bestMoveIsCheck = pvsData.isCheck;
            if (ttScore > alpha) {
               hashBound = TT::B_exact;
               if constexpr(pvnode) updatePV(pv, bestMove, childPV);
               if (ttScore >= beta) {
                  stats.incr(Stats::sid_ttbeta);

                  // increase history bonus of this move
                  if (!pvsData.isInCheck){
                     if (pvsData.isQuiet /*&& depth > 1*/) // quiet move history
                        updateTables(*this, p, depth + (ttScore > (beta + SearchConfig::betaMarginDynamicHistory)), height, bestMove, TT::B_beta, pvsData.cmhPtr);
                     else if (isCapture(bestMove)) // capture history
                        historyT.updateCap<1>(depth + (ttScore > (beta + SearchConfig::betaMarginDynamicHistory)), bestMove, p);
                  }

                  TT::setEntry(*this, pHash, bestMove, TT::createHashScore(ttScore, height), TT::createHashScore(evalScore, height),
                                      static_cast<TT::Bound>(TT::B_beta |
                                      (pvsData.ttPV            ? TT::B_ttPVFlag      : TT::B_none) |
                                      (pvsData.bestMoveIsCheck ? TT::B_isCheckFlag   : TT::B_none) |
                                      (pvsData.isInCheck       ? TT::B_isInCheckFlag : TT::B_none)),
                                      depth);
                  return ttScore;
               }
               stats.incr(Stats::sid_ttalpha);
               alpha = ttScore;
            }
         }
         else if (pvsData.rootnode && !pvsData.isInCheck && ttScore < alpha - SearchConfig::failLowRootMargin /*&& !isMateScore(ttScore)*/) {
            return alpha - SearchConfig::failLowRootMargin;
         }
      }
   }

   ScoreType score = matedScore(height);
   bool skipQuiet = false;
   bool skipCap = false;

   // depending if probecut already generates capture or not, generate all moves or only missing quiets
   if (!moveGenerated) {
      if (capMoveGenerated) MoveGen::generate<MoveGen::GP_quiet>(p, moves, true);
      else
         if ( pvsData.isInCheck ) MoveGen::generate<MoveGen::GP_evasion>(p, moves, false);
         else MoveGen::generate<MoveGen::GP_all>(p, moves, false);
   }
   if (moves.empty()) return pvsData.isInCheck ? matedScore(height) : drawScore(p, height);

#ifdef USE_PARTIAL_SORT
   MoveSorter::score(*this, moves, p, evalData.gp, height, pvsData.cmhPtr, true, pvsData.isInCheck, pvsData.validTTmove ? &e : NULL,
                     refutation != INVALIDMINIMOVE && isCapture(Move2Type(refutation)) ? refutation : INVALIDMINIMOVE);
   size_t offset = 0;
   const Move* it = nullptr;
   while ((it = MoveSorter::pickNext(moves, offset)) && !stopFlag) {
#else
   MoveSorter::scoreAndSort(*this, moves, p, evalData.gp, height, pvsData.cmhPtr, true, pvsData.isInCheck, pvsData.validTTmove ? &e : NULL,
                            refutation != INVALIDMINIMOVE && isCapture(Move2Type(refutation)) ? refutation : INVALIDMINIMOVE);
   for (auto it = moves.begin(); it != moves.end() && !stopFlag; ++it) {
#endif

      pvsData.isTTMove = false;
      pvsData.isQuiet = Move2Type(*it) == T_std && !isNoisy(p,*it);
      // skip quiet if LMP was triggered (!!even if move gives check now!!)
      if (skipQuiet && pvsData.isQuiet && !pvsData.isInCheck){
         stats.incr(Stats::sid_lmp);
         continue;
      }
      // skip other bad capture (!!even if move gives check now!!)
      if (skipCap && Move2Type(*it) == T_capture && !pvsData.isInCheck){
         stats.incr(Stats::sid_see2);
         continue;
      }
      if (isSkipMove(*it, skipMoves)) continue;        // skipmoves
      if (pvsData.validTTmove && pvsData.ttMoveTried && sameMove(e.m, *it)) continue; // already tried

      Position p2 = p;
      const Position::MoveInfo moveInfo(p2,*it);

      // do not apply NNUE update here, but later after prunning, right before next call to pvs
      if (!applyMove(p2, moveInfo, true)) continue;

      // prefetch as soon as possible
      TT::prefetch(computeHash(p2));
      const Square to = Move2To(*it);
#ifdef DEBUG_KING_CAP
      if (p.c == Co_White && to == p.king[Co_Black]) return matingScore(height - 1);
      if (p.c == Co_Black && to == p.king[Co_White]) return matingScore(height - 1);
#endif

      ++pvsData.validMoveCount;
      if (pvsData.isQuiet) ++pvsData.validQuietMoveCount;

      pvsData.isCheck = isPosInCheck(p2);
      pvsData.isAdvancedPawnPush = PieceTools::getPieceType(p, Move2From(*it)) == P_wp && (SQRANK(to) > 5 || SQRANK(to) < 2);
      pvsData.earlyMove = pvsData.validMoveCount < (2 /*+2*pvsData.rootnode*/);

      // extensions
      DepthType extension = 0;
      if (DynamicConfig::level > 80) {
         extension = getExtension(p, depth, *it, pvsData, evalData, evalScore, extension);
      }
      PVList childPV;
      // PVS
      if (pvsData.earlyMove || !SearchConfig::doPVS){
         stack[p2.halfmoves].p = p2;
         stack[p2.halfmoves].h = p2.h;         
#ifdef WITH_NNUE
         NNUEEvaluator newEvaluator = p.evaluator();
         p2.associateEvaluator(newEvaluator);
         applyMoveNNUEUpdate(p2, moveInfo);
#endif
         score = -pvs<pvnode>(-beta, -alpha, p2, depth - 1 + extension, height + 1, childPV, seldepth, static_cast<DepthType>(extensions + extension), pvsData.isCheck, false);
         ++pvsData.validNonPrunedCount;
      }
      else {
         // reductions & prunings
         const bool isPrunable           = /*pvsData.isNotPawnEndGame &&*/ !pvsData.isAdvancedPawnPush && !isMateScore(alpha) && !killerT.isKiller(*it, height) && !DynamicConfig::mateFinder;
         const bool isReductible         = /*pvsData.isNotPawnEndGame &&*/ !pvsData.isAdvancedPawnPush && !DynamicConfig::mateFinder;
         const bool noCheck              = !pvsData.isInCheck && !pvsData.isCheck;
         const bool isPrunableStd        = isPrunable && pvsData.isQuiet;
         const bool isPrunableStdNoCheck = isPrunableStd && noCheck;
         const bool isPrunableCap        = isPrunable && Move2Type(*it) == T_capture && noCheck;
         const bool isPrunableBadCap     = isPrunableCap && isBadCap(*it);

         // futility
         if (pvsData.futility && isPrunableStdNoCheck) {
            stats.incr(Stats::sid_futility);
            continue;
         }

         // LMP
         if (pvsData.lmp && isPrunableStdNoCheck && pvsData.validMoveCount > SearchConfig::lmpLimit[pvsData.improving][depth + depthCorrection]) {
            stats.incr(Stats::sid_lmp);
            skipQuiet = true;
            continue;
         }

         // History pruning (including CMH)
         if (pvsData.historyPruning && isPrunableStdNoCheck &&
             Move2Score(*it) < SearchConfig::historyPruningCoeff.threshold(depth, evalData.gp, pvsData.improving, pvsData.cutNode)) {
            stats.incr(Stats::sid_historyPruning);
            continue;
         }

         // CMH pruning alone
         if (pvsData.CMHPruning && isPrunableStdNoCheck) {
            if (isCMHBad(p, Move2From(*it), Move2To(*it), pvsData.cmhPtr, -HISTORY_MAX/4)) {
               stats.incr(Stats::sid_CMHPruning);
               continue;
            }
         }

         // capture history pruning
         if (pvsData.capHistoryPruning && isPrunableCap &&
              historyT.historyCap[PieceIdx(p.board_const(Move2From(*it)))][to][Abs(p.board_const(to))-1] < SearchConfig::captureHistoryPruningCoeff.threshold(depth, evalData.gp, pvsData.improving, pvsData.cutNode)){
            stats.incr(Stats::sid_capHistPruning);
            continue;
         }

         // SEE (capture)
         if (!pvsData.rootnode && isPrunableBadCap) {
            if (pvsData.futility) {
               stats.incr(Stats::sid_see);
               skipCap = true;
               continue;
            }
            else if ( badCapScore(*it) < - 1 * SearchConfig::seeCaptureFactor * (depth + depthCorrection + std::max(0, dangerGoodAttack - dangerUnderAttack)/SearchConfig::seeCapDangerDivisor)) {
               stats.incr(Stats::sid_see2);
               skipCap = true;
               continue;
            }
         }

         // LMR
         DepthType reduction = 0;
         if (SearchConfig::doLMR && depth >= SearchConfig::lmrMinDepth && isReductible) {
            reduction = getReduction(p, depth, *it, pvsData, evalData, evalScore, extension);
         }

         DepthType nextDepth = depth - 1 - reduction + extension;

         // SEE (quiet)
         ScoreType seeValue = 0;
         if (isPrunableStdNoCheck) {
            seeValue = SEE(p, *it);
            if (!pvsData.rootnode && seeValue < -SearchConfig::seeQuietFactor * (nextDepth + depthCorrection) * (nextDepth + std::max(0, dangerGoodAttack - dangerUnderAttack)/SearchConfig::seeQuietDangerDivisor)){
               stats.incr(Stats::sid_seeQuiet);
               continue;
            }
            // reduce bad quiet more
            else if (seeValue < 0 && nextDepth > 1) --nextDepth;
         }

         ++pvsData.validNonPrunedCount;

         // PVS
         stack[p2.halfmoves].p = p2;
         stack[p2.halfmoves].h = p2.h;         
#ifdef WITH_NNUE
         NNUEEvaluator newEvaluator = p.evaluator();
         p2.associateEvaluator(newEvaluator);
         applyMoveNNUEUpdate(p2, moveInfo);
#endif
         score = -pvs<false>(-alpha - 1, -alpha, p2, nextDepth, height + 1, childPV, seldepth, static_cast<DepthType>(extensions + extension), pvsData.isCheck, true);
         if (reduction > 0 && score > alpha) {
            const bool lateExtension = false; ///@todo score > alpha + SearchConfig::lmrLateExtensionMargin;
            stats.incr(Stats::sid_lmrFail);
            childPV.clear();
            score = -pvs<false>(-alpha - 1, -alpha, p2, depth - 1 + extension + lateExtension, height + 1, childPV, seldepth, static_cast<DepthType>(extensions + extension), pvsData.isCheck, !pvsData.cutNode);
         }
         if ( score > alpha && (pvsData.rootnode || score < beta)) {
            stats.incr(Stats::sid_pvsFail);
            childPV.clear();
            // potential new pv node
            score = -pvs<true>(-beta, -alpha, p2, depth - 1 + extension, height + 1, childPV, seldepth, static_cast<DepthType>(extensions + extension), pvsData.isCheck, false);
         }
      }

      if (stopFlag) return STOPSCORE;

      if (pvsData.rootnode) { 
         rootScores.push_back({*it, score}); 
      }
      if (score > bestScore) {
         if (pvsData.rootnode) previousBest = *it;
         bestScore = score;
         bestMove  = *it;
         pvsData.bestMoveIsCheck = pvsData.isCheck;
         //bestScoreUpdated = true;
         if (score > alpha) {
            if constexpr(pvnode) updatePV(pv, bestMove, childPV);
            //alphaUpdated = true;
            alpha = score;
            hashBound = TT::B_exact;
            if (score >= beta) {
               stats.incr(Stats::sid_beta);
               if (!pvsData.isInCheck){
                  const DepthType bonus = depth + (score > beta + SearchConfig::betaMarginDynamicHistory);
                  if(pvsData.isQuiet /*&& depth > 1*/ /*&& !( depth == 1 && validQuietMoveCount == 1 )*/) { // quiet move history
                     // increase history bonus of this move
                     updateTables(*this, p, bonus, height, bestMove, TT::B_beta, pvsData.cmhPtr);
                     // reduce history bonus of all previous
                     for (auto it2 = moves.begin(); it2 != moves.end() && !sameMove(*it2, bestMove); ++it2) {
                        if (Move2Type(*it2) == T_std)
                           historyT.update<-1>(bonus, *it2, p, pvsData.cmhPtr);
                     }
                  }
                  else if( isCapture(bestMove)){ // capture history
                     historyT.updateCap<1>(bonus, bestMove, p);
                     for (auto it2 = moves.begin(); it2 != moves.end() && !sameMove(*it2, bestMove); ++it2) {
                        if (isCapture(*it2))
                           historyT.updateCap<-1>(bonus, *it2, p);
                     }
                  }
               }
               hashBound = TT::B_beta;
               break;
            }
            stats.incr(Stats::sid_alpha);
         }
      }
      else if (pvsData.rootnode && !pvsData.isInCheck && pvsData.earlyMove && score < alpha - SearchConfig::failLowRootMargin) {
         return alpha - SearchConfig::failLowRootMargin;
      }
   }

   if (alphaInit == alpha ) stats.incr(Stats::sid_alphanoupdate);

   // check for draw and check mate
   if (pvsData.validMoveCount == 0){
      return (pvsData.isInCheck || !pvsData.withoutSkipMove) ? matedScore(height) : drawScore(p, height);
   }
   // post move loop version
   else if (is50moves(p,false)){
      return drawScore(p, height);
   }

   // insert data in TT
   TT::setEntry(*this, pHash, bestMove, TT::createHashScore(bestScore, height), TT::createHashScore(evalScore, height),
                       static_cast<TT::Bound>(hashBound |
                       (pvsData.ttPV            ? TT::B_ttPVFlag      : TT::B_none) |
                       (pvsData.bestMoveIsCheck ? TT::B_isCheckFlag   : TT::B_none) |
                       (pvsData.isInCheck       ? TT::B_isInCheckFlag : TT::B_none)),
                       depth);

   return bestScore;
}
