#include "position.hpp"

#include "bitboardTools.hpp"
#include "dynamicConfig.hpp"
#include "hash.hpp"
#include "logging.hpp"
#include "material.hpp"
#include "moveGen.hpp"
#include "tools.hpp"

template<typename T> T readFromString(const std::string& s) {
   std::stringstream ss(s);
   T tmp;
   ss >> tmp;
   return tmp;
}

bool readFEN(const std::string& fen, RootPosition& p, bool silent, bool withMoveCount) {
   if (fen.find_first_not_of(' ') == std::string::npos) {
      Logging::LogIt(Logging::logError) << "Empty fen";
      return false;
   }

   p.clear(); // "clear" position

   std::vector<std::string> strList;
   std::stringstream iss(fen);
   std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));

   if (!silent) Logging::LogIt(Logging::logInfo) << "Reading fen " << fen;

   // reset position
   p.h  = nullHash;
   p.ph = nullHash;
   for (Square k = 0; k < NbSquare; ++k) p.board(k) = P_none;

   Square j = 1;
   Square i = 0;
   while ((j <= NbSquare) && (i < (char)strList[0].length())) {
      char letter = strList[0].at(i);
      ++i;
      const Square k = static_cast<Square>((7 - (j - 1) / 8) * 8 + ((j - 1) % 8));
      switch (letter) {
         case 'p': p.board(k) = P_bp; break;
         case 'r': p.board(k) = P_br; break;
         case 'n': p.board(k) = P_bn; break;
         case 'b': p.board(k) = P_bb; break;
         case 'q': p.board(k) = P_bq; break;
         case 'k':
            p.board(k) = P_bk;
            p.king[Co_Black] = k;
            break;
         case 'P': p.board(k) = P_wp; break;
         case 'R': p.board(k) = P_wr; break;
         case 'N': p.board(k) = P_wn; break;
         case 'B': p.board(k) = P_wb; break;
         case 'Q': p.board(k) = P_wq; break;
         case 'K':
            p.board(k) = P_wk;
            p.king[Co_White] = k;
            break;
         case '/': --j; break;
         case '1': break;
         case '2': ++j; break;
         case '3': j += 2; break;
         case '4': j += 3; break;
         case '5': j += 4; break;
         case '6': j += 5; break;
         case '7': j += 6; break;
         case '8': j += 7; break;
         default: Logging::LogIt(Logging::logError) << "FEN ERROR -1 : invalid character in fen string :" << letter << "\n" << fen; return false;
      }
      ++j;
   }

   if (DynamicConfig::isKingMandatory() && (p.king[Co_White] == INVALIDSQUARE || p.king[Co_Black] == INVALIDSQUARE)) {
      Logging::LogIt(Logging::logError) << "FEN ERROR 0 : missing king";
      return false;
   }

   p.c = Co_White; // set the turn; default is white
   if (strList.size() >= 2) {
      if (strList[1] == "w") p.c = Co_White;
      else if (strList[1] == "b")
         p.c = Co_Black;
      else {
         Logging::LogIt(Logging::logError) << "FEN ERROR 1 : bad Color";
         return false;
      }
   }

   // Initialize all castle possibilities (default is none)
   p.castling = C_none;
   if (strList.size() >= 3) {
      bool found = false;
      //Logging::LogIt(Logging::logInfo) << "is not FRC";
      if (strList[2].find('K') != std::string::npos) {
         p.castling |= C_wks;
         found = true;
      }
      if (strList[2].find('Q') != std::string::npos) {
         p.castling |= C_wqs;
         found = true;
      }
      if (strList[2].find('k') != std::string::npos) {
         p.castling |= C_bks;
         found = true;
      }
      if (strList[2].find('q') != std::string::npos) {
         p.castling |= C_bqs;
         found = true;
      }

      if (!found) {
         for (const char& cr : strList[2]) {
            if ((cr >= 'A' && cr <= 'H') || (cr >= 'a' && cr <= 'h')) {
               //Logging::LogIt(Logging::logInfo) << "Found FRC like castling " << cr;
               const Color c  = std::isupper(cr) ? Co_White : Co_Black;
               const char  kf = static_cast<char>(std::toupper(FileNames[SQFILE(p.king[c])].at(0)));
               if (std::toupper(cr) > kf) { p.castling |= (c == Co_White ? C_wks : C_bks); }
               else {
                  p.castling |= (c == Co_White ? C_wqs : C_bqs);
               }
               if (found && !DynamicConfig::FRC) {
                  Logging::LogIt(Logging::logInfo) << "FRC position found (castling letters), activating FRC";
                  DynamicConfig::FRC = true; // force FRC !
               }
               found = true;
            }
         }
      }
      bool noCastling = false;
      if (strList[2].find('-') != std::string::npos) {
         found = true;
         noCastling = true;
         /*Logging::LogIt(Logging::logInfo) << "No castling right given" ;*/
      }
      if (!found) {
         if (!silent) Logging::LogIt(Logging::logWarn) << "No castling right given";
      }
      else { ///@todo detect illegal stuff in here
         bool possibleFRC = false;
         p.rootInfo().kingInit[Co_White] = p.king[Co_White];
         p.rootInfo().kingInit[Co_Black] = p.king[Co_Black];
         if (p.king[Co_White] != Sq_e1 && p.castling & C_w_all) possibleFRC = true;
         if (p.king[Co_Black] != Sq_e8 && p.castling & C_b_all) possibleFRC = true;
         if (p.castling & C_wqs) {
            for (Square s = Sq_a1; s <= Sq_h1; ++s) {
               if (s < p.king[Co_White] && p.board_const(s) == P_wr) {
                  p.rootInfo().rooksInit[Co_White][CT_OOO] = s;
                  if (s != Sq_a1) possibleFRC = true;
                  break;
               }
            }
         }
         if (p.castling & C_wks) {
            for (Square s = Sq_a1; s <= Sq_h1; ++s) {
               if (s > p.king[Co_White] && p.board_const(s) == P_wr) {
                  p.rootInfo().rooksInit[Co_White][CT_OO] = s;
                  if (s != Sq_h1) possibleFRC = true;
                  break;
               }
            }
         }
         if (p.castling & C_bqs) {
            for (Square s = Sq_a8; s <= Sq_h8; ++s) {
               if (s < p.king[Co_Black] && p.board_const(s) == P_br) {
                  p.rootInfo().rooksInit[Co_Black][CT_OOO] = s;
                  if (s != Sq_a8) possibleFRC = true;
                  break;
               }
            }
         }
         if (p.castling & C_bks) {
            for (Square s = Sq_a8; s <= Sq_h8; ++s) {
               if (s > p.king[Co_Black] && p.board_const(s) == P_br) {
                  p.rootInfo().rooksInit[Co_Black][CT_OO] = s;
                  if (s != Sq_h8) possibleFRC = true;
                  break;
               }
            }
         }
         if (possibleFRC && !noCastling && !DynamicConfig::FRC) {
            Logging::LogIt(Logging::logInfo) << "FRC position found (pieces position), activating FRC";
            DynamicConfig::FRC = true; // force FRC !
         }
      }
   }
   else if (!silent)
      Logging::LogIt(Logging::logInfo) << "No castling right given";

   /*
   std::cout << SquareNames[p.rootInfo().kingInit[Co_White]] << std::endl;
   std::cout << SquareNames[p.rootInfo().kingInit[Co_Black]] << std::endl;
   std::cout << SquareNames[p.rootInfo().rooksInit[Co_White][CT_OOO]] << std::endl;
   std::cout << SquareNames[p.rootInfo().rooksInit[Co_White][CT_OO]] << std::endl;
   std::cout << SquareNames[p.rootInfo().rooksInit[Co_Black][CT_OOO]] << std::endl;
   std::cout << SquareNames[p.rootInfo().rooksInit[Co_Black][CT_OO]] << std::endl;
   */

   // read en passant and save it (default is invalid)
   p.ep = INVALIDSQUARE;
   if ((strList.size() >= 4) && strList[3] != "-") {
      if (strList[3].length() >= 2) {
         if ((strList[3].at(0) >= 'a') && (strList[3].at(0) <= 'h') && ((strList[3].at(1) == '3') || (strList[3].at(1) == '6')))
            p.ep = stringToSquare(strList[3]);
         else {
            Logging::LogIt(Logging::logError) << "FEN ERROR 2 : bad en passant square : " << strList[3];
            return false;
         }
      }
      else {
         Logging::LogIt(Logging::logError) << "FEN ERROR 3 : bad en passant square : " << strList[3];
         return false;
      }
   }
   else if (!silent)
      Logging::LogIt(Logging::logInfo) << "No en passant square given";

   assert(p.ep == INVALIDSQUARE || (SQRANK(p.ep) == 2 || SQRANK(p.ep) == 5));

   // read 50 moves rules
   if (withMoveCount && strList.size() >= 5) p.fifty = static_cast<decltype(p.fifty)>(readFromString<int>(strList[4]));
   else
      p.fifty = 0;

   // read number of move
   if (withMoveCount && strList.size() >= 6) p.moves = static_cast<decltype(p.moves)>(readFromString<int>(strList[5]));
   else
      p.moves = 1;

   if (p.moves == 0) { // fix a LittleBlitzer bug here ...
      Logging::LogIt(Logging::logInfo) << "Wrong move counter " << static_cast<int>(p.moves) << " using 1 instead";
      p.moves = 1;
   }

   p.halfmoves = static_cast<decltype(p.halfmoves)>((p.moves - 1) * 2 + 1 + (p.c == Co_Black ? 1 : 0));

   BBTools::setBitBoards(p);
   MaterialHash::initMaterial(p);
   p.h  = computeHash(p);
   p.ph = computePHash(p);

