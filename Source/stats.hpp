#pragma once

#include "definition.hpp"

#include "logging.hpp"

/* This array is used to store statistic of search and evaluation
 * for each thread.
 */
struct Stats{
    enum StatId { sid_nodes = 0, sid_qnodes, sid_tthits, sid_ttInsert, sid_ttPawnhits, sid_ttPawnInsert, sid_ttschits, sid_ttscmiss, sid_materialTableHits, sid_materialTableMiss, sid_materialTableHelper, sid_materialTableDraw , sid_materialTableDraw2, sid_staticNullMove, sid_lmr, sid_lmrFail, sid_pvsFail, sid_razoringTry, sid_razoring, sid_nullMoveTry, sid_nullMoveTry2, sid_nullMoveTry3, sid_nullMove, sid_nullMove2, sid_probcutTry, sid_probcutTry2, sid_probcut, sid_lmp, sid_historyPruning, sid_futility, sid_CMHPruning, sid_see, sid_see2, sid_seeQuiet, sid_iid, sid_ttalpha, sid_ttbeta, sid_checkExtension, sid_checkExtension2, sid_recaptureExtension, sid_castlingExtension, sid_CMHExtension, sid_pawnPushExtension, sid_singularExtension, sid_singularExtension2, sid_singularExtension3, sid_singularExtension4, sid_queenThreatExtension, sid_BMExtension, sid_mateThreatExtension, sid_endGameExtension, sid_goodHistoryExtension, sid_tbHit1, sid_tbHit2, sid_dangerPrune, sid_dangerReduce, sid_hashComputed, sid_qfutility, sid_qsee, sid_delta, sid_evalNoKing, sid_maxid };
    static const std::array<std::string,sid_maxid> Names;
    std::array<Counter,sid_maxid> counters;
    void init(){ Logging::LogIt(Logging::logInfo) << "Init stat" ;  counters.fill(0ull); }
};

