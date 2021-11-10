#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "com.hpp"
#include "definition.hpp"
#include "learn_tools.hpp"
#include "logging.hpp"
#include "material.hpp"
#include "moveGen.hpp"
#include "pieceTools.hpp"
#include "position.hpp"
#include "positionTools.hpp"
#include "searcher.hpp"

#ifdef WITH_DATA2BIN

// Mostly copy/paste from nodchip Stockfish repository and adapted to Minic
// Tools for handling various learning data format

namespace {
int parse_game_result_from_pgn_extract(std::string result) {
   // White Win
   if (result == "\"1-0\"") { return 1; }
   // Black Win&
   else if (result == "\"0-1\"") {
      return -1;
   }
   // Draw
   else {
      return 0;
   }
}

// 0.25 -->  0.25 * PawnValueEg
// #-4  --> -mate_in(4)
// #3   -->  mate_in(3)
// -M4  --> -mate_in(4)
// +M3  -->  mate_in(3)
ScoreType parse_score_from_pgn_extract(std::string eval, bool& success) {
   success = true;

   if (eval.substr(0, 1) == "#") {
      if (eval.substr(1, 1) == "-") { return -MATE + (stoi(eval.substr(2, eval.length() - 2))); }
      else {
         return MATE - (stoi(eval.substr(1, eval.length() - 1)));
      }
   }
   else if (eval.substr(0, 2) == "-M") {
      //std::cout << "eval=" << eval << std::endl;
      return -MATE + (stoi(eval.substr(2, eval.length() - 2)));
   }
   else if (eval.substr(0, 2) == "+M") {
      //std::cout << "eval=" << eval << std::endl;
      return MATE - (stoi(eval.substr(2, eval.length() - 2)));
   }
   else {
      char*  endptr;
      double value = strtod(eval.c_str(), &endptr);
      if (*endptr != '\0') {
         success = false;
         return 0;
      }
      else {
         return ScoreType(value * 100);
      }
   }
}

} // namespace

