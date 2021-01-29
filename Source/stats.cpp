#include "stats.hpp"

const std::array<std::string,Stats::sid_maxid> Stats::Names = { 
    "nodes", "qnodes", 
    "tthits", "ttInsert", "ttPawnhits", "ttPawnInsert", "ttScHits", "ttScMiss", 
    "materialHits", "materialMiss", "materialHelper", "materialDraw", "materialDraw2", 
    "staticNullMove", "lmr", "lmrfail", "pvsfail", "razoringTry", "razoring", 
    "nullMoveTry", "nullMoveTry2", "nullMoveTry3", "nullMove", "nullMove2", "probcutTry", "probcutTry2", "probcut", 
    "lmp", "historyPruning", "futility", "CMHPruning", "see", "see2", "seeQuiet", "iid", 
    "ttalpha", "ttbeta", 
    "checkExtension", "checkExtension2", "recaptureExtension", "castlingExtension", "CMHExtension", "pawnPushExtension", 
    "singularExtension", "singularExtension2", "singularExtension3", "singularExtension4", "queenThreatExtension", 
    "BMExtension", "mateThreatExtension", "endGameExtension", "goodHistoryExtension", 
    "TBHit1", "TBHit2", "dangerPrune", "dangerReduce", "computedHash", "qfutility", "qsee", 
    "delta", "evalNoKing", "evalStd", "evalNNUE"
};
