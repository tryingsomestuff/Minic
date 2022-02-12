#include "tools.hpp"

#include "dynamicConfig.hpp"
#include "evalDef.hpp"
#include "hash.hpp"
#include "logging.hpp"
#include "pieceTools.hpp"
#include "position.hpp"
#include "positionTools.hpp"
#include "searcher.hpp"
#include "score.hpp"
#include "threading.hpp"

std::string trim(const std::string& str, const std::string& whitespace) {
   const auto strBegin = str.find_first_not_of(whitespace);
   if (strBegin == std::string::npos) return ""; // no content
   const auto strEnd   = str.find_last_not_of(whitespace);
   const auto strRange = strEnd - strBegin + 1;
   return str.substr(strBegin, strRange);
}

void tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters) {
   std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
   std::string::size_type pos     = str.find_first_of(delimiters, lastPos);
   while (std::string::npos != pos || std::string::npos != lastPos) {
      tokens.push_back(str.substr(lastPos, pos - lastPos));
      lastPos = str.find_first_not_of(delimiters, pos);
      pos     = str.find_first_of(delimiters, lastPos);
   }
}

std::vector<std::string> tokenize(const std::string& str, const std::string& delimiters) {
   std::vector<std::string> tokens;
   tokenize(str,tokens,delimiters);
   return tokens;
}

#ifdef DEBUG_KING_CAP
void debug_king_cap(const Position& p) {
   if (!p.whiteKing() || !p.blackKing()) {
      std::cout << "no more king" << std::endl;
      std::cout << ToString(p) << std::endl;
      std::cout << ToString(p.lastMove) << std::endl;
      Distributed::finalize();
      exit(1);
   }
}
#else
void debug_king_cap(const Position&) { ; }
#endif

std::string ToString(const MiniMove& m) { return ToString(Move(m), false); }

std::string ToString(const Move& m, bool withScore) {
   if (sameMove(m, INVALIDMOVE)) return "invalid move"; ///@todo 0000 ?
   if (sameMove(m, NULLMOVE)) return "null move"; ///@todo 0000 ?

   const std::string score = (withScore ? " (" + std::to_string(Move2Score(m)) + ")" : "");
   if (!DynamicConfig::FRC) { // FRC castling is encoded king takes rook
      switch (Move2Type(m)) {
         case T_bks: return "e8g8" + score;
         case T_wks: return "e1g1" + score;
         case T_bqs: return "e8c8" + score;
         case T_wqs: return "e1c1" + score;
         default:
         break;
      }
   }
   constexpr std::string_view promSuffixe[] = {"", "", "", "", "q", "r", "b", "n", "q", "r", "b", "n", "", "", "", ""};
   const std::string_view & prom = promSuffixe[Move2Type(m)];
   return SquareNames[Move2From(m)] + SquareNames[Move2To(m)] + std::string(prom) + score;
}

std::string ToString(const PVList& moves) {
   std::stringstream ss;
   for (size_t k = 0; k < moves.size(); ++k) {
      if (moves[k] == INVALIDMOVE) break;
      ss << ToString(moves[k]) << " ";
   }
   return ss.str();
}

