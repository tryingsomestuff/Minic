#include "extendedPosition.hpp"

#if defined(WITH_TEST_SUITE) || defined(WITH_PGN_PARSER) || defined(WITH_EVAL_TUNING)

#include <algorithm>
#include <cctype>
#include <functional>
#include <locale>
#include <numeric>

#include "com.hpp"
#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "moveGen.hpp"
#include "positionTools.hpp"
#include "searcher.hpp"
#include "tools.hpp"

namespace {
uint64_t numberOf(const Position &p, Piece t) { return BB::countBit(p.pieces_const(t)); }
} // namespace

std::string showAlgAbr(const Move m, const Position &p) {
   const Square from  = Move2From(m);
   const Square to    = Move2To(m);
   const MType  mtype = Move2Type(m);
   if (m == INVALIDMOVE) return "xx";

   bool     isCheck    = false;
   bool     isNotLegal = false;
   Position p2         = p;
#ifdef WITH_NNUE
   NNUEEvaluator evaluator = p.evaluator();
   p2.associateEvaluator(evaluator);
#endif
   if (applyMove(p2, m)) {
      if (isPosInCheck(p2)) isCheck = true;
   }
   else {
      isNotLegal = true;
   }

   if (mtype == T_wks || mtype == T_bks) return std::string("0-0") + (isCheck ? "+" : "") + (isNotLegal ? "~" : "");
   if (mtype == T_wqs || mtype == T_bqs) return std::string("0-0-0") + (isCheck ? "+" : "") + (isNotLegal ? "~" : "");

   std::string s;
   Piece t = p.board_const(from);

   // add piece type if not pawn
   s += PieceNames[PieceIdx((Piece)std::abs(t))];
   if (t == P_wp || t == P_bp) s.clear(); // no piece symbol for pawn

   // ensure move is not ambiguous
   bool isSamePiece = false;
   bool isSameFile  = false;
   bool isSameRank  = false;
   if (numberOf(p, t) > 1) {
      std::vector<Square> v;
      BitBoard            b = p.pieces_const(t);
      while (b) v.push_back(BB::popBit(b));
      for (auto it = v.begin(); it != v.end(); ++it) {
         if (*it == from) continue; // to not compare to myself ...
         MoveList l;
         MoveGen::generateSquare<MoveGen::GP_all>(p, l, *it);
         for (auto mit = l.begin(); mit != l.end(); ++mit) {
            if (*mit == m) continue; // to not compare to myself ... should no happend thanks to previous verification
            Position p3 = p;
            if (applyMove(p3, *mit)) { // only if move is legal
               if (Move2To(*mit) == to &&
                   (t == p.board_const(Move2From(*mit)))) { // another move is landing on the same square with the same piece type
                  isSamePiece = true;
                  if (SQFILE(Move2From(*mit)) == SQFILE(from)) { isSameFile = true; }
                  if (SQRANK(Move2From(*mit)) == SQRANK(from)) { isSameRank = true; }
               }
            }
         }
      }
   }

   if (((t == P_wp || t == P_bp) && isCapture(m))) s += FileNames[SQFILE(from)];
   else if (isSamePiece) {
      if (!isSameFile) s += FileNames[SQFILE(from)];
      else if (isSameFile && !isSameRank)
         s += RankNames[SQRANK(from)];
      else if (isSameFile && isSameRank) {
         s += FileNames[SQFILE(from)];
         s += RankNames[SQRANK(from)];
      }
   }

   // add 'x' if capture
   if (isCapture(m)) { s += "x"; }

   // add landing position
   s += SquareNames[to];

   // and promotion to
   if (isPromotion(m)) {
      switch (mtype) {
         case T_cappromq:
         case T_promq:
            s += "=";
            s += "Q";
            break;
         case T_cappromr:
         case T_promr:
            s += "=";
            s += "R";
            break;
         case T_cappromb:
         case T_promb:
            s += "=";
            s += "B";
            break;
         case T_cappromn:
         case T_promn:
            s += "=";
            s += "N";
            break;
         default: break;
      }
   }

   if (isCheck) s += "+";
   if (isNotLegal) s += "~";

   return s;
}

void split(std::vector<std::string> &v, const std::string &str, const std::string &sep) {
   size_t start = 0, end = 0;
   while (end != std::string::npos) {
      end = str.find(sep, start);
      v.push_back(str.substr(start, (end == std::string::npos) ? std::string::npos : end - start));
      start = ((end > (std::string::npos - sep.size())) ? std::string::npos : end + sep.size());
   }
}

