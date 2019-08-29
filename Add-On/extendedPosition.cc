#include <cctype>

BitBoard getPieceBitboard(const Position & p, Piece t){
    return p.allB[t+PieceShift];
}

uint64_t numberOf(const Position & p, Piece t){ return countBit(getPieceBitboard(p,t));}

std::string showAlgAbr(Move m, const Position & p) {
    Square from  = Move2From(m);
    Square to    = Move2To(m);
    MType  mtype = Move2Type(m);
    if ( m == INVALIDMOVE) return "xx";

    bool isCheck = false;
    bool isNotLegal = false;
    Position p2 = p;
    if (apply(p2,m)){
        if ( isAttacked(p2, kingSquare(p2)) ) isCheck = true;
    }
    else{ isNotLegal = true; }

    if ( mtype == T_wks || mtype == T_bks) return std::string("0-0")   + (isCheck?"+":"") + (isNotLegal?"~":"");
    if ( mtype == T_wqs || mtype == T_bqs) return std::string("0-0-0") + (isCheck?"+":"") + (isNotLegal?"~":"");

    std::string s;
    Piece t = p.b[from];

    // add piece type if not pawn
    s+= PieceNames[PieceShift + std::abs(t)];
    if ( t==P_wp || t==P_bp ) s.clear(); // no piece symbol for pawn

    // ensure move is not ambiguous
    bool isSamePiece= false;
    bool isSameFile = false;
    bool isSameRank = false;
    if ( numberOf(p,t)>1 ){
        std::vector<Square> v;
        BitBoard b = getPieceBitboard(p,t);
        while (b) v.push_back(BBTools::popBit(b));
        for(auto it = v.begin() ; it != v.end() ; ++it){
            if ( *it == from ) continue; // to not compare to myself ...
            MoveList l;
            generateSquare<GP_all>(p,l,*it);
            for(auto mit = l.begin() ; mit != l.end() ; ++mit){
                if ( *mit == m ) continue; // to not compare to myself ... should no happend thanks to previous verification
                Position p3 = p;
                if (apply(p3,*mit)){ // only if move is legal
                   if ( Move2To(*mit) == to && (t == p.b[Move2From(*mit)]) ){ // another move is landing on the same square with the same piece type
                      isSamePiece = true;
                      if ( SQFILE(Move2From(*mit)) == SQFILE(from)){ isSameFile = true; }
                      if ( SQRANK(Move2From(*mit)) == SQRANK(from)){ isSameRank = true; }
                   }
                }
            }
        }
    }

    if (((t==P_wp || t==P_bp) && isCapture(m)) ) s+= FileNames[SQFILE(from)];
    else if ( isSamePiece ){
        if ( !isSameFile )                     s+= FileNames[SQFILE(from)];
        else if ( isSameFile && !isSameRank )  s+= RankNames[SQRANK(from)];
        else if ( isSameFile && isSameRank )  {s+= FileNames[SQFILE(from)]; s+= RankNames[SQRANK(from)];}
    }

    // add 'x' if capture
    if ( isCapture(m)){
        s+="x";
    }

    // add landing position
    s+= SquareNames[to];

    // and promotion to
    if (isPromotion(m)){
        switch(mtype){
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
        default:
            break;
        }
    }

    if ( isCheck )    s += "+";
    if ( isNotLegal ) s += "~";

    return s;
}

void split( std::vector<std::string> & v, const std::string & str, const std::string & sep){
    size_t start = 0, end = 0;
    while ( end != std::string::npos){
        end = str.find( sep, start);
        v.push_back( str.substr( start,(end == std::string::npos) ? std::string::npos : end - start));
        start = (   ( end > (std::string::npos - sep.size()) ) ?  std::string::npos  :  end + sep.size());
    }
}

