#include "tools.hpp"

#include "dynamicConfig.hpp"
#include "evalDef.hpp"
#include "hash.hpp"
#include "logging.hpp"
#include "pieceTools.hpp"
#include "position.hpp"
#include "positionTools.hpp"
#include "score.hpp"
#include "smp.hpp"

std::string trim(const std::string& str, const std::string& whitespace){
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) return ""; // no content
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

void tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters ){
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    std::string::size_type pos     = str.find_first_of(delimiters, lastPos);
    while (std::string::npos != pos || std::string::npos != lastPos) {
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        lastPos = str.find_first_not_of(delimiters, pos);
        pos     = str.find_first_of(delimiters, lastPos);
    }
}

#ifdef DEBUG_KING_CAP
void debug_king_cap(const Position & p){
    if ( !p.whiteKing()||!p.blackKing()){
       std::cout << "no more king" << std::endl;
       std::cout << ToString(p) << std::endl;
       std::cout << ToString(p.lastMove) << std::endl;
       Distributed::finalize();
       exit(1);
    }
}
#else
void debug_king_cap(const Position &){;}
#endif

std::string ToString(const MiniMove & m){
    return ToString(Move(m),false);
}

std::string ToString(const Move & m, bool withScore){
    if (sameMove(m,INVALIDMOVE)) return "invalid move";
    if (sameMove(m,NULLMOVE))    return "null move";
    
    std::string prom;
    const std::string score = (withScore ? " (" + std::to_string(Move2Score(m)) + ")" : "");
    switch (Move2Type(m)) {
    case T_bks: return (DynamicConfig::FRC?"O-O":"e8g8") + score;
    case T_wks: return (DynamicConfig::FRC?"O-O":"e1g1") + score;
    case T_bqs: return (DynamicConfig::FRC?"O-O-O":"e8c8") + score;
    case T_wqs: return (DynamicConfig::FRC?"O-O-O":"e1c1") + score;
    default:
        static const std::string promSuffixe[] = { "","","","","q","r","b","n","q","r","b","n" };
        prom = promSuffixe[Move2Type(m)];
        break;
    }
    return SquareNames[Move2From(m)] + SquareNames[Move2To(m)] + prom + score;
}

std::string ToString(const PVList & moves){
    std::stringstream ss;
    for (size_t k = 0; k < moves.size(); ++k) { if (moves[k] == INVALIDMOVE) break;  ss << ToString(moves[k]) << " "; }
    return ss.str();
}

std::string ToString(const Position::Material & mat) {
    std::stringstream str;
    str << "\n" << Logging::_protocolComment[Logging::ct] << "K  :" << (int)mat[Co_White][M_k] << "\n" << Logging::_protocolComment[Logging::ct] << "Q  :" << (int)mat[Co_White][M_q] << "\n" << Logging::_protocolComment[Logging::ct] << "R  :" << (int)mat[Co_White][M_r] << "\n" << Logging::_protocolComment[Logging::ct] << "B  :" << (int)mat[Co_White][M_b] << "\n" << Logging::_protocolComment[Logging::ct] << "L  :" << (int)mat[Co_White][M_bl] << "\n" << Logging::_protocolComment[Logging::ct] << "D  :" << (int)mat[Co_White][M_bd] << "\n" << Logging::_protocolComment[Logging::ct] << "N  :" << (int)mat[Co_White][M_n] << "\n" << Logging::_protocolComment[Logging::ct] << "P  :" << (int)mat[Co_White][M_p] << "\n" << Logging::_protocolComment[Logging::ct] << "Ma :" << (int)mat[Co_White][M_M] << "\n" << Logging::_protocolComment[Logging::ct] << "Mi :" << (int)mat[Co_White][M_m] << "\n" << Logging::_protocolComment[Logging::ct] << "T  :" << (int)mat[Co_White][M_t] << "\n";
    str << "\n" << Logging::_protocolComment[Logging::ct] << "k  :" << (int)mat[Co_Black][M_k] << "\n" << Logging::_protocolComment[Logging::ct] << "q  :" << (int)mat[Co_Black][M_q] << "\n" << Logging::_protocolComment[Logging::ct] << "r  :" << (int)mat[Co_Black][M_r] << "\n" << Logging::_protocolComment[Logging::ct] << "b  :" << (int)mat[Co_Black][M_b] << "\n" << Logging::_protocolComment[Logging::ct] << "l  :" << (int)mat[Co_Black][M_bl] << "\n" << Logging::_protocolComment[Logging::ct] << "d  :" << (int)mat[Co_Black][M_bd] << "\n" << Logging::_protocolComment[Logging::ct] << "n  :" << (int)mat[Co_Black][M_n] << "\n" << Logging::_protocolComment[Logging::ct] << "p  :" << (int)mat[Co_Black][M_p] << "\n" << Logging::_protocolComment[Logging::ct] << "ma :" << (int)mat[Co_Black][M_M] << "\n" << Logging::_protocolComment[Logging::ct] << "mi :" << (int)mat[Co_Black][M_m] << "\n" << Logging::_protocolComment[Logging::ct] << "t  :" << (int)mat[Co_Black][M_t] << "\n";
    return str.str();
}

std::string ToString(const Position & p, bool noEval){
    std::stringstream ss;
    ss << "Position" << std::endl;
    for (Square j = 7; j >= 0; --j) {
        ss << Logging::_protocolComment[Logging::ct] << " +-+-+-+-+-+-+-+-+" << std::endl << Logging::_protocolComment[Logging::ct] << " |";
        for (Square i = 0; i < 8; ++i) ss << PieceTools::getName(p,i+j*8) << '|';
        ss << std::endl;
    }
    ss << Logging::_protocolComment[Logging::ct] << " +-+-+-+-+-+-+-+-+" << std::endl;
    if ( p.ep >=0 ) ss << Logging::_protocolComment[Logging::ct] << " ep " << SquareNames[p.ep] << std::endl;
    //ss << Logging::_protocolComment[Logging::ct] << " wk " << (p.king[Co_White]!=INVALIDSQUARE?SquareNames[p.king[Co_White]]:"none") << std::endl;
    //ss << Logging::_protocolComment[Logging::ct] << " bk " << (p.king[Co_Black]!=INVALIDSQUARE?SquareNames[p.king[Co_Black]]:"none") << std::endl;
    ss << Logging::_protocolComment[Logging::ct] << " Turn " << (p.c == Co_White ? "white" : "black") << std::endl;
    ScoreType sc = 0;
    if ( ! noEval ){
        EvalData data;
#ifdef WITH_NNUE
        NNUEEvaluator evaluator;
        if ( !p.associatedEvaluator ){
           p.associatedEvaluator = &evaluator;
           p.resetNNUEEvaluator(evaluator);
        }
#endif
        sc = eval(p, data, ThreadPool::instance().main());
        ss << Logging::_protocolComment[Logging::ct] << " Phase " << data.gp << std::endl << Logging::_protocolComment[Logging::ct] << " Static score " << sc << std::endl << Logging::_protocolComment[Logging::ct] << " Hash " << computeHash(p) << std::endl << Logging::_protocolComment[Logging::ct] << " FEN " << GetFEN(p);
    }
    //ss << ToString(p.mat);
    return ss.str();
}
