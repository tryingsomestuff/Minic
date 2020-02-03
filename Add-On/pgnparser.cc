class PGNGame{
public:
    std::vector<Position> p;
    int result;
    int whiteElo = 0;
    int blackElo = 0;
    std::string resultStr;
    std::vector<Move> moves;
    int n = 0;
    static const int minElo = 3200;
};

int parseResult(const std::string& result) {
  if      (result == "1-0")     { return  1; }
  else if (result == "0-1")     { return -1; }
  else if (result == "1/2-1/2") { return  0; }
  return -2;
}

Move SANToMove(const std::string & s, const Position & p){
   MoveList moves;
   generate<GP_all>(p,moves);
   //std::cout << ToString(p) << std::endl;
   //std::cout << moves.size() << std::endl;
   for (auto it = moves.begin() ; it != moves.end() ; ++it){
       std::string san = showAlgAbr(*it,p);
       if ( san == "0-0" ) san = "O-O";
       if ( san == "0-0-0" ) san = "O-O-O";
       if ( san == "0-0+" ) san = "O-O+";
       if ( san == "0-0-0+" ) san = "O-O-O+";
       //std::cout << ToString(*it) << " : " << san << "\t";
       if ( san == s ) return *it;
   }
   //std::cout << std::endl;
   return INVALIDMOVE;
}

void pgnparse__(std::ifstream & is,std::ofstream & os) {
  bool read = true;
  int k = 0;
  int a = 0;
  int c = 0;
  int e = 0;

  std::set<Hash> hashes;

  while(read){
      ++k;
      if (k%1000==0) std::cout << "Parsing game " << k << std::endl;
      //std::cout << "new loop" << std::endl;
      if (is.eof())   { read = false; }
      if (!is.good()) { read = false; }

      PGNGame game;
      game.p.push_back(startPosition); ///@todo allow starting from another position

      std::string s;
      std::string result;
      std::string whiteElo;
      std::string blackElo;
      std::string kResultToken = "[Result \"";
      bool tagFound = false;
      //std::cout << "looking for result comment" << std::endl;
      for (; !tagFound ;) { // looking for Result comment
        if ( ! std::getline(is, s)) { read = false; break; }
        //std::cout << "..." << s << std::endl;
        if (s.empty()) { if (tagFound) break; else continue; } // end of header
        if (s.substr(0, kResultToken.size()) == kResultToken) {
          tagFound = true;
          result = s.substr(kResultToken.size(), s.size() - kResultToken.size() - 2);
        }
      }

      kResultToken = "[WhiteElo \"";
      tagFound = false;
      //std::cout << "looking for whiteelo comment" << std::endl;
      for (; !tagFound ;) { // looking for Result comment
        if ( ! std::getline(is, s)) { read = false; break; }
        //std::cout << "..." << s << std::endl;
        if (s.empty()) { if (tagFound) break; else continue; } // end of header
        if (s.substr(0, kResultToken.size()) == kResultToken) {
          tagFound = true;
          whiteElo = s.substr(kResultToken.size(), s.size() - kResultToken.size() - 2);
        }
      }

      kResultToken = "[BlackElo \"";
      tagFound = false;
      //std::cout << "looking for whiteelo comment" << std::endl;
      for (; !tagFound ;) { // looking for Result comment
        if ( ! std::getline(is, s)) { read = false; break; }
        //std::cout << "..." << s << std::endl;
        if (s.empty()) { if (tagFound) break; else continue; } // end of header
        if (s.substr(0, kResultToken.size()) == kResultToken) {
          tagFound = true;
          blackElo = s.substr(kResultToken.size(), s.size() - kResultToken.size() - 2);
        }
      }

      if ( !read ) break;

      auto gameResult = parseResult(result);
      if (gameResult != -2) { game.result = gameResult; game.resultStr = result; /*std::cout << "Result : " << result << std::endl;*/ }
      else { throw std::runtime_error("Unknown result: " + result); }

      if ( !whiteElo.empty()) game.whiteElo = std::stoi(whiteElo);
      if ( !blackElo.empty()) game.blackElo = std::stoi(blackElo);

      // Read in the moves
      for (int i = 0;read;) {
        if ( ! (is >> s) ) { read = false; break; };
        //std::cout << "reading   " << s << std::endl;
        if (s.empty() ) { continue; }
        if (parseResult(s) != -2) { break; } // result is also written at the end of each game

        // Skip comments
        if (s.front() == '{' || s.front() == '['){
            if ( s.back() == '}' || s.back() == ']')  continue;
            while (true){
                if ( ! (is >> s) ) { read = false; break; };
                if ( s.back() == '}' || s.back() == ']')  break;
            }
            continue;
        }

        // Skip the move numbers
        if (s.back() == '.') { continue;}

        // Drop annotations
        if (s.back() == '!' || s.back() == '?' ) {
          size_t len = s.length();
          auto backIt = s.end();
          backIt--;
          while (*backIt == '!' || *backIt == '?' ) {
            backIt--;
            len--;
          }
          s = s.substr(0, len);
        }

        if ( s.back() == '#') s.back() = '+';

        game.moves.push_back(SANToMove(s,game.p[i]));
        if (game.moves[i] == INVALIDMOVE) { throw std::runtime_error("Unable to parse pgn move " + s); }
        ++i;
        game.n++;
        game.p.push_back(game.p[i-1]);
        if ( ! apply(game.p[i],game.moves[i-1])){ throw std::runtime_error("Unable to apply move " + ToString(game.p[i])); }
      }

      if ( game.whiteElo < PGNGame::minElo || game.blackElo < PGNGame::minElo ) continue;
      ++a;
      if (a%100==0) std::cout << "Analyzing game " << a << " " << c << " " << e << std::endl;
      EvalData data;
      DepthType seldepth = 0;
      for (int i = 16 ; i <= game.n-6 ; ++i){
          Position p = game.p[i];
          if ( hashes.find(computeHash(p)) != hashes.end()) continue;
          hashes.insert(computeHash(p));
          const ScoreType equalMargin = 120;
          const ScoreType seval = ThreadPool::instance().main().eval(p,data);
          if (seval < equalMargin){ /// @todo use material table here !
             ++e;
             const ScoreType squiet = ThreadPool::instance().main().qsearchNoPruning(-10000,10000,p,1,seldepth);
             const ScoreType quietMargin = 80;
             if ( std::abs(seval-squiet) < quietMargin){
                os << GetFENShort2(p) << " c9 \"" << game.resultStr << "\";" << std::endl;
                ++c;
             }
             /*
             else{
                std::cout << GetFEN(p) << " || " << seval << " " << squiet << " " << ScaleScore(sc.scores[sc_Mat],gp) << " " << sc.scores[sc_Mat][MG] << " " << sc.scores[sc_Mat][EG] << " " << gp << std::endl;
             }
             */
          }
      }
  }
  std::cout << "...done" << std::endl;
}

int PGNParse(const std::string & file){
    std::ifstream is(file);
    std::ofstream os(file + ".edp");
    pgnparse__(is,os);
    return 0;
}