std::string ToString(const Position::Material& mat) {
   std::stringstream str;
   str << "\n"
       << Logging::_protocolComment[Logging::ct] << "K  :" << static_cast<int>(mat[Co_White][M_k]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "Q  :" << static_cast<int>(mat[Co_White][M_q]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "R  :" << static_cast<int>(mat[Co_White][M_r]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "B  :" << static_cast<int>(mat[Co_White][M_b]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "L  :" << static_cast<int>(mat[Co_White][M_bl]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "D  :" << static_cast<int>(mat[Co_White][M_bd]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "N  :" << static_cast<int>(mat[Co_White][M_n]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "P  :" << static_cast<int>(mat[Co_White][M_p]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "Ma :" << static_cast<int>(mat[Co_White][M_M]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "Mi :" << static_cast<int>(mat[Co_White][M_m]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "T  :" << static_cast<int>(mat[Co_White][M_t]) << "\n";
   str << "\n"
       << Logging::_protocolComment[Logging::ct] << "k  :" << static_cast<int>(mat[Co_Black][M_k]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "q  :" << static_cast<int>(mat[Co_Black][M_q]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "r  :" << static_cast<int>(mat[Co_Black][M_r]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "b  :" << static_cast<int>(mat[Co_Black][M_b]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "l  :" << static_cast<int>(mat[Co_Black][M_bl]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "d  :" << static_cast<int>(mat[Co_Black][M_bd]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "n  :" << static_cast<int>(mat[Co_Black][M_n]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "p  :" << static_cast<int>(mat[Co_Black][M_p]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "ma :" << static_cast<int>(mat[Co_Black][M_M]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "mi :" << static_cast<int>(mat[Co_Black][M_m]) << "\n"
       << Logging::_protocolComment[Logging::ct] << "t  :" << static_cast<int>(mat[Co_Black][M_t]) << "\n";
   return str.str();
}

std::string ToString(const Position& p, bool noEval) {
   std::stringstream ss;
   ss << "Position" << std::endl;
   for (Square j = 7; j >= 0; --j) {
      ss << Logging::_protocolComment[Logging::ct] << " +-+-+-+-+-+-+-+-+" << std::endl;
      ss << Logging::_protocolComment[Logging::ct] << " |";
      for (Square i = 0; i < 8; ++i) ss << PieceTools::getName(p, static_cast<Square>(i + j * 8)) << '|';
      ss << std::endl;
   }
   ss << Logging::_protocolComment[Logging::ct] << " +-+-+-+-+-+-+-+-+" << std::endl;
   if (p.ep >= 0) ss << Logging::_protocolComment[Logging::ct] << " ep " << SquareNames[p.ep] << std::endl;
   //ss << Logging::_protocolComment[Logging::ct] << " wk " << (p.king[Co_White]!=INVALIDSQUARE?SquareNames[p.king[Co_White]]:"none") << std::endl;
   //ss << Logging::_protocolComment[Logging::ct] << " bk " << (p.king[Co_Black]!=INVALIDSQUARE?SquareNames[p.king[Co_Black]]:"none") << std::endl;
   ss << Logging::_protocolComment[Logging::ct] << " Turn " << (p.c == Co_White ? "white" : "black") << std::endl;
   ScoreType sc = 0;
   if (!noEval) {
      EvalData data;
#ifdef WITH_NNUE
      NNUEEvaluator evaluator;
      if (!p.associatedEvaluator) {
         p.associatedEvaluator = &evaluator;
         p.resetNNUEEvaluator(evaluator);
      }
#endif
      const bool tmp = DynamicConfig::forceNNUE;
      if (DynamicConfig::useNNUE) DynamicConfig::forceNNUE = true;
      sc = eval(p, data, ThreadPool::instance().main(),true);
      if (DynamicConfig::useNNUE) DynamicConfig::forceNNUE = tmp;
      ss << Logging::_protocolComment[Logging::ct] << " Phase " << data.gp << std::endl;
      ss << Logging::_protocolComment[Logging::ct] << " Static score " << sc << std::endl;
      ss << Logging::_protocolComment[Logging::ct] << " Hash " << computeHash(p) << std::endl;
   }
   ss << Logging::_protocolComment[Logging::ct] << " FEN " << GetFEN(p);
   //ss << ToString(p.mat);
   return ss.str();
}

std::string ToString(const BitBoard& b) {
   std::bitset<64> bs(b);
   std::stringstream ss;
   for (int j = 7; j >= 0; --j) {
      ss << "\n";
      ss << Logging::_protocolComment[Logging::ct] << "+-+-+-+-+-+-+-+-+"
         << "\n";
      ss << Logging::_protocolComment[Logging::ct] << "|";
      for (int i = 0; i < 8; ++i) ss << (bs[i + j * 8] ? "X" : " ") << '|';
   }
   ss << "\n";
   ss << Logging::_protocolComment[Logging::ct] << "+-+-+-+-+-+-+-+-+";
   return ss.str();
}

bool checkEval(const Position & p, ScoreType e, Searcher & context, const std::string & txt){
   EvalData data;
   const ScoreType f = eval(p, data, context, true, true);
#ifdef DEBUG_EVALSYM
   Position p2 = p;
   p2.c = ~p2.c;
   const ScoreType g = eval(p2, data, context, true, false);
   if ( std::abs(f + g - 2*EvalConfig::tempo) > 2){
      std::cout << "*********************" << std::endl;
      std::cout << ToString(p) << std::endl;
      std::cout << f << std::endl;
      std::cout << g - 2*EvalConfig::tempo << std::endl;
      std::cout << "EVALSYMERROR" << std::endl;
   }
#endif
   if ( std::abs(e - f) > std::max(std::abs(e)/20,10)){
      std::cout << "*********************" << std::endl;
      std::cout << ToString(p) << std::endl;
      std::cout << e << std::endl;
      std::cout << f << std::endl;
      std::cout << "EVALERROR : " << txt << std::endl;
      return false;
   }
   else{
      std::cout << "EVALOK : " << txt << std::endl;
      return true;
   }      
}