std::vector<std::string> split2(const std::string &line, char sep, char delim) {
   std::vector<std::string> v;
   const char * c = line.c_str(); // prepare to parse the line
   bool inSep {false};
   for (const char *p = c; *p; ++p) { // iterate through the string
      if (*p == delim) {              // toggle flag if we're btw delim
         inSep = !inSep;
      }
      else if (*p == sep && !inSep) { // if sep OUTSIDE delim
         std::string str(c, p - c);
         str.erase(std::remove(str.begin(), str.end(), ':'), str.end());
         v.push_back(str); // keep the field
         c = p + 1;        // and start parsing next one
      }
   }
   v.push_back(std::string(c)); // last field delimited by end of line instead of sep
   return v;
}

std::string ltrim(std::string &s) {
   s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
   return s;
}

ExtendedPosition::ExtendedPosition(const std::string &extFEN, bool withMoveCount): RootPosition(extFEN, withMoveCount) {
   if (!withMoveCount) {
      halfmoves = 0;
      moves     = 1;
      fifty     = 0;
   }
   //Logging::LogIt(Logging::logInfo) << ToString(*this);
   std::vector<std::string> strList;
   std::stringstream        iss(extFEN);
   std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));
   if (strList.size() < (withMoveCount ? 7u : 5u)) Logging::LogIt(Logging::logFatal) << "Not an extended position";
   std::vector<std::string>(strList.begin() + (withMoveCount ? 6 : 4), strList.end()).swap(strList);
   const std::string extendedParamsStr =
       std::accumulate(strList.begin(), strList.end(), std::string(""), [](const std::string &a, const std::string &b) { return a + ' ' + b; });
   //Logging::LogIt(Logging::logInfo) << "extended parameters : " << extendedParamsStr;
   std::vector<std::string> strParamList;
   split(strParamList, extendedParamsStr, ";");
   for (size_t k = 0; k < strParamList.size(); ++k) {
      //Logging::LogIt(Logging::logInfo) << "extended parameters : " << k << " " << strParamList[k];
      strParamList[k] = ltrim(strParamList[k]);
      if (strParamList[k].empty()) continue;
      std::vector<std::string> pair;
      split(pair, strParamList[k], " ");
      if (pair.size() < 2) Logging::LogIt(Logging::logFatal) << "Not un extended parameter pair";
      std::vector<std::string> values = pair;
      values.erase(values.begin());
      _extendedParams[pair[0]] = values;
      //Logging::LogIt(Logging::logInfo) << "extended parameters pair : " << pair[0] << " => " << values[0];
   }
}

bool ExtendedPosition::shallFindBest() { return _extendedParams.find("bm") != _extendedParams.end(); }

bool ExtendedPosition::shallAvoidBad() { return _extendedParams.find("am") != _extendedParams.end(); }

std::vector<std::string> ExtendedPosition::bestMoves() { return _extendedParams["bm"]; }

std::vector<std::string> ExtendedPosition::badMoves() { return _extendedParams["am"]; }

std::vector<std::string> ExtendedPosition::comment0() { return _extendedParams["c0"]; }

std::string ExtendedPosition::id() {
   if (_extendedParams.find("id") != _extendedParams.end()) return _extendedParams["id"][0];
   else
      return "";
}

template<typename T> std::ostream &operator<<(std::ostream &os, const std::vector<T> &v) {
   for (auto it = v.begin(); it != v.end(); ++it) { os << *it << " "; }
   return os;
}

std::string ExtendedPosition::epdString() const {
   std::string epd = GetFENShort2(*this) + " ";
   for (auto &p : _extendedParams) {
      epd += p.first + " ";
      for (auto &cc : p.second) { epd += cc + " "; }
      epd += ";";
   }
   return epd;
}