bool convert_bin(const std::vector<std::string>& filenames,
                 const std::string&              output_file_name,
                 const int                       ply_minimum,
                 const int                       ply_maximum,
                 const int                       interpolate_eval) {
   std::fstream fs;
   uint64_t     data_size          = 0;
   uint64_t     filtered_size      = 0;
   uint64_t     filtered_size_fen  = 0;
   uint64_t     filtered_size_move = 0;
   uint64_t     filtered_size_ply  = 0;
   // convert plain to bin
   fs.open(output_file_name, std::ios::app | std::ios::binary);
   for (auto filename : filenames) {
      std::cout << "converting " << filename << " from plain to binary format... " << std::endl;
      std::string   line;
      std::string   line2;
      std::ifstream ifs;
      ifs.open(filename);
      PackedSfenValue p;
      RootPosition    pos;
#if defined(WITH_NNUE)
      NNUEEvaluator evaluator;
      pos.associateEvaluator(evaluator);
      pos.resetNNUEEvaluator(pos.Evaluator());
#endif
      data_size                     = 0;
      filtered_size                 = 0;
      filtered_size_fen             = 0;
      filtered_size_move            = 0;
      filtered_size_ply             = 0;
      p.gamePly                     = 1; // Not included in apery format. Should be initialized
      bool         ignore_flag_fen  = false;
      bool         ignore_flag_move = false;
      bool         ignore_flag_ply  = false;
      bool         scored           = false;
      bool         hasPos           = false;
      unsigned int count            = 0;
      while (std::getline(ifs, line)) {
         count++;
         if ((count % 1000000 == 0)) { std::cout << "lines " << count << ", skipped pos " << filtered_size << std::endl; }
         std::stringstream ss(line);
         std::string       token;
         std::string       value;

         auto skipit = [&]() {
            std::cout << "skipping from line " << count << std::endl;
            while (std::getline(ifs, line2)) {
               std::cout << "skipped line " << line2 << std::endl;
               std::stringstream sss(line2);
               sss >> token;
               if (token == "e") {
                  ignore_flag_fen  = false;
                  ignore_flag_move = false;
                  ignore_flag_ply  = false;
                  scored           = false;
                  hasPos           = false;
                  break;
               }
            }
         };

         ss >> token;
         if (token == "fen") {
            std::string input_fen = line.substr(4);
            if (readFEN(input_fen, pos, true, true)) {
               sfen_pack(pos, p.sfen);
               hasPos = true;
            }
            else {
               skipit();
            }
         }
         else if (token == "move") {
            ss >> value;
            Square from = INVALIDSQUARE;
            Square to   = INVALIDSQUARE;
            MType  type = T_std;
            // forbid castling moves
            // this will create a "discontinuity" in binpack interpretation of the move sequence
            if (hasPos && readMove(pos, value, from, to, type, true)) { 
               p.move = ToSFMove(pos, from, to, type); // use SF style move encoding
               //p.move = ToMove(from,to,type); // use Minic style move encoding
            }
            else {
               skipit();
            }
         }
         else if (token == "score") {
            int16_t score;
            ss >> score;
            p.score = std::min(std::max(score, int16_t(-MATE)), int16_t(MATE));
         }
         else if (token == "ply") {
            int temp;
            ss >> temp;
            if (temp < ply_minimum || temp > ply_maximum) {
               ignore_flag_ply = true;
               filtered_size_ply++;
            }
            p.gamePly = uint16_t(temp); // No cast here?
            if (interpolate_eval != 0) { p.score = std::min(3000, interpolate_eval * temp); }
         }
         else if (token == "result") {
            int temp;
            ss >> temp;
            p.game_result = int8_t(temp); // Do you need a cast here?
            if (interpolate_eval) { p.score = p.score * p.game_result; }
            scored = true;
         }
         else if (token == "e") {
            if (!(ignore_flag_fen || ignore_flag_move || ignore_flag_ply) && scored && hasPos) {
               fs.write((char*)&p, sizeof(PackedSfenValue));
               data_size += 1;
            }
            else {
               filtered_size++;
            }
            ignore_flag_fen  = false;
            ignore_flag_move = false;
            ignore_flag_ply  = false;
            scored           = false;
            hasPos           = false;
         }
         else {
            std::cout << "bad token : " << token << std::endl;
            skipit();
         }
      }
      std::cout << "done " << data_size << " parsed " << filtered_size << " is filtered"
                << " (illegal fen:" << filtered_size_fen << ", illegal move:" << filtered_size_move << ", illegal ply:" << filtered_size_ply << ")"
                << std::endl;
      ifs.close();
   }
   std::cout << "all done" << std::endl;
   fs.close();
   return true;
}

namespace {

inline void ltrim(std::string& s) {
   s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
}

inline void rtrim(std::string& s) {
   s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
}

inline void trim(std::string& s) {
   ltrim(s);
   rtrim(s);
}

inline bool is_like_fen(std::string fen) {
   int count_space = std::count(fen.cbegin(), fen.cend(), ' ');
   int count_slash = std::count(fen.cbegin(), fen.cend(), '/');
   return count_space == 5 && count_slash == 7;
}
} // namespace

