#include "positionTools.hpp"

#include <cstdlib>

#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "moveGen.hpp"
#include "pieceTools.hpp"
#include "tools.hpp"

std::string GetFENShort(const Position &p) { // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR"
   std::stringstream ss;
   int count = 0;
   for (int i = 7; i >= 0; --i) {
      for (int j = 0; j < 8; j++) {
         const Square k = static_cast<Square>(8 * i + j);
         if (p.board_const(k) == P_none) ++count;
         else {
            if (count != 0) {
               ss << count;
               count = 0;
            }
            ss << PieceTools::getName(p, k);
         }
         if (j == 7) {
            if (count != 0) {
               ss << count;
               count = 0;
            }
            if (i != 0) ss << "/";
         }
      }
   }
   return ss.str();
}

std::string GetFENShort2(const Position &p) { // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d5
   std::stringstream ss;
   ss << GetFENShort(p) << " " << (p.c == Co_White ? "w" : "b") << " ";
   bool withCastling = false;

   std::string castlingChar = "KQkq";
   if (DynamicConfig::FRC) {
      if (p.castling & C_wqs) {
         for (Square s = Sq_a1; s <= Sq_h1; ++s) {
            if (s < p.king[Co_White] && p.board_const(s) == P_wr) {
               castlingChar[1] = static_cast<char>(std::toupper(FileNames[SQFILE(s)][0]));
               break;
            }
         }
      }
      if (p.castling & C_wks) {
         for (Square s = Sq_a1; s <= Sq_h1; ++s) {
            if (s > p.king[Co_White] && p.board_const(s) == P_wr) {
               castlingChar[0] = static_cast<char>(std::toupper(FileNames[SQFILE(s)][0]));
               break;
            }
         }
      }
      if (p.castling & C_bqs) {
         for (Square s = Sq_a8; s <= Sq_h8; ++s) {
            if (s < p.king[Co_Black] && p.board_const(s) == P_br) {
               castlingChar[3] = static_cast<char>(std::tolower(FileNames[SQFILE(s)][0]));
               break;
            }
         }
      }
      if (p.castling & C_bks) {
         for (Square s = Sq_a8; s <= Sq_h8; ++s) {
            if (s > p.king[Co_Black] && p.board_const(s) == P_br) {
               castlingChar[2] = static_cast<char>(std::tolower(FileNames[SQFILE(s)][0]));
               break;
            }
         }
      }
   }

   if (p.castling & C_wks) {
      ss << castlingChar[0];
      withCastling = true;
   }
   if (p.castling & C_wqs) {
      ss << castlingChar[1];
      withCastling = true;
   }
   if (p.castling & C_bks) {
      ss << castlingChar[2];
      withCastling = true;
   }
   if (p.castling & C_bqs) {
      ss << castlingChar[3];
      withCastling = true;
   }
   if (!withCastling) ss << "-";
   if (p.ep != INVALIDSQUARE) ss << " " << SquareNames[p.ep];
   else
      ss << " -";
   return ss.str();
}

std::string GetFEN(const Position &p) { // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d5 0 2"
   std::stringstream ss;
   ss << GetFENShort2(p) << " " << static_cast<int>(p.fifty) << " " << static_cast<int>(p.moves);
   return ss.str();
}

std::string SanitizeCastling(const Position &p, const std::string &str) {
   // standard chess
   if ((str == "e1g1" && p.board_const(Sq_e1) == P_wk) || (str == "e8g8" && p.board_const(Sq_e8) == P_bk)) return "0-0";
   if ((str == "e1c1" && p.board_const(Sq_e1) == P_wk) || (str == "e8c8" && p.board_const(Sq_e8) == P_bk)) return "0-0-0";
   // FRC is handled in readMove
   return str;
}

Square kingSquare(const Position &p) { return p.king[p.c]; }

bool readMove(const Position &p, const std::string &ss, Square &from, Square &to, MType &moveType, bool forbidCastling) {
   if (ss.empty()) {
      Logging::LogIt(Logging::logFatal) << "Trying to read empty move ! ";
      moveType = T_std;
      return false;
   }
   std::string str(ss);
   str = SanitizeCastling(p, str);
   // add space to go to own internal notation (if not castling)
   if (str != "0-0" && str != "0-0-0" && str != "O-O" && str != "O-O-O") str.insert(2, " ");
   else if ( forbidCastling ) return false;

   std::vector<std::string> strList;
   std::stringstream iss(str);
   std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));

   moveType = T_std;
   if (strList.empty()) {
      Logging::LogIt(Logging::logError) << "Trying to read bad move, seems empty " << str;
      return false;
   }

   // detect castling
   if (strList[0] == "0-0" || strList[0] == "O-O") {
      moveType = (p.c == Co_White) ? T_wks : T_bks;
      from     = kingSquare(p);
      to       = (p.c == Co_White) ? p.rootInfo().rooksInit[Co_White][CT_OO] : p.rootInfo().rooksInit[Co_Black][CT_OO];
   }
   else if (strList[0] == "0-0-0" || strList[0] == "O-O-O") {
      moveType = (p.c == Co_White) ? T_wqs : T_bqs;
      from     = kingSquare(p);
      to       = (p.c == Co_White) ? p.rootInfo().rooksInit[Co_White][CT_OOO] : p.rootInfo().rooksInit[Co_Black][CT_OOO];
   }
   else {
      if (strList.size() == 1) {
         Logging::LogIt(Logging::logError) << "Trying to read bad move, malformed (=1) " << str;
         return false;
      }
      if (strList.size() > 2 && strList[2] != "ep") {
         Logging::LogIt(Logging::logError) << "Trying to read bad move, malformed (>2)" << str;
         return false;
      }
      if (strList[0].size() == 2 && (strList[0].at(0) >= 'a') && (strList[0].at(0) <= 'h') && ((strList[0].at(1) >= 1) && (strList[0].at(1) <= '8')))
         from = stringToSquare(strList[0]);
      else {
         Logging::LogIt(Logging::logError) << "Trying to read bad move, invalid from square " << str;
         return false;
      }
      bool isCapture = false;
      // be carefull, promotion possible !
      if (strList[1].size() >= 2 && (strList[1].at(0) >= 'a') && (strList[1].at(0) <= 'h') &&
          ((strList[1].at(1) >= '1') && (strList[1].at(1) <= '8'))) {
         if (strList[1].size() > 2) { // promotion
            std::string prom;
            if (strList[1].size() == 3) { // probably e7 e8q notation
               prom = strList[1][2];
               to   = stringToSquare(strList[1].substr(0, 2));
            }
            else { // probably e7 e8=q notation
               std::vector<std::string> strListTo;
               tokenize(strList[1], strListTo, "=");
               if (strListTo.size() != 2) {
                  Logging::LogIt(Logging::logError) << "Trying to read bad move, invalid promotion syntaxe " << str;
                  return false;
               }
               to   = stringToSquare(strListTo[0]);
               prom = strListTo[1];
            }
            isCapture = p.board_const(to) != P_none;
            if (prom == "Q" || prom == "q") moveType = isCapture ? T_cappromq : T_promq;
            else if (prom == "R" || prom == "r")
               moveType = isCapture ? T_cappromr : T_promr;
            else if (prom == "B" || prom == "b")
               moveType = isCapture ? T_cappromb : T_promb;
            else if (prom == "N" || prom == "n")
               moveType = isCapture ? T_cappromn : T_promn;
            else {
               Logging::LogIt(Logging::logError) << "Trying to read bad move, invalid to square " << str;
               return false;
            }
         }
         else {
            to        = stringToSquare(strList[1]);
            isCapture = p.board_const(to) != P_none;
            if (isCapture) moveType = T_capture;
         }
      }
      else {
         Logging::LogIt(Logging::logError) << "Trying to read bad move, invalid to square " << str;
         return false;
      }
      if (PieceTools::getPieceType(p, from) == P_wp && to == p.ep) moveType = T_ep;
   }
   if (DynamicConfig::FRC) {
      // In FRC, some castling may be encoded king takes rooks ... Let's check that, the dirty way
      if (p.board_const(from) == P_wk && p.board_const(to) == P_wr) { moveType = (p.rootInfo().rooksInit[Co_White][CT_OOO] == to ? T_wqs : T_wks); }
      if (p.board_const(from) == P_bk && p.board_const(to) == P_br) { moveType = (p.rootInfo().rooksInit[Co_Black][CT_OOO] == to ? T_bqs : T_bks); }
   }
   if (!DynamicConfig::FRC && !DynamicConfig::antichess && !isPseudoLegal(p, ToMove(from, to, moveType))) { ///@todo FRC and other variants!
      Logging::LogIt(Logging::logError) << "Trying to read bad move, not legal. " << ToString(p) << ", move is " << str << "(" << static_cast<int>(from) << " " << static_cast<int>(to) << " " << static_cast<int>(moveType) << ")";
      return false;
   }

   return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"

float gamePhase(const Position::Material &mat, ScoreType &matScoreW, ScoreType &matScoreB) {
   const float totalMatScore = 2.f * absValueGP(P_wq) + 4.f * absValueGP(P_wr) + 4.f * absValueGP(P_wb) + 4.f * absValueGP(P_wn) +
                               16.f * absValueGP(P_wp); // cannot be static for tuning process ...
   const ScoreType matPieceScoreW = mat[Co_White][M_q] * absValueGP(P_wq) + mat[Co_White][M_r] * absValueGP(P_wr) +
                                    mat[Co_White][M_b] * absValueGP(P_wb) + mat[Co_White][M_n] * absValueGP(P_wn);
   const ScoreType matPieceScoreB = mat[Co_Black][M_q] * absValueGP(P_wq) + mat[Co_Black][M_r] * absValueGP(P_wr) +
                                    mat[Co_Black][M_b] * absValueGP(P_wb) + mat[Co_Black][M_n] * absValueGP(P_wn);
   const ScoreType matPawnScoreW = mat[Co_White][M_p] * absValueGP(P_wp);
   const ScoreType matPawnScoreB = mat[Co_Black][M_p] * absValueGP(P_wp);
   matScoreW = matPieceScoreW + matPawnScoreW;
   matScoreB = matPieceScoreB + matPawnScoreB;
   return (matScoreW + matScoreB) / totalMatScore; // based on MG values
}

#pragma GCC diagnostic pop

bool readEPDFile(const std::string &fileName, std::vector<std::string> &positions) {
   Logging::LogIt(Logging::logInfo) << "Loading EPD file : " << fileName;
   std::ifstream str(fileName);
   if (str) {
      std::string line;
      while (std::getline(str, line)) positions.push_back(line);
      return true;
   }
   else {
      Logging::LogIt(Logging::logError) << "Cannot open EPD file " << fileName;
      return false;
   }
}

#if !defined(ARDUINO) && !defined(ESP32)
std::string chess960::getDFRCXFEN(){
   const std::string fenB = positions[std::rand() % 960];
   const std::string fenW = positions[std::rand() % 960];
   const std::string fenBP = tokenize(tokenize(fenB)[0],"/")[0];
   const std::string fenWP = tokenize(tokenize(fenW)[0],"/").back();
   const std::string fenBC = tokenize(fenB)[2].substr(2,2);
   const std::string fenWC = tokenize(fenW)[2].substr(0,2);
   const std::string middle = "/pppppppp/8/8/8/8/PPPPPPPP/";
   return fenBP + middle + fenWP + " w " + fenWC + fenBC + " - 0 1";
}

