#include "pgnparser.hpp"

#ifdef WITH_PGN_PARSER

#include "evalDef.hpp"
#include "extendedPosition.hpp"
#include "logging.hpp"
#include "moveGen.hpp"
#include "searcher.hpp"
#include "tools.hpp"

int parseResult(const std::string& result) {
   if (result == "1-0") { return 1; }
   else if (result == "0-1") {
      return -1;
   }
   else if (result == "1/2-1/2") {
      return 0;
   }
   return -2;
}

Move SANToMove(const std::string& s, const Position& p) {
   MoveList moves;
   MoveGen::generate<MoveGen::GP_all>(p, moves);
   //std::cout << ToString(p) << std::endl;
   //std::cout << moves.size() << std::endl;
   for (auto it = moves.begin(); it != moves.end(); ++it) {
      std::string san = showAlgAbr(*it, p);
      if (san == "0-0") san = "O-O";
      if (san == "0-0-0") san = "O-O-O";
      if (san == "0-0+") san = "O-O+";
      if (san == "0-0-0+") san = "O-O-O+";
      //std::cout << ToString(*it) << " : " << san << "\t";
      if (san == s) return *it;
   }
   //std::cout << std::endl;
   return INVALIDMOVE;
}

void pgnparse__(std::ifstream& is, std::ofstream& os) {
   bool           read  = true;
   int            k     = 0;
   int            a     = 0;
   int            c     = 0;
   int            e     = 0;
   ScoreType      score = -INFSCORE;
   std::set<Hash> hashes;

   while (read) {
      ++k;
      if (k % 1000 == 0) std::cout << "Parsing game " << k << std::endl;
      //std::cout << "new loop" << std::endl;
      if (is.eof()) { read = false; }
      if (!is.good()) { read = false; }

      PGNGame game;
      game.p.push_back(RootPosition(startPosition)); ///@todo allow starting from another position

      int currentStreamPos = is.tellg();

      std::string s;
      std::string result;
      std::string whiteElo;
      std::string blackElo;
      std::string kToken   = "[Result \"";
      bool        tagFound = false;
      //std::cout << "looking for result comment" << std::endl;
      for (; !tagFound;) { // looking for Result comment
         if (!std::getline(is, s)) {
            read = false;
            break;
         }
         s = trim(s);
         //std::cout << "..." << s << " " << s.size() << std::endl;
         if (s.empty()) {
            if (tagFound) break;
            else
               continue;
         } // end of header
         if (s.substr(0, kToken.size()) == kToken) {
            tagFound = true;
            score    = -INFSCORE;
            result   = s.substr(kToken.size(), s.size() - kToken.size() - 3);
            //std::cout << result << std::endl;
         }
      }
      if (!read) break;

      if (result == "*") { // unfinished game, let's skip it
         while (true) {
            is >> s;
            if (s == "*") break;
         }
         continue;
      }

      is.seekg(currentStreamPos, std::ios::beg);
      kToken   = "[WhiteElo \"";
      tagFound = false;
      //std::cout << "looking for whiteelo comment" << std::endl;
      for (; !tagFound;) { // looking for whiteElo comment
         if (!std::getline(is, s)) {
            read = false;
            break;
         }
         s = trim(s);
         //std::cout << "..." << s << " " << s.size() << std::endl;
         if (s.empty()) {
            if (tagFound) break;
            else
               continue;
         } // end of header
         if (s.substr(0, kToken.size()) == kToken) {
            tagFound = true;
            whiteElo = s.substr(kToken.size(), s.size() - kToken.size() - 3);
            //std::cout << whiteElo << std::endl;
         }
      }
      if (!read) break;

      is.seekg(currentStreamPos, std::ios::beg);
      kToken   = "[BlackElo \"";
      tagFound = false;
      //std::cout << "looking for whiteelo comment" << std::endl;
      for (; !tagFound;) { // looking for blackElo comment
         if (!std::getline(is, s)) {
            read = false;
            break;
         }
         s = trim(s);
         //std::cout << "..." << s << " " << s.size() << std::endl;
         if (s.empty()) {
            if (tagFound) break;
            else
               continue;
         } // end of header
         if (s.substr(0, kToken.size()) == kToken) {
            tagFound = true;
            blackElo = s.substr(kToken.size(), s.size() - kToken.size() - 3);
            //std::cout << blackElo << std::endl;
         }
      }
      if (!read) break;

      auto gameResult = parseResult(result);
      if (gameResult != -2) {
         game.result    = gameResult;
         game.resultStr = result;
         /*std::cout << "Result : " << result << std::endl;*/
      }
      else {
         read = false;
         Logging::LogIt(Logging::logError) << "Unknown result: " << result;
         break;
      }

      if (!whiteElo.empty()) game.whiteElo = std::stoi(whiteElo);
      if (!blackElo.empty()) game.blackElo = std::stoi(blackElo);

      // Read in the moves
      for (int i = 0; read;) {
         if (!(is >> s)) {
            read = false;
            break;
         };
         //std::cout << "reading   " << s << std::endl;
         if (s.empty()) { continue; }
         if (parseResult(s) != -2) { break; } // result is also written at the end of each game

         // skip comments
         if (s.front() == '[') {
            if (s.back() == ']') continue;
            while (true) {
               if (!(is >> s)) {
                  read = false;
                  break;
               }
               if (s.back() == ']') break;
            }
            continue;
         }

         // read annotations
         if (s.front() == '{') {
            if (s.back() == '}') continue;
            while (true) {
               if (!(is >> s)) {
                  read = false;
                  break;
               }
               else {
                  if (s.find("eval") != std::string::npos) {
                     std::string strScore;
                     is >> strScore;
                     if (strScore.find("#") == std::string::npos) {
                        score = ScoreType(100 * std::stof(strScore));
                        //std::cout << score << std::endl;
                     }
                  }
               }
               if (s.back() == '}') break;
            }
            continue;
         }

         // Skip the move numbers
         if (s.back() == '.') { continue; }

         // Drop moves annotations
         if (s.back() == '!' || s.back() == '?') {
            size_t len    = s.length();
            auto   backIt = s.end();
            --backIt;
            while (*backIt == '!' || *backIt == '?') {
               --backIt;
               --len;
            }
            s = s.substr(0, len);
         }

         if (s.back() == '#') s.back() = '+';

         //std::cout << ToString(game.p[i]) << " " << s << std::endl;
         Move m = SANToMove(s, game.p[i]);
         game.moves.push_back(ToMove(Move2From(m), Move2To(m), Move2Type(m), score));
         //std::cout << Move2Score(game.moves[i]) << std::endl;
         if (game.moves[i] == INVALIDMOVE) {
            read = false;
            Logging::LogIt(Logging::logError) << "Unable to parse pgn move: " << s;
            break;
         }
         ++i;
         ++game.n;
         game.p.push_back(game.p[i - 1]);
         if (!applyMove(game.p[i], game.moves[i - 1])) {
            read = false;
            Logging::LogIt(Logging::logError) << "Unable to apply move " + ToString(game.p[i]);
            break;
         }
      }

      if (game.whiteElo < PGNGame::minElo || game.blackElo < PGNGame::minElo) continue;

      if (a % 100 == 0) std::cout << "Analyzing game: " << a << " eval ok: " << e << " quiet ok: " << c << std::endl;
      ++a;
      EvalData  data;
      DepthType seldepth = 0;
      //std::cout << "game length " << game.n << std::endl;
      for (int i = 8; i <= game.n - 6; ++i) {
         Position  p  = game.p[i];
         ScoreType ss = Move2Score(game.moves[i]);
         //std::cout << score << std::endl;
         if (ss == -INFSCORE || hashes.find(computeHash(p) || std::abs(ss) > 350) != hashes.end()) continue; // do not insert same position twice ...
         hashes.insert(computeHash(p));
         const ScoreType seval = eval(p, data, ThreadPool::instance().main());
         //if (e%100==0) std::cout << "eval " << seval << std::endl;
         if (std::abs(seval) < PGNGame::equalMargin) {
            ++e;
            const ScoreType squiet = ThreadPool::instance().main().qsearchNoPruning(-10000, 10000, p, 1, seldepth);
            if (std::abs(seval - squiet) < PGNGame::quietMargin) {
               os << GetFENShort2(p) << " c9 \"" << game.resultStr << "\";"
                  << " c31 \"" << ss << "\";" << std::endl;
               ++c;
            }
            /*
             else {
                std::cout << GetFEN(p) << " || " << seval << " " << squiet << " " << ScaleScore(sc.scores[sc_Mat],gp) << " " << sc.scores[sc_Mat][MG] << " " << sc.scores[sc_Mat][EG] << " " << gp << std::endl;
             }
             */
         }
      }
   }
   std::cout << "...done" << std::endl;
}

int PGNParse(const std::string& file) {
   std::ifstream is(file);
   std::ofstream os(file + ".edp");
   pgnparse__(is, os);
   return 0;
}

#endif