void ExtendedPosition::test(const std::vector<std::string> &positions,
                            const std::vector<int> &        timeControls,
                            bool                            breakAtFirstSuccess,
                            bool                            multipleBest,
                            const std::vector<int> &        scores,
                            std::function<int(int)>         eloF,
                            bool                            withMoveCount) {
   struct Results {
      Results(): k(0), t(0), score(0) {}
      int                                      k;
      int                                      t;
      std::string                              name;
      std::vector<std::string>                 bm;
      std::vector<std::string>                 am;
      std::vector<std::pair<std::string, int>> mea;
      std::string                              computerMove;
      int                                      score;
   };

   if (scores.size() != timeControls.size()) { Logging::LogIt(Logging::logFatal) << "Wrong timeControl versus score vector size"; }

   Results **results = new Results *[positions.size()];

   unsigned int threads = DynamicConfig::threads; //copy

   auto worker = [&](size_t begin, size_t end) {
      // run the test and fill results table
      for (size_t k = begin; k < end; ++k) {
         Logging::LogIt(Logging::logInfo) << "####################################################";
         Logging::LogIt(Logging::logInfo) << "Test #" << k << " " << positions[k];
         results[k] = new Results[timeControls.size()];
         for (size_t t = 0; t < timeControls.size(); ++t) {
            ExtendedPosition extP(positions[k], withMoveCount);
#ifdef WITH_NNUE
            NNUEEvaluator    evaluator;
            extP.associateEvaluator(evaluator);
            extP.resetNNUEEvaluator(extP.evaluator());
#endif
            Logging::LogIt(Logging::logInfo) << "Current test time control " << timeControls[t];

            TimeMan::isDynamic                   = false;
            TimeMan::nbMoveInTC                  = -1;
            TimeMan::msecPerMove                 = timeControls[t];
            TimeMan::msecInTC                    = -1;
            TimeMan::msecInc                     = -1;
            TimeMan::msecUntilNextTC             = -1;
            ThreadPool::instance().currentMoveMs = TimeMan::getNextMSecPerMove(extP);

            ///@todo support threading here by using ThinkAsync ?

            ThreadData d;
            d.p = extP;
            ThreadPool::instance().distributeData(d);
            ThreadPool::instance().main().stopFlag = false;
            //COM::position = extP; // only need for display purpose
            ThreadPool::instance().main().searchDriver(false);
            d = ThreadPool::instance().main().getData();
            const Move bestMove = d.best;

            results[k][t].name = extP.id();
            results[k][t].k    = static_cast<int>(k);
            results[k][t].t    = timeControls[t];

            results[k][t].computerMove = showAlgAbr(bestMove, extP);
            Logging::LogIt(Logging::logInfo) << "Best move found is  " << results[k][t].computerMove;

            if (!multipleBest && extP.shallFindBest()) {
               Logging::LogIt(Logging::logInfo) << "Best move should be " << extP.bestMoves()[0];
               results[k][t].bm    = extP.bestMoves();
               results[k][t].score = 0;
               bool success        = false;
               for (size_t i = 0; i < results[k][t].bm.size(); ++i) {
                  if (results[k][t].computerMove == results[k][t].bm[i]) {
                     results[k][t].score = scores[t];
                     success             = true;
                     break;
                  }
               }
               if (breakAtFirstSuccess && success) break;
            }
            else if (!multipleBest && extP.shallAvoidBad()) {
               Logging::LogIt(Logging::logInfo) << "Bad move was " << extP.badMoves()[0];
               results[k][t].am    = extP.badMoves();
               results[k][t].score = scores[t];
               bool success        = true;
               for (size_t i = 0; i < results[k][t].am.size(); ++i) {
                  if (results[k][t].computerMove == results[k][t].am[i]) {
                     results[k][t].score = 0;
                     success             = false;
                     break;
                  }
               }
               if (breakAtFirstSuccess && success) break;
            }
            else { // look into c0 section ...
               Logging::LogIt(Logging::logInfo) << "Mea style " << extP.comment0()[0];
               std::vector<std::string> tokens = extP.comment0();
               for (size_t ms = 0; ms < tokens.size(); ++ms) {
                  std::string tmp = tokens[ms];
                  tmp.erase(std::remove(tmp.begin(), tmp.end(), '"'), tmp.end());
                  tmp.erase(std::remove(tmp.begin(), tmp.end(), ','), tmp.end());
                  //std::cout << tmp << std::endl;
                  std::vector<std::string> keyval;
                  tokenize(tmp, keyval, "=");
                  if (keyval.size() > 2) { // "=" prom sign inside ...
                     std::stringstream strTmp;
                     copy(keyval.begin(), keyval.begin() + 1, std::ostream_iterator<std::string>(strTmp, "="));
                     strTmp >> keyval[0];
                     keyval[1] = keyval.back();
                  }
                  std::cout << keyval[0] << " " << keyval[1] << std::endl;
                  results[k][t].mea.push_back(std::make_pair(keyval[0], std::stoi(keyval[1])));
               }
               results[k][t].score = 0;
               bool success = false;
               constexpr std::array<int, 23>   ms = {10,  20,  30,  40,  50,  60,  70,  80,  90,  100, 125,  150,
                                                    175, 200, 250, 300, 400, 500, 600, 700, 800, 900, 10000};
               constexpr std::array<double, 23> bonus = {3.0, 2.9, 2.8, 2.7, 2.6, 2.5, 2.4, 2.3, 2.2, 2.1, 2.0, 1.9,
                                                         1.8, 1.7, 1.6, 1.5, 1.4, 1.3, 1.2, 1.1, 1.0, 1.0, 0.0};
               for (size_t i = 0; i < results[k][t].mea.size(); ++i) {
                  if (results[k][t].computerMove == results[k][t].mea[i].first) {
                     results[k][t].score     = results[k][t].mea[i].second;
                     const SearchData &datas = d.datas;
                     TimeType          msec  = 1000;
                     DepthType         dd    = d.depth;
                     for (; dd >= 0; --dd) {
                        if (sameMove(datas.moves[dd], bestMove)) { msec = datas.times[dd]; }
                        else
                           break;
                     }
                     size_t id = 0;
                     for (; id < 23 && ms[id] < msec; ++id) { ; }

                     Logging::LogIt(Logging::logInfo) << "Good " << i + 1 << " best move : " << results[k][t].mea[i].first;
                     Logging::LogIt(Logging::logInfo) << "Found at depth " << static_cast<int>(dd) << " in " << datas.times[dd] << " id " << id;
                     Logging::LogIt(Logging::logInfo) << "Bonus " << bonus[id] << " score " << results[k][t].mea[i].second;

                     results[k][t].score = static_cast<int>(results[k][t].score * bonus[id]);
                     success             = true;
                     break;
                  }
               }
               if (breakAtFirstSuccess && success) break;
            }
         }
      }
   };

   threadedWork(worker, threads, positions.size());

   // display results
   int totalScore = 0;
   std::cout << std::setw(25) << "Test" << std::setw(14) << "Move";
   for (size_t j = 0; j < timeControls.size(); ++j) { std::cout << std::setw(8) << timeControls[j]; }
   std::cout << std::setw(6) << "score" << std::endl;
   for (size_t k = 0; k < positions.size(); ++k) {
      int score = 0;
      for (size_t t = 0; t < timeControls.size(); ++t) { score += results[k][t].score; }
      totalScore += score;
      std::stringstream str;
      const std::vector<std::string> v = (results[k][0].bm.empty() ? results[k][0].am : results[k][0].bm);
      std::ostream_iterator<std::string> strIt(str, " ");
      std::copy(v.begin(), v.end(), strIt);
      std::cout << std::setw(25) << results[k][0].name << std::setw(14) << (results[k][0].bm.empty() ? std::string("!") + str.str() : str.str());
      for (size_t j = 0; j < timeControls.size(); ++j) { std::cout << std::setw(8) << results[k][j].computerMove; }
      std::cout << std::setw(6) << score << std::endl;
   }

   Logging::LogIt(Logging::logInfo) << "Total score " << totalScore << " => ELO " << eloF(totalScore);

   // clear results table
   for (size_t k = 0; k < positions.size(); ++k) { delete[] results[k]; }
   delete[] results;
}

