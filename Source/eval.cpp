#include "attack.hpp"
#include "bitboardTools.hpp"
#include "dynamicConfig.hpp"
#include "evalConfig.hpp"
#include "evalTools.hpp"
#include "hash.hpp"
#include "moveApply.hpp"
#include "positionTools.hpp"
#include "score.hpp"
#include "searcher.hpp"
#include "timers.hpp"

using namespace BB; // to much of it ...

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"

template<Color C>
FORCE_FINLINE void evalPawnPasser(const Position &p, BitBoard pieceBBiterator, EvalScore &score) {
   applyOn(pieceBBiterator, [&](const Square & k){
      const EvalScore kingNearBonus = (isValidSquare(p.king[C])  ? EvalConfig::kingNearPassedPawnSupport[chebyshevDistance(p.king[C], k)][ColorRank<C>(k)]  : 0) +
                                      (isValidSquare(p.king[~C]) ? EvalConfig::kingNearPassedPawnDefend[chebyshevDistance(p.king[~C], k)][ColorRank<~C>(k)] : 0);
      const bool unstoppable = (p.mat[~C][M_t] == 0) && ((chebyshevDistance(p.king[~C], PromotionSquare<C>(k)) - static_cast<int>(p.c != C)) >
                                                         std::min(static_cast<Square>(5), chebyshevDistance(PromotionSquare<C>(k), k)));
      if (unstoppable)
         score += ColorSignHelper<C>() * (value(P_wr) - value(P_wp)); // yes rook not queen to force promotion asap
      else
         score += (EvalConfig::passerBonus[ColorRank<C>(k)] + kingNearBonus) * ColorSignHelper<C>();
   });
}

template<Color C>
FORCE_FINLINE void evalPawn(BitBoard pieceBBiterator, EvalScore &score) {
   applyOn(pieceBBiterator, [&](const Square & k){
      const Square kk = ColorSquarePstHelper<C>(k);
      score += EvalConfig::PST[0][kk] * ColorSignHelper<C>();
   });
}

template<Color C>
FORCE_FINLINE void evalPawnFreePasser(const Position &p, BitBoard pieceBBiterator, EvalScore &score) {
   applyOn(pieceBBiterator, [&](const Square & k){
      score += EvalConfig::freePasserBonus[ColorRank<C>(k)] * ScoreType((BBTools::frontSpan<C>(k) & p.allPieces[~C]) == emptyBitBoard) * ColorSignHelper<C>();
   });
}

template<Color C>
FORCE_FINLINE void evalPawnProtected(BitBoard pieceBBiterator, EvalScore &score) {
   applyOn(pieceBBiterator, [&](const Square & k){
      score += EvalConfig::protectedPasserBonus[ColorRank<C>(k)] * ColorSignHelper<C>(); 
   });
}

template<Color C>
FORCE_FINLINE void evalPawnCandidate(BitBoard pieceBBiterator, EvalScore &score) {
   applyOn(pieceBBiterator, [&](const Square & k){
      score += EvalConfig::candidate[ColorRank<C>(k)] * ColorSignHelper<C>(); 
   });
}

template<Color C> 
BitBoard getPinned(const Position &p, const Square s) {
   BitBoard pinned = emptyBitBoard;
   if (!isValidSquare(s)) return pinned; // this allows for pinned to queen detection more easily
   const BitBoard pinner = BBTools::attack<P_wb>(s, p.pieces_const<P_wb>(~C) | p.pieces_const<P_wq>(~C), p.allPieces[~C]) |
                           BBTools::attack<P_wr>(s, p.pieces_const<P_wr>(~C) | p.pieces_const<P_wq>(~C), p.allPieces[~C]);
   applyOn(pinner, [&](const Square & k){
      pinned |= BBTools::between(k,p.king[C]) & p.allPieces[C]; 
   });
   return pinned;
}

bool isLazyHigh(ScoreType lazyThreshold, const EvalFeatures &features, EvalScore &score) {
   //if (DynamicConfig::FRC) return false;
   score = features.SumUp();
   return Abs(score[MG] + score[EG]) / 2 > lazyThreshold;
}

bool forbidNNUE([[maybe_unused]] const Position &p){
   /*
   // for opposite colored bishop alone (with pawns)
   if (p.mat[Co_White][M_t] == 1 && 
       p.mat[Co_Black][M_t] == 1 &&
       p.mat[Co_White][M_b] == 1 && 
       p.mat[Co_Black][M_b] == 1 && 
       countBit(p.allBishop() & whiteSquare) == 1 &&
       Abs(p.mat[Co_White][M_p] - p.mat[Co_Black][M_p]) < 4 ) return true;
   */
   return false;
}

ScoreType armageddonScore(ScoreType score, unsigned int ply, DepthType height, Color c) {
   return std::clamp(shiftArmageddon(score, ply, c), matedScore(height), matingScore(height-1));
}

ScoreType variantScore(ScoreType score, unsigned int ply, DepthType height, Color c) {
   if (DynamicConfig::armageddon) return armageddonScore(score, ply, height, c);
   return score;
}

// if antichess, we'll just try to minimize material for now
ScoreType evalAntiChess(const Position &p, EvalData &data, [[maybe_unused]] Searcher &context, [[maybe_unused]] bool allowEGEvaluation, [[maybe_unused]] bool display) {
    const bool white2Play = p.c == Co_White;

    //@todo some special case : opposite color bishop => draw

    ScoreType matScoreW = 0;
    ScoreType matScoreB = 0;
    data.gp = gamePhase(p.mat, matScoreW, matScoreB);
    EvalScore score = EvalScore(
        (- p.mat[Co_White][M_k] + p.mat[Co_Black][M_k]) * absValue(P_wk) +
        (- p.mat[Co_White][M_q] + p.mat[Co_Black][M_q]) * absValue(P_wq) +
        (- p.mat[Co_White][M_r] + p.mat[Co_Black][M_r]) * absValue(P_wr) +
        (- p.mat[Co_White][M_b] + p.mat[Co_Black][M_b]) * absValue(P_wb) +
        (- p.mat[Co_White][M_n] + p.mat[Co_Black][M_n]) * absValue(P_wn) +
        (- p.mat[Co_White][M_p] + p.mat[Co_Black][M_p]) * absValue(P_wp) ,
        (- p.mat[Co_White][M_k] + p.mat[Co_Black][M_k]) * absValueEG(P_wk) +
        (- p.mat[Co_White][M_q] + p.mat[Co_Black][M_q]) * absValueEG(P_wq) +
        (- p.mat[Co_White][M_r] + p.mat[Co_Black][M_r]) * absValueEG(P_wr) +
        (- p.mat[Co_White][M_b] + p.mat[Co_Black][M_b]) * absValueEG(P_wb) +
        (- p.mat[Co_White][M_n] + p.mat[Co_Black][M_n]) * absValueEG(P_wn) +
        (- p.mat[Co_White][M_p] + p.mat[Co_Black][M_p]) * absValueEG(P_wp) );

    const ScoreType ret = ScaleScore(score, data.gp, 1.f);
    return (white2Play ? +1 : -1) * ret;
}

#ifdef WITH_NNUE
ScoreType NNUEEVal(const Position & p, EvalData &data, Searcher &context, EvalFeatures &features, bool secondTime = false){
   START_TIMER
   if (DynamicConfig::armageddon) features.scalingFactor = 1.f;              ///@todo better
   // call the net
   ScoreType nnueScore = static_cast<ScoreType>(p.evaluator().propagate(p.c, std::min(32,static_cast<int>(BB::countBit(p.occupancy())))));
   // fuse MG and EG score applying the EG scaling factor ///@todo, doesn't the net already learned that ????
   nnueScore = ScaleScore({nnueScore, nnueScore}, data.gp, features.scalingFactor);
   // NNUE evaluation scaling
   nnueScore = static_cast<ScoreType>((nnueScore * DynamicConfig::NNUEScaling) / 64);
   // take contempt into account (no tempo with NNUE, 
   // the HalfKA net already take it into account but this is necessary for "dynamic" contempt from opponent)
   nnueScore += ScaleScore(context.contempt, data.gp);
   // clamp score
   nnueScore = clampScore(nnueScore);
   if (!DynamicConfig::armageddon){
      // apply scaling factor based on fifty move rule
      nnueScore = fiftyScale(nnueScore, p.fifty);
   }
   context.stats.incr(secondTime ? Stats::sid_evalNNUE2 : Stats::sid_evalNNUE);
   // apply variants scoring if requiered
   STOP_AND_SUM_TIMER(NNUE)
   return variantScore(nnueScore, p.halfmoves, context.height_, p.c);   
}
#endif