std::vector<std::string> split2(const std::string & line, char sep, char delim){
   std::vector<std::string> v;
   const char * c = line.c_str();       // prepare to parse the line
   bool inSep{false};
   for (const char* p = c; *p ; ++p) {  // iterate through the string
       if (*p == delim) {               // toggle flag if we're btw delim
           inSep = !inSep;
       }
       else if (*p == sep && !inSep) {  // if sep OUTSIDE delim
           std::string str(c,p-c);
           str.erase(std::remove(str.begin(), str.end(), ':'), str.end());
           v.push_back(str);            // keep the field
           c = p+1;                     // and start parsing next one
       }
   }
   v.push_back(std::string(c));         // last field delimited by end of line instead of sep
   return v;
}

std::string ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
    return s;
}


class ExtendedPosition : public Position{
public:
    ExtendedPosition(const std::string & s, bool withMoveCount = true);
    bool shallFindBest();
    bool shallAvoidBad();
    bool meaStyle();
    std::vector<std::string> bestMoves();
    std::vector<std::string> badMoves();
    std::vector<std::string> comment0();
    std::string id();
    static bool readEPDFile(const std::string & fileName, std::vector<std::string> & positions);
    static void test(const std::vector<std::string> & positions,
                     const std::vector<int> &         timeControls,
                     bool                             breakAtFirstSuccess,
                     const std::vector<int> &         scores,
                     std::function< int(int) >        eloF,
                     bool                             withMoveCount = true);
    static void testStatic(const std::vector<std::string> & positions,
                           int chunck = 4,
                           bool withMoveCount = false);
//private:
    std::map<std::string,std::vector<std::string> > _extendedParams;
};

ExtendedPosition::ExtendedPosition(const std::string & extFEN, bool withMoveCount) : Position(extFEN){
    if (!withMoveCount) {
        halfmoves = 0; moves = 1; fifty = 0;
    }
    //Logging::LogIt(Logging::logInfo) << ToString(*this);
    std::vector<std::string> strList;
    std::stringstream iss(extFEN);
    std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));
    if ( strList.size() < (withMoveCount?7u:5u)) Logging::LogIt(Logging::logFatal) << "Not an extended position";
    std::vector<std::string>(strList.begin()+(withMoveCount?6:4), strList.end()).swap(strList);
    const std::string extendedParamsStr = std::accumulate(strList.begin(), strList.end(), std::string(""),[](const std::string & a, const std::string & b) {return a + ' ' + b;});
    //LogIt(logInfo) << "extended parameters : " << extendedParamsStr;
    std::vector<std::string> strParamList;
    split(strParamList,extendedParamsStr,";");
    for(size_t k = 0 ; k < strParamList.size() ; ++k){
        //LogIt(logInfo) << "extended parameters : " << k << " " << strParamList[k];
        strParamList[k] = ltrim(strParamList[k]);
        if ( strParamList[k].empty()) continue;
        std::vector<std::string> pair;
        split(pair,strParamList[k]," ");
        if ( pair.size() < 2 ) Logging::LogIt(Logging::logFatal) << "Not un extended parameter pair";
        std::vector<std::string> values = pair;
        values.erase(values.begin());
        _extendedParams[pair[0]] = values;
        //LogIt(logInfo) << "extended parameters pair : " << pair[0] << " => " << values[0];
    }
}

bool ExtendedPosition::shallFindBest(){ return _extendedParams.find("bm") != _extendedParams.end();}

bool ExtendedPosition::shallAvoidBad(){ return _extendedParams.find("am") != _extendedParams.end();}

std::vector<std::string> ExtendedPosition::bestMoves(){ return _extendedParams["bm"];}

std::vector<std::string> ExtendedPosition::badMoves(){ return _extendedParams["am"];}

std::vector<std::string> ExtendedPosition::comment0(){ return _extendedParams["c0"];}

std::string ExtendedPosition::id(){
    if ( _extendedParams.find("id") != _extendedParams.end() ) return _extendedParams["id"][0];
    else return "";
}

bool ExtendedPosition::readEPDFile(const std::string & fileName, std::vector<std::string> & positions){
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