#ifdef WITH_NNUE
   // If position is associated with an NNUE evaluator,
   // we reset the evaluator using the new position state.
   if (DynamicConfig::useNNUE && p.hasEvaluator()) p.resetNNUEEvaluator(p.evaluator());
#endif

   p.rootInfo().initCaslingPermHashTable();

   return true;
}

void RootInformation::initCaslingPermHashTable() {
   // works also for FRC !
   castlePermHashTable.fill(C_all);
   if (isValidSquare(kingInit[Co_White])){
       castlePermHashTable[kingInit[Co_White]] = C_all_but_w;
       if (isValidSquare(rooksInit[Co_White][CT_OOO])) castlePermHashTable[rooksInit[Co_White][CT_OOO]] = C_all_but_wqs;
       if (isValidSquare(rooksInit[Co_White][CT_OO] )) castlePermHashTable[rooksInit[Co_White][CT_OO]]  = C_all_but_wks;
   }
   if (isValidSquare(kingInit[Co_Black])){
       castlePermHashTable[kingInit[Co_Black]] = C_all_but_b;
       if (isValidSquare(rooksInit[Co_Black][CT_OOO])) castlePermHashTable[rooksInit[Co_Black][CT_OOO]] = C_all_but_bqs;
       if (isValidSquare(rooksInit[Co_Black][CT_OO] )) castlePermHashTable[rooksInit[Co_Black][CT_OO]]  = C_all_but_bks;
   }
}

Position::~Position() {
   //I do not delete the root pointer, only a RootPosition can do that
}

Position::Position() {}

/*
#include <immintrin.h>

Position & Position::operator=(const Position & p){
   if(&p != this){
      constexpr size_t n = 7; // => sizeof(Position) == 224b
      static_assert(sizeof(Position) == n*sizeof(__m256i));
      __m256i * me_256 = reinterpret_cast<__m256i *>(this);
      const __m256i * other_256 = reinterpret_cast<const __m256i *>(&p);
      for(size_t i = 0; i < n; ++i, ++me_256, ++other_256){
         _mm256_store_si256(me_256, _mm256_load_si256(other_256));
      }
   }
   return *this;
}

Position::Position(const Position& p) {
   if(&p != this){
      this->operator=(p);     
   }
}
*/

RootPosition::RootPosition(const std::string& fen, bool withMoveCount) : RootPosition() {
   readFEN(fen, *this, true, withMoveCount);
}