ScoreType eval(const Position &p, EvalData &data, Searcher &context, bool allowEGEvaluation, bool display) {
   START_TIMER

   if (DynamicConfig::antichess) return evalAntiChess(p, data, context, allowEGEvaluation, display);
   ///@todo other variants

   // in some variant (like antichess) king is not mandatory
   const bool kingIsMandatory = DynamicConfig::isKingMandatory();

   // if no more pieces except maybe kings, end of game as draw in most standard variants
   if ( (p.occupancy() & ~p.allKing()) == emptyBitBoard ){
      if (kingIsMandatory ){ 
         // drawScore take some variants into account like armageddon
         return context.drawScore(p, context.height_);
      }
      else {
         ///@todo implements things for some variants ?
      }
   }

   // is it player with the white pieces turn ?
   const bool white2Play = p.c == Co_White;

#ifdef DEBUG_KING_CAP
   if (kingIsMandatory) {
      if (p.king[Co_White] == INVALIDSQUARE) {
         STOP_AND_SUM_TIMER(Eval)
         context.stats.incr(Stats::sid_evalNoKing);
         return data.gp = 0, (white2Play ? -1 : +1) * matingScore(0);
      }
      if (p.king[Co_Black] == INVALIDSQUARE) {
         STOP_AND_SUM_TIMER(Eval)
         context.stats.incr(Stats::sid_evalNoKing);
         return data.gp = 0, (white2Play ? +1 : -1) * matingScore(0);
      }
   }
#endif

   ///@todo activate (or modulate) some features based on skill level

   // main features (feature won't survive Eval scope), but EvalData will !
   EvalFeatures features;

   // Material evaluation (most often from Material table)
   if (const Hash matHash = MaterialHash::getMaterialHash(p.mat); matHash != nullHash) {
      context.stats.incr(Stats::sid_materialTableHits);
      // Get material hash data
      const MaterialHash::MaterialHashEntry &MEntry = MaterialHash::materialHashTable[matHash];
      // update EvalData and EvalFeatures
      data.gp = MEntry.gamePhase();
      features.scores[F_material] += MEntry.score;

      // end game knowledge (helper or scaling), only for most standard chess variants with kings on the board
      if (allowEGEvaluation && kingIsMandatory && (p.mat[Co_White][M_t] + p.mat[Co_Black][M_t] < 5) ) {
         // let's verify that the is no naive capture before using end-game knowledge
         MoveList moves;
         MoveGen::generate<MoveGen::GP_cap>(p, moves);
         bool hasCapture = false;
         for (auto m : moves){
            Position p2 = p;
            if (const MoveInfo moveInfo(p2, m); applyMove(p2,moveInfo, true)){
               hasCapture = true;
               break;
            }
         }
         // probe endgame knowledge only if position is quiet from stm pov
         if (!hasCapture) {
            const Color winningSideEG = features.scores[F_material][EG] > 0 ? Co_White : Co_Black;
            // helpers for various endgame
            if (MEntry.t == MaterialHash::Ter_WhiteWinWithHelper || MEntry.t == MaterialHash::Ter_BlackWinWithHelper) {
               STOP_AND_SUM_TIMER(Eval)
               context.stats.incr(Stats::sid_materialTableHelper);
               const ScoreType materialTableScore =
                   (white2Play ? +1 : -1) * (MaterialHash::helperTable[matHash](p, winningSideEG, features.scores[F_material][EG], context.height_));
               return variantScore(materialTableScore, p.halfmoves, context.height_, p.c);
            }
            // real FIDE draws (shall not happens for now in fact ///@todo !!!)
            else if (MEntry.t == MaterialHash::Ter_Draw) {
               if (!isPosInCheck(p)) {
                  STOP_AND_SUM_TIMER(Eval)
                  context.stats.incr(Stats::sid_materialTableDraw);
                  return context.drawScore(p, context.height_); // drawScore take armageddon into account
               }
            }
            // non FIDE draws
            else if (MEntry.t == MaterialHash::Ter_MaterialDraw) {
               if (!isPosInCheck(p)) {
                  STOP_AND_SUM_TIMER(Eval)
                  context.stats.incr(Stats::sid_materialTableDraw2);
                  return context.drawScore(p, context.height_); // drawScore take armageddon into account
               }
            }
            // apply some scaling (more later...)
            else if (MEntry.t == MaterialHash::Ter_WhiteWin || MEntry.t == MaterialHash::Ter_BlackWin)
               features.scalingFactor = static_cast<float>(EvalConfig::scalingFactorWin) / 128.f;
            else if (MEntry.t == MaterialHash::Ter_HardToWin)
               features.scalingFactor = static_cast<float>(EvalConfig::scalingFactorHardWin) / 128.f;
            else if (MEntry.t == MaterialHash::Ter_LikelyDraw)
               features.scalingFactor = static_cast<float>(EvalConfig::scalingFactorLikelyDraw) / 128.f;
         }
         /*
         else {
            std::cout << "posible cap" << std::endl;
         }
         */
      }
   }
   else { // game phase and material scores out of table
      ///@todo we don't care about imbalance here ?
      ScoreType matScoreW = 0;
      ScoreType matScoreB = 0;
      data.gp = gamePhase(p.mat, matScoreW, matScoreB);
      features.scores[F_material] += EvalScore(
          (p.mat[Co_White][M_q] - p.mat[Co_Black][M_q]) * absValue(P_wq) +
          (p.mat[Co_White][M_r] - p.mat[Co_Black][M_r]) * absValue(P_wr) +
          (p.mat[Co_White][M_b] - p.mat[Co_Black][M_b]) * absValue(P_wb) +
          (p.mat[Co_White][M_n] - p.mat[Co_Black][M_n]) * absValue(P_wn) +
          (p.mat[Co_White][M_p] - p.mat[Co_Black][M_p]) * absValue(P_wp) ,
          (p.mat[Co_White][M_q] - p.mat[Co_Black][M_q]) * absValueEG(P_wq) +
          (p.mat[Co_White][M_r] - p.mat[Co_Black][M_r]) * absValueEG(P_wr) +
          (p.mat[Co_White][M_b] - p.mat[Co_Black][M_b]) * absValueEG(P_wb) +
          (p.mat[Co_White][M_n] - p.mat[Co_Black][M_n]) * absValueEG(P_wn) +
          (p.mat[Co_White][M_p] - p.mat[Co_Black][M_p]) * absValueEG(P_wp) );
      //std::cout << ToString(p,true) << std::endl;
      context.stats.incr(Stats::sid_materialTableMiss);
   }

   //std::cout << "allow material eval? " << allowEGEvaluation << std::endl;
   //std::cout << GetFEN(p) << std::endl;

#ifndef WITH_MATERIAL_TABLE
   // in case we are not using a material table, we still need KPK, KRK and KBNK helper
   // but only for most standard chess variants with kings on the board
   // end game knowledge (helper or scaling)
   if (allowEGEvaluation && kingIsMandatory && (p.mat[Co_White][M_t] + p.mat[Co_Black][M_t] < 5)) {
     static const auto matHashKPK = MaterialHash::getMaterialHash2(MaterialHash::materialFromString("KPK"));
     static const auto matHashKKP = MaterialHash::getMaterialHash2(MaterialHash::materialFromString("KKP"));
     static const auto matHashKQK = MaterialHash::getMaterialHash2(MaterialHash::materialFromString("KQK"));
     static const auto matHashKKQ = MaterialHash::getMaterialHash2(MaterialHash::materialFromString("KKQ"));
     static const auto matHashKRK = MaterialHash::getMaterialHash2(MaterialHash::materialFromString("KRK"));
     static const auto matHashKKR = MaterialHash::getMaterialHash2(MaterialHash::materialFromString("KKR"));
     static const auto matHashKLNK = MaterialHash::getMaterialHash2(MaterialHash::materialFromString("KLNK"));
     static const auto matHashKKLN = MaterialHash::getMaterialHash2(MaterialHash::materialFromString("KKLN"));
     static const auto matHashKDNK = MaterialHash::getMaterialHash2(MaterialHash::materialFromString("KDNK"));
     static const auto matHashKKDN = MaterialHash::getMaterialHash2(MaterialHash::materialFromString("KKDN"));
     STOP_AND_SUM_TIMER(Eval)
     const Color winningSideEG = features.scores[F_material][EG] > 0 ? Co_White : Co_Black;
     context.stats.incr(Stats::sid_materialTableHelper);
     ScoreType materialTableScore = 0;
     const Hash matHash2 = MaterialHash::getMaterialHash2(p.mat);
     bool matHelperHit = false;
#if !defined(WITH_SMALL_MEMORY)
     if (matHash2 == matHashKPK || matHash2 == matHashKKP){
        materialTableScore = (white2Play ? +1 : -1) * (MaterialHash::helperKPK(p, winningSideEG, features.scores[F_material][EG], context.height_));
        matHelperHit = true;
     }
#endif
     if (matHash2 == matHashKQK || matHash2 == matHashKKQ || matHash2 == matHashKRK || matHash2 == matHashKKR){
        materialTableScore = (white2Play ? +1 : -1) * (MaterialHash::helperKXK(p, winningSideEG, features.scores[F_material][EG], context.height_));
        matHelperHit = true;
     }
     if (matHash2 == matHashKLNK || matHash2 == matHashKKLN || matHash2 == matHashKDNK || matHash2 == matHashKKDN){
        materialTableScore = (white2Play ? +1 : -1) * (MaterialHash::helperKmmK(p, winningSideEG, features.scores[F_material][EG], context.height_));
        matHelperHit = true;
     }
     if (matHelperHit){
         return variantScore(materialTableScore, p.halfmoves, context.height_, p.c);
     }
   }
#endif

#ifdef WITH_EVAL_TUNING
   features.scores[F_material] += MaterialHash::Imbalance(p.mat, Co_White) - MaterialHash::Imbalance(p.mat, Co_Black);
#endif

#ifdef WITH_NNUE
   const bool forbiddenNNUE = forbidNNUE(p) && !DynamicConfig::forceNNUE;
   if (DynamicConfig::useNNUE && !forbiddenNNUE) {
      // we will stay to classic eval when the game is already decided (to gain some nps)
      ///@todo use data.gp inside NNUE condition ?
      if (EvalScore score; DynamicConfig::forceNNUE ||
          !isLazyHigh(static_cast<ScoreType>(DynamicConfig::NNUEThreshold), features, score)) {
         STOP_AND_SUM_TIMER(Eval)
         ScoreType nnueEval = NNUEEVal(p, data, context, features);

         // random factor in opening if requiered
         if (p.halfmoves < 10 && DynamicConfig::randomOpen != 0){
            nnueEval += static_cast<ScoreType>(randomInt<int /*NO SEED USE => random device*/>(-1*DynamicConfig::randomOpen, DynamicConfig::randomOpen));
         }
         return nnueEval;
      }
      // fall back to classic eval
      context.stats.incr(Stats::sid_evalStd);
   }
#endif

   STOP_AND_SUM_TIMER(Eval1)

   // prefetch pawn evaluation table as soon as possible
   context.prefetchPawn(computeHash(p));

   // pre compute some usefull bitboards (///@todo maybe this is not efficient ...)
   const colored<BitBoard> pawns      = {p.whitePawn(), p.blackPawn()};
   const colored<BitBoard> knights    = {p.whiteKnight(), p.blackKnight()};
   const colored<BitBoard> bishops    = {p.whiteBishop(), p.blackBishop()};
   const colored<BitBoard> rooks      = {p.whiteRook(), p.blackRook()};
   const colored<BitBoard> queens     = {p.whiteQueen(), p.blackQueen()};
   const colored<BitBoard> kings      = {p.whiteKing(), p.blackKing()};
   const colored<BitBoard> nonPawnMat = {p.allPieces[Co_White] & ~pawns[Co_White], 
                                         p.allPieces[Co_Black] & ~pawns[Co_Black]};
   const colored<BitBoard> kingZone   = {isValidSquare(p.king[Co_White]) ? BBTools::mask[p.king[Co_White]].kingZone : emptyBitBoard, 
                                         isValidSquare(p.king[Co_Black]) ? BBTools::mask[p.king[Co_Black]].kingZone : emptyBitBoard};
   const BitBoard occupancy = p.occupancy();

   // helpers that will be filled by evalPiece calls
   colored<ScoreType> kdanger = {0, 0};
   colored<BitBoard>  att     = {emptyBitBoard, emptyBitBoard}; // bitboard of squares attacked by Color
   colored<BitBoard>  att2    = {emptyBitBoard, emptyBitBoard}; // bitboard of squares attacked twice by Color
   array2d<BitBoard,2,6>  attFromPiece = {{emptyBitBoard}};     // bitboard of squares attacked by specific piece of Color
   array2d<BitBoard,2,6>  checkers     = {{emptyBitBoard}};     // bitboard of Color pieces squares attacking king

   // PST, attack, danger
   const bool withForwadness = DynamicConfig::styleForwardness != 50;
   const array1d<ScoreType,2> staticColorSignHelper = { +1, -1};
   for (Color c = Co_White ; c <= Co_Black ; ++c){
      // will do pawns later
      for (Piece pp = P_wn ; pp <= P_wk ; ++pp){
         applyOn(p.pieces_const(c,pp), [&](const Square & k){
            const Square kk = relative_square(~c,k);
            features.scores[F_positional] += EvalConfig::PST[pp - 1][kk] * staticColorSignHelper[c];
            if (withForwadness)
               features.scores[F_positional] += EvalScore {ScoreType(((DynamicConfig::styleForwardness - 50) * SQRANK(kk)) / 8), 0} * staticColorSignHelper[c];
            // aligned threats removing own piece (not pawn) in occupancy
            const BitBoard shadowTarget = BBTools::pfCoverage[pp - 1](k, occupancy ^ nonPawnMat[c], c);
            if (shadowTarget) {
               kdanger[~c] += countBit(shadowTarget & kingZone[~c]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack][pp - 1];
               const BitBoard target = BBTools::pfCoverage[pp - 1](k, occupancy, c); // real targets
               if (target) {
                  attFromPiece[c][pp - 1] |= target;
                  att2[c] |= att[c] & target;
                  att[c] |= target;
                  if (target & p.pieces_const<P_wk>(~c)) checkers[c][pp - 1] |= SquareToBitboard(k);
                  kdanger[c] -= countBit(target & kingZone[c]) * EvalConfig::kingAttWeight[EvalConfig::katt_defence][pp - 1];
               }
            }
         });
      }
   }

   // random factor in opening if requiered
   if (p.halfmoves < 10 && DynamicConfig::randomOpen != 0){
      features.scores[F_positional] += static_cast<ScoreType>(randomInt<int /*NO SEED USE => random device*/>(-1*DynamicConfig::randomOpen, DynamicConfig::randomOpen));
   }

   STOP_AND_SUM_TIMER(Eval2)

   Searcher::PawnEntry *pePtr = nullptr;
#ifdef WITH_EVAL_TUNING
   Searcher::PawnEntry dummy; // used for evaluations tuning
   pePtr = &dummy;
   {
#else
   if (!context.getPawnEntry(computePHash(p), pePtr)) {
#endif
      assert(pePtr);
      Searcher::PawnEntry &pe = *pePtr;
      pe.reset();
      pe.pawnTargets[Co_White]   = BBTools::pawnAttacks<Co_White>(pawns[Co_White]);
      pe.pawnTargets[Co_Black]   = BBTools::pawnAttacks<Co_Black>(pawns[Co_Black]);
      // semiOpen white means with white pawn, and without black pawn
      pe.semiOpenFiles[Co_White] = BBTools::fillFile(pawns[Co_White]) & ~BBTools::fillFile(pawns[Co_Black]);
      pe.semiOpenFiles[Co_Black] = BBTools::fillFile(pawns[Co_Black]) & ~BBTools::fillFile(pawns[Co_White]);
      pe.passed[Co_White]        = BBTools::pawnPassed<Co_White>(pawns[Co_White], pawns[Co_Black]);
      pe.passed[Co_Black]        = BBTools::pawnPassed<Co_Black>(pawns[Co_Black], pawns[Co_White]);
      pe.holes[Co_White]         = BBTools::pawnHoles<Co_White>(pawns[Co_White]) & holesZone[Co_White];
      pe.holes[Co_Black]         = BBTools::pawnHoles<Co_Black>(pawns[Co_Black]) & holesZone[Co_Black];
      pe.openFiles               = BBTools::openFiles(pawns[Co_White], pawns[Co_Black]);

      // PST
      evalPawn<Co_White>(pawns[Co_White], pe.score);
      evalPawn<Co_Black>(pawns[Co_Black], pe.score);

      // danger in king zone (from pawns attacking or defending)
      pe.danger[Co_White] -= countBit(pe.pawnTargets[Co_White] & kingZone[Co_White]) * EvalConfig::kingAttWeight[EvalConfig::katt_defence][0];
      pe.danger[Co_White] += countBit(pe.pawnTargets[Co_Black] & kingZone[Co_White]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack][0];
      pe.danger[Co_Black] -= countBit(pe.pawnTargets[Co_Black] & kingZone[Co_Black]) * EvalConfig::kingAttWeight[EvalConfig::katt_defence][0];
      pe.danger[Co_Black] += countBit(pe.pawnTargets[Co_White] & kingZone[Co_Black]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack][0];

      // pawn passer
      evalPawnPasser<Co_White>(p, pe.passed[Co_White], pe.score);
      evalPawnPasser<Co_Black>(p, pe.passed[Co_Black], pe.score);

      // pawn protected
      evalPawnProtected<Co_White>(pe.passed[Co_White] & pe.pawnTargets[Co_White], pe.score);
      evalPawnProtected<Co_Black>(pe.passed[Co_Black] & pe.pawnTargets[Co_Black], pe.score);

      // pawn candidate
      evalPawnCandidate<Co_White>(BBTools::pawnCandidates<Co_White>(pawns[Co_White], pawns[Co_Black]), pe.score);
      evalPawnCandidate<Co_Black>(BBTools::pawnCandidates<Co_Black>(pawns[Co_Black], pawns[Co_White]), pe.score);

      ///@todo hidden passed

      // bad pawns
      const colored<BitBoard> semiOpenPawn = {BBTools::pawnSemiOpen<Co_White>(pawns[Co_White], pawns[Co_Black]),
                                              BBTools::pawnSemiOpen<Co_Black>(pawns[Co_Black], pawns[Co_White])};
      const colored<BitBoard> backward     = {BBTools::pawnBackward<Co_White>(pawns[Co_White], pawns[Co_Black]),
                                              BBTools::pawnBackward<Co_Black>(pawns[Co_Black], pawns[Co_White])};
      const BitBoard backwardOpenW     = backward[Co_White] & semiOpenPawn[Co_White];
      const BitBoard backwardCloseW    = backward[Co_White] & ~semiOpenPawn[Co_White];
      const BitBoard backwardOpenB     = backward[Co_Black] & semiOpenPawn[Co_Black];
      const BitBoard backwardCloseB    = backward[Co_Black] & ~semiOpenPawn[Co_Black];
      const colored<BitBoard> doubled  = {BBTools::pawnDoubled<Co_White>(pawns[Co_White]),
                                         BBTools::pawnDoubled<Co_Black>(pawns[Co_Black])};
      const BitBoard doubledOpenW      = doubled[Co_White] & semiOpenPawn[Co_White];
      const BitBoard doubledCloseW     = doubled[Co_White] & ~semiOpenPawn[Co_White];
      const BitBoard doubledOpenB      = doubled[Co_Black] & semiOpenPawn[Co_Black];
      const BitBoard doubledCloseB     = doubled[Co_Black] & ~semiOpenPawn[Co_Black];
      const colored<BitBoard> isolated = {BBTools::pawnIsolated(pawns[Co_White]),
                                          BBTools::pawnIsolated(pawns[Co_Black])};
      const BitBoard isolatedOpenW     = isolated[Co_White] & semiOpenPawn[Co_White];
      const BitBoard isolatedCloseW    = isolated[Co_White] & ~semiOpenPawn[Co_White];
      const BitBoard isolatedOpenB     = isolated[Co_Black] & semiOpenPawn[Co_Black];
      const BitBoard isolatedCloseB    = isolated[Co_Black] & ~semiOpenPawn[Co_Black];
      const colored<BitBoard> detached = {BBTools::pawnDetached<Co_White>(pawns[Co_White], pawns[Co_Black]),
                                          BBTools::pawnDetached<Co_Black>(pawns[Co_Black], pawns[Co_White])};
      const BitBoard detachedOpenW     = detached[Co_White] & semiOpenPawn[Co_White] & ~backward[Co_White];
      const BitBoard detachedCloseW    = detached[Co_White] & ~semiOpenPawn[Co_White] & ~backward[Co_White];
      const BitBoard detachedOpenB     = detached[Co_Black] & semiOpenPawn[Co_Black] & ~backward[Co_Black];
      const BitBoard detachedCloseB    = detached[Co_Black] & ~semiOpenPawn[Co_Black] & ~backward[Co_Black];

      for (Rank r = Rank_2; r <= Rank_7; ++r) { // simply r2..r7 for classic chess works
         // pawn backward
         pe.score -= EvalConfig::backwardPawn[r][EvalConfig::Close] * countBit(backwardCloseW & ranks[r]);
         pe.score -= EvalConfig::backwardPawn[r][EvalConfig::SemiOpen] * countBit(backwardOpenW & ranks[r]);
         // double pawn malus
         pe.score -= EvalConfig::doublePawn[r][EvalConfig::Close] * countBit(doubledCloseW & ranks[r]);
         pe.score -= EvalConfig::doublePawn[r][EvalConfig::SemiOpen] * countBit(doubledOpenW & ranks[r]);
         // isolated pawn malus
         pe.score -= EvalConfig::isolatedPawn[r][EvalConfig::Close] * countBit(isolatedCloseW & ranks[r]);
         pe.score -= EvalConfig::isolatedPawn[r][EvalConfig::SemiOpen] * countBit(isolatedOpenW & ranks[r]);
         // detached pawn (not backward)
         pe.score -= EvalConfig::detachedPawn[r][EvalConfig::Close] * countBit(detachedCloseW & ranks[r]);
         pe.score -= EvalConfig::detachedPawn[r][EvalConfig::SemiOpen] * countBit(detachedOpenW & ranks[r]);

         // pawn backward
         pe.score += EvalConfig::backwardPawn[7 - r][EvalConfig::Close] * countBit(backwardCloseB & ranks[r]);
         pe.score += EvalConfig::backwardPawn[7 - r][EvalConfig::SemiOpen] * countBit(backwardOpenB & ranks[r]);
         // double pawn malus
         pe.score += EvalConfig::doublePawn[7 - r][EvalConfig::Close] * countBit(doubledCloseB & ranks[r]);
         pe.score += EvalConfig::doublePawn[7 - r][EvalConfig::SemiOpen] * countBit(doubledOpenB & ranks[r]);
         // isolated pawn malus
         pe.score += EvalConfig::isolatedPawn[7 - r][EvalConfig::Close] * countBit(isolatedCloseB & ranks[r]);
         pe.score += EvalConfig::isolatedPawn[7 - r][EvalConfig::SemiOpen] * countBit(isolatedOpenB & ranks[r]);
         // detached pawn (not backward)
         pe.score += EvalConfig::detachedPawn[7 - r][EvalConfig::Close] * countBit(detachedCloseB & ranks[r]);
         pe.score += EvalConfig::detachedPawn[7 - r][EvalConfig::SemiOpen] * countBit(detachedOpenB & ranks[r]);
      }

      // pawn impact on king safety (PST and king troppism alone is not enough)
      if (kingIsMandatory){
        // pawn shield
        const colored<BitBoard> kingShield = {kingZone[Co_White] & ~BBTools::shiftS<Co_White>(ranks[SQRANK(p.king[Co_White])]),
                                              kingZone[Co_Black] & ~BBTools::shiftS<Co_Black>(ranks[SQRANK(p.king[Co_Black])])};
        const int pawnShieldW = countBit(kingShield[Co_White] & pawns[Co_White]);
        const int pawnShieldB = countBit(kingShield[Co_Black] & pawns[Co_Black]);
        pe.score += EvalConfig::pawnShieldBonus * std::min(pawnShieldW * pawnShieldW, 9);
        pe.score -= EvalConfig::pawnShieldBonus * std::min(pawnShieldB * pawnShieldB, 9);
        
        // malus for king on a pawnless flank
        const File wkf = SQFILE(p.king[Co_White]);
        const File bkf = SQFILE(p.king[Co_Black]);
        if (!(pawns[Co_White] & kingFlank[wkf])) pe.score += EvalConfig::pawnlessFlank;
        if (!(pawns[Co_Black] & kingFlank[bkf])) pe.score -= EvalConfig::pawnlessFlank;
        
        // pawn storm
        pe.score -= EvalConfig::pawnStormMalus * countBit(kingFlank[wkf] & (rank3 | rank4) & pawns[Co_Black]);
        pe.score += EvalConfig::pawnStormMalus * countBit(kingFlank[bkf] & (rank5 | rank6) & pawns[Co_White]);
        
        // open file near king
        pe.danger[Co_White] += EvalConfig::kingAttOpenfile * countBit(kingFlank[wkf] & pe.openFiles) / 8;
        pe.danger[Co_White] += EvalConfig::kingAttSemiOpenfileOpp * countBit(kingFlank[wkf] & pe.semiOpenFiles[Co_White]) / 8;
        pe.danger[Co_White] += EvalConfig::kingAttSemiOpenfileOur * countBit(kingFlank[wkf] & pe.semiOpenFiles[Co_Black]) / 8;
        pe.danger[Co_Black] += EvalConfig::kingAttOpenfile * countBit(kingFlank[bkf] & pe.openFiles) / 8;
        pe.danger[Co_Black] += EvalConfig::kingAttSemiOpenfileOpp * countBit(kingFlank[bkf] & pe.semiOpenFiles[Co_Black]) / 8;
        pe.danger[Co_Black] += EvalConfig::kingAttSemiOpenfileOur * countBit(kingFlank[bkf] & pe.semiOpenFiles[Co_White]) / 8;
        
        // fawn (mostly for fun ...)
        pe.score -= EvalConfig::pawnFawnMalusKS *
                    (countBit((pawns[Co_White] & (BBSq_h2 | BBSq_g3)) | (pawns[Co_Black] & BBSq_h3) | (kings[Co_White] & kingSide)) / 4);
        pe.score += EvalConfig::pawnFawnMalusKS *
                    (countBit((pawns[Co_Black] & (BBSq_h7 | BBSq_g6)) | (pawns[Co_White] & BBSq_h6) | (kings[Co_Black] & kingSide)) / 4);
        pe.score -= EvalConfig::pawnFawnMalusQS *
                    (countBit((pawns[Co_White] & (BBSq_a2 | BBSq_b3)) | (pawns[Co_Black] & BBSq_a3) | (kings[Co_White] & queenSide)) / 4);
        pe.score += EvalConfig::pawnFawnMalusQS *
                    (countBit((pawns[Co_Black] & (BBSq_a7 | BBSq_b6)) | (pawns[Co_White] & BBSq_a6) | (kings[Co_Black] & queenSide)) / 4);
      }

      context.stats.incr(Stats::sid_ttPawnInsert);
      pe.h = Hash64to32(computePHash(p)); // this associate the pawn entry to the current K&P positions
   }
   assert(pePtr);
   const Searcher::PawnEntry &pe = *pePtr; // let's use a ref instead of a pointer
   // add computed or hash score for pawn structure
   features.scores[F_pawnStruct] += pe.score;

   // update global things with freshy gotten pawn entry stuff
   kdanger[Co_White] += pe.danger[Co_White];
   kdanger[Co_Black] += pe.danger[Co_Black];
   checkers[Co_White][0] = BBTools::pawnAttacks<Co_Black>(kings[Co_Black]) & pawns[Co_White];
   checkers[Co_Black][0] = BBTools::pawnAttacks<Co_White>(kings[Co_White]) & pawns[Co_Black];
   att2[Co_White] |= att[Co_White] & pe.pawnTargets[Co_White];
   att2[Co_Black] |= att[Co_Black] & pe.pawnTargets[Co_Black];
   att[Co_White] |= pe.pawnTargets[Co_White];
   att[Co_Black] |= pe.pawnTargets[Co_Black];

   STOP_AND_SUM_TIMER(Eval3)

   //const BitBoard attackedOrNotDefended[2]    = {att[Co_White]  | ~att[Co_Black]  , att[Co_Black]  | ~att[Co_White] };
   const colored<BitBoard> attackedAndNotDefended   = {att[Co_White] & ~att[Co_Black], att[Co_Black] & ~att[Co_White]};
   const colored<BitBoard> attacked2AndNotDefended2 = {att2[Co_White] & ~att2[Co_Black], att2[Co_Black] & ~att2[Co_White]};

   const colored<BitBoard> weakSquare = { att[Co_Black] & ~att2[Co_White] & (~att[Co_White] | attFromPiece[Co_White][P_wk - 1] | attFromPiece[Co_White][P_wq - 1]),
                                          att[Co_White] & ~att2[Co_Black] & (~att[Co_Black] | attFromPiece[Co_Black][P_wk - 1] | attFromPiece[Co_Black][P_wq - 1])};
   const colored<BitBoard> safeSquare = { ~att[Co_Black] | (weakSquare[Co_Black] & att2[Co_White]),
                                          ~att[Co_White] | (weakSquare[Co_White] & att2[Co_Black])};
   const colored<BitBoard> protectedSquare = {pe.pawnTargets[Co_White] | attackedAndNotDefended[Co_White] | attacked2AndNotDefended2[Co_White],
                                              pe.pawnTargets[Co_Black] | attackedAndNotDefended[Co_Black] | attacked2AndNotDefended2[Co_Black]};

   data.haveThreats[Co_White] = (att[Co_White] & p.allPieces[Co_Black]) != emptyBitBoard;
   data.haveThreats[Co_Black] = (att[Co_Black] & p.allPieces[Co_White]) != emptyBitBoard;

   data.goodThreats[Co_White] = ((attFromPiece[Co_White][P_wp-1] & p.allPieces[Co_Black] & ~p.blackPawn())
                                | (attFromPiece[Co_White][P_wn-1] & (p.blackQueen() | p.blackRook()))
                                | (attFromPiece[Co_White][P_wb-1] & (p.blackQueen() | p.blackRook()))
                                | (attFromPiece[Co_White][P_wr-1] & p.blackQueen())) != emptyBitBoard;
   data.goodThreats[Co_Black] = ((attFromPiece[Co_Black][P_wp-1] & p.allPieces[Co_White] & ~p.whitePawn())
                                | (attFromPiece[Co_Black][P_wn-1] & (p.whiteQueen() | p.whiteRook()))
                                | (attFromPiece[Co_Black][P_wb-1] & (p.whiteQueen() | p.whiteRook()))
                                | (attFromPiece[Co_Black][P_wr-1] & p.whiteQueen())) != emptyBitBoard;

   // reward safe checks
   kdanger[Co_White] += EvalConfig::kingAttSafeCheck[0] * countBit(checkers[Co_Black][0] & att[Co_Black]);
   kdanger[Co_Black] += EvalConfig::kingAttSafeCheck[0] * countBit(checkers[Co_White][0] & att[Co_White]);
   for (Piece pp = P_wn; pp < P_wk; ++pp) {
      kdanger[Co_White] += EvalConfig::kingAttSafeCheck[pp - 1] * countBit(checkers[Co_Black][pp - 1] & safeSquare[Co_Black]);
      kdanger[Co_Black] += EvalConfig::kingAttSafeCheck[pp - 1] * countBit(checkers[Co_White][pp - 1] & safeSquare[Co_White]);
   }

   // less danger if no enemy queen
   const Square whiteQueenSquare = queens[Co_White] ? BBTools::SquareFromBitBoard(queens[Co_White]) : INVALIDSQUARE;
   const Square blackQueenSquare = queens[Co_Black] ? BBTools::SquareFromBitBoard(queens[Co_Black]) : INVALIDSQUARE;
   kdanger[Co_White] -= blackQueenSquare == INVALIDSQUARE ? EvalConfig::kingAttNoQueen : 0;
   kdanger[Co_Black] -= whiteQueenSquare == INVALIDSQUARE ? EvalConfig::kingAttNoQueen : 0;

   // danger : use king danger score. **DO NOT** apply this in end-game
   const ScoreType dw = EvalConfig::kingAttTable[std::min(std::max(ScoreType(kdanger[Co_White] / 32), ScoreType(0)), ScoreType(63))];
   const ScoreType db = EvalConfig::kingAttTable[std::min(std::max(ScoreType(kdanger[Co_Black] / 32), ScoreType(0)), ScoreType(63))];
   features.scores[F_attack] -= EvalScore(dw, 0);
   features.scores[F_attack] += EvalScore(db, 0);
   data.danger[Co_White] = kdanger[Co_White];
   data.danger[Co_Black] = kdanger[Co_Black];

   // own piece in front of pawn
   features.scores[F_development] += EvalConfig::pieceFrontPawn * countBit(BBTools::shiftN<Co_White>(pawns[Co_White]) & nonPawnMat[Co_White]);
   features.scores[F_development] -= EvalConfig::pieceFrontPawn * countBit(BBTools::shiftN<Co_Black>(pawns[Co_Black]) & nonPawnMat[Co_Black]);

   // pawn in front of own minor
   const colored<BitBoard> minor = {knights[Co_White] | bishops[Co_White], knights[Co_Black] | bishops[Co_Black]};
   applyOn(BBTools::shiftS<Co_White>(pawns[Co_White]) & minor[Co_White], [&](const Square & k){ 
      features.scores[F_development] += EvalConfig::pawnFrontMinor[ColorRank<Co_White>(k)]; 
   });
   applyOn(BBTools::shiftS<Co_Black>(pawns[Co_Black]) & minor[Co_Black], [&](const Square & k){
      features.scores[F_development] -= EvalConfig::pawnFrontMinor[ColorRank<Co_Black>(k)]; 
   });

   // center control
   features.scores[F_development] += EvalConfig::centerControl * countBit(protectedSquare[Co_White] & extendedCenter);
   features.scores[F_development] -= EvalConfig::centerControl * countBit(protectedSquare[Co_Black] & extendedCenter);

   // pawn hole, unprotected
   features.scores[F_positional] += EvalConfig::holesMalus * countBit(pe.holes[Co_White] & ~protectedSquare[Co_White]) / 4;
   features.scores[F_positional] -= EvalConfig::holesMalus * countBit(pe.holes[Co_Black] & ~protectedSquare[Co_Black]) / 4;

   // free passer bonus
   evalPawnFreePasser<Co_White>(p, pe.passed[Co_White], features.scores[F_pawnStruct]);
   evalPawnFreePasser<Co_Black>(p, pe.passed[Co_Black], features.scores[F_pawnStruct]);

   // rook behind passed
   features.scores[F_pawnStruct] += EvalConfig::rookBehindPassed * (countBit(rooks[Co_White] & BBTools::rearSpan<Co_White>(pe.passed[Co_White])) -
                                                                    countBit(rooks[Co_Black] & BBTools::rearSpan<Co_White>(pe.passed[Co_White])));
   features.scores[F_pawnStruct] -= EvalConfig::rookBehindPassed * (countBit(rooks[Co_Black] & BBTools::rearSpan<Co_Black>(pe.passed[Co_Black])) -
                                                                    countBit(rooks[Co_White] & BBTools::rearSpan<Co_Black>(pe.passed[Co_Black])));

   // protected minor blocking openfile
   features.scores[F_positional] += EvalConfig::minorOnOpenFile * countBit(pe.openFiles & (minor[Co_White]) & pe.pawnTargets[Co_White]);
   features.scores[F_positional] -= EvalConfig::minorOnOpenFile * countBit(pe.openFiles & (minor[Co_Black]) & pe.pawnTargets[Co_Black]);

   // knight on opponent hole, protected by pawn
   features.scores[F_positional] += EvalConfig::outpostN * countBit(pe.holes[Co_Black] & knights[Co_White] & pe.pawnTargets[Co_White]);
   features.scores[F_positional] -= EvalConfig::outpostN * countBit(pe.holes[Co_White] & knights[Co_Black] & pe.pawnTargets[Co_Black]);

   // bishop on opponent hole, protected by pawn
   features.scores[F_positional] += EvalConfig::outpostB * countBit(pe.holes[Co_Black] & bishops[Co_White] & pe.pawnTargets[Co_White]);
   features.scores[F_positional] -= EvalConfig::outpostB * countBit(pe.holes[Co_White] & bishops[Co_Black] & pe.pawnTargets[Co_Black]);

   // knight far from both kings gets a penalty
   if (kingIsMandatory){
    applyOn(knights[Co_White], [&](const Square & knighSq){
        features.scores[F_positional] +=
            EvalConfig::knightTooFar[std::min(chebyshevDistance(p.king[Co_White], knighSq), chebyshevDistance(p.king[Co_Black], knighSq))];
    });
    applyOn(knights[Co_Black], [&](const Square & knighSq){
        features.scores[F_positional] -=
            EvalConfig::knightTooFar[std::min(chebyshevDistance(p.king[Co_White], knighSq), chebyshevDistance(p.king[Co_Black], knighSq))];
    });
   }

   // number of hanging pieces
   const colored<BitBoard> hanging = {nonPawnMat[Co_White] & weakSquare[Co_White], nonPawnMat[Co_Black] & weakSquare[Co_Black]};
   features.scores[F_attack] += EvalConfig::hangingPieceMalus * (countBit(hanging[Co_White]) - countBit(hanging[Co_Black]));

   // threats by minor
   BitBoard targetThreat = (nonPawnMat[Co_White] | (pawns[Co_White] & weakSquare[Co_White])) & (attFromPiece[Co_Black][P_wn - 1] | attFromPiece[Co_Black][P_wb - 1]);
   applyOn(targetThreat, [&](const Square & k){
      features.scores[F_attack] += EvalConfig::threatByMinor[PieceTools::getPieceType(p, k) - 1];
   });
   targetThreat = (nonPawnMat[Co_Black] | (pawns[Co_Black] & weakSquare[Co_Black])) & (attFromPiece[Co_White][P_wn - 1] | attFromPiece[Co_White][P_wb - 1]);
   applyOn(targetThreat, [&](const Square & k){
      features.scores[F_attack] -= EvalConfig::threatByMinor[PieceTools::getPieceType(p, k) - 1];
   });

   // threats by rook
   targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wr - 1];
   applyOn(targetThreat, [&](const Square & k){
      features.scores[F_attack] += EvalConfig::threatByRook[PieceTools::getPieceType(p, k) - 1];
   });
   targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wr - 1];
   applyOn(targetThreat, [&](const Square & k){
      features.scores[F_attack] -= EvalConfig::threatByRook[PieceTools::getPieceType(p, k) - 1];
   });

   // threats by queen
   targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wq - 1];
   applyOn(targetThreat, [&](const Square & k){
      features.scores[F_attack] += EvalConfig::threatByQueen[PieceTools::getPieceType(p, k) - 1];
   });
   targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wq - 1];
   applyOn(targetThreat, [&](const Square & k){
      features.scores[F_attack] -= EvalConfig::threatByQueen[PieceTools::getPieceType(p, k) - 1];
   });

   // threats by king
   targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wk - 1];
   applyOn(targetThreat, [&](const Square & k){
      features.scores[F_attack] += EvalConfig::threatByKing[PieceTools::getPieceType(p, k) - 1];
   });
   targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wk - 1];
   applyOn(targetThreat, [&](const Square & k){
      features.scores[F_attack] -= EvalConfig::threatByKing[PieceTools::getPieceType(p, k) - 1];
   });

   // threat by safe pawn
   const colored<BitBoard> safePawnAtt = {nonPawnMat[Co_Black] & BBTools::pawnAttacks<Co_White>(pawns[Co_White] & safeSquare[Co_White]),
                                          nonPawnMat[Co_White] & BBTools::pawnAttacks<Co_Black>(pawns[Co_Black] & safeSquare[Co_Black])};
   features.scores[F_attack] += EvalConfig::pawnSafeAtt * (countBit(safePawnAtt[Co_White]) - countBit(safePawnAtt[Co_Black]));

   // safe pawn push (protected once or not attacked)
   const colored<BitBoard> safePawnPush = {BBTools::shiftN<Co_White>(pawns[Co_White]) & ~occupancy & safeSquare[Co_White],
                                           BBTools::shiftN<Co_Black>(pawns[Co_Black]) & ~occupancy & safeSquare[Co_Black]};
   features.scores[F_mobility] += EvalConfig::pawnMobility * (countBit(safePawnPush[Co_White]) - countBit(safePawnPush[Co_Black]));

   // threat by safe pawn push
   features.scores[F_attack] +=
       EvalConfig::pawnSafePushAtt * (countBit(nonPawnMat[Co_Black] & BBTools::pawnAttacks<Co_White>(safePawnPush[Co_White])) -
                                      countBit(nonPawnMat[Co_White] & BBTools::pawnAttacks<Co_Black>(safePawnPush[Co_Black])));

   STOP_AND_SUM_TIMER(Eval4)

   // pieces mobility (second attack loop needed, knowing safeSquare ...)
   evalMob<P_wn, Co_White>(p, knights[Co_White], features.scores[F_mobility], safeSquare[Co_White], occupancy, data.mobility[Co_White]);
   evalMob<P_wb, Co_White>(p, bishops[Co_White], features.scores[F_mobility], safeSquare[Co_White], occupancy, data.mobility[Co_White]);
   evalMob<P_wr, Co_White>(p, rooks  [Co_White], features.scores[F_mobility], safeSquare[Co_White], occupancy, data.mobility[Co_White]);
   evalMobQ<     Co_White>(p, queens [Co_White], features.scores[F_mobility], safeSquare[Co_White], occupancy, data.mobility[Co_White]);
   evalMobK<     Co_White>(p, kings  [Co_White], features.scores[F_mobility], ~att[Co_Black]      , occupancy, data.mobility[Co_White]);
   evalMob<P_wn, Co_Black>(p, knights[Co_Black], features.scores[F_mobility], safeSquare[Co_Black], occupancy, data.mobility[Co_Black]);
   evalMob<P_wb, Co_Black>(p, bishops[Co_Black], features.scores[F_mobility], safeSquare[Co_Black], occupancy, data.mobility[Co_Black]);
   evalMob<P_wr, Co_Black>(p, rooks  [Co_Black], features.scores[F_mobility], safeSquare[Co_Black], occupancy, data.mobility[Co_Black]);
   evalMobQ<     Co_Black>(p, queens [Co_Black], features.scores[F_mobility], safeSquare[Co_Black], occupancy, data.mobility[Co_Black]);
   evalMobK<     Co_Black>(p, kings  [Co_Black], features.scores[F_mobility], ~att[Co_White]      , occupancy, data.mobility[Co_Black]);

   STOP_AND_SUM_TIMER(Eval5)

   // rook on open file
   features.scores[F_positional] += EvalConfig::rookOnOpenFile * countBit(rooks[Co_White] & pe.openFiles);
   features.scores[F_positional] += EvalConfig::rookOnOpenSemiFileOur * countBit(rooks[Co_White] & pe.semiOpenFiles[Co_White]);
   features.scores[F_positional] += EvalConfig::rookOnOpenSemiFileOpp * countBit(rooks[Co_White] & pe.semiOpenFiles[Co_Black]);
   features.scores[F_positional] -= EvalConfig::rookOnOpenFile * countBit(rooks[Co_Black] & pe.openFiles);
   features.scores[F_positional] -= EvalConfig::rookOnOpenSemiFileOur * countBit(rooks[Co_Black] & pe.semiOpenFiles[Co_Black]);
   features.scores[F_positional] -= EvalConfig::rookOnOpenSemiFileOpp * countBit(rooks[Co_Black] & pe.semiOpenFiles[Co_White]);

   // enemy rook facing king
   features.scores[F_positional] += EvalConfig::rookFrontKingMalus * countBit(BBTools::frontSpan<Co_White>(kings[Co_White]) & rooks[Co_Black]);
   features.scores[F_positional] -= EvalConfig::rookFrontKingMalus * countBit(BBTools::frontSpan<Co_Black>(kings[Co_Black]) & rooks[Co_White]);

   // enemy rook facing queen
   features.scores[F_positional] += EvalConfig::rookFrontQueenMalus * countBit(BBTools::frontSpan<Co_White>(queens[Co_White]) & rooks[Co_Black]);
   features.scores[F_positional] -= EvalConfig::rookFrontQueenMalus * countBit(BBTools::frontSpan<Co_Black>(queens[Co_Black]) & rooks[Co_White]);

   // queen aligned with own rook
   features.scores[F_positional] += EvalConfig::rookQueenSameFile * countBit(BBTools::fillFile(queens[Co_White]) & rooks[Co_White]);
   features.scores[F_positional] -= EvalConfig::rookQueenSameFile * countBit(BBTools::fillFile(queens[Co_Black]) & rooks[Co_Black]);

   // pins on king and queen
   const colored<BitBoard> pinnedK = {getPinned<Co_White>(p, p.king[Co_White]), getPinned<Co_Black>(p, p.king[Co_Black])};
   const colored<BitBoard> pinnedQ = {getPinned<Co_White>(p, whiteQueenSquare), getPinned<Co_Black>(p, blackQueenSquare)};
   for (Piece pp = P_wp; pp < P_wk; ++pp) {
      if (const BitBoard bw = p.pieces_const(Co_White, pp); bw) {
         if (pinnedK[Co_White] & bw) features.scores[F_attack] -= EvalConfig::pinnedKing[pp - 1] * countBit(pinnedK[Co_White] & bw);
         if (pinnedQ[Co_White] & bw) features.scores[F_attack] -= EvalConfig::pinnedQueen[pp - 1] * countBit(pinnedQ[Co_White] & bw);
      }
      if (const BitBoard bb = p.pieces_const(Co_Black, pp); bb) {
         if (pinnedK[Co_Black] & bb) features.scores[F_attack] += EvalConfig::pinnedKing[pp - 1] * countBit(pinnedK[Co_Black] & bb);
         if (pinnedQ[Co_Black] & bb) features.scores[F_attack] += EvalConfig::pinnedQueen[pp - 1] * countBit(pinnedQ[Co_Black] & bb);
      }
   }

   // attack : queen distance to opponent king (wrong if multiple queens ...)
   if (kingIsMandatory && blackQueenSquare != INVALIDSQUARE)
      features.scores[F_positional] -= EvalConfig::queenNearKing * (7 - chebyshevDistance(p.king[Co_White], blackQueenSquare));
   if (kingIsMandatory && whiteQueenSquare != INVALIDSQUARE)
      features.scores[F_positional] += EvalConfig::queenNearKing * (7 - chebyshevDistance(p.king[Co_Black], whiteQueenSquare));

   // number of pawn and piece type value  ///@todo closedness instead ?
   features.scores[F_material] += EvalConfig::adjRook[p.mat[Co_White][M_p]] * ScoreType(p.mat[Co_White][M_r]);
   features.scores[F_material] -= EvalConfig::adjRook[p.mat[Co_Black][M_p]] * ScoreType(p.mat[Co_Black][M_r]);
   features.scores[F_material] += EvalConfig::adjKnight[p.mat[Co_White][M_p]] * ScoreType(p.mat[Co_White][M_n]);
   features.scores[F_material] -= EvalConfig::adjKnight[p.mat[Co_Black][M_p]] * ScoreType(p.mat[Co_Black][M_n]);

   // bad bishop
   if (p.whiteLightBishop()) features.scores[F_material] -= EvalConfig::badBishop[countBit(pawns[Co_White] & whiteSquare)];
   if (p.whiteDarkBishop()) features.scores[F_material] -= EvalConfig::badBishop[countBit(pawns[Co_White] & blackSquare)];
   if (p.blackLightBishop()) features.scores[F_material] += EvalConfig::badBishop[countBit(pawns[Co_Black] & whiteSquare)];
   if (p.blackDarkBishop()) features.scores[F_material] += EvalConfig::badBishop[countBit(pawns[Co_Black] & blackSquare)];

   // adjust piece pair score
   features.scores[F_material] += ((p.mat[Co_White][M_b] > 1 ? EvalConfig::bishopPairBonus[p.mat[Co_White][M_p]] : 0) -
                                   (p.mat[Co_Black][M_b] > 1 ? EvalConfig::bishopPairBonus[p.mat[Co_Black][M_p]] : 0));
   features.scores[F_material] += ((p.mat[Co_White][M_n] > 1 ? EvalConfig::knightPairMalus : 0) -
                                   (p.mat[Co_Black][M_n] > 1 ? EvalConfig::knightPairMalus : 0));
   features.scores[F_material] += ((p.mat[Co_White][M_r] > 1 ? EvalConfig::rookPairMalus : 0) -
                                   (p.mat[Co_Black][M_r] > 1 ? EvalConfig::rookPairMalus : 0));

   // complexity
   // an "anti human" trick : winning side wants to keep the position more complex
   features.scores[F_complexity] += EvalScore {1, 0} * (white2Play ? +1 : -1) * 
                                    (p.mat[Co_White][M_t] + p.mat[Co_Black][M_t] + (p.mat[Co_White][M_p] + p.mat[Co_Black][M_p]) / 2);

   if (display) displayEval(data, features);

   // taking Style into account, giving each score a [0..2] factor
   features.scores[F_material]    = features.scores[F_material]   .scale(1 + (DynamicConfig::styleMaterial    - 50) / 50.f, 1.f);
   features.scores[F_positional]  = features.scores[F_positional] .scale(1 + (DynamicConfig::stylePositional  - 50) / 50.f, 1.f);
   features.scores[F_development] = features.scores[F_development].scale(1 + (DynamicConfig::styleDevelopment - 50) / 50.f, 1.f);
   features.scores[F_mobility]    = features.scores[F_mobility]   .scale(1 + (DynamicConfig::styleMobility    - 50) / 50.f, 1.f);
   features.scores[F_pawnStruct]  = features.scores[F_pawnStruct] .scale(1 + (DynamicConfig::stylePawnStruct  - 50) / 50.f, 1.f);
   features.scores[F_attack]      = features.scores[F_attack]     .scale(1 + (DynamicConfig::styleAttack      - 50) / 50.f, 1.f);
   features.scores[F_complexity]  = features.scores[F_complexity] .scale((DynamicConfig::styleComplexity      - 50) / 50.f, 0.f);

   // sum every score contribution
   EvalScore score = features.SumUp();

