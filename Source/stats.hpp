#pragma once

#include "definition.hpp"
#include "logging.hpp"

/*!
 * This array is used to store statistic of search and evaluation
 * for each thread.
 */
struct Stats {
   enum StatId {
      sid_nodes = 0,
      sid_qnodes,
      sid_tthits,
      sid_ttInsert,
      sid_ttPawnhits,
      sid_ttPawnInsert,
      sid_ttschits,
      sid_ttscmiss,
      sid_staticScoreIsFromSearch,
      sid_qsearchInCheck,
      sid_qEvalNullMoveTrick,
      sid_materialTableHits,
      sid_materialTableHitsSearch,
      sid_materialTableMiss,
      sid_materialTableMissSearch,
      sid_materialTableHelper,
      sid_materialTableDraw,
      sid_materialTableDraw2,
      sid_staticNullMove,
      sid_threatsPruning,
      sid_lmr,
      sid_lmrcap,
      sid_lmrFail,
      sid_pvsFail,
      sid_razoringTry,
      sid_razoring,
      sid_razoringNoQ,
      sid_nullMoveTry,
      sid_nullMoveTry2,
      sid_nullMoveTry3,
      sid_nullMove,
      sid_nullMove2,
      sid_probcutTry,
      sid_probcutTry2,
      sid_probcut,
      sid_lmp,
      sid_lmrAR,
      sid_historyPruning,
      sid_futility,
      sid_CMHPruning,
      sid_capHistPruning,
      sid_see,
      sid_see2,
      sid_seeQuiet,
      sid_iid,
      sid_ttalpha,
      sid_ttbeta,
      sid_alpha,
      sid_beta,
      sid_alphanoupdate,
      sid_qttalpha,
      sid_qttbeta,
      sid_qalpha,
      sid_qbeta,
      sid_qalphanoupdate,
      sid_checkExtension,
      sid_checkExtension2,
      sid_recaptureExtension,
      sid_castlingExtension,
      sid_CMHExtension,
      sid_pawnPushExtension,
      sid_singularExtension,
      sid_singularExtension2,
      sid_singularExtension3,
      sid_singularExtension4,
      sid_queenThreatExtension,
      sid_BMExtension,
      sid_mateThreatExtension,
      sid_endGameExtension,
      sid_goodHistoryExtension,
      sid_tbHit1,
      sid_tbHit2,
      sid_dangerPrune,
      sid_dangerForwardPrune,
      sid_dangerReduce,
      sid_hashComputed,
      sid_qfutility,
      sid_qsee,
      sid_deltaAlpha,
      sid_deltaBeta,
      sid_evalNoKing,
      sid_evalStd,
      sid_evalNNUE,
      sid_PVupdate,
      sid_maxid
   };

   static const std::array<std::string, sid_maxid> Names;
   std::array<Counter, sid_maxid>                  counters;

#ifdef WITH_STATS
   inline void incr(StatId id) { ++counters[id]; }
#else
   inline void incr(StatId) {}
#endif

   void init() {
      Logging::LogIt(Logging::logInfo) << "Init stat";
      counters.fill(0ull);
   }
};
