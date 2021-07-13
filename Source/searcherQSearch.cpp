#include "searcher.hpp"

ScoreType Searcher::qsearchNoPruning(ScoreType alpha, ScoreType beta, const Position& p, DepthType height, DepthType& seldepth, PVList* pv) {
   // is updated recursively in pvs and qsearch calls but also affected to Searcher data in order to be available inside eval.
   _height = height;

   EvalData data;
   ++stats.counters[Stats::sid_qnodes];
   const ScoreType evalScore = eval(p, data, *this);

   if (evalScore >= beta) return evalScore;
   if (evalScore > alpha) alpha = evalScore;

   const bool isInCheck = isAttacked(p, kingSquare(p));
   ScoreType  bestScore = isInCheck ? -MATE + height : evalScore;

   CMHPtrArray cmhPtr;
   getCMHPtr(p.halfmoves, cmhPtr);

   MoveList moves;
   //if ( isInCheck ) MoveGen::generate<MoveGen::GP_all>(p,moves); ///@todo generate only evasion !
   /*else*/ MoveGen::generate<MoveGen::GP_cap>(p, moves);
   MoveSorter::scoreAndSort(*this, moves, p, data.gp, height, cmhPtr, false, isInCheck);

   for (auto it = moves.begin(); it != moves.end(); ++it) {
      Position p2 = p;
#ifdef WITH_NNUE
      NNUEEvaluator newEvaluator = p.Evaluator();
      p2.associateEvaluator(newEvaluator);
#endif
      if (!applyMove(p2, *it)) continue;
      PVList childPV;
      const ScoreType score = -qsearchNoPruning(-beta, -alpha, p2, height + 1, seldepth, pv ? &childPV : nullptr);
      if (score > bestScore) {
         bestScore = score;
         if (score > alpha) {
            if (pv) updatePV(*pv, *it, childPV);
            if (score >= beta) return score;
            alpha = score;
         }
      }
   }
   return bestScore;
}

inline ScoreType bestCapture(const Position &p){
   return p.pieces_const(~p.c,P_wq) ? value(P_wq) : value(P_wr);
          /*p.pieces_const(~p.c,P_wr) ? value(P_wr) :
          p.pieces_const(~p.c,P_wb) ? value(P_wb) :
          p.pieces_const(~p.c,P_wn) ? value(P_wn) :
          p.pieces_const(~p.c,P_wp) ? value(P_wp) : 0;*/
}

inline ScoreType qDeltaMargin(const Position& p) {
   const ScoreType delta = (p.pieces_const(p.c, P_wp) & BB::seventhRank[p.c]) ? value(P_wq) : value(P_wp);
   return delta + bestCapture(p);
}