#ifdef VERBOSE_EVAL
   if (display) {
      Logging::LogIt(Logging::logInfo) << "Post correction ... ";
      displayEval(data, features);
      Logging::LogIt(Logging::logInfo) << "> All (before 2nd order) " << score;
   }
#endif

   // initiative (kind of second order pawn structure stuff for end-games)
   const BitBoard &allPawns = p.allPawn();
   EvalScore initiativeBonus = EvalConfig::initiative[0] * countBit(allPawns) +
                               EvalConfig::initiative[1] * ((allPawns & queenSide) && (allPawns & kingSide)) +
                               EvalConfig::initiative[2] * (countBit(occupancy & ~allPawns) == 2) - EvalConfig::initiative[3];
   initiativeBonus = EvalScore(sgn(score[MG]) * std::max(initiativeBonus[MG], static_cast<ScoreType>(-Abs(score[MG]))),
                               sgn(score[EG]) * std::max(initiativeBonus[EG], static_cast<ScoreType>(-Abs(score[EG]))));
   score += initiativeBonus;

#ifdef VERBOSE_EVAL
   if (display) {
      Logging::LogIt(Logging::logInfo) << "Initiative    " << initiativeBonus;
      Logging::LogIt(Logging::logInfo) << "> All (with initiative) " << score;
   }