// pgn-extract --fencomments -Wlalg --nochecks --nomovenumbers --noresults -w500000 -N -V -o data.plain games.pgn
bool convert_bin_from_pgn_extract(const std::vector<std::string>& filenames,
                                  const std::string&              output_file_name,
                                  const bool                      pgn_eval_side_to_move,
                                  const bool                      convert_no_eval_fens_as_score_zero) {
   std::cout << "pgn_eval_side_to_move=" << pgn_eval_side_to_move << std::endl;
   std::cout << "convert_no_eval_fens_as_score_zero=" << convert_no_eval_fens_as_score_zero << std::endl;

   RootPosition pos;

   std::fstream ofs;
   ofs.open(output_file_name, std::ios::out | std::ios::binary);

   int game_count = 0;
   int fen_count  = 0;

   for (auto filename : filenames) {
      //std::cout << " convert " << filename << std::endl;
      std::ifstream ifs;
      ifs.open(filename);

      int game_result = 0;

      std::string line;
      while (std::getline(ifs, line)) {
         //std::cout << "line : " << line << std::endl;

         if (line.empty()) { continue; }

         else if (line.substr(0, 1) == "[") {
            std::regex  pattern_result(R"(\[Result (.+?)\])");
            std::smatch match;

            // example: [Result "1-0"]
            if (std::regex_search(line, match, pattern_result)) {
               game_result = parse_game_result_from_pgn_extract(match.str(1));
               game_count++;
               if (game_count % 10000 == 0) { std::cout << " game_count=" << game_count << ", fen_count=" << fen_count << std::endl; }
            }

            continue;
         }

         else {
            int  gamePly = 1;
            auto itr     = line.cbegin();

            while (true) {
               gamePly++;

               PackedSfenValue psv;
               memset((char*)&psv, 0, sizeof(PackedSfenValue));

               // fen
               {
                  bool fen_found = false;

                  while (!fen_found) {
                     std::regex  pattern_bracket(R"(\{(.+?)\})");
                     std::smatch match;
                     if (!std::regex_search(itr, line.cend(), match, pattern_bracket)) { break; }

                     itr += match.position(0) + match.length(0) - 1;
                     std::string str_fen = match.str(1);
                     trim(str_fen);

                     //std::cout << "possible fen " << str_fen << std::endl;
                     if (is_like_fen(str_fen)) {
                        //std::cout << "validated fen " << str_fen << std::endl;
                        fen_found = true;
                        readFEN(str_fen, pos, true, true);
                        sfen_pack(pos, psv.sfen);
                     }
                  }

                  if (!fen_found) {
                     //std::cout << "fen not found" << std::endl;
                     break;
                  }
               }

               // move
               {
                  std::regex  pattern_move(R"(\}(.+?)\{)");
                  std::smatch match;
                  if (!std::regex_search(itr, line.cend(), match, pattern_move)) {
                     //std::cout << "move not found" << std::endl;
                     break;
                  }

                  itr += match.position(0) + match.length(0) - 1;
                  std::string str_move = match.str(1);
                  //std::cout << "move " << str_move << std::endl;
                  trim(str_move);
                  Square from = INVALIDSQUARE;
                  Square to   = INVALIDSQUARE;
                  MType  type = T_std;
                  bool   b    = readMove(pos, str_move, from, to, type);
                  if (b) {
                     psv.move = ToSFMove(pos, from, to, type); // use SF style move encoding
                  }
               }

               // eval
               bool eval_found = false;
               {
                  std::regex  pattern_bracket(R"(\{(.+?)\})");
                  std::smatch match;
                  if (!std::regex_search(itr, line.cend(), match, pattern_bracket)) {
                     //std::cout << "eval not found" << std::endl;
                     break;
                  }

                  std::string str_eval_clk = match.str(1);
                  trim(str_eval_clk);
                  //std::cout << "eval " << str_eval_clk << std::endl;

                  // example: { [%eval 0.25] [%clk 0:10:00] }
                  // example: { [%eval #-4] [%clk 0:10:00] }
                  // example: { [%eval #3] [%clk 0:10:00] }
                  // example: { +0.71/22 1.2s }
                  // example: { -M4/7 0.003s }
                  // example: { M3/245 0.017s }
                  // example: { +M1/245 0.010s, White mates }
                  // example: { 0.60 }
                  // example: { book }
                  // example: { rnbqkb1r/pp3ppp/2p1pn2/3p4/2PP4/2N2N2/PP2PPPP/R1BQKB1R w KQkq - 0 5 }

                  // Considering the absence of eval
                  if (!is_like_fen(str_eval_clk)) {
                     itr += match.position(0) + match.length(0) - 1;

                     if (str_eval_clk != "book") {
                        std::regex pattern_eval1(R"(\[\%eval (.+?)\])");
                        std::regex pattern_eval2(R"((.+?)\/)");

                        std::string str_eval;
                        if (std::regex_search(str_eval_clk, match, pattern_eval1) || std::regex_search(str_eval_clk, match, pattern_eval2)) {
                           str_eval = match.str(1);
                           trim(str_eval);
                        }
                        else {
                           str_eval = str_eval_clk;
                        }

                        bool      success = false;
                        ScoreType value   = parse_score_from_pgn_extract(str_eval, success);
                        if (success) {
                           eval_found = true;
                           psv.score  = std::clamp(value, ScoreType(-MATE), ScoreType(MATE));
                        }
                     }
                  }
               }

               // write
               if (eval_found || convert_no_eval_fens_as_score_zero) {
                  if (!eval_found && convert_no_eval_fens_as_score_zero) { psv.score = 0; }

                  psv.gamePly     = gamePly;
                  psv.game_result = game_result;

                  if (pos.c == Co_Black) {
                     if (!pgn_eval_side_to_move) { psv.score *= -1; }
                     psv.game_result *= -1;
                  }

                  ofs.write((char*)&psv, sizeof(PackedSfenValue));

                  fen_count++;
               }
            }

            game_result = 0;
         }
      }
   }

   std::cout << " game_count=" << game_count << ", fen_count=" << fen_count << std::endl;
   std::cout << " all done" << std::endl;
   ofs.close();
   return true;
}