ScoreType Searcher::qsearch(ScoreType       alpha,
                            ScoreType       beta,
                            const Position& p,
                            DepthType       height,
                            DepthType&      seldepth,
                            DepthType       qply,
                            bool            qRoot,
                            bool            pvnode,
                            int8_t          isInCheckHint) {

   ///@todo more validation !!
   assert(p.halfmoves - 1 < MAX_PLY && p.halfmoves - 1 >= 0);

   // is updated recursively in pvs and qsearch calls but also affected to Searcher data in order to be available inside eval.
   _height = height;

   // no time verification in qsearch, too slow
   if (stopFlag) return STOPSCORE; 
   
   // update nodes count as soon as we enter a node
   ++stats.counters[Stats::sid_qnodes];

   alpha = std::max(alpha, (ScoreType)(-MATE + height));
   beta  = std::min(beta, (ScoreType)(MATE - height + 1));
   if (alpha >= beta) return alpha;

   // update selective depth
   seldepth = std::max(seldepth,height);

   EvalData data;
   // we cannot search deeper than MAX_DEPTH, is so just return static evaluation
   if (height >= MAX_DEPTH - 1) return eval(p, data, *this, false);

   Move bestMove = INVALIDMOVE;

   debug_king_cap(p);

   // probe TT
   TT::Entry       e;
   const Hash      pHash          = computeHash(p);
   const bool      ttDepthOk      = TT::getEntry(*this, p, pHash, 0, e);
   const TT::Bound bound          = TT::Bound(e.b & ~TT::B_allFlags);
   const bool      ttHit          = e.h != nullHash;
   const bool      validTTmove    = ttHit && e.m != INVALIDMINIMOVE;
   const bool      ttPV           = pvnode || (validTTmove && (e.b & TT::B_ttPVFlag));
   const bool      ttIsInCheck    = validTTmove && (e.b & TT::B_isInCheckFlag);
   const bool      isInCheck      = isInCheckHint != -1 ? isInCheckHint : ttIsInCheck || isAttacked(p, kingSquare(p));
   const bool      specialQSearch = isInCheck || qRoot;
   const DepthType hashDepth      = specialQSearch ? 0 : -1;
   
   // if depth of TT entry is enough
   if (ttDepthOk && e.d >= hashDepth) { 
      // if not at a pvnode, we verify bound and not shuffling with quiet moves
      // in this case we can "safely" return hash score
      if (!pvnode && p.fifty < SearchConfig::ttMaxFiftyValideDepth && ((bound == TT::B_alpha && e.s <= alpha) || (bound == TT::B_beta && e.s >= beta) || (bound == TT::B_exact))) {
         return adjustHashScore(e.s, height);
      }
   }

   // at qRoot (just coming from pvs) we check for draws (3rep, fifty and material)
   ///@todo is that gain elo ???
   if (qRoot && interiorNodeRecognizer<true, false, true>(p) == MaterialHash::Ter_Draw) return drawScore(p, height); 

   // set TT move as current best, this allows to re-insert it in pv in case there is no capture, or no move incrases alpha
   if (validTTmove && (isInCheck || isCapture(e.m))) bestMove = e.m;

   // get a static score for that position.
   ScoreType evalScore;
   // do not eval position when in check, we won't use it (won't forward prune)
   if (isInCheck){
      evalScore = -MATE + height;
      stats.incr(Stats::sid_qsearchInCheck);
   }
   // skip eval if nullmove just applied, we can hack using opposite of opponent score corrected by tempo
   // But this may don't work well with (a too) asymetric HalfKA NNUE
   else if (!DynamicConfig::useNNUE && p.lastMove == NULLMOVE && height > 0){
      evalScore = 2 * EvalConfig::tempo - stack[p.halfmoves - 1].eval;
#ifdef DEBUG_STATICEVAL      
      checkEval(p,evalScore,*this,"null move trick (qsearch)");
#endif
      stats.incr(Stats::sid_qEvalNullMoveTrick);
   }
   else {
      // if we had a TT hit (with or without associated move), we can use its eval instead of calling eval()
      if (ttHit) {
         stats.incr(Stats::sid_ttschits);
         evalScore = e.e;
#ifdef DEBUG_STATICEVAL         
         checkEval(p,evalScore,*this,"from TT (qsearch)");
#endif
         // in this case we force mid game value in sorting ... 
         // anyway, this affects only quiet move sorting, so here only for check evasion ...
         data.gp = 0.5; 
      }
      else {
         // we tried everthing ... now this position must be evaluated
         stats.incr(Stats::sid_ttscmiss);
         evalScore = eval(p, data, *this);
#ifdef DEBUG_STATICEVAL         
         checkEval(p,evalScore,*this,"from eval (qsearch)");
#endif
      }
   }
   // backup this score as the "static" one
   const ScoreType staticScore = evalScore;

   // delta pruning based on static score, and best possible capture on the board
   if (!isInCheck && SearchConfig::doQDeltaPruning && staticScore + qDeltaMargin(p) < alpha){
/*      
      if (! checkEval(p,evalScore,*this,"for delta (qsearch)")){
         std::cout << "*********************" << std::endl;
         std::cout << "DELTA" << std::endl;
         std::cout << ToString(p) << std::endl;
         std::cout << staticScore << std::endl;
         std::cout << qDeltaMargin(p) << std::endl;
         std::cout << alpha << std::endl;
      }
*/      
      return stats.incr(Stats::sid_delta), staticScore;
   }

   // early cut-off based on staticScore score
   if (staticScore >= beta) {
      if (!isInCheck && !ttHit)
         // Be carefull here, _data in Entry is always (INVALIDMOVE,B_none,-2) here, so that collisions are a lot more likely
         // depth -2 is used to ensure this will never be used directly (only evaluation score is of interest here...)
         TT::setEntry(*this, pHash, INVALIDMOVE, createHashScore(staticScore, height), createHashScore(staticScore, height), TT::B_none, -2);
      return staticScore;
   }
   else if (staticScore > alpha) alpha = staticScore;

   // maybe we can use tt score (instead of static evaluation) as a better draft if possible and not in check ?
   bool evalScoreIsHashScore = false;
   
   if (!isInCheck) {
      if (ttHit && ((bound == TT::B_alpha && e.s <= evalScore) || (bound == TT::B_beta && e.s >= evalScore) || (bound == TT::B_exact)))
         evalScore = e.s, evalScoreIsHashScore = true;
   }
   
   TT::Bound       b              = TT::B_alpha;
   ScoreType       bestScore      = /*evalScore*/staticScore; ///@todo uses TT draft instead of static score
   const ScoreType alphaInit      = alpha;
   int             validMoveCount = 0;

   // we try the tt move before move generation
   if (validTTmove && (isInCheck || isCapture(e.m))) {
      Position p2 = p;
#ifdef WITH_NNUE
      NNUEEvaluator newEvaluator = p.Evaluator();
      p2.associateEvaluator(newEvaluator);
#endif
      if (applyMove(p2, e.m)) {
         ++validMoveCount;
         //stack[p2.halfmoves].p = p2; ///@todo another expensive copy !!!!
         //stack[p2.halfmoves].h = p2.h;
         TT::prefetch(computeHash(p2));
         const ScoreType score = -qsearch(-beta, -alpha, p2, height + 1, seldepth, isInCheck ? 0 : qply + 1, false, false);
         if (score > bestScore) {
            bestMove  = e.m;
            bestScore = score;
            if (score > alpha) {
               if (score >= beta) {
                  b = TT::B_beta;
                  TT::setEntry(*this, pHash, bestMove, createHashScore(bestScore, height), createHashScore(evalScore, height),
                               TT::Bound(b | (ttPV ? TT::B_ttPVFlag : TT::B_none) | (isInCheck ? TT::B_isInCheckFlag : TT::B_none)), hashDepth);
                  return bestScore;
               }
               b = TT::B_exact;
               alpha = score;
            }
         }
      }
   }

   // generate move list
   MoveList moves;
   if (isInCheck) MoveGen::generate<MoveGen::GP_all>(p, moves); ///@todo generate only evasion !
   else
      MoveGen::generate<MoveGen::GP_cap>(p, moves); ///@todo generate only recapture if qly > 5

   const Square recapture     = VALIDMOVE(p.lastMove) ? Move2To(p.lastMove) : INVALIDSQUARE;
   const bool   onlyRecapture = qply > 5 && isCapture(p.lastMove) && recapture != INVALIDSQUARE;

   CMHPtrArray cmhPtr;
   getCMHPtr(p.halfmoves, cmhPtr);

#ifdef USE_PARTIAL_SORT
   MoveSorter::score(*this, moves, p, data.gp, height, cmhPtr, false, isInCheck, validTTmove ? &e : NULL); ///@todo warning gp is often = 0.5 here !
   size_t offset = 0;
   const Move* it = nullptr;
   while ((it = MoveSorter::pickNext(moves, offset))) {
#else
   MoveSorter::scoreAndSort(*this, moves, p, data.gp, height, cmhPtr, false, isInCheck,
                            validTTmove ? &e : NULL); ///@todo warning gp is often = 0.5 here !
   for (auto it = moves.begin(); it != moves.end(); ++it) {
#endif
      if (validTTmove && sameMove(e.m, *it)) continue; // already tried
      if (!isInCheck) {
         if (onlyRecapture && Move2To(*it) != recapture) continue; // only recapture now ...
         if (SearchConfig::doQFutility && validMoveCount &&
             staticScore + SearchConfig::qfutilityMargin[evalScoreIsHashScore] + (isPromotionCap(*it) ? (value(P_wq) - value(P_wp)) : 0) +
                     (Move2Type(*it) == T_ep ? value(P_wp) : PieceTools::getAbsValue(p, Move2To(*it))) <=
                 alphaInit) {
            stats.incr(Stats::sid_qfutility);
            continue;
         }
         if (!SEE_GE(p, *it, 0)) {
            stats.incr(Stats::sid_qsee);
            continue;
         }           
      }
      Position p2 = p;
#ifdef WITH_NNUE
      NNUEEvaluator newEvaluator = p.Evaluator();
      p2.associateEvaluator(newEvaluator);
#endif
      if (!applyMove(p2, *it)) continue;
      ++validMoveCount;
      //stack[p2.halfmoves].p = p2; ///@todo another expensive copy !!!!
      //stack[p2.halfmoves].h = p2.h;
      TT::prefetch(computeHash(p2));
      const ScoreType score = -qsearch(-beta, -alpha, p2, height + 1, seldepth, isInCheck ? 0 : qply + 1, false, false);
      if (score > bestScore) {
         bestMove  = *it;
         bestScore = score;
         if (score > alpha) {
            if (score >= beta) {
               b = TT::B_beta;
               break;
            }
            b     = TT::B_exact;
            alpha = score;
         }
      }
   }

   if (validMoveCount == 0 && isInCheck) bestScore = -MATE + height;

   TT::setEntry(*this, pHash, bestMove, createHashScore(bestScore, height), createHashScore(evalScore, height),
                TT::Bound(b | (ttPV ? TT::B_ttPVFlag : TT::B_none) | (isInCheck ? TT::B_isInCheckFlag : TT::B_none)), hashDepth);
                
   return bestScore;
}