#endif

   // add contempt
   score += context.contempt;

#ifdef VERBOSE_EVAL
   if (display) { Logging::LogIt(Logging::logInfo) << "> All (with contempt) " << score; }
#endif

   // compute a scale factor in some other end-game cases that are not using the material table:
   if (features.scalingFactor == 1.f && !DynamicConfig::armageddon) {
      const Color strongSide = score[EG] > 0 ? Co_White : Co_Black;
      
      // opposite colored bishops (scale based on passed pawn of strong side)
      if (p.mat[Co_White][M_b] == 1 && p.mat[Co_Black][M_b] == 1 && countBit(p.allBishop() & whiteSquare) == 1) {
         if (p.mat[Co_White][M_t] == 1 && p.mat[Co_Black][M_t] == 1) // only bishops and pawn
            features.scalingFactor =
                (EvalConfig::scalingFactorOppBishopAlone + EvalConfig::scalingFactorOppBishopAloneSlope * countBit(pe.passed[strongSide])) / 128.f;
         else // more pieces present
            features.scalingFactor =
                (EvalConfig::scalingFactorOppBishop + EvalConfig::scalingFactorOppBishopSlope * countBit(p.allPieces[strongSide])) / 128.f;
      }
      
      // queen versus no queen (scale on number of minor of no queen side)
      else if (countBit(p.allQueen()) == 1) {
         features.scalingFactor =
             (EvalConfig::scalingFactorQueenNoQueen + EvalConfig::scalingFactorQueenNoQueenSlope * p.mat[p.whiteQueen() ? Co_Black : Co_White][M_m]) / 128.f;
      }
      
      // scale based on the number of pawn of strong side
      else
         features.scalingFactor =
             std::min(1.f, (EvalConfig::scalingFactorPawns + EvalConfig::scalingFactorPawnsSlope * p.mat[strongSide][M_p]) / 128.f);

      // moreover scaledown if pawn on only one side
      if (allPawns && !((allPawns & queenSide) && (allPawns & kingSide))) { features.scalingFactor -= EvalConfig::scalingFactorPawnsOneSide / 128.f; }
   }
   if (DynamicConfig::armageddon) features.scalingFactor = 1.f; ///@todo better