bool convert_plain(const std::vector<std::string>& filenames, const std::string& output_file_name) {
   std::ofstream ofs;
   ofs.open(output_file_name, std::ios::app);
   for (auto filename : filenames) {
      std::cout << "convert " << filename << " ... " << std::endl;
      // Just convert packedsfenvalue to text
      std::fstream fs;
      fs.open(filename, std::ios::in | std::ios::binary);
      PackedSfenValue p;
      while (true) {
         if (fs.read((char*)&p, sizeof(PackedSfenValue))) {
            Position tpos; // fully empty position !
            set_from_packed_sfen(tpos, p.sfen);
            // write as plain text
            ofs << "fen " << GetFEN(tpos) << std::endl;
            ofs << "move " << ToString(FromSFMove(tpos, p.move)) << std::endl;
            ofs << "score " << p.score << std::endl;
            ofs << "ply " << int(p.gamePly) << std::endl;
            ofs << "result " << int(p.game_result) << std::endl;
            ofs << "e" << std::endl;
         }
         else {
            break;
         }
      }
      fs.close();
      std::cout << "done" << std::endl;
   }
   ofs.close();
   std::cout << "all done" << std::endl;

   return true;
}

bool rescore(const std::vector<std::string>& filenames, const std::string& output_file_name) {
   std::ofstream ofs;
   ofs.open(output_file_name, std::ios::app);

   // init sub search
   DynamicConfig::minOutputLevel = Logging::logOff;
   DynamicConfig::disableTT      = false;
   DynamicConfig::level          = 100;

   uint64_t count = 0;

   for (auto filename : filenames) {
      std::cout << "rescoring " << filename << " ... " << std::endl;
      // Just convert packedsfenvalue to text
      std::fstream fs;
      fs.open(filename, std::ios::in | std::ios::binary);
      PackedSfenValue p;
      ///@todo threading !!!!
      while (true) {
         if ((++count % 100000) == 0) { std::cout << count << std::endl; }
         if (fs.read((char*)&p, sizeof(PackedSfenValue))) {
            RootPosition tpos; // fully empty position !
            set_from_packed_sfen(tpos, p.sfen);
#ifdef WITH_NNUE
            NNUEEvaluator evaluator;
            tpos.associateEvaluator(evaluator);
            tpos.resetNNUEEvaluator(evaluator);
#endif              
            ThreadData data;
            const Hash matHash = MaterialHash::getMaterialHash(tpos.mat);
            float gp = 1;
            if (matHash != nullHash) {
               const MaterialHash::MaterialHashEntry& MEntry = MaterialHash::materialHashTable[matHash];
               gp                                            = MEntry.gamePhase();
            }
            DepthType depth(DynamicConfig::genFenDepth * gp + DynamicConfig::genFenDepthEG * (1.f - gp));
            DynamicConfig::randomPly = 0;
            data.p                   = tpos;
            data.depth               = depth;
            ThreadPool::instance().main().setData(data);
            // do not update COM::position here
            ThreadPool::instance().main().searchDriver(false);
            data = ThreadPool::instance().main().getData();
            p.score = data.score;
            ofs.write((char*)&p, sizeof(PackedSfenValue));
         }
         else {
            std::cout << "read error" << std::endl;
            break;
         }
      }
      fs.close();
      std::cout << "done" << std::endl;
   }
   ofs.close();
   std::cout << "all done" << std::endl;

   return true;
}

#endif // WITH_DATA2BIN