const std::string chess960::positions[960] = {
    "bbqnnrkr/pppppppp/8/8/8/8/PPPPPPPP/BBQNNRKR w HFhf - 0 1", "bqnbnrkr/pppppppp/8/8/8/8/PPPPPPPP/BQNBNRKR w HFhf - 0 1",
    "bqnnrbkr/pppppppp/8/8/8/8/PPPPPPPP/BQNNRBKR w HEhe - 0 1", "bqnnrkrb/pppppppp/8/8/8/8/PPPPPPPP/BQNNRKRB w GEge - 0 1",
    "qbbnnrkr/pppppppp/8/8/8/8/PPPPPPPP/QBBNNRKR w HFhf - 0 1", "qnbbnrkr/pppppppp/8/8/8/8/PPPPPPPP/QNBBNRKR w HFhf - 0 1",
    "qnbnrbkr/pppppppp/8/8/8/8/PPPPPPPP/QNBNRBKR w HEhe - 0 1", "qnbnrkrb/pppppppp/8/8/8/8/PPPPPPPP/QNBNRKRB w GEge - 0 1",
    "qbnnbrkr/pppppppp/8/8/8/8/PPPPPPPP/QBNNBRKR w HFhf - 0 1", "qnnbbrkr/pppppppp/8/8/8/8/PPPPPPPP/QNNBBRKR w HFhf - 0 1",
    "qnnrbbkr/pppppppp/8/8/8/8/PPPPPPPP/QNNRBBKR w HDhd - 0 1", "qnnrbkrb/pppppppp/8/8/8/8/PPPPPPPP/QNNRBKRB w GDgd - 0 1",
    "qbnnrkbr/pppppppp/8/8/8/8/PPPPPPPP/QBNNRKBR w HEhe - 0 1", "qnnbrkbr/pppppppp/8/8/8/8/PPPPPPPP/QNNBRKBR w HEhe - 0 1",
    "qnnrkbbr/pppppppp/8/8/8/8/PPPPPPPP/QNNRKBBR w HDhd - 0 1", "qnnrkrbb/pppppppp/8/8/8/8/PPPPPPPP/QNNRKRBB w FDfd - 0 1",
    "bbnqnrkr/pppppppp/8/8/8/8/PPPPPPPP/BBNQNRKR w HFhf - 0 1", "bnqbnrkr/pppppppp/8/8/8/8/PPPPPPPP/BNQBNRKR w HFhf - 0 1",
    "bnqnrbkr/pppppppp/8/8/8/8/PPPPPPPP/BNQNRBKR w HEhe - 0 1", "bnqnrkrb/pppppppp/8/8/8/8/PPPPPPPP/BNQNRKRB w GEge - 0 1",
    "nbbqnrkr/pppppppp/8/8/8/8/PPPPPPPP/NBBQNRKR w HFhf - 0 1", "nqbbnrkr/pppppppp/8/8/8/8/PPPPPPPP/NQBBNRKR w HFhf - 0 1",
    "nqbnrbkr/pppppppp/8/8/8/8/PPPPPPPP/NQBNRBKR w HEhe - 0 1", "nqbnrkrb/pppppppp/8/8/8/8/PPPPPPPP/NQBNRKRB w GEge - 0 1",
    "nbqnbrkr/pppppppp/8/8/8/8/PPPPPPPP/NBQNBRKR w HFhf - 0 1", "nqnbbrkr/pppppppp/8/8/8/8/PPPPPPPP/NQNBBRKR w HFhf - 0 1",
    "nqnrbbkr/pppppppp/8/8/8/8/PPPPPPPP/NQNRBBKR w HDhd - 0 1", "nqnrbkrb/pppppppp/8/8/8/8/PPPPPPPP/NQNRBKRB w GDgd - 0 1",
    "nbqnrkbr/pppppppp/8/8/8/8/PPPPPPPP/NBQNRKBR w HEhe - 0 1", "nqnbrkbr/pppppppp/8/8/8/8/PPPPPPPP/NQNBRKBR w HEhe - 0 1",
    "nqnrkbbr/pppppppp/8/8/8/8/PPPPPPPP/NQNRKBBR w HDhd - 0 1", "nqnrkrbb/pppppppp/8/8/8/8/PPPPPPPP/NQNRKRBB w FDfd - 0 1",
    "bbnnqrkr/pppppppp/8/8/8/8/PPPPPPPP/BBNNQRKR w HFhf - 0 1", "bnnbqrkr/pppppppp/8/8/8/8/PPPPPPPP/BNNBQRKR w HFhf - 0 1",
    "bnnqrbkr/pppppppp/8/8/8/8/PPPPPPPP/BNNQRBKR w HEhe - 0 1", "bnnqrkrb/pppppppp/8/8/8/8/PPPPPPPP/BNNQRKRB w GEge - 0 1",
    "nbbnqrkr/pppppppp/8/8/8/8/PPPPPPPP/NBBNQRKR w HFhf - 0 1", "nnbbqrkr/pppppppp/8/8/8/8/PPPPPPPP/NNBBQRKR w HFhf - 0 1",
    "nnbqrbkr/pppppppp/8/8/8/8/PPPPPPPP/NNBQRBKR w HEhe - 0 1", "nnbqrkrb/pppppppp/8/8/8/8/PPPPPPPP/NNBQRKRB w GEge - 0 1",
    "nbnqbrkr/pppppppp/8/8/8/8/PPPPPPPP/NBNQBRKR w HFhf - 0 1", "nnqbbrkr/pppppppp/8/8/8/8/PPPPPPPP/NNQBBRKR w HFhf - 0 1",
    "nnqrbbkr/pppppppp/8/8/8/8/PPPPPPPP/NNQRBBKR w HDhd - 0 1", "nnqrbkrb/pppppppp/8/8/8/8/PPPPPPPP/NNQRBKRB w GDgd - 0 1",
    "nbnqrkbr/pppppppp/8/8/8/8/PPPPPPPP/NBNQRKBR w HEhe - 0 1", "nnqbrkbr/pppppppp/8/8/8/8/PPPPPPPP/NNQBRKBR w HEhe - 0 1",
    "nnqrkbbr/pppppppp/8/8/8/8/PPPPPPPP/NNQRKBBR w HDhd - 0 1", "nnqrkrbb/pppppppp/8/8/8/8/PPPPPPPP/NNQRKRBB w FDfd - 0 1",
    "bbnnrqkr/pppppppp/8/8/8/8/PPPPPPPP/BBNNRQKR w HEhe - 0 1", "bnnbrqkr/pppppppp/8/8/8/8/PPPPPPPP/BNNBRQKR w HEhe - 0 1",
    "bnnrqbkr/pppppppp/8/8/8/8/PPPPPPPP/BNNRQBKR w HDhd - 0 1", "bnnrqkrb/pppppppp/8/8/8/8/PPPPPPPP/BNNRQKRB w GDgd - 0 1",
    "nbbnrqkr/pppppppp/8/8/8/8/PPPPPPPP/NBBNRQKR w HEhe - 0 1", "nnbbrqkr/pppppppp/8/8/8/8/PPPPPPPP/NNBBRQKR w HEhe - 0 1",
    "nnbrqbkr/pppppppp/8/8/8/8/PPPPPPPP/NNBRQBKR w HDhd - 0 1", "nnbrqkrb/pppppppp/8/8/8/8/PPPPPPPP/NNBRQKRB w GDgd - 0 1",
    "nbnrbqkr/pppppppp/8/8/8/8/PPPPPPPP/NBNRBQKR w HDhd - 0 1", "nnrbbqkr/pppppppp/8/8/8/8/PPPPPPPP/NNRBBQKR w HChc - 0 1",
    "nnrqbbkr/pppppppp/8/8/8/8/PPPPPPPP/NNRQBBKR w HChc - 0 1", "nnrqbkrb/pppppppp/8/8/8/8/PPPPPPPP/NNRQBKRB w GCgc - 0 1",
    "nbnrqkbr/pppppppp/8/8/8/8/PPPPPPPP/NBNRQKBR w HDhd - 0 1", "nnrbqkbr/pppppppp/8/8/8/8/PPPPPPPP/NNRBQKBR w HChc - 0 1",
    "nnrqkbbr/pppppppp/8/8/8/8/PPPPPPPP/NNRQKBBR w HChc - 0 1", "nnrqkrbb/pppppppp/8/8/8/8/PPPPPPPP/NNRQKRBB w FCfc - 0 1",
    "bbnnrkqr/pppppppp/8/8/8/8/PPPPPPPP/BBNNRKQR w HEhe - 0 1", "bnnbrkqr/pppppppp/8/8/8/8/PPPPPPPP/BNNBRKQR w HEhe - 0 1",
    "bnnrkbqr/pppppppp/8/8/8/8/PPPPPPPP/BNNRKBQR w HDhd - 0 1", "bnnrkqrb/pppppppp/8/8/8/8/PPPPPPPP/BNNRKQRB w GDgd - 0 1",
    "nbbnrkqr/pppppppp/8/8/8/8/PPPPPPPP/NBBNRKQR w HEhe - 0 1", "nnbbrkqr/pppppppp/8/8/8/8/PPPPPPPP/NNBBRKQR w HEhe - 0 1",
    "nnbrkbqr/pppppppp/8/8/8/8/PPPPPPPP/NNBRKBQR w HDhd - 0 1", "nnbrkqrb/pppppppp/8/8/8/8/PPPPPPPP/NNBRKQRB w GDgd - 0 1",
    "nbnrbkqr/pppppppp/8/8/8/8/PPPPPPPP/NBNRBKQR w HDhd - 0 1", "nnrbbkqr/pppppppp/8/8/8/8/PPPPPPPP/NNRBBKQR w HChc - 0 1",
    "nnrkbbqr/pppppppp/8/8/8/8/PPPPPPPP/NNRKBBQR w HChc - 0 1", "nnrkbqrb/pppppppp/8/8/8/8/PPPPPPPP/NNRKBQRB w GCgc - 0 1",
    "nbnrkqbr/pppppppp/8/8/8/8/PPPPPPPP/NBNRKQBR w HDhd - 0 1", "nnrbkqbr/pppppppp/8/8/8/8/PPPPPPPP/NNRBKQBR w HChc - 0 1",
    "nnrkqbbr/pppppppp/8/8/8/8/PPPPPPPP/NNRKQBBR w HChc - 0 1", "nnrkqrbb/pppppppp/8/8/8/8/PPPPPPPP/NNRKQRBB w FCfc - 0 1",
    "bbnnrkrq/pppppppp/8/8/8/8/PPPPPPPP/BBNNRKRQ w GEge - 0 1", "bnnbrkrq/pppppppp/8/8/8/8/PPPPPPPP/BNNBRKRQ w GEge - 0 1",
    "bnnrkbrq/pppppppp/8/8/8/8/PPPPPPPP/BNNRKBRQ w GDgd - 0 1", "bnnrkrqb/pppppppp/8/8/8/8/PPPPPPPP/BNNRKRQB w FDfd - 0 1",
    "nbbnrkrq/pppppppp/8/8/8/8/PPPPPPPP/NBBNRKRQ w GEge - 0 1", "nnbbrkrq/pppppppp/8/8/8/8/PPPPPPPP/NNBBRKRQ w GEge - 0 1",
    "nnbrkbrq/pppppppp/8/8/8/8/PPPPPPPP/NNBRKBRQ w GDgd - 0 1", "nnbrkrqb/pppppppp/8/8/8/8/PPPPPPPP/NNBRKRQB w FDfd - 0 1",
    "nbnrbkrq/pppppppp/8/8/8/8/PPPPPPPP/NBNRBKRQ w GDgd - 0 1", "nnrbbkrq/pppppppp/8/8/8/8/PPPPPPPP/NNRBBKRQ w GCgc - 0 1",
    "nnrkbbrq/pppppppp/8/8/8/8/PPPPPPPP/NNRKBBRQ w GCgc - 0 1", "nnrkbrqb/pppppppp/8/8/8/8/PPPPPPPP/NNRKBRQB w FCfc - 0 1",
    "nbnrkrbq/pppppppp/8/8/8/8/PPPPPPPP/NBNRKRBQ w FDfd - 0 1", "nnrbkrbq/pppppppp/8/8/8/8/PPPPPPPP/NNRBKRBQ w FCfc - 0 1",
    "nnrkrbbq/pppppppp/8/8/8/8/PPPPPPPP/NNRKRBBQ w ECec - 0 1", "nnrkrqbb/pppppppp/8/8/8/8/PPPPPPPP/NNRKRQBB w ECec - 0 1",
    "bbqnrnkr/pppppppp/8/8/8/8/PPPPPPPP/BBQNRNKR w HEhe - 0 1", "bqnbrnkr/pppppppp/8/8/8/8/PPPPPPPP/BQNBRNKR w HEhe - 0 1",
    "bqnrnbkr/pppppppp/8/8/8/8/PPPPPPPP/BQNRNBKR w HDhd - 0 1", "bqnrnkrb/pppppppp/8/8/8/8/PPPPPPPP/BQNRNKRB w GDgd - 0 1",
    "qbbnrnkr/pppppppp/8/8/8/8/PPPPPPPP/QBBNRNKR w HEhe - 0 1", "qnbbrnkr/pppppppp/8/8/8/8/PPPPPPPP/QNBBRNKR w HEhe - 0 1",
    "qnbrnbkr/pppppppp/8/8/8/8/PPPPPPPP/QNBRNBKR w HDhd - 0 1", "qnbrnkrb/pppppppp/8/8/8/8/PPPPPPPP/QNBRNKRB w GDgd - 0 1",
    "qbnrbnkr/pppppppp/8/8/8/8/PPPPPPPP/QBNRBNKR w HDhd - 0 1", "qnrbbnkr/pppppppp/8/8/8/8/PPPPPPPP/QNRBBNKR w HChc - 0 1",
    "qnrnbbkr/pppppppp/8/8/8/8/PPPPPPPP/QNRNBBKR w HChc - 0 1", "qnrnbkrb/pppppppp/8/8/8/8/PPPPPPPP/QNRNBKRB w GCgc - 0 1",
    "qbnrnkbr/pppppppp/8/8/8/8/PPPPPPPP/QBNRNKBR w HDhd - 0 1", "qnrbnkbr/pppppppp/8/8/8/8/PPPPPPPP/QNRBNKBR w HChc - 0 1",
    "qnrnkbbr/pppppppp/8/8/8/8/PPPPPPPP/QNRNKBBR w HChc - 0 1", "qnrnkrbb/pppppppp/8/8/8/8/PPPPPPPP/QNRNKRBB w FCfc - 0 1",
    "bbnqrnkr/pppppppp/8/8/8/8/PPPPPPPP/BBNQRNKR w HEhe - 0 1", "bnqbrnkr/pppppppp/8/8/8/8/PPPPPPPP/BNQBRNKR w HEhe - 0 1",
    "bnqrnbkr/pppppppp/8/8/8/8/PPPPPPPP/BNQRNBKR w HDhd - 0 1", "bnqrnkrb/pppppppp/8/8/8/8/PPPPPPPP/BNQRNKRB w GDgd - 0 1",
    "nbbqrnkr/pppppppp/8/8/8/8/PPPPPPPP/NBBQRNKR w HEhe - 0 1", "nqbbrnkr/pppppppp/8/8/8/8/PPPPPPPP/NQBBRNKR w HEhe - 0 1",
    "nqbrnbkr/pppppppp/8/8/8/8/PPPPPPPP/NQBRNBKR w HDhd - 0 1", "nqbrnkrb/pppppppp/8/8/8/8/PPPPPPPP/NQBRNKRB w GDgd - 0 1",
    "nbqrbnkr/pppppppp/8/8/8/8/PPPPPPPP/NBQRBNKR w HDhd - 0 1", "nqrbbnkr/pppppppp/8/8/8/8/PPPPPPPP/NQRBBNKR w HChc - 0 1",
    "nqrnbbkr/pppppppp/8/8/8/8/PPPPPPPP/NQRNBBKR w HChc - 0 1", "nqrnbkrb/pppppppp/8/8/8/8/PPPPPPPP/NQRNBKRB w GCgc - 0 1",
    "nbqrnkbr/pppppppp/8/8/8/8/PPPPPPPP/NBQRNKBR w HDhd - 0 1", "nqrbnkbr/pppppppp/8/8/8/8/PPPPPPPP/NQRBNKBR w HChc - 0 1",
    "nqrnkbbr/pppppppp/8/8/8/8/PPPPPPPP/NQRNKBBR w HChc - 0 1", "nqrnkrbb/pppppppp/8/8/8/8/PPPPPPPP/NQRNKRBB w FCfc - 0 1",
    "bbnrqnkr/pppppppp/8/8/8/8/PPPPPPPP/BBNRQNKR w HDhd - 0 1", "bnrbqnkr/pppppppp/8/8/8/8/PPPPPPPP/BNRBQNKR w HChc - 0 1",
    "bnrqnbkr/pppppppp/8/8/8/8/PPPPPPPP/BNRQNBKR w HChc - 0 1", "bnrqnkrb/pppppppp/8/8/8/8/PPPPPPPP/BNRQNKRB w GCgc - 0 1",
    "nbbrqnkr/pppppppp/8/8/8/8/PPPPPPPP/NBBRQNKR w HDhd - 0 1", "nrbbqnkr/pppppppp/8/8/8/8/PPPPPPPP/NRBBQNKR w HBhb - 0 1",
    "nrbqnbkr/pppppppp/8/8/8/8/PPPPPPPP/NRBQNBKR w HBhb - 0 1", "nrbqnkrb/pppppppp/8/8/8/8/PPPPPPPP/NRBQNKRB w GBgb - 0 1",
    "nbrqbnkr/pppppppp/8/8/8/8/PPPPPPPP/NBRQBNKR w HChc - 0 1", "nrqbbnkr/pppppppp/8/8/8/8/PPPPPPPP/NRQBBNKR w HBhb - 0 1",
    "nrqnbbkr/pppppppp/8/8/8/8/PPPPPPPP/NRQNBBKR w HBhb - 0 1", "nrqnbkrb/pppppppp/8/8/8/8/PPPPPPPP/NRQNBKRB w GBgb - 0 1",
    "nbrqnkbr/pppppppp/8/8/8/8/PPPPPPPP/NBRQNKBR w HChc - 0 1", "nrqbnkbr/pppppppp/8/8/8/8/PPPPPPPP/NRQBNKBR w HBhb - 0 1",
    "nrqnkbbr/pppppppp/8/8/8/8/PPPPPPPP/NRQNKBBR w HBhb - 0 1", "nrqnkrbb/pppppppp/8/8/8/8/PPPPPPPP/NRQNKRBB w FBfb - 0 1",
    "bbnrnqkr/pppppppp/8/8/8/8/PPPPPPPP/BBNRNQKR w HDhd - 0 1", "bnrbnqkr/pppppppp/8/8/8/8/PPPPPPPP/BNRBNQKR w HChc - 0 1",
    "bnrnqbkr/pppppppp/8/8/8/8/PPPPPPPP/BNRNQBKR w HChc - 0 1", "bnrnqkrb/pppppppp/8/8/8/8/PPPPPPPP/BNRNQKRB w GCgc - 0 1",
    "nbbrnqkr/pppppppp/8/8/8/8/PPPPPPPP/NBBRNQKR w HDhd - 0 1", "nrbbnqkr/pppppppp/8/8/8/8/PPPPPPPP/NRBBNQKR w HBhb - 0 1",
    "nrbnqbkr/pppppppp/8/8/8/8/PPPPPPPP/NRBNQBKR w HBhb - 0 1", "nrbnqkrb/pppppppp/8/8/8/8/PPPPPPPP/NRBNQKRB w GBgb - 0 1",
    "nbrnbqkr/pppppppp/8/8/8/8/PPPPPPPP/NBRNBQKR w HChc - 0 1", "nrnbbqkr/pppppppp/8/8/8/8/PPPPPPPP/NRNBBQKR w HBhb - 0 1",
    "nrnqbbkr/pppppppp/8/8/8/8/PPPPPPPP/NRNQBBKR w HBhb - 0 1", "nrnqbkrb/pppppppp/8/8/8/8/PPPPPPPP/NRNQBKRB w GBgb - 0 1",
    "nbrnqkbr/pppppppp/8/8/8/8/PPPPPPPP/NBRNQKBR w HChc - 0 1", "nrnbqkbr/pppppppp/8/8/8/8/PPPPPPPP/NRNBQKBR w HBhb - 0 1",
    "nrnqkbbr/pppppppp/8/8/8/8/PPPPPPPP/NRNQKBBR w HBhb - 0 1", "nrnqkrbb/pppppppp/8/8/8/8/PPPPPPPP/NRNQKRBB w FBfb - 0 1",
    "bbnrnkqr/pppppppp/8/8/8/8/PPPPPPPP/BBNRNKQR w HDhd - 0 1", "bnrbnkqr/pppppppp/8/8/8/8/PPPPPPPP/BNRBNKQR w HChc - 0 1",
    "bnrnkbqr/pppppppp/8/8/8/8/PPPPPPPP/BNRNKBQR w HChc - 0 1", "bnrnkqrb/pppppppp/8/8/8/8/PPPPPPPP/BNRNKQRB w GCgc - 0 1",
    "nbbrnkqr/pppppppp/8/8/8/8/PPPPPPPP/NBBRNKQR w HDhd - 0 1", "nrbbnkqr/pppppppp/8/8/8/8/PPPPPPPP/NRBBNKQR w HBhb - 0 1",
    "nrbnkbqr/pppppppp/8/8/8/8/PPPPPPPP/NRBNKBQR w HBhb - 0 1", "nrbnkqrb/pppppppp/8/8/8/8/PPPPPPPP/NRBNKQRB w GBgb - 0 1",
    "nbrnbkqr/pppppppp/8/8/8/8/PPPPPPPP/NBRNBKQR w HChc - 0 1", "nrnbbkqr/pppppppp/8/8/8/8/PPPPPPPP/NRNBBKQR w HBhb - 0 1",
    "nrnkbbqr/pppppppp/8/8/8/8/PPPPPPPP/NRNKBBQR w HBhb - 0 1", "nrnkbqrb/pppppppp/8/8/8/8/PPPPPPPP/NRNKBQRB w GBgb - 0 1",
    "nbrnkqbr/pppppppp/8/8/8/8/PPPPPPPP/NBRNKQBR w HChc - 0 1", "nrnbkqbr/pppppppp/8/8/8/8/PPPPPPPP/NRNBKQBR w HBhb - 0 1",
    "nrnkqbbr/pppppppp/8/8/8/8/PPPPPPPP/NRNKQBBR w HBhb - 0 1", "nrnkqrbb/pppppppp/8/8/8/8/PPPPPPPP/NRNKQRBB w FBfb - 0 1",
    "bbnrnkrq/pppppppp/8/8/8/8/PPPPPPPP/BBNRNKRQ w GDgd - 0 1", "bnrbnkrq/pppppppp/8/8/8/8/PPPPPPPP/BNRBNKRQ w GCgc - 0 1",
    "bnrnkbrq/pppppppp/8/8/8/8/PPPPPPPP/BNRNKBRQ w GCgc - 0 1", "bnrnkrqb/pppppppp/8/8/8/8/PPPPPPPP/BNRNKRQB w FCfc - 0 1",
    "nbbrnkrq/pppppppp/8/8/8/8/PPPPPPPP/NBBRNKRQ w GDgd - 0 1", "nrbbnkrq/pppppppp/8/8/8/8/PPPPPPPP/NRBBNKRQ w GBgb - 0 1",
    "nrbnkbrq/pppppppp/8/8/8/8/PPPPPPPP/NRBNKBRQ w GBgb - 0 1", "nrbnkrqb/pppppppp/8/8/8/8/PPPPPPPP/NRBNKRQB w FBfb - 0 1",
    "nbrnbkrq/pppppppp/8/8/8/8/PPPPPPPP/NBRNBKRQ w GCgc - 0 1", "nrnbbkrq/pppppppp/8/8/8/8/PPPPPPPP/NRNBBKRQ w GBgb - 0 1",
    "nrnkbbrq/pppppppp/8/8/8/8/PPPPPPPP/NRNKBBRQ w GBgb - 0 1", "nrnkbrqb/pppppppp/8/8/8/8/PPPPPPPP/NRNKBRQB w FBfb - 0 1",
    "nbrnkrbq/pppppppp/8/8/8/8/PPPPPPPP/NBRNKRBQ w FCfc - 0 1", "nrnbkrbq/pppppppp/8/8/8/8/PPPPPPPP/NRNBKRBQ w FBfb - 0 1",
    "nrnkrbbq/pppppppp/8/8/8/8/PPPPPPPP/NRNKRBBQ w EBeb - 0 1", "nrnkrqbb/pppppppp/8/8/8/8/PPPPPPPP/NRNKRQBB w EBeb - 0 1",
    "bbqnrknr/pppppppp/8/8/8/8/PPPPPPPP/BBQNRKNR w HEhe - 0 1", "bqnbrknr/pppppppp/8/8/8/8/PPPPPPPP/BQNBRKNR w HEhe - 0 1",
    "bqnrkbnr/pppppppp/8/8/8/8/PPPPPPPP/BQNRKBNR w HDhd - 0 1", "bqnrknrb/pppppppp/8/8/8/8/PPPPPPPP/BQNRKNRB w GDgd - 0 1",
    "qbbnrknr/pppppppp/8/8/8/8/PPPPPPPP/QBBNRKNR w HEhe - 0 1", "qnbbrknr/pppppppp/8/8/8/8/PPPPPPPP/QNBBRKNR w HEhe - 0 1",
    "qnbrkbnr/pppppppp/8/8/8/8/PPPPPPPP/QNBRKBNR w HDhd - 0 1", "qnbrknrb/pppppppp/8/8/8/8/PPPPPPPP/QNBRKNRB w GDgd - 0 1",
    "qbnrbknr/pppppppp/8/8/8/8/PPPPPPPP/QBNRBKNR w HDhd - 0 1", "qnrbbknr/pppppppp/8/8/8/8/PPPPPPPP/QNRBBKNR w HChc - 0 1",
    "qnrkbbnr/pppppppp/8/8/8/8/PPPPPPPP/QNRKBBNR w HChc - 0 1", "qnrkbnrb/pppppppp/8/8/8/8/PPPPPPPP/QNRKBNRB w GCgc - 0 1",
    "qbnrknbr/pppppppp/8/8/8/8/PPPPPPPP/QBNRKNBR w HDhd - 0 1", "qnrbknbr/pppppppp/8/8/8/8/PPPPPPPP/QNRBKNBR w HChc - 0 1",
    "qnrknbbr/pppppppp/8/8/8/8/PPPPPPPP/QNRKNBBR w HChc - 0 1", "qnrknrbb/pppppppp/8/8/8/8/PPPPPPPP/QNRKNRBB w FCfc - 0 1",
    "bbnqrknr/pppppppp/8/8/8/8/PPPPPPPP/BBNQRKNR w HEhe - 0 1", "bnqbrknr/pppppppp/8/8/8/8/PPPPPPPP/BNQBRKNR w HEhe - 0 1",
    "bnqrkbnr/pppppppp/8/8/8/8/PPPPPPPP/BNQRKBNR w HDhd - 0 1", "bnqrknrb/pppppppp/8/8/8/8/PPPPPPPP/BNQRKNRB w GDgd - 0 1",
    "nbbqrknr/pppppppp/8/8/8/8/PPPPPPPP/NBBQRKNR w HEhe - 0 1", "nqbbrknr/pppppppp/8/8/8/8/PPPPPPPP/NQBBRKNR w HEhe - 0 1",
    "nqbrkbnr/pppppppp/8/8/8/8/PPPPPPPP/NQBRKBNR w HDhd - 0 1", "nqbrknrb/pppppppp/8/8/8/8/PPPPPPPP/NQBRKNRB w GDgd - 0 1",
    "nbqrbknr/pppppppp/8/8/8/8/PPPPPPPP/NBQRBKNR w HDhd - 0 1", "nqrbbknr/pppppppp/8/8/8/8/PPPPPPPP/NQRBBKNR w HChc - 0 1",
    "nqrkbbnr/pppppppp/8/8/8/8/PPPPPPPP/NQRKBBNR w HChc - 0 1", "nqrkbnrb/pppppppp/8/8/8/8/PPPPPPPP/NQRKBNRB w GCgc - 0 1",
    "nbqrknbr/pppppppp/8/8/8/8/PPPPPPPP/NBQRKNBR w HDhd - 0 1", "nqrbknbr/pppppppp/8/8/8/8/PPPPPPPP/NQRBKNBR w HChc - 0 1",
    "nqrknbbr/pppppppp/8/8/8/8/PPPPPPPP/NQRKNBBR w HChc - 0 1", "nqrknrbb/pppppppp/8/8/8/8/PPPPPPPP/NQRKNRBB w FCfc - 0 1",
    "bbnrqknr/pppppppp/8/8/8/8/PPPPPPPP/BBNRQKNR w HDhd - 0 1", "bnrbqknr/pppppppp/8/8/8/8/PPPPPPPP/BNRBQKNR w HChc - 0 1",
    "bnrqkbnr/pppppppp/8/8/8/8/PPPPPPPP/BNRQKBNR w HChc - 0 1", "bnrqknrb/pppppppp/8/8/8/8/PPPPPPPP/BNRQKNRB w GCgc - 0 1",
    "nbbrqknr/pppppppp/8/8/8/8/PPPPPPPP/NBBRQKNR w HDhd - 0 1", "nrbbqknr/pppppppp/8/8/8/8/PPPPPPPP/NRBBQKNR w HBhb - 0 1",
    "nrbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/NRBQKBNR w HBhb - 0 1", "nrbqknrb/pppppppp/8/8/8/8/PPPPPPPP/NRBQKNRB w GBgb - 0 1",
    "nbrqbknr/pppppppp/8/8/8/8/PPPPPPPP/NBRQBKNR w HChc - 0 1", "nrqbbknr/pppppppp/8/8/8/8/PPPPPPPP/NRQBBKNR w HBhb - 0 1",
    "nrqkbbnr/pppppppp/8/8/8/8/PPPPPPPP/NRQKBBNR w HBhb - 0 1", "nrqkbnrb/pppppppp/8/8/8/8/PPPPPPPP/NRQKBNRB w GBgb - 0 1",
    "nbrqknbr/pppppppp/8/8/8/8/PPPPPPPP/NBRQKNBR w HChc - 0 1", "nrqbknbr/pppppppp/8/8/8/8/PPPPPPPP/NRQBKNBR w HBhb - 0 1",
    "nrqknbbr/pppppppp/8/8/8/8/PPPPPPPP/NRQKNBBR w HBhb - 0 1", "nrqknrbb/pppppppp/8/8/8/8/PPPPPPPP/NRQKNRBB w FBfb - 0 1",
    "bbnrkqnr/pppppppp/8/8/8/8/PPPPPPPP/BBNRKQNR w HDhd - 0 1", "bnrbkqnr/pppppppp/8/8/8/8/PPPPPPPP/BNRBKQNR w HChc - 0 1",
    "bnrkqbnr/pppppppp/8/8/8/8/PPPPPPPP/BNRKQBNR w HChc - 0 1", "bnrkqnrb/pppppppp/8/8/8/8/PPPPPPPP/BNRKQNRB w GCgc - 0 1",
    "nbbrkqnr/pppppppp/8/8/8/8/PPPPPPPP/NBBRKQNR w HDhd - 0 1", "nrbbkqnr/pppppppp/8/8/8/8/PPPPPPPP/NRBBKQNR w HBhb - 0 1",
    "nrbkqbnr/pppppppp/8/8/8/8/PPPPPPPP/NRBKQBNR w HBhb - 0 1", "nrbkqnrb/pppppppp/8/8/8/8/PPPPPPPP/NRBKQNRB w GBgb - 0 1",
    "nbrkbqnr/pppppppp/8/8/8/8/PPPPPPPP/NBRKBQNR w HChc - 0 1", "nrkbbqnr/pppppppp/8/8/8/8/PPPPPPPP/NRKBBQNR w HBhb - 0 1",
    "nrkqbbnr/pppppppp/8/8/8/8/PPPPPPPP/NRKQBBNR w HBhb - 0 1", "nrkqbnrb/pppppppp/8/8/8/8/PPPPPPPP/NRKQBNRB w GBgb - 0 1",
    "nbrkqnbr/pppppppp/8/8/8/8/PPPPPPPP/NBRKQNBR w HChc - 0 1", "nrkbqnbr/pppppppp/8/8/8/8/PPPPPPPP/NRKBQNBR w HBhb - 0 1",
    "nrkqnbbr/pppppppp/8/8/8/8/PPPPPPPP/NRKQNBBR w HBhb - 0 1", "nrkqnrbb/pppppppp/8/8/8/8/PPPPPPPP/NRKQNRBB w FBfb - 0 1",
    "bbnrknqr/pppppppp/8/8/8/8/PPPPPPPP/BBNRKNQR w HDhd - 0 1", "bnrbknqr/pppppppp/8/8/8/8/PPPPPPPP/BNRBKNQR w HChc - 0 1",
    "bnrknbqr/pppppppp/8/8/8/8/PPPPPPPP/BNRKNBQR w HChc - 0 1", "bnrknqrb/pppppppp/8/8/8/8/PPPPPPPP/BNRKNQRB w GCgc - 0 1",
    "nbbrknqr/pppppppp/8/8/8/8/PPPPPPPP/NBBRKNQR w HDhd - 0 1", "nrbbknqr/pppppppp/8/8/8/8/PPPPPPPP/NRBBKNQR w HBhb - 0 1",
    "nrbknbqr/pppppppp/8/8/8/8/PPPPPPPP/NRBKNBQR w HBhb - 0 1", "nrbknqrb/pppppppp/8/8/8/8/PPPPPPPP/NRBKNQRB w GBgb - 0 1",
    "nbrkbnqr/pppppppp/8/8/8/8/PPPPPPPP/NBRKBNQR w HChc - 0 1", "nrkbbnqr/pppppppp/8/8/8/8/PPPPPPPP/NRKBBNQR w HBhb - 0 1",
    "nrknbbqr/pppppppp/8/8/8/8/PPPPPPPP/NRKNBBQR w HBhb - 0 1", "nrknbqrb/pppppppp/8/8/8/8/PPPPPPPP/NRKNBQRB w GBgb - 0 1",
    "nbrknqbr/pppppppp/8/8/8/8/PPPPPPPP/NBRKNQBR w HChc - 0 1", "nrkbnqbr/pppppppp/8/8/8/8/PPPPPPPP/NRKBNQBR w HBhb - 0 1",
    "nrknqbbr/pppppppp/8/8/8/8/PPPPPPPP/NRKNQBBR w HBhb - 0 1", "nrknqrbb/pppppppp/8/8/8/8/PPPPPPPP/NRKNQRBB w FBfb - 0 1",
    "bbnrknrq/pppppppp/8/8/8/8/PPPPPPPP/BBNRKNRQ w GDgd - 0 1", "bnrbknrq/pppppppp/8/8/8/8/PPPPPPPP/BNRBKNRQ w GCgc - 0 1",
    "bnrknbrq/pppppppp/8/8/8/8/PPPPPPPP/BNRKNBRQ w GCgc - 0 1", "bnrknrqb/pppppppp/8/8/8/8/PPPPPPPP/BNRKNRQB w FCfc - 0 1",
    "nbbrknrq/pppppppp/8/8/8/8/PPPPPPPP/NBBRKNRQ w GDgd - 0 1", "nrbbknrq/pppppppp/8/8/8/8/PPPPPPPP/NRBBKNRQ w GBgb - 0 1",
    "nrbknbrq/pppppppp/8/8/8/8/PPPPPPPP/NRBKNBRQ w GBgb - 0 1", "nrbknrqb/pppppppp/8/8/8/8/PPPPPPPP/NRBKNRQB w FBfb - 0 1",
    "nbrkbnrq/pppppppp/8/8/8/8/PPPPPPPP/NBRKBNRQ w GCgc - 0 1", "nrkbbnrq/pppppppp/8/8/8/8/PPPPPPPP/NRKBBNRQ w GBgb - 0 1",
    "nrknbbrq/pppppppp/8/8/8/8/PPPPPPPP/NRKNBBRQ w GBgb - 0 1", "nrknbrqb/pppppppp/8/8/8/8/PPPPPPPP/NRKNBRQB w FBfb - 0 1",
    "nbrknrbq/pppppppp/8/8/8/8/PPPPPPPP/NBRKNRBQ w FCfc - 0 1", "nrkbnrbq/pppppppp/8/8/8/8/PPPPPPPP/NRKBNRBQ w FBfb - 0 1",
    "nrknrbbq/pppppppp/8/8/8/8/PPPPPPPP/NRKNRBBQ w EBeb - 0 1", "nrknrqbb/pppppppp/8/8/8/8/PPPPPPPP/NRKNRQBB w EBeb - 0 1",
    "bbqnrkrn/pppppppp/8/8/8/8/PPPPPPPP/BBQNRKRN w GEge - 0 1", "bqnbrkrn/pppppppp/8/8/8/8/PPPPPPPP/BQNBRKRN w GEge - 0 1",
    "bqnrkbrn/pppppppp/8/8/8/8/PPPPPPPP/BQNRKBRN w GDgd - 0 1", "bqnrkrnb/pppppppp/8/8/8/8/PPPPPPPP/BQNRKRNB w FDfd - 0 1",
    "qbbnrkrn/pppppppp/8/8/8/8/PPPPPPPP/QBBNRKRN w GEge - 0 1", "qnbbrkrn/pppppppp/8/8/8/8/PPPPPPPP/QNBBRKRN w GEge - 0 1",
    "qnbrkbrn/pppppppp/8/8/8/8/PPPPPPPP/QNBRKBRN w GDgd - 0 1", "qnbrkrnb/pppppppp/8/8/8/8/PPPPPPPP/QNBRKRNB w FDfd - 0 1",
    "qbnrbkrn/pppppppp/8/8/8/8/PPPPPPPP/QBNRBKRN w GDgd - 0 1", "qnrbbkrn/pppppppp/8/8/8/8/PPPPPPPP/QNRBBKRN w GCgc - 0 1",
    "qnrkbbrn/pppppppp/8/8/8/8/PPPPPPPP/QNRKBBRN w GCgc - 0 1", "qnrkbrnb/pppppppp/8/8/8/8/PPPPPPPP/QNRKBRNB w FCfc - 0 1",
    "qbnrkrbn/pppppppp/8/8/8/8/PPPPPPPP/QBNRKRBN w FDfd - 0 1", "qnrbkrbn/pppppppp/8/8/8/8/PPPPPPPP/QNRBKRBN w FCfc - 0 1",
    "qnrkrbbn/pppppppp/8/8/8/8/PPPPPPPP/QNRKRBBN w ECec - 0 1", "qnrkrnbb/pppppppp/8/8/8/8/PPPPPPPP/QNRKRNBB w ECec - 0 1",
    "bbnqrkrn/pppppppp/8/8/8/8/PPPPPPPP/BBNQRKRN w GEge - 0 1", "bnqbrkrn/pppppppp/8/8/8/8/PPPPPPPP/BNQBRKRN w GEge - 0 1",
    "bnqrkbrn/pppppppp/8/8/8/8/PPPPPPPP/BNQRKBRN w GDgd - 0 1", "bnqrkrnb/pppppppp/8/8/8/8/PPPPPPPP/BNQRKRNB w FDfd - 0 1",
    "nbbqrkrn/pppppppp/8/8/8/8/PPPPPPPP/NBBQRKRN w GEge - 0 1", "nqbbrkrn/pppppppp/8/8/8/8/PPPPPPPP/NQBBRKRN w GEge - 0 1",
    "nqbrkbrn/pppppppp/8/8/8/8/PPPPPPPP/NQBRKBRN w GDgd - 0 1", "nqbrkrnb/pppppppp/8/8/8/8/PPPPPPPP/NQBRKRNB w FDfd - 0 1",
    "nbqrbkrn/pppppppp/8/8/8/8/PPPPPPPP/NBQRBKRN w GDgd - 0 1", "nqrbbkrn/pppppppp/8/8/8/8/PPPPPPPP/NQRBBKRN w GCgc - 0 1",
    "nqrkbbrn/pppppppp/8/8/8/8/PPPPPPPP/NQRKBBRN w GCgc - 0 1", "nqrkbrnb/pppppppp/8/8/8/8/PPPPPPPP/NQRKBRNB w FCfc - 0 1",
    "nbqrkrbn/pppppppp/8/8/8/8/PPPPPPPP/NBQRKRBN w FDfd - 0 1", "nqrbkrbn/pppppppp/8/8/8/8/PPPPPPPP/NQRBKRBN w FCfc - 0 1",
    "nqrkrbbn/pppppppp/8/8/8/8/PPPPPPPP/NQRKRBBN w ECec - 0 1", "nqrkrnbb/pppppppp/8/8/8/8/PPPPPPPP/NQRKRNBB w ECec - 0 1",
    "bbnrqkrn/pppppppp/8/8/8/8/PPPPPPPP/BBNRQKRN w GDgd - 0 1", "bnrbqkrn/pppppppp/8/8/8/8/PPPPPPPP/BNRBQKRN w GCgc - 0 1",
    "bnrqkbrn/pppppppp/8/8/8/8/PPPPPPPP/BNRQKBRN w GCgc - 0 1", "bnrqkrnb/pppppppp/8/8/8/8/PPPPPPPP/BNRQKRNB w FCfc - 0 1",
    "nbbrqkrn/pppppppp/8/8/8/8/PPPPPPPP/NBBRQKRN w GDgd - 0 1", "nrbbqkrn/pppppppp/8/8/8/8/PPPPPPPP/NRBBQKRN w GBgb - 0 1",
    "nrbqkbrn/pppppppp/8/8/8/8/PPPPPPPP/NRBQKBRN w GBgb - 0 1", "nrbqkrnb/pppppppp/8/8/8/8/PPPPPPPP/NRBQKRNB w FBfb - 0 1",
    "nbrqbkrn/pppppppp/8/8/8/8/PPPPPPPP/NBRQBKRN w GCgc - 0 1", "nrqbbkrn/pppppppp/8/8/8/8/PPPPPPPP/NRQBBKRN w GBgb - 0 1",
    "nrqkbbrn/pppppppp/8/8/8/8/PPPPPPPP/NRQKBBRN w GBgb - 0 1", "nrqkbrnb/pppppppp/8/8/8/8/PPPPPPPP/NRQKBRNB w FBfb - 0 1",
    "nbrqkrbn/pppppppp/8/8/8/8/PPPPPPPP/NBRQKRBN w FCfc - 0 1", "nrqbkrbn/pppppppp/8/8/8/8/PPPPPPPP/NRQBKRBN w FBfb - 0 1",
    "nrqkrbbn/pppppppp/8/8/8/8/PPPPPPPP/NRQKRBBN w EBeb - 0 1", "nrqkrnbb/pppppppp/8/8/8/8/PPPPPPPP/NRQKRNBB w EBeb - 0 1",
    "bbnrkqrn/pppppppp/8/8/8/8/PPPPPPPP/BBNRKQRN w GDgd - 0 1", "bnrbkqrn/pppppppp/8/8/8/8/PPPPPPPP/BNRBKQRN w GCgc - 0 1",
    "bnrkqbrn/pppppppp/8/8/8/8/PPPPPPPP/BNRKQBRN w GCgc - 0 1", "bnrkqrnb/pppppppp/8/8/8/8/PPPPPPPP/BNRKQRNB w FCfc - 0 1",
    "nbbrkqrn/pppppppp/8/8/8/8/PPPPPPPP/NBBRKQRN w GDgd - 0 1", "nrbbkqrn/pppppppp/8/8/8/8/PPPPPPPP/NRBBKQRN w GBgb - 0 1",
    "nrbkqbrn/pppppppp/8/8/8/8/PPPPPPPP/NRBKQBRN w GBgb - 0 1", "nrbkqrnb/pppppppp/8/8/8/8/PPPPPPPP/NRBKQRNB w FBfb - 0 1",
    "nbrkbqrn/pppppppp/8/8/8/8/PPPPPPPP/NBRKBQRN w GCgc - 0 1", "nrkbbqrn/pppppppp/8/8/8/8/PPPPPPPP/NRKBBQRN w GBgb - 0 1",
    "nrkqbbrn/pppppppp/8/8/8/8/PPPPPPPP/NRKQBBRN w GBgb - 0 1", "nrkqbrnb/pppppppp/8/8/8/8/PPPPPPPP/NRKQBRNB w FBfb - 0 1",
    "nbrkqrbn/pppppppp/8/8/8/8/PPPPPPPP/NBRKQRBN w FCfc - 0 1", "nrkbqrbn/pppppppp/8/8/8/8/PPPPPPPP/NRKBQRBN w FBfb - 0 1",
    "nrkqrbbn/pppppppp/8/8/8/8/PPPPPPPP/NRKQRBBN w EBeb - 0 1", "nrkqrnbb/pppppppp/8/8/8/8/PPPPPPPP/NRKQRNBB w EBeb - 0 1",
    "bbnrkrqn/pppppppp/8/8/8/8/PPPPPPPP/BBNRKRQN w FDfd - 0 1", "bnrbkrqn/pppppppp/8/8/8/8/PPPPPPPP/BNRBKRQN w FCfc - 0 1",
    "bnrkrbqn/pppppppp/8/8/8/8/PPPPPPPP/BNRKRBQN w ECec - 0 1", "bnrkrqnb/pppppppp/8/8/8/8/PPPPPPPP/BNRKRQNB w ECec - 0 1",
    "nbbrkrqn/pppppppp/8/8/8/8/PPPPPPPP/NBBRKRQN w FDfd - 0 1", "nrbbkrqn/pppppppp/8/8/8/8/PPPPPPPP/NRBBKRQN w FBfb - 0 1",
    "nrbkrbqn/pppppppp/8/8/8/8/PPPPPPPP/NRBKRBQN w EBeb - 0 1", "nrbkrqnb/pppppppp/8/8/8/8/PPPPPPPP/NRBKRQNB w EBeb - 0 1",
    "nbrkbrqn/pppppppp/8/8/8/8/PPPPPPPP/NBRKBRQN w FCfc - 0 1", "nrkbbrqn/pppppppp/8/8/8/8/PPPPPPPP/NRKBBRQN w FBfb - 0 1",
    "nrkrbbqn/pppppppp/8/8/8/8/PPPPPPPP/NRKRBBQN w DBdb - 0 1", "nrkrbqnb/pppppppp/8/8/8/8/PPPPPPPP/NRKRBQNB w DBdb - 0 1",
    "nbrkrqbn/pppppppp/8/8/8/8/PPPPPPPP/NBRKRQBN w ECec - 0 1", "nrkbrqbn/pppppppp/8/8/8/8/PPPPPPPP/NRKBRQBN w EBeb - 0 1",
    "nrkrqbbn/pppppppp/8/8/8/8/PPPPPPPP/NRKRQBBN w DBdb - 0 1", "nrkrqnbb/pppppppp/8/8/8/8/PPPPPPPP/NRKRQNBB w DBdb - 0 1",
    "bbnrkrnq/pppppppp/8/8/8/8/PPPPPPPP/BBNRKRNQ w FDfd - 0 1", "bnrbkrnq/pppppppp/8/8/8/8/PPPPPPPP/BNRBKRNQ w FCfc - 0 1",
    "bnrkrbnq/pppppppp/8/8/8/8/PPPPPPPP/BNRKRBNQ w ECec - 0 1", "bnrkrnqb/pppppppp/8/8/8/8/PPPPPPPP/BNRKRNQB w ECec - 0 1",
    "nbbrkrnq/pppppppp/8/8/8/8/PPPPPPPP/NBBRKRNQ w FDfd - 0 1", "nrbbkrnq/pppppppp/8/8/8/8/PPPPPPPP/NRBBKRNQ w FBfb - 0 1",
    "nrbkrbnq/pppppppp/8/8/8/8/PPPPPPPP/NRBKRBNQ w EBeb - 0 1", "nrbkrnqb/pppppppp/8/8/8/8/PPPPPPPP/NRBKRNQB w EBeb - 0 1",
    "nbrkbrnq/pppppppp/8/8/8/8/PPPPPPPP/NBRKBRNQ w FCfc - 0 1", "nrkbbrnq/pppppppp/8/8/8/8/PPPPPPPP/NRKBBRNQ w FBfb - 0 1",
    "nrkrbbnq/pppppppp/8/8/8/8/PPPPPPPP/NRKRBBNQ w DBdb - 0 1", "nrkrbnqb/pppppppp/8/8/8/8/PPPPPPPP/NRKRBNQB w DBdb - 0 1",
    "nbrkrnbq/pppppppp/8/8/8/8/PPPPPPPP/NBRKRNBQ w ECec - 0 1", "nrkbrnbq/pppppppp/8/8/8/8/PPPPPPPP/NRKBRNBQ w EBeb - 0 1",
    "nrkrnbbq/pppppppp/8/8/8/8/PPPPPPPP/NRKRNBBQ w DBdb - 0 1", "nrkrnqbb/pppppppp/8/8/8/8/PPPPPPPP/NRKRNQBB w DBdb - 0 1",
    "bbqrnnkr/pppppppp/8/8/8/8/PPPPPPPP/BBQRNNKR w HDhd - 0 1", "bqrbnnkr/pppppppp/8/8/8/8/PPPPPPPP/BQRBNNKR w HChc - 0 1",
    "bqrnnbkr/pppppppp/8/8/8/8/PPPPPPPP/BQRNNBKR w HChc - 0 1", "bqrnnkrb/pppppppp/8/8/8/8/PPPPPPPP/BQRNNKRB w GCgc - 0 1",
    "qbbrnnkr/pppppppp/8/8/8/8/PPPPPPPP/QBBRNNKR w HDhd - 0 1", "qrbbnnkr/pppppppp/8/8/8/8/PPPPPPPP/QRBBNNKR w HBhb - 0 1",
    "qrbnnbkr/pppppppp/8/8/8/8/PPPPPPPP/QRBNNBKR w HBhb - 0 1", "qrbnnkrb/pppppppp/8/8/8/8/PPPPPPPP/QRBNNKRB w GBgb - 0 1",
    "qbrnbnkr/pppppppp/8/8/8/8/PPPPPPPP/QBRNBNKR w HChc - 0 1", "qrnbbnkr/pppppppp/8/8/8/8/PPPPPPPP/QRNBBNKR w HBhb - 0 1",
    "qrnnbbkr/pppppppp/8/8/8/8/PPPPPPPP/QRNNBBKR w HBhb - 0 1", "qrnnbkrb/pppppppp/8/8/8/8/PPPPPPPP/QRNNBKRB w GBgb - 0 1",
    "qbrnnkbr/pppppppp/8/8/8/8/PPPPPPPP/QBRNNKBR w HChc - 0 1", "qrnbnkbr/pppppppp/8/8/8/8/PPPPPPPP/QRNBNKBR w HBhb - 0 1",
    "qrnnkbbr/pppppppp/8/8/8/8/PPPPPPPP/QRNNKBBR w HBhb - 0 1", "qrnnkrbb/pppppppp/8/8/8/8/PPPPPPPP/QRNNKRBB w FBfb - 0 1",
    "bbrqnnkr/pppppppp/8/8/8/8/PPPPPPPP/BBRQNNKR w HChc - 0 1", "brqbnnkr/pppppppp/8/8/8/8/PPPPPPPP/BRQBNNKR w HBhb - 0 1",
    "brqnnbkr/pppppppp/8/8/8/8/PPPPPPPP/BRQNNBKR w HBhb - 0 1", "brqnnkrb/pppppppp/8/8/8/8/PPPPPPPP/BRQNNKRB w GBgb - 0 1",
    "rbbqnnkr/pppppppp/8/8/8/8/PPPPPPPP/RBBQNNKR w HAha - 0 1", "rqbbnnkr/pppppppp/8/8/8/8/PPPPPPPP/RQBBNNKR w HAha - 0 1",
    "rqbnnbkr/pppppppp/8/8/8/8/PPPPPPPP/RQBNNBKR w HAha - 0 1", "rqbnnkrb/pppppppp/8/8/8/8/PPPPPPPP/RQBNNKRB w GAga - 0 1",
    "rbqnbnkr/pppppppp/8/8/8/8/PPPPPPPP/RBQNBNKR w HAha - 0 1", "rqnbbnkr/pppppppp/8/8/8/8/PPPPPPPP/RQNBBNKR w HAha - 0 1",
    "rqnnbbkr/pppppppp/8/8/8/8/PPPPPPPP/RQNNBBKR w HAha - 0 1", "rqnnbkrb/pppppppp/8/8/8/8/PPPPPPPP/RQNNBKRB w GAga - 0 1",
    "rbqnnkbr/pppppppp/8/8/8/8/PPPPPPPP/RBQNNKBR w HAha - 0 1", "rqnbnkbr/pppppppp/8/8/8/8/PPPPPPPP/RQNBNKBR w HAha - 0 1",
    "rqnnkbbr/pppppppp/8/8/8/8/PPPPPPPP/RQNNKBBR w HAha - 0 1", "rqnnkrbb/pppppppp/8/8/8/8/PPPPPPPP/RQNNKRBB w FAfa - 0 1",
    "bbrnqnkr/pppppppp/8/8/8/8/PPPPPPPP/BBRNQNKR w HChc - 0 1", "brnbqnkr/pppppppp/8/8/8/8/PPPPPPPP/BRNBQNKR w HBhb - 0 1",
    "brnqnbkr/pppppppp/8/8/8/8/PPPPPPPP/BRNQNBKR w HBhb - 0 1", "brnqnkrb/pppppppp/8/8/8/8/PPPPPPPP/BRNQNKRB w GBgb - 0 1",
    "rbbnqnkr/pppppppp/8/8/8/8/PPPPPPPP/RBBNQNKR w HAha - 0 1", "rnbbqnkr/pppppppp/8/8/8/8/PPPPPPPP/RNBBQNKR w HAha - 0 1",
    "rnbqnbkr/pppppppp/8/8/8/8/PPPPPPPP/RNBQNBKR w HAha - 0 1", "rnbqnkrb/pppppppp/8/8/8/8/PPPPPPPP/RNBQNKRB w GAga - 0 1",
    "rbnqbnkr/pppppppp/8/8/8/8/PPPPPPPP/RBNQBNKR w HAha - 0 1", "rnqbbnkr/pppppppp/8/8/8/8/PPPPPPPP/RNQBBNKR w HAha - 0 1",
    "rnqnbbkr/pppppppp/8/8/8/8/PPPPPPPP/RNQNBBKR w HAha - 0 1", "rnqnbkrb/pppppppp/8/8/8/8/PPPPPPPP/RNQNBKRB w GAga - 0 1",
    "rbnqnkbr/pppppppp/8/8/8/8/PPPPPPPP/RBNQNKBR w HAha - 0 1", "rnqbnkbr/pppppppp/8/8/8/8/PPPPPPPP/RNQBNKBR w HAha - 0 1",
    "rnqnkbbr/pppppppp/8/8/8/8/PPPPPPPP/RNQNKBBR w HAha - 0 1", "rnqnkrbb/pppppppp/8/8/8/8/PPPPPPPP/RNQNKRBB w FAfa - 0 1",
    "bbrnnqkr/pppppppp/8/8/8/8/PPPPPPPP/BBRNNQKR w HChc - 0 1", "brnbnqkr/pppppppp/8/8/8/8/PPPPPPPP/BRNBNQKR w HBhb - 0 1",
    "brnnqbkr/pppppppp/8/8/8/8/PPPPPPPP/BRNNQBKR w HBhb - 0 1", "brnnqkrb/pppppppp/8/8/8/8/PPPPPPPP/BRNNQKRB w GBgb - 0 1",
    "rbbnnqkr/pppppppp/8/8/8/8/PPPPPPPP/RBBNNQKR w HAha - 0 1", "rnbbnqkr/pppppppp/8/8/8/8/PPPPPPPP/RNBBNQKR w HAha - 0 1",
    "rnbnqbkr/pppppppp/8/8/8/8/PPPPPPPP/RNBNQBKR w HAha - 0 1", "rnbnqkrb/pppppppp/8/8/8/8/PPPPPPPP/RNBNQKRB w GAga - 0 1",
    "rbnnbqkr/pppppppp/8/8/8/8/PPPPPPPP/RBNNBQKR w HAha - 0 1", "rnnbbqkr/pppppppp/8/8/8/8/PPPPPPPP/RNNBBQKR w HAha - 0 1",
    "rnnqbbkr/pppppppp/8/8/8/8/PPPPPPPP/RNNQBBKR w HAha - 0 1", "rnnqbkrb/pppppppp/8/8/8/8/PPPPPPPP/RNNQBKRB w GAga - 0 1",
    "rbnnqkbr/pppppppp/8/8/8/8/PPPPPPPP/RBNNQKBR w HAha - 0 1", "rnnbqkbr/pppppppp/8/8/8/8/PPPPPPPP/RNNBQKBR w HAha - 0 1",
    "rnnqkbbr/pppppppp/8/8/8/8/PPPPPPPP/RNNQKBBR w HAha - 0 1", "rnnqkrbb/pppppppp/8/8/8/8/PPPPPPPP/RNNQKRBB w FAfa - 0 1",
    "bbrnnkqr/pppppppp/8/8/8/8/PPPPPPPP/BBRNNKQR w HChc - 0 1", "brnbnkqr/pppppppp/8/8/8/8/PPPPPPPP/BRNBNKQR w HBhb - 0 1",
    "brnnkbqr/pppppppp/8/8/8/8/PPPPPPPP/BRNNKBQR w HBhb - 0 1", "brnnkqrb/pppppppp/8/8/8/8/PPPPPPPP/BRNNKQRB w GBgb - 0 1",
    "rbbnnkqr/pppppppp/8/8/8/8/PPPPPPPP/RBBNNKQR w HAha - 0 1", "rnbbnkqr/pppppppp/8/8/8/8/PPPPPPPP/RNBBNKQR w HAha - 0 1",
    "rnbnkbqr/pppppppp/8/8/8/8/PPPPPPPP/RNBNKBQR w HAha - 0 1", "rnbnkqrb/pppppppp/8/8/8/8/PPPPPPPP/RNBNKQRB w GAga - 0 1",
    "rbnnbkqr/pppppppp/8/8/8/8/PPPPPPPP/RBNNBKQR w HAha - 0 1", "rnnbbkqr/pppppppp/8/8/8/8/PPPPPPPP/RNNBBKQR w HAha - 0 1",
    "rnnkbbqr/pppppppp/8/8/8/8/PPPPPPPP/RNNKBBQR w HAha - 0 1", "rnnkbqrb/pppppppp/8/8/8/8/PPPPPPPP/RNNKBQRB w GAga - 0 1",
    "rbnnkqbr/pppppppp/8/8/8/8/PPPPPPPP/RBNNKQBR w HAha - 0 1", "rnnbkqbr/pppppppp/8/8/8/8/PPPPPPPP/RNNBKQBR w HAha - 0 1",
    "rnnkqbbr/pppppppp/8/8/8/8/PPPPPPPP/RNNKQBBR w HAha - 0 1", "rnnkqrbb/pppppppp/8/8/8/8/PPPPPPPP/RNNKQRBB w FAfa - 0 1",
    "bbrnnkrq/pppppppp/8/8/8/8/PPPPPPPP/BBRNNKRQ w GCgc - 0 1", "brnbnkrq/pppppppp/8/8/8/8/PPPPPPPP/BRNBNKRQ w GBgb - 0 1",
    "brnnkbrq/pppppppp/8/8/8/8/PPPPPPPP/BRNNKBRQ w GBgb - 0 1", "brnnkrqb/pppppppp/8/8/8/8/PPPPPPPP/BRNNKRQB w FBfb - 0 1",
    "rbbnnkrq/pppppppp/8/8/8/8/PPPPPPPP/RBBNNKRQ w GAga - 0 1", "rnbbnkrq/pppppppp/8/8/8/8/PPPPPPPP/RNBBNKRQ w GAga - 0 1",
    "rnbnkbrq/pppppppp/8/8/8/8/PPPPPPPP/RNBNKBRQ w GAga - 0 1", "rnbnkrqb/pppppppp/8/8/8/8/PPPPPPPP/RNBNKRQB w FAfa - 0 1",
    "rbnnbkrq/pppppppp/8/8/8/8/PPPPPPPP/RBNNBKRQ w GAga - 0 1", "rnnbbkrq/pppppppp/8/8/8/8/PPPPPPPP/RNNBBKRQ w GAga - 0 1",
    "rnnkbbrq/pppppppp/8/8/8/8/PPPPPPPP/RNNKBBRQ w GAga - 0 1", "rnnkbrqb/pppppppp/8/8/8/8/PPPPPPPP/RNNKBRQB w FAfa - 0 1",
    "rbnnkrbq/pppppppp/8/8/8/8/PPPPPPPP/RBNNKRBQ w FAfa - 0 1", "rnnbkrbq/pppppppp/8/8/8/8/PPPPPPPP/RNNBKRBQ w FAfa - 0 1",
    "rnnkrbbq/pppppppp/8/8/8/8/PPPPPPPP/RNNKRBBQ w EAea - 0 1", "rnnkrqbb/pppppppp/8/8/8/8/PPPPPPPP/RNNKRQBB w EAea - 0 1",
    "bbqrnknr/pppppppp/8/8/8/8/PPPPPPPP/BBQRNKNR w HDhd - 0 1", "bqrbnknr/pppppppp/8/8/8/8/PPPPPPPP/BQRBNKNR w HChc - 0 1",
    "bqrnkbnr/pppppppp/8/8/8/8/PPPPPPPP/BQRNKBNR w HChc - 0 1", "bqrnknrb/pppppppp/8/8/8/8/PPPPPPPP/BQRNKNRB w GCgc - 0 1",
    "qbbrnknr/pppppppp/8/8/8/8/PPPPPPPP/QBBRNKNR w HDhd - 0 1", "qrbbnknr/pppppppp/8/8/8/8/PPPPPPPP/QRBBNKNR w HBhb - 0 1",
    "qrbnkbnr/pppppppp/8/8/8/8/PPPPPPPP/QRBNKBNR w HBhb - 0 1", "qrbnknrb/pppppppp/8/8/8/8/PPPPPPPP/QRBNKNRB w GBgb - 0 1",
    "qbrnbknr/pppppppp/8/8/8/8/PPPPPPPP/QBRNBKNR w HChc - 0 1", "qrnbbknr/pppppppp/8/8/8/8/PPPPPPPP/QRNBBKNR w HBhb - 0 1",
    "qrnkbbnr/pppppppp/8/8/8/8/PPPPPPPP/QRNKBBNR w HBhb - 0 1", "qrnkbnrb/pppppppp/8/8/8/8/PPPPPPPP/QRNKBNRB w GBgb - 0 1",
    "qbrnknbr/pppppppp/8/8/8/8/PPPPPPPP/QBRNKNBR w HChc - 0 1", "qrnbknbr/pppppppp/8/8/8/8/PPPPPPPP/QRNBKNBR w HBhb - 0 1",
    "qrnknbbr/pppppppp/8/8/8/8/PPPPPPPP/QRNKNBBR w HBhb - 0 1", "qrnknrbb/pppppppp/8/8/8/8/PPPPPPPP/QRNKNRBB w FBfb - 0 1",
    "bbrqnknr/pppppppp/8/8/8/8/PPPPPPPP/BBRQNKNR w HChc - 0 1", "brqbnknr/pppppppp/8/8/8/8/PPPPPPPP/BRQBNKNR w HBhb - 0 1",
    "brqnkbnr/pppppppp/8/8/8/8/PPPPPPPP/BRQNKBNR w HBhb - 0 1", "brqnknrb/pppppppp/8/8/8/8/PPPPPPPP/BRQNKNRB w GBgb - 0 1",
    "rbbqnknr/pppppppp/8/8/8/8/PPPPPPPP/RBBQNKNR w HAha - 0 1", "rqbbnknr/pppppppp/8/8/8/8/PPPPPPPP/RQBBNKNR w HAha - 0 1",
    "rqbnkbnr/pppppppp/8/8/8/8/PPPPPPPP/RQBNKBNR w HAha - 0 1", "rqbnknrb/pppppppp/8/8/8/8/PPPPPPPP/RQBNKNRB w GAga - 0 1",
    "rbqnbknr/pppppppp/8/8/8/8/PPPPPPPP/RBQNBKNR w HAha - 0 1", "rqnbbknr/pppppppp/8/8/8/8/PPPPPPPP/RQNBBKNR w HAha - 0 1",
    "rqnkbbnr/pppppppp/8/8/8/8/PPPPPPPP/RQNKBBNR w HAha - 0 1", "rqnkbnrb/pppppppp/8/8/8/8/PPPPPPPP/RQNKBNRB w GAga - 0 1",
    "rbqnknbr/pppppppp/8/8/8/8/PPPPPPPP/RBQNKNBR w HAha - 0 1", "rqnbknbr/pppppppp/8/8/8/8/PPPPPPPP/RQNBKNBR w HAha - 0 1",
    "rqnknbbr/pppppppp/8/8/8/8/PPPPPPPP/RQNKNBBR w HAha - 0 1", "rqnknrbb/pppppppp/8/8/8/8/PPPPPPPP/RQNKNRBB w FAfa - 0 1",
    "bbrnqknr/pppppppp/8/8/8/8/PPPPPPPP/BBRNQKNR w HChc - 0 1", "brnbqknr/pppppppp/8/8/8/8/PPPPPPPP/BRNBQKNR w HBhb - 0 1",
    "brnqkbnr/pppppppp/8/8/8/8/PPPPPPPP/BRNQKBNR w HBhb - 0 1", "brnqknrb/pppppppp/8/8/8/8/PPPPPPPP/BRNQKNRB w GBgb - 0 1",
    "rbbnqknr/pppppppp/8/8/8/8/PPPPPPPP/RBBNQKNR w HAha - 0 1", "rnbbqknr/pppppppp/8/8/8/8/PPPPPPPP/RNBBQKNR w HAha - 0 1",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w HAha - 0 1", "rnbqknrb/pppppppp/8/8/8/8/PPPPPPPP/RNBQKNRB w GAga - 0 1",
    "rbnqbknr/pppppppp/8/8/8/8/PPPPPPPP/RBNQBKNR w HAha - 0 1", "rnqbbknr/pppppppp/8/8/8/8/PPPPPPPP/RNQBBKNR w HAha - 0 1",
    "rnqkbbnr/pppppppp/8/8/8/8/PPPPPPPP/RNQKBBNR w HAha - 0 1", "rnqkbnrb/pppppppp/8/8/8/8/PPPPPPPP/RNQKBNRB w GAga - 0 1",
    "rbnqknbr/pppppppp/8/8/8/8/PPPPPPPP/RBNQKNBR w HAha - 0 1", "rnqbknbr/pppppppp/8/8/8/8/PPPPPPPP/RNQBKNBR w HAha - 0 1",
    "rnqknbbr/pppppppp/8/8/8/8/PPPPPPPP/RNQKNBBR w HAha - 0 1", "rnqknrbb/pppppppp/8/8/8/8/PPPPPPPP/RNQKNRBB w FAfa - 0 1",
    "bbrnkqnr/pppppppp/8/8/8/8/PPPPPPPP/BBRNKQNR w HChc - 0 1", "brnbkqnr/pppppppp/8/8/8/8/PPPPPPPP/BRNBKQNR w HBhb - 0 1",
    "brnkqbnr/pppppppp/8/8/8/8/PPPPPPPP/BRNKQBNR w HBhb - 0 1", "brnkqnrb/pppppppp/8/8/8/8/PPPPPPPP/BRNKQNRB w GBgb - 0 1",
    "rbbnkqnr/pppppppp/8/8/8/8/PPPPPPPP/RBBNKQNR w HAha - 0 1", "rnbbkqnr/pppppppp/8/8/8/8/PPPPPPPP/RNBBKQNR w HAha - 0 1",
    "rnbkqbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBKQBNR w HAha - 0 1", "rnbkqnrb/pppppppp/8/8/8/8/PPPPPPPP/RNBKQNRB w GAga - 0 1",
    "rbnkbqnr/pppppppp/8/8/8/8/PPPPPPPP/RBNKBQNR w HAha - 0 1", "rnkbbqnr/pppppppp/8/8/8/8/PPPPPPPP/RNKBBQNR w HAha - 0 1",
    "rnkqbbnr/pppppppp/8/8/8/8/PPPPPPPP/RNKQBBNR w HAha - 0 1", "rnkqbnrb/pppppppp/8/8/8/8/PPPPPPPP/RNKQBNRB w GAga - 0 1",
    "rbnkqnbr/pppppppp/8/8/8/8/PPPPPPPP/RBNKQNBR w HAha - 0 1", "rnkbqnbr/pppppppp/8/8/8/8/PPPPPPPP/RNKBQNBR w HAha - 0 1",
    "rnkqnbbr/pppppppp/8/8/8/8/PPPPPPPP/RNKQNBBR w HAha - 0 1", "rnkqnrbb/pppppppp/8/8/8/8/PPPPPPPP/RNKQNRBB w FAfa - 0 1",
    "bbrnknqr/pppppppp/8/8/8/8/PPPPPPPP/BBRNKNQR w HChc - 0 1", "brnbknqr/pppppppp/8/8/8/8/PPPPPPPP/BRNBKNQR w HBhb - 0 1",
    "brnknbqr/pppppppp/8/8/8/8/PPPPPPPP/BRNKNBQR w HBhb - 0 1", "brnknqrb/pppppppp/8/8/8/8/PPPPPPPP/BRNKNQRB w GBgb - 0 1",
    "rbbnknqr/pppppppp/8/8/8/8/PPPPPPPP/RBBNKNQR w HAha - 0 1", "rnbbknqr/pppppppp/8/8/8/8/PPPPPPPP/RNBBKNQR w HAha - 0 1",
    "rnbknbqr/pppppppp/8/8/8/8/PPPPPPPP/RNBKNBQR w HAha - 0 1", "rnbknqrb/pppppppp/8/8/8/8/PPPPPPPP/RNBKNQRB w GAga - 0 1",
    "rbnkbnqr/pppppppp/8/8/8/8/PPPPPPPP/RBNKBNQR w HAha - 0 1", "rnkbbnqr/pppppppp/8/8/8/8/PPPPPPPP/RNKBBNQR w HAha - 0 1",
    "rnknbbqr/pppppppp/8/8/8/8/PPPPPPPP/RNKNBBQR w HAha - 0 1", "rnknbqrb/pppppppp/8/8/8/8/PPPPPPPP/RNKNBQRB w GAga - 0 1",
    "rbnknqbr/pppppppp/8/8/8/8/PPPPPPPP/RBNKNQBR w HAha - 0 1", "rnkbnqbr/pppppppp/8/8/8/8/PPPPPPPP/RNKBNQBR w HAha - 0 1",
    "rnknqbbr/pppppppp/8/8/8/8/PPPPPPPP/RNKNQBBR w HAha - 0 1", "rnknqrbb/pppppppp/8/8/8/8/PPPPPPPP/RNKNQRBB w FAfa - 0 1",
    "bbrnknrq/pppppppp/8/8/8/8/PPPPPPPP/BBRNKNRQ w GCgc - 0 1", "brnbknrq/pppppppp/8/8/8/8/PPPPPPPP/BRNBKNRQ w GBgb - 0 1",
    "brnknbrq/pppppppp/8/8/8/8/PPPPPPPP/BRNKNBRQ w GBgb - 0 1", "brnknrqb/pppppppp/8/8/8/8/PPPPPPPP/BRNKNRQB w FBfb - 0 1",
    "rbbnknrq/pppppppp/8/8/8/8/PPPPPPPP/RBBNKNRQ w GAga - 0 1", "rnbbknrq/pppppppp/8/8/8/8/PPPPPPPP/RNBBKNRQ w GAga - 0 1",
    "rnbknbrq/pppppppp/8/8/8/8/PPPPPPPP/RNBKNBRQ w GAga - 0 1", "rnbknrqb/pppppppp/8/8/8/8/PPPPPPPP/RNBKNRQB w FAfa - 0 1",
    "rbnkbnrq/pppppppp/8/8/8/8/PPPPPPPP/RBNKBNRQ w GAga - 0 1", "rnkbbnrq/pppppppp/8/8/8/8/PPPPPPPP/RNKBBNRQ w GAga - 0 1",
    "rnknbbrq/pppppppp/8/8/8/8/PPPPPPPP/RNKNBBRQ w GAga - 0 1", "rnknbrqb/pppppppp/8/8/8/8/PPPPPPPP/RNKNBRQB w FAfa - 0 1",
    "rbnknrbq/pppppppp/8/8/8/8/PPPPPPPP/RBNKNRBQ w FAfa - 0 1", "rnkbnrbq/pppppppp/8/8/8/8/PPPPPPPP/RNKBNRBQ w FAfa - 0 1",
    "rnknrbbq/pppppppp/8/8/8/8/PPPPPPPP/RNKNRBBQ w EAea - 0 1", "rnknrqbb/pppppppp/8/8/8/8/PPPPPPPP/RNKNRQBB w EAea - 0 1",
    "bbqrnkrn/pppppppp/8/8/8/8/PPPPPPPP/BBQRNKRN w GDgd - 0 1", "bqrbnkrn/pppppppp/8/8/8/8/PPPPPPPP/BQRBNKRN w GCgc - 0 1",
    "bqrnkbrn/pppppppp/8/8/8/8/PPPPPPPP/BQRNKBRN w GCgc - 0 1", "bqrnkrnb/pppppppp/8/8/8/8/PPPPPPPP/BQRNKRNB w FCfc - 0 1",
    "qbbrnkrn/pppppppp/8/8/8/8/PPPPPPPP/QBBRNKRN w GDgd - 0 1", "qrbbnkrn/pppppppp/8/8/8/8/PPPPPPPP/QRBBNKRN w GBgb - 0 1",
    "qrbnkbrn/pppppppp/8/8/8/8/PPPPPPPP/QRBNKBRN w GBgb - 0 1", "qrbnkrnb/pppppppp/8/8/8/8/PPPPPPPP/QRBNKRNB w FBfb - 0 1",
    "qbrnbkrn/pppppppp/8/8/8/8/PPPPPPPP/QBRNBKRN w GCgc - 0 1", "qrnbbkrn/pppppppp/8/8/8/8/PPPPPPPP/QRNBBKRN w GBgb - 0 1",
    "qrnkbbrn/pppppppp/8/8/8/8/PPPPPPPP/QRNKBBRN w GBgb - 0 1", "qrnkbrnb/pppppppp/8/8/8/8/PPPPPPPP/QRNKBRNB w FBfb - 0 1",
    "qbrnkrbn/pppppppp/8/8/8/8/PPPPPPPP/QBRNKRBN w FCfc - 0 1", "qrnbkrbn/pppppppp/8/8/8/8/PPPPPPPP/QRNBKRBN w FBfb - 0 1",
    "qrnkrbbn/pppppppp/8/8/8/8/PPPPPPPP/QRNKRBBN w EBeb - 0 1", "qrnkrnbb/pppppppp/8/8/8/8/PPPPPPPP/QRNKRNBB w EBeb - 0 1",
    "bbrqnkrn/pppppppp/8/8/8/8/PPPPPPPP/BBRQNKRN w GCgc - 0 1", "brqbnkrn/pppppppp/8/8/8/8/PPPPPPPP/BRQBNKRN w GBgb - 0 1",
    "brqnkbrn/pppppppp/8/8/8/8/PPPPPPPP/BRQNKBRN w GBgb - 0 1", "brqnkrnb/pppppppp/8/8/8/8/PPPPPPPP/BRQNKRNB w FBfb - 0 1",
    "rbbqnkrn/pppppppp/8/8/8/8/PPPPPPPP/RBBQNKRN w GAga - 0 1", "rqbbnkrn/pppppppp/8/8/8/8/PPPPPPPP/RQBBNKRN w GAga - 0 1",
    "rqbnkbrn/pppppppp/8/8/8/8/PPPPPPPP/RQBNKBRN w GAga - 0 1", "rqbnkrnb/pppppppp/8/8/8/8/PPPPPPPP/RQBNKRNB w FAfa - 0 1",
    "rbqnbkrn/pppppppp/8/8/8/8/PPPPPPPP/RBQNBKRN w GAga - 0 1", "rqnbbkrn/pppppppp/8/8/8/8/PPPPPPPP/RQNBBKRN w GAga - 0 1",
    "rqnkbbrn/pppppppp/8/8/8/8/PPPPPPPP/RQNKBBRN w GAga - 0 1", "rqnkbrnb/pppppppp/8/8/8/8/PPPPPPPP/RQNKBRNB w FAfa - 0 1",
    "rbqnkrbn/pppppppp/8/8/8/8/PPPPPPPP/RBQNKRBN w FAfa - 0 1", "rqnbkrbn/pppppppp/8/8/8/8/PPPPPPPP/RQNBKRBN w FAfa - 0 1",
    "rqnkrbbn/pppppppp/8/8/8/8/PPPPPPPP/RQNKRBBN w EAea - 0 1", "rqnkrnbb/pppppppp/8/8/8/8/PPPPPPPP/RQNKRNBB w EAea - 0 1",
    "bbrnqkrn/pppppppp/8/8/8/8/PPPPPPPP/BBRNQKRN w GCgc - 0 1", "brnbqkrn/pppppppp/8/8/8/8/PPPPPPPP/BRNBQKRN w GBgb - 0 1",
    "brnqkbrn/pppppppp/8/8/8/8/PPPPPPPP/BRNQKBRN w GBgb - 0 1", "brnqkrnb/pppppppp/8/8/8/8/PPPPPPPP/BRNQKRNB w FBfb - 0 1",
    "rbbnqkrn/pppppppp/8/8/8/8/PPPPPPPP/RBBNQKRN w GAga - 0 1", "rnbbqkrn/pppppppp/8/8/8/8/PPPPPPPP/RNBBQKRN w GAga - 0 1",
    "rnbqkbrn/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBRN w GAga - 0 1", "rnbqkrnb/pppppppp/8/8/8/8/PPPPPPPP/RNBQKRNB w FAfa - 0 1",
    "rbnqbkrn/pppppppp/8/8/8/8/PPPPPPPP/RBNQBKRN w GAga - 0 1", "rnqbbkrn/pppppppp/8/8/8/8/PPPPPPPP/RNQBBKRN w GAga - 0 1",
    "rnqkbbrn/pppppppp/8/8/8/8/PPPPPPPP/RNQKBBRN w GAga - 0 1", "rnqkbrnb/pppppppp/8/8/8/8/PPPPPPPP/RNQKBRNB w FAfa - 0 1",
    "rbnqkrbn/pppppppp/8/8/8/8/PPPPPPPP/RBNQKRBN w FAfa - 0 1", "rnqbkrbn/pppppppp/8/8/8/8/PPPPPPPP/RNQBKRBN w FAfa - 0 1",
    "rnqkrbbn/pppppppp/8/8/8/8/PPPPPPPP/RNQKRBBN w EAea - 0 1", "rnqkrnbb/pppppppp/8/8/8/8/PPPPPPPP/RNQKRNBB w EAea - 0 1",
    "bbrnkqrn/pppppppp/8/8/8/8/PPPPPPPP/BBRNKQRN w GCgc - 0 1", "brnbkqrn/pppppppp/8/8/8/8/PPPPPPPP/BRNBKQRN w GBgb - 0 1",
    "brnkqbrn/pppppppp/8/8/8/8/PPPPPPPP/BRNKQBRN w GBgb - 0 1", "brnkqrnb/pppppppp/8/8/8/8/PPPPPPPP/BRNKQRNB w FBfb - 0 1",
    "rbbnkqrn/pppppppp/8/8/8/8/PPPPPPPP/RBBNKQRN w GAga - 0 1", "rnbbkqrn/pppppppp/8/8/8/8/PPPPPPPP/RNBBKQRN w GAga - 0 1",
    "rnbkqbrn/pppppppp/8/8/8/8/PPPPPPPP/RNBKQBRN w GAga - 0 1", "rnbkqrnb/pppppppp/8/8/8/8/PPPPPPPP/RNBKQRNB w FAfa - 0 1",
    "rbnkbqrn/pppppppp/8/8/8/8/PPPPPPPP/RBNKBQRN w GAga - 0 1", "rnkbbqrn/pppppppp/8/8/8/8/PPPPPPPP/RNKBBQRN w GAga - 0 1",
    "rnkqbbrn/pppppppp/8/8/8/8/PPPPPPPP/RNKQBBRN w GAga - 0 1", "rnkqbrnb/pppppppp/8/8/8/8/PPPPPPPP/RNKQBRNB w FAfa - 0 1",
    "rbnkqrbn/pppppppp/8/8/8/8/PPPPPPPP/RBNKQRBN w FAfa - 0 1", "rnkbqrbn/pppppppp/8/8/8/8/PPPPPPPP/RNKBQRBN w FAfa - 0 1",
    "rnkqrbbn/pppppppp/8/8/8/8/PPPPPPPP/RNKQRBBN w EAea - 0 1", "rnkqrnbb/pppppppp/8/8/8/8/PPPPPPPP/RNKQRNBB w EAea - 0 1",
    "bbrnkrqn/pppppppp/8/8/8/8/PPPPPPPP/BBRNKRQN w FCfc - 0 1", "brnbkrqn/pppppppp/8/8/8/8/PPPPPPPP/BRNBKRQN w FBfb - 0 1",
    "brnkrbqn/pppppppp/8/8/8/8/PPPPPPPP/BRNKRBQN w EBeb - 0 1", "brnkrqnb/pppppppp/8/8/8/8/PPPPPPPP/BRNKRQNB w EBeb - 0 1",
    "rbbnkrqn/pppppppp/8/8/8/8/PPPPPPPP/RBBNKRQN w FAfa - 0 1", "rnbbkrqn/pppppppp/8/8/8/8/PPPPPPPP/RNBBKRQN w FAfa - 0 1",
    "rnbkrbqn/pppppppp/8/8/8/8/PPPPPPPP/RNBKRBQN w EAea - 0 1", "rnbkrqnb/pppppppp/8/8/8/8/PPPPPPPP/RNBKRQNB w EAea - 0 1",
    "rbnkbrqn/pppppppp/8/8/8/8/PPPPPPPP/RBNKBRQN w FAfa - 0 1", "rnkbbrqn/pppppppp/8/8/8/8/PPPPPPPP/RNKBBRQN w FAfa - 0 1",
    "rnkrbbqn/pppppppp/8/8/8/8/PPPPPPPP/RNKRBBQN w DAda - 0 1", "rnkrbqnb/pppppppp/8/8/8/8/PPPPPPPP/RNKRBQNB w DAda - 0 1",
    "rbnkrqbn/pppppppp/8/8/8/8/PPPPPPPP/RBNKRQBN w EAea - 0 1", "rnkbrqbn/pppppppp/8/8/8/8/PPPPPPPP/RNKBRQBN w EAea - 0 1",
    "rnkrqbbn/pppppppp/8/8/8/8/PPPPPPPP/RNKRQBBN w DAda - 0 1", "rnkrqnbb/pppppppp/8/8/8/8/PPPPPPPP/RNKRQNBB w DAda - 0 1",
    "bbrnkrnq/pppppppp/8/8/8/8/PPPPPPPP/BBRNKRNQ w FCfc - 0 1", "brnbkrnq/pppppppp/8/8/8/8/PPPPPPPP/BRNBKRNQ w FBfb - 0 1",
    "brnkrbnq/pppppppp/8/8/8/8/PPPPPPPP/BRNKRBNQ w EBeb - 0 1", "brnkrnqb/pppppppp/8/8/8/8/PPPPPPPP/BRNKRNQB w EBeb - 0 1",
    "rbbnkrnq/pppppppp/8/8/8/8/PPPPPPPP/RBBNKRNQ w FAfa - 0 1", "rnbbkrnq/pppppppp/8/8/8/8/PPPPPPPP/RNBBKRNQ w FAfa - 0 1",
    "rnbkrbnq/pppppppp/8/8/8/8/PPPPPPPP/RNBKRBNQ w EAea - 0 1", "rnbkrnqb/pppppppp/8/8/8/8/PPPPPPPP/RNBKRNQB w EAea - 0 1",
    "rbnkbrnq/pppppppp/8/8/8/8/PPPPPPPP/RBNKBRNQ w FAfa - 0 1", "rnkbbrnq/pppppppp/8/8/8/8/PPPPPPPP/RNKBBRNQ w FAfa - 0 1",
    "rnkrbbnq/pppppppp/8/8/8/8/PPPPPPPP/RNKRBBNQ w DAda - 0 1", "rnkrbnqb/pppppppp/8/8/8/8/PPPPPPPP/RNKRBNQB w DAda - 0 1",
    "rbnkrnbq/pppppppp/8/8/8/8/PPPPPPPP/RBNKRNBQ w EAea - 0 1", "rnkbrnbq/pppppppp/8/8/8/8/PPPPPPPP/RNKBRNBQ w EAea - 0 1",
    "rnkrnbbq/pppppppp/8/8/8/8/PPPPPPPP/RNKRNBBQ w DAda - 0 1", "rnkrnqbb/pppppppp/8/8/8/8/PPPPPPPP/RNKRNQBB w DAda - 0 1",
    "bbqrknnr/pppppppp/8/8/8/8/PPPPPPPP/BBQRKNNR w HDhd - 0 1", "bqrbknnr/pppppppp/8/8/8/8/PPPPPPPP/BQRBKNNR w HChc - 0 1",
    "bqrknbnr/pppppppp/8/8/8/8/PPPPPPPP/BQRKNBNR w HChc - 0 1", "bqrknnrb/pppppppp/8/8/8/8/PPPPPPPP/BQRKNNRB w GCgc - 0 1",
    "qbbrknnr/pppppppp/8/8/8/8/PPPPPPPP/QBBRKNNR w HDhd - 0 1", "qrbbknnr/pppppppp/8/8/8/8/PPPPPPPP/QRBBKNNR w HBhb - 0 1",
    "qrbknbnr/pppppppp/8/8/8/8/PPPPPPPP/QRBKNBNR w HBhb - 0 1", "qrbknnrb/pppppppp/8/8/8/8/PPPPPPPP/QRBKNNRB w GBgb - 0 1",
    "qbrkbnnr/pppppppp/8/8/8/8/PPPPPPPP/QBRKBNNR w HChc - 0 1", "qrkbbnnr/pppppppp/8/8/8/8/PPPPPPPP/QRKBBNNR w HBhb - 0 1",
    "qrknbbnr/pppppppp/8/8/8/8/PPPPPPPP/QRKNBBNR w HBhb - 0 1", "qrknbnrb/pppppppp/8/8/8/8/PPPPPPPP/QRKNBNRB w GBgb - 0 1",
    "qbrknnbr/pppppppp/8/8/8/8/PPPPPPPP/QBRKNNBR w HChc - 0 1", "qrkbnnbr/pppppppp/8/8/8/8/PPPPPPPP/QRKBNNBR w HBhb - 0 1",
    "qrknnbbr/pppppppp/8/8/8/8/PPPPPPPP/QRKNNBBR w HBhb - 0 1", "qrknnrbb/pppppppp/8/8/8/8/PPPPPPPP/QRKNNRBB w FBfb - 0 1",
    "bbrqknnr/pppppppp/8/8/8/8/PPPPPPPP/BBRQKNNR w HChc - 0 1", "brqbknnr/pppppppp/8/8/8/8/PPPPPPPP/BRQBKNNR w HBhb - 0 1",
    "brqknbnr/pppppppp/8/8/8/8/PPPPPPPP/BRQKNBNR w HBhb - 0 1", "brqknnrb/pppppppp/8/8/8/8/PPPPPPPP/BRQKNNRB w GBgb - 0 1",
    "rbbqknnr/pppppppp/8/8/8/8/PPPPPPPP/RBBQKNNR w HAha - 0 1", "rqbbknnr/pppppppp/8/8/8/8/PPPPPPPP/RQBBKNNR w HAha - 0 1",
    "rqbknbnr/pppppppp/8/8/8/8/PPPPPPPP/RQBKNBNR w HAha - 0 1", "rqbknnrb/pppppppp/8/8/8/8/PPPPPPPP/RQBKNNRB w GAga - 0 1",
    "rbqkbnnr/pppppppp/8/8/8/8/PPPPPPPP/RBQKBNNR w HAha - 0 1", "rqkbbnnr/pppppppp/8/8/8/8/PPPPPPPP/RQKBBNNR w HAha - 0 1",
    "rqknbbnr/pppppppp/8/8/8/8/PPPPPPPP/RQKNBBNR w HAha - 0 1", "rqknbnrb/pppppppp/8/8/8/8/PPPPPPPP/RQKNBNRB w GAga - 0 1",
    "rbqknnbr/pppppppp/8/8/8/8/PPPPPPPP/RBQKNNBR w HAha - 0 1", "rqkbnnbr/pppppppp/8/8/8/8/PPPPPPPP/RQKBNNBR w HAha - 0 1",
    "rqknnbbr/pppppppp/8/8/8/8/PPPPPPPP/RQKNNBBR w HAha - 0 1", "rqknnrbb/pppppppp/8/8/8/8/PPPPPPPP/RQKNNRBB w FAfa - 0 1",
    "bbrkqnnr/pppppppp/8/8/8/8/PPPPPPPP/BBRKQNNR w HChc - 0 1", "brkbqnnr/pppppppp/8/8/8/8/PPPPPPPP/BRKBQNNR w HBhb - 0 1",
    "brkqnbnr/pppppppp/8/8/8/8/PPPPPPPP/BRKQNBNR w HBhb - 0 1", "brkqnnrb/pppppppp/8/8/8/8/PPPPPPPP/BRKQNNRB w GBgb - 0 1",
    "rbbkqnnr/pppppppp/8/8/8/8/PPPPPPPP/RBBKQNNR w HAha - 0 1", "rkbbqnnr/pppppppp/8/8/8/8/PPPPPPPP/RKBBQNNR w HAha - 0 1",
    "rkbqnbnr/pppppppp/8/8/8/8/PPPPPPPP/RKBQNBNR w HAha - 0 1", "rkbqnnrb/pppppppp/8/8/8/8/PPPPPPPP/RKBQNNRB w GAga - 0 1",
    "rbkqbnnr/pppppppp/8/8/8/8/PPPPPPPP/RBKQBNNR w HAha - 0 1", "rkqbbnnr/pppppppp/8/8/8/8/PPPPPPPP/RKQBBNNR w HAha - 0 1",
    "rkqnbbnr/pppppppp/8/8/8/8/PPPPPPPP/RKQNBBNR w HAha - 0 1", "rkqnbnrb/pppppppp/8/8/8/8/PPPPPPPP/RKQNBNRB w GAga - 0 1",
    "rbkqnnbr/pppppppp/8/8/8/8/PPPPPPPP/RBKQNNBR w HAha - 0 1", "rkqbnnbr/pppppppp/8/8/8/8/PPPPPPPP/RKQBNNBR w HAha - 0 1",
    "rkqnnbbr/pppppppp/8/8/8/8/PPPPPPPP/RKQNNBBR w HAha - 0 1", "rkqnnrbb/pppppppp/8/8/8/8/PPPPPPPP/RKQNNRBB w FAfa - 0 1",
    "bbrknqnr/pppppppp/8/8/8/8/PPPPPPPP/BBRKNQNR w HChc - 0 1", "brkbnqnr/pppppppp/8/8/8/8/PPPPPPPP/BRKBNQNR w HBhb - 0 1",
    "brknqbnr/pppppppp/8/8/8/8/PPPPPPPP/BRKNQBNR w HBhb - 0 1", "brknqnrb/pppppppp/8/8/8/8/PPPPPPPP/BRKNQNRB w GBgb - 0 1",
    "rbbknqnr/pppppppp/8/8/8/8/PPPPPPPP/RBBKNQNR w HAha - 0 1", "rkbbnqnr/pppppppp/8/8/8/8/PPPPPPPP/RKBBNQNR w HAha - 0 1",
    "rkbnqbnr/pppppppp/8/8/8/8/PPPPPPPP/RKBNQBNR w HAha - 0 1", "rkbnqnrb/pppppppp/8/8/8/8/PPPPPPPP/RKBNQNRB w GAga - 0 1",
    "rbknbqnr/pppppppp/8/8/8/8/PPPPPPPP/RBKNBQNR w HAha - 0 1", "rknbbqnr/pppppppp/8/8/8/8/PPPPPPPP/RKNBBQNR w HAha - 0 1",
    "rknqbbnr/pppppppp/8/8/8/8/PPPPPPPP/RKNQBBNR w HAha - 0 1", "rknqbnrb/pppppppp/8/8/8/8/PPPPPPPP/RKNQBNRB w GAga - 0 1",
    "rbknqnbr/pppppppp/8/8/8/8/PPPPPPPP/RBKNQNBR w HAha - 0 1", "rknbqnbr/pppppppp/8/8/8/8/PPPPPPPP/RKNBQNBR w HAha - 0 1",
    "rknqnbbr/pppppppp/8/8/8/8/PPPPPPPP/RKNQNBBR w HAha - 0 1", "rknqnrbb/pppppppp/8/8/8/8/PPPPPPPP/RKNQNRBB w FAfa - 0 1",
    "bbrknnqr/pppppppp/8/8/8/8/PPPPPPPP/BBRKNNQR w HChc - 0 1", "brkbnnqr/pppppppp/8/8/8/8/PPPPPPPP/BRKBNNQR w HBhb - 0 1",
    "brknnbqr/pppppppp/8/8/8/8/PPPPPPPP/BRKNNBQR w HBhb - 0 1", "brknnqrb/pppppppp/8/8/8/8/PPPPPPPP/BRKNNQRB w GBgb - 0 1",
    "rbbknnqr/pppppppp/8/8/8/8/PPPPPPPP/RBBKNNQR w HAha - 0 1", "rkbbnnqr/pppppppp/8/8/8/8/PPPPPPPP/RKBBNNQR w HAha - 0 1",
    "rkbnnbqr/pppppppp/8/8/8/8/PPPPPPPP/RKBNNBQR w HAha - 0 1", "rkbnnqrb/pppppppp/8/8/8/8/PPPPPPPP/RKBNNQRB w GAga - 0 1",
    "rbknbnqr/pppppppp/8/8/8/8/PPPPPPPP/RBKNBNQR w HAha - 0 1", "rknbbnqr/pppppppp/8/8/8/8/PPPPPPPP/RKNBBNQR w HAha - 0 1",
    "rknnbbqr/pppppppp/8/8/8/8/PPPPPPPP/RKNNBBQR w HAha - 0 1", "rknnbqrb/pppppppp/8/8/8/8/PPPPPPPP/RKNNBQRB w GAga - 0 1",
    "rbknnqbr/pppppppp/8/8/8/8/PPPPPPPP/RBKNNQBR w HAha - 0 1", "rknbnqbr/pppppppp/8/8/8/8/PPPPPPPP/RKNBNQBR w HAha - 0 1",
    "rknnqbbr/pppppppp/8/8/8/8/PPPPPPPP/RKNNQBBR w HAha - 0 1", "rknnqrbb/pppppppp/8/8/8/8/PPPPPPPP/RKNNQRBB w FAfa - 0 1",
    "bbrknnrq/pppppppp/8/8/8/8/PPPPPPPP/BBRKNNRQ w GCgc - 0 1", "brkbnnrq/pppppppp/8/8/8/8/PPPPPPPP/BRKBNNRQ w GBgb - 0 1",
    "brknnbrq/pppppppp/8/8/8/8/PPPPPPPP/BRKNNBRQ w GBgb - 0 1", "brknnrqb/pppppppp/8/8/8/8/PPPPPPPP/BRKNNRQB w FBfb - 0 1",
    "rbbknnrq/pppppppp/8/8/8/8/PPPPPPPP/RBBKNNRQ w GAga - 0 1", "rkbbnnrq/pppppppp/8/8/8/8/PPPPPPPP/RKBBNNRQ w GAga - 0 1",
    "rkbnnbrq/pppppppp/8/8/8/8/PPPPPPPP/RKBNNBRQ w GAga - 0 1", "rkbnnrqb/pppppppp/8/8/8/8/PPPPPPPP/RKBNNRQB w FAfa - 0 1",
    "rbknbnrq/pppppppp/8/8/8/8/PPPPPPPP/RBKNBNRQ w GAga - 0 1", "rknbbnrq/pppppppp/8/8/8/8/PPPPPPPP/RKNBBNRQ w GAga - 0 1",
    "rknnbbrq/pppppppp/8/8/8/8/PPPPPPPP/RKNNBBRQ w GAga - 0 1", "rknnbrqb/pppppppp/8/8/8/8/PPPPPPPP/RKNNBRQB w FAfa - 0 1",
    "rbknnrbq/pppppppp/8/8/8/8/PPPPPPPP/RBKNNRBQ w FAfa - 0 1", "rknbnrbq/pppppppp/8/8/8/8/PPPPPPPP/RKNBNRBQ w FAfa - 0 1",
    "rknnrbbq/pppppppp/8/8/8/8/PPPPPPPP/RKNNRBBQ w EAea - 0 1", "rknnrqbb/pppppppp/8/8/8/8/PPPPPPPP/RKNNRQBB w EAea - 0 1",
    "bbqrknrn/pppppppp/8/8/8/8/PPPPPPPP/BBQRKNRN w GDgd - 0 1", "bqrbknrn/pppppppp/8/8/8/8/PPPPPPPP/BQRBKNRN w GCgc - 0 1",
    "bqrknbrn/pppppppp/8/8/8/8/PPPPPPPP/BQRKNBRN w GCgc - 0 1", "bqrknrnb/pppppppp/8/8/8/8/PPPPPPPP/BQRKNRNB w FCfc - 0 1",
    "qbbrknrn/pppppppp/8/8/8/8/PPPPPPPP/QBBRKNRN w GDgd - 0 1", "qrbbknrn/pppppppp/8/8/8/8/PPPPPPPP/QRBBKNRN w GBgb - 0 1",
    "qrbknbrn/pppppppp/8/8/8/8/PPPPPPPP/QRBKNBRN w GBgb - 0 1", "qrbknrnb/pppppppp/8/8/8/8/PPPPPPPP/QRBKNRNB w FBfb - 0 1",
    "qbrkbnrn/pppppppp/8/8/8/8/PPPPPPPP/QBRKBNRN w GCgc - 0 1", "qrkbbnrn/pppppppp/8/8/8/8/PPPPPPPP/QRKBBNRN w GBgb - 0 1",
    "qrknbbrn/pppppppp/8/8/8/8/PPPPPPPP/QRKNBBRN w GBgb - 0 1", "qrknbrnb/pppppppp/8/8/8/8/PPPPPPPP/QRKNBRNB w FBfb - 0 1",
    "qbrknrbn/pppppppp/8/8/8/8/PPPPPPPP/QBRKNRBN w FCfc - 0 1", "qrkbnrbn/pppppppp/8/8/8/8/PPPPPPPP/QRKBNRBN w FBfb - 0 1",
    "qrknrbbn/pppppppp/8/8/8/8/PPPPPPPP/QRKNRBBN w EBeb - 0 1", "qrknrnbb/pppppppp/8/8/8/8/PPPPPPPP/QRKNRNBB w EBeb - 0 1",
    "bbrqknrn/pppppppp/8/8/8/8/PPPPPPPP/BBRQKNRN w GCgc - 0 1", "brqbknrn/pppppppp/8/8/8/8/PPPPPPPP/BRQBKNRN w GBgb - 0 1",
    "brqknbrn/pppppppp/8/8/8/8/PPPPPPPP/BRQKNBRN w GBgb - 0 1", "brqknrnb/pppppppp/8/8/8/8/PPPPPPPP/BRQKNRNB w FBfb - 0 1",
    "rbbqknrn/pppppppp/8/8/8/8/PPPPPPPP/RBBQKNRN w GAga - 0 1", "rqbbknrn/pppppppp/8/8/8/8/PPPPPPPP/RQBBKNRN w GAga - 0 1",
    "rqbknbrn/pppppppp/8/8/8/8/PPPPPPPP/RQBKNBRN w GAga - 0 1", "rqbknrnb/pppppppp/8/8/8/8/PPPPPPPP/RQBKNRNB w FAfa - 0 1",
    "rbqkbnrn/pppppppp/8/8/8/8/PPPPPPPP/RBQKBNRN w GAga - 0 1", "rqkbbnrn/pppppppp/8/8/8/8/PPPPPPPP/RQKBBNRN w GAga - 0 1",
    "rqknbbrn/pppppppp/8/8/8/8/PPPPPPPP/RQKNBBRN w GAga - 0 1", "rqknbrnb/pppppppp/8/8/8/8/PPPPPPPP/RQKNBRNB w FAfa - 0 1",
    "rbqknrbn/pppppppp/8/8/8/8/PPPPPPPP/RBQKNRBN w FAfa - 0 1", "rqkbnrbn/pppppppp/8/8/8/8/PPPPPPPP/RQKBNRBN w FAfa - 0 1",
    "rqknrbbn/pppppppp/8/8/8/8/PPPPPPPP/RQKNRBBN w EAea - 0 1", "rqknrnbb/pppppppp/8/8/8/8/PPPPPPPP/RQKNRNBB w EAea - 0 1",
    "bbrkqnrn/pppppppp/8/8/8/8/PPPPPPPP/BBRKQNRN w GCgc - 0 1", "brkbqnrn/pppppppp/8/8/8/8/PPPPPPPP/BRKBQNRN w GBgb - 0 1",
    "brkqnbrn/pppppppp/8/8/8/8/PPPPPPPP/BRKQNBRN w GBgb - 0 1", "brkqnrnb/pppppppp/8/8/8/8/PPPPPPPP/BRKQNRNB w FBfb - 0 1",
    "rbbkqnrn/pppppppp/8/8/8/8/PPPPPPPP/RBBKQNRN w GAga - 0 1", "rkbbqnrn/pppppppp/8/8/8/8/PPPPPPPP/RKBBQNRN w GAga - 0 1",
    "rkbqnbrn/pppppppp/8/8/8/8/PPPPPPPP/RKBQNBRN w GAga - 0 1", "rkbqnrnb/pppppppp/8/8/8/8/PPPPPPPP/RKBQNRNB w FAfa - 0 1",
    "rbkqbnrn/pppppppp/8/8/8/8/PPPPPPPP/RBKQBNRN w GAga - 0 1", "rkqbbnrn/pppppppp/8/8/8/8/PPPPPPPP/RKQBBNRN w GAga - 0 1",
    "rkqnbbrn/pppppppp/8/8/8/8/PPPPPPPP/RKQNBBRN w GAga - 0 1", "rkqnbrnb/pppppppp/8/8/8/8/PPPPPPPP/RKQNBRNB w FAfa - 0 1",
    "rbkqnrbn/pppppppp/8/8/8/8/PPPPPPPP/RBKQNRBN w FAfa - 0 1", "rkqbnrbn/pppppppp/8/8/8/8/PPPPPPPP/RKQBNRBN w FAfa - 0 1",
    "rkqnrbbn/pppppppp/8/8/8/8/PPPPPPPP/RKQNRBBN w EAea - 0 1", "rkqnrnbb/pppppppp/8/8/8/8/PPPPPPPP/RKQNRNBB w EAea - 0 1",
    "bbrknqrn/pppppppp/8/8/8/8/PPPPPPPP/BBRKNQRN w GCgc - 0 1", "brkbnqrn/pppppppp/8/8/8/8/PPPPPPPP/BRKBNQRN w GBgb - 0 1",
    "brknqbrn/pppppppp/8/8/8/8/PPPPPPPP/BRKNQBRN w GBgb - 0 1", "brknqrnb/pppppppp/8/8/8/8/PPPPPPPP/BRKNQRNB w FBfb - 0 1",
    "rbbknqrn/pppppppp/8/8/8/8/PPPPPPPP/RBBKNQRN w GAga - 0 1", "rkbbnqrn/pppppppp/8/8/8/8/PPPPPPPP/RKBBNQRN w GAga - 0 1",
    "rkbnqbrn/pppppppp/8/8/8/8/PPPPPPPP/RKBNQBRN w GAga - 0 1", "rkbnqrnb/pppppppp/8/8/8/8/PPPPPPPP/RKBNQRNB w FAfa - 0 1",
    "rbknbqrn/pppppppp/8/8/8/8/PPPPPPPP/RBKNBQRN w GAga - 0 1", "rknbbqrn/pppppppp/8/8/8/8/PPPPPPPP/RKNBBQRN w GAga - 0 1",
    "rknqbbrn/pppppppp/8/8/8/8/PPPPPPPP/RKNQBBRN w GAga - 0 1", "rknqbrnb/pppppppp/8/8/8/8/PPPPPPPP/RKNQBRNB w FAfa - 0 1",
    "rbknqrbn/pppppppp/8/8/8/8/PPPPPPPP/RBKNQRBN w FAfa - 0 1", "rknbqrbn/pppppppp/8/8/8/8/PPPPPPPP/RKNBQRBN w FAfa - 0 1",
    "rknqrbbn/pppppppp/8/8/8/8/PPPPPPPP/RKNQRBBN w EAea - 0 1", "rknqrnbb/pppppppp/8/8/8/8/PPPPPPPP/RKNQRNBB w EAea - 0 1",
    "bbrknrqn/pppppppp/8/8/8/8/PPPPPPPP/BBRKNRQN w FCfc - 0 1", "brkbnrqn/pppppppp/8/8/8/8/PPPPPPPP/BRKBNRQN w FBfb - 0 1",
    "brknrbqn/pppppppp/8/8/8/8/PPPPPPPP/BRKNRBQN w EBeb - 0 1", "brknrqnb/pppppppp/8/8/8/8/PPPPPPPP/BRKNRQNB w EBeb - 0 1",
    "rbbknrqn/pppppppp/8/8/8/8/PPPPPPPP/RBBKNRQN w FAfa - 0 1", "rkbbnrqn/pppppppp/8/8/8/8/PPPPPPPP/RKBBNRQN w FAfa - 0 1",
    "rkbnrbqn/pppppppp/8/8/8/8/PPPPPPPP/RKBNRBQN w EAea - 0 1", "rkbnrqnb/pppppppp/8/8/8/8/PPPPPPPP/RKBNRQNB w EAea - 0 1",
    "rbknbrqn/pppppppp/8/8/8/8/PPPPPPPP/RBKNBRQN w FAfa - 0 1", "rknbbrqn/pppppppp/8/8/8/8/PPPPPPPP/RKNBBRQN w FAfa - 0 1",
    "rknrbbqn/pppppppp/8/8/8/8/PPPPPPPP/RKNRBBQN w DAda - 0 1", "rknrbqnb/pppppppp/8/8/8/8/PPPPPPPP/RKNRBQNB w DAda - 0 1",
    "rbknrqbn/pppppppp/8/8/8/8/PPPPPPPP/RBKNRQBN w EAea - 0 1", "rknbrqbn/pppppppp/8/8/8/8/PPPPPPPP/RKNBRQBN w EAea - 0 1",
    "rknrqbbn/pppppppp/8/8/8/8/PPPPPPPP/RKNRQBBN w DAda - 0 1", "rknrqnbb/pppppppp/8/8/8/8/PPPPPPPP/RKNRQNBB w DAda - 0 1",
    "bbrknrnq/pppppppp/8/8/8/8/PPPPPPPP/BBRKNRNQ w FCfc - 0 1", "brkbnrnq/pppppppp/8/8/8/8/PPPPPPPP/BRKBNRNQ w FBfb - 0 1",
    "brknrbnq/pppppppp/8/8/8/8/PPPPPPPP/BRKNRBNQ w EBeb - 0 1", "brknrnqb/pppppppp/8/8/8/8/PPPPPPPP/BRKNRNQB w EBeb - 0 1",
    "rbbknrnq/pppppppp/8/8/8/8/PPPPPPPP/RBBKNRNQ w FAfa - 0 1", "rkbbnrnq/pppppppp/8/8/8/8/PPPPPPPP/RKBBNRNQ w FAfa - 0 1",
    "rkbnrbnq/pppppppp/8/8/8/8/PPPPPPPP/RKBNRBNQ w EAea - 0 1", "rkbnrnqb/pppppppp/8/8/8/8/PPPPPPPP/RKBNRNQB w EAea - 0 1",
    "rbknbrnq/pppppppp/8/8/8/8/PPPPPPPP/RBKNBRNQ w FAfa - 0 1", "rknbbrnq/pppppppp/8/8/8/8/PPPPPPPP/RKNBBRNQ w FAfa - 0 1",
    "rknrbbnq/pppppppp/8/8/8/8/PPPPPPPP/RKNRBBNQ w DAda - 0 1", "rknrbnqb/pppppppp/8/8/8/8/PPPPPPPP/RKNRBNQB w DAda - 0 1",
    "rbknrnbq/pppppppp/8/8/8/8/PPPPPPPP/RBKNRNBQ w EAea - 0 1", "rknbrnbq/pppppppp/8/8/8/8/PPPPPPPP/RKNBRNBQ w EAea - 0 1",
    "rknrnbbq/pppppppp/8/8/8/8/PPPPPPPP/RKNRNBBQ w DAda - 0 1", "rknrnqbb/pppppppp/8/8/8/8/PPPPPPPP/RKNRNQBB w DAda - 0 1",
    "bbqrkrnn/pppppppp/8/8/8/8/PPPPPPPP/BBQRKRNN w FDfd - 0 1", "bqrbkrnn/pppppppp/8/8/8/8/PPPPPPPP/BQRBKRNN w FCfc - 0 1",
    "bqrkrbnn/pppppppp/8/8/8/8/PPPPPPPP/BQRKRBNN w ECec - 0 1", "bqrkrnnb/pppppppp/8/8/8/8/PPPPPPPP/BQRKRNNB w ECec - 0 1",
    "qbbrkrnn/pppppppp/8/8/8/8/PPPPPPPP/QBBRKRNN w FDfd - 0 1", "qrbbkrnn/pppppppp/8/8/8/8/PPPPPPPP/QRBBKRNN w FBfb - 0 1",
    "qrbkrbnn/pppppppp/8/8/8/8/PPPPPPPP/QRBKRBNN w EBeb - 0 1", "qrbkrnnb/pppppppp/8/8/8/8/PPPPPPPP/QRBKRNNB w EBeb - 0 1",
    "qbrkbrnn/pppppppp/8/8/8/8/PPPPPPPP/QBRKBRNN w FCfc - 0 1", "qrkbbrnn/pppppppp/8/8/8/8/PPPPPPPP/QRKBBRNN w FBfb - 0 1",
    "qrkrbbnn/pppppppp/8/8/8/8/PPPPPPPP/QRKRBBNN w DBdb - 0 1", "qrkrbnnb/pppppppp/8/8/8/8/PPPPPPPP/QRKRBNNB w DBdb - 0 1",
    "qbrkrnbn/pppppppp/8/8/8/8/PPPPPPPP/QBRKRNBN w ECec - 0 1", "qrkbrnbn/pppppppp/8/8/8/8/PPPPPPPP/QRKBRNBN w EBeb - 0 1",
    "qrkrnbbn/pppppppp/8/8/8/8/PPPPPPPP/QRKRNBBN w DBdb - 0 1", "qrkrnnbb/pppppppp/8/8/8/8/PPPPPPPP/QRKRNNBB w DBdb - 0 1",
    "bbrqkrnn/pppppppp/8/8/8/8/PPPPPPPP/BBRQKRNN w FCfc - 0 1", "brqbkrnn/pppppppp/8/8/8/8/PPPPPPPP/BRQBKRNN w FBfb - 0 1",
    "brqkrbnn/pppppppp/8/8/8/8/PPPPPPPP/BRQKRBNN w EBeb - 0 1", "brqkrnnb/pppppppp/8/8/8/8/PPPPPPPP/BRQKRNNB w EBeb - 0 1",
    "rbbqkrnn/pppppppp/8/8/8/8/PPPPPPPP/RBBQKRNN w FAfa - 0 1", "rqbbkrnn/pppppppp/8/8/8/8/PPPPPPPP/RQBBKRNN w FAfa - 0 1",
    "rqbkrbnn/pppppppp/8/8/8/8/PPPPPPPP/RQBKRBNN w EAea - 0 1", "rqbkrnnb/pppppppp/8/8/8/8/PPPPPPPP/RQBKRNNB w EAea - 0 1",
    "rbqkbrnn/pppppppp/8/8/8/8/PPPPPPPP/RBQKBRNN w FAfa - 0 1", "rqkbbrnn/pppppppp/8/8/8/8/PPPPPPPP/RQKBBRNN w FAfa - 0 1",
    "rqkrbbnn/pppppppp/8/8/8/8/PPPPPPPP/RQKRBBNN w DAda - 0 1", "rqkrbnnb/pppppppp/8/8/8/8/PPPPPPPP/RQKRBNNB w DAda - 0 1",
    "rbqkrnbn/pppppppp/8/8/8/8/PPPPPPPP/RBQKRNBN w EAea - 0 1", "rqkbrnbn/pppppppp/8/8/8/8/PPPPPPPP/RQKBRNBN w EAea - 0 1",
    "rqkrnbbn/pppppppp/8/8/8/8/PPPPPPPP/RQKRNBBN w DAda - 0 1", "rqkrnnbb/pppppppp/8/8/8/8/PPPPPPPP/RQKRNNBB w DAda - 0 1",
    "bbrkqrnn/pppppppp/8/8/8/8/PPPPPPPP/BBRKQRNN w FCfc - 0 1", "brkbqrnn/pppppppp/8/8/8/8/PPPPPPPP/BRKBQRNN w FBfb - 0 1",
    "brkqrbnn/pppppppp/8/8/8/8/PPPPPPPP/BRKQRBNN w EBeb - 0 1", "brkqrnnb/pppppppp/8/8/8/8/PPPPPPPP/BRKQRNNB w EBeb - 0 1",
    "rbbkqrnn/pppppppp/8/8/8/8/PPPPPPPP/RBBKQRNN w FAfa - 0 1", "rkbbqrnn/pppppppp/8/8/8/8/PPPPPPPP/RKBBQRNN w FAfa - 0 1",
    "rkbqrbnn/pppppppp/8/8/8/8/PPPPPPPP/RKBQRBNN w EAea - 0 1", "rkbqrnnb/pppppppp/8/8/8/8/PPPPPPPP/RKBQRNNB w EAea - 0 1",
    "rbkqbrnn/pppppppp/8/8/8/8/PPPPPPPP/RBKQBRNN w FAfa - 0 1", "rkqbbrnn/pppppppp/8/8/8/8/PPPPPPPP/RKQBBRNN w FAfa - 0 1",
    "rkqrbbnn/pppppppp/8/8/8/8/PPPPPPPP/RKQRBBNN w DAda - 0 1", "rkqrbnnb/pppppppp/8/8/8/8/PPPPPPPP/RKQRBNNB w DAda - 0 1",
    "rbkqrnbn/pppppppp/8/8/8/8/PPPPPPPP/RBKQRNBN w EAea - 0 1", "rkqbrnbn/pppppppp/8/8/8/8/PPPPPPPP/RKQBRNBN w EAea - 0 1",
    "rkqrnbbn/pppppppp/8/8/8/8/PPPPPPPP/RKQRNBBN w DAda - 0 1", "rkqrnnbb/pppppppp/8/8/8/8/PPPPPPPP/RKQRNNBB w DAda - 0 1",
    "bbrkrqnn/pppppppp/8/8/8/8/PPPPPPPP/BBRKRQNN w ECec - 0 1", "brkbrqnn/pppppppp/8/8/8/8/PPPPPPPP/BRKBRQNN w EBeb - 0 1",
    "brkrqbnn/pppppppp/8/8/8/8/PPPPPPPP/BRKRQBNN w DBdb - 0 1", "brkrqnnb/pppppppp/8/8/8/8/PPPPPPPP/BRKRQNNB w DBdb - 0 1",
    "rbbkrqnn/pppppppp/8/8/8/8/PPPPPPPP/RBBKRQNN w EAea - 0 1", "rkbbrqnn/pppppppp/8/8/8/8/PPPPPPPP/RKBBRQNN w EAea - 0 1",
    "rkbrqbnn/pppppppp/8/8/8/8/PPPPPPPP/RKBRQBNN w DAda - 0 1", "rkbrqnnb/pppppppp/8/8/8/8/PPPPPPPP/RKBRQNNB w DAda - 0 1",
    "rbkrbqnn/pppppppp/8/8/8/8/PPPPPPPP/RBKRBQNN w DAda - 0 1", "rkrbbqnn/pppppppp/8/8/8/8/PPPPPPPP/RKRBBQNN w CAca - 0 1",
    "rkrqbbnn/pppppppp/8/8/8/8/PPPPPPPP/RKRQBBNN w CAca - 0 1", "rkrqbnnb/pppppppp/8/8/8/8/PPPPPPPP/RKRQBNNB w CAca - 0 1",
    "rbkrqnbn/pppppppp/8/8/8/8/PPPPPPPP/RBKRQNBN w DAda - 0 1", "rkrbqnbn/pppppppp/8/8/8/8/PPPPPPPP/RKRBQNBN w CAca - 0 1",
    "rkrqnbbn/pppppppp/8/8/8/8/PPPPPPPP/RKRQNBBN w CAca - 0 1", "rkrqnnbb/pppppppp/8/8/8/8/PPPPPPPP/RKRQNNBB w CAca - 0 1",
    "bbrkrnqn/pppppppp/8/8/8/8/PPPPPPPP/BBRKRNQN w ECec - 0 1", "brkbrnqn/pppppppp/8/8/8/8/PPPPPPPP/BRKBRNQN w EBeb - 0 1",
    "brkrnbqn/pppppppp/8/8/8/8/PPPPPPPP/BRKRNBQN w DBdb - 0 1", "brkrnqnb/pppppppp/8/8/8/8/PPPPPPPP/BRKRNQNB w DBdb - 0 1",
    "rbbkrnqn/pppppppp/8/8/8/8/PPPPPPPP/RBBKRNQN w EAea - 0 1", "rkbbrnqn/pppppppp/8/8/8/8/PPPPPPPP/RKBBRNQN w EAea - 0 1",
    "rkbrnbqn/pppppppp/8/8/8/8/PPPPPPPP/RKBRNBQN w DAda - 0 1", "rkbrnqnb/pppppppp/8/8/8/8/PPPPPPPP/RKBRNQNB w DAda - 0 1",
    "rbkrbnqn/pppppppp/8/8/8/8/PPPPPPPP/RBKRBNQN w DAda - 0 1", "rkrbbnqn/pppppppp/8/8/8/8/PPPPPPPP/RKRBBNQN w CAca - 0 1",
    "rkrnbbqn/pppppppp/8/8/8/8/PPPPPPPP/RKRNBBQN w CAca - 0 1", "rkrnbqnb/pppppppp/8/8/8/8/PPPPPPPP/RKRNBQNB w CAca - 0 1",
    "rbkrnqbn/pppppppp/8/8/8/8/PPPPPPPP/RBKRNQBN w DAda - 0 1", "rkrbnqbn/pppppppp/8/8/8/8/PPPPPPPP/RKRBNQBN w CAca - 0 1",
    "rkrnqbbn/pppppppp/8/8/8/8/PPPPPPPP/RKRNQBBN w CAca - 0 1", "rkrnqnbb/pppppppp/8/8/8/8/PPPPPPPP/RKRNQNBB w CAca - 0 1",
    "bbrkrnnq/pppppppp/8/8/8/8/PPPPPPPP/BBRKRNNQ w ECec - 0 1", "brkbrnnq/pppppppp/8/8/8/8/PPPPPPPP/BRKBRNNQ w EBeb - 0 1",
    "brkrnbnq/pppppppp/8/8/8/8/PPPPPPPP/BRKRNBNQ w DBdb - 0 1", "brkrnnqb/pppppppp/8/8/8/8/PPPPPPPP/BRKRNNQB w DBdb - 0 1",
    "rbbkrnnq/pppppppp/8/8/8/8/PPPPPPPP/RBBKRNNQ w EAea - 0 1", "rkbbrnnq/pppppppp/8/8/8/8/PPPPPPPP/RKBBRNNQ w EAea - 0 1",
    "rkbrnbnq/pppppppp/8/8/8/8/PPPPPPPP/RKBRNBNQ w DAda - 0 1", "rkbrnnqb/pppppppp/8/8/8/8/PPPPPPPP/RKBRNNQB w DAda - 0 1",
    "rbkrbnnq/pppppppp/8/8/8/8/PPPPPPPP/RBKRBNNQ w DAda - 0 1", "rkrbbnnq/pppppppp/8/8/8/8/PPPPPPPP/RKRBBNNQ w CAca - 0 1",
    "rkrnbbnq/pppppppp/8/8/8/8/PPPPPPPP/RKRNBBNQ w CAca - 0 1", "rkrnbnqb/pppppppp/8/8/8/8/PPPPPPPP/RKRNBNQB w CAca - 0 1",
    "rbkrnnbq/pppppppp/8/8/8/8/PPPPPPPP/RBKRNNBQ w DAda - 0 1", "rkrbnnbq/pppppppp/8/8/8/8/PPPPPPPP/RKRBNNBQ w CAca - 0 1",
    "rkrnnbbq/pppppppp/8/8/8/8/PPPPPPPP/RKRNNBBQ w CAca - 0 1", "rkrnnqbb/pppppppp/8/8/8/8/PPPPPPPP/RKRNNQBB w CAca - 0 1"};

#endif