#ifdef VERBOSE_EVAL
   if (display) { Logging::LogIt(Logging::logInfo) << "Scaling factor (corrected) " << features.scalingFactor; }
#endif

   // fuse MG and EG score
   // scale both game phase and end-game scaling factor
   ScoreType ret = ScaleScore(score, data.gp, features.scalingFactor);

#ifdef VERBOSE_EVAL
   if (display) { Logging::LogIt(Logging::logInfo) << "> All (with game phase scaling) " << ret; }
#endif

   if (!DynamicConfig::armageddon){
      // apply scaling factor based on fifty move rule
      ret = fiftyScale(ret, p.fifty);
   }

#ifdef VERBOSE_EVAL
   if (display) { Logging::LogIt(Logging::logInfo) << "> All (with last scaling) " << ret; }
#endif

   // apply tempo
   ret += EvalConfig::tempo * (white2Play ? +1 : -1);

#ifdef VERBOSE_EVAL
   if (display) { Logging::LogIt(Logging::logInfo) << "> All (with tempo) " << ret; }
#endif

   // clamp score
   ret = clampScore(ret);

#ifdef VERBOSE_EVAL
   if (display) { Logging::LogIt(Logging::logInfo) << "> All (clamped) " << ret; }
#endif

   // take side to move into account
   ret = (white2Play ? +1 : -1) * ret;

   if (display) { Logging::LogIt(Logging::logInfo) << "==> All (fully scaled) " << ret; }

   data.evalDone = true;
   // apply variante scoring if requiered
   const ScoreType hceScore = variantScore(ret, p.halfmoves, context.height_, p.c);

#ifdef WITH_NNUE
   if ( DynamicConfig::useNNUE && !forbiddenNNUE && Abs(hceScore) <= DynamicConfig::NNUEThreshold2 ){
      // if HCE is small (there is something more than just material value going on ...), fall back to NNUE;
      const ScoreType nnueScore = NNUEEVal(p, data, context, features, true);
      STOP_AND_SUM_TIMER(Eval)
      return nnueScore;
   }
#endif

   STOP_AND_SUM_TIMER(Eval)
   return hceScore;
}

#pragma GCC diagnostic pop