void ExtendedPosition::testStatic(const std::vector<std::string> &positions, bool withMoveCount) {
   struct Results {
      Results(): k(0), t(0), score(0) {}
      int         k;
      int         t;
      ScoreType   score;
      std::string name;
   };

   Results *results = new Results[positions.size()];

   // run the test and fill results table
   for (size_t k = 0; k < positions.size(); ++k) {
      std::cout << "Test #" << k << " " << positions[k] << std::endl;
      ExtendedPosition extP(positions[k], withMoveCount);
      //std::cout << " " << t << std::endl;
      EvalData  data;
      ScoreType ret = eval(extP, data, ThreadPool::instance().main(), true, true);

      results[k].name  = extP.id();
      results[k].k     = static_cast<int>(k);
      results[k].score = ret;

      std::cout << "score is  " << ret << std::endl;
   }

   // display results
   std::cout << std::setw(25) << "Test" << std::setw(14) << "score" << std::endl;
   for (size_t k = 0; k < positions.size() - 2; k += 4) {
      std::cout << std::setw(25) << results[k].name << std::setw(14) << results[k].score << std::endl;
      // only compare unsigned score ...
      if (std::abs(std::abs(results[k].score) - std::abs(results[k + 2].score)) > 0) { Logging::LogIt(Logging::logWarn) << "Score differ !"; }
      std::cout << std::setw(25) << results[k + 1].name << std::setw(14) << results[k + 1].score << std::endl;
      // only compare unsigned score ...
      if (std::abs(std::abs(results[k + 1].score) - std::abs(results[k + 3].score)) > 0) { Logging::LogIt(Logging::logWarn) << "Score differ !"; }
   }

   // clear results table
   delete[] results;
}

#endif
