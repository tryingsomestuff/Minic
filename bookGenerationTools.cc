size_t countLine(std::istream &is){
    // skip when bad
    if( is.bad() ) return 0;
    // save state
    std::istream::iostate state_backup = is.rdstate();
    // clear state
    is.clear();
    std::istream::streampos pos_backup = is.tellg();

    is.seekg(0);
    size_t line_cnt;
    size_t lf_cnt = (size_t)std::count(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>(), '\n');
    line_cnt = lf_cnt;
    // if the file is not end with '\n' , then line_cnt should plus 1
    is.unget();
    if( is.get() != '\n' ) { ++line_cnt ; }

    // recover state
    is.clear() ; // previous reading may set eofbit
    is.seekg(pos_backup);
    is.setstate(state_backup);

    return line_cnt;
}

std::string getFrom(const std::string & to, Position & p, const Piece & t, const std::string & helper = "" ){
    const BitBoard * const allB[13] = { &(p.blackKing),&(p.blackQueen),&(p.blackRook),&(p.blackBishop),&(p.blackKnight),&(p.blackPawn),&dummy,&(p.whitePawn),&(p.whiteKnight),&(p.whiteBishop),&(p.whiteRook),&(p.whiteQueen),&(p.whiteKing) };
    assert(allB[t + PieceShift] != 0);
    if ( allB[t + PieceShift] != 0 ){
        BitBoard froms = *(allB[t + PieceShift]);
        while(froms){
           const Square s = BB::popBit(froms);
           MoveList l;
           generateSquare(p,l,s,GP_all);
           for(auto m = l.begin() ; m != l.end() ; ++m){
               if ( Squares[Move2To(*m)] == to ){
                   if ( helper.empty()){
                       return Squares[Move2From(*m)];
                   }
                   else{
                       std::string from = Squares[Move2From(*m)];
#ifndef __linux__
                       if (isalpha(helper[0])) {
#else
                       if (std::isalpha(helper[0])){
#endif
                          if ( from[0] == helper[0]){
                              return from;
                          }
                       }
                       else{
                           if ( from[1] == helper[0]){
                               return from;
                           }
                       }
                   }
               }
           }
        }
    }
    assert(false); // we must find something !
    return "";
}

std::string getAlgAlt(const std::string & s, Position & p){
   const Color c = p.c;
   std::string sAlgAbr = s;
   std::string from;
   std::string to;
   std::string prom;
   Piece pp;
   size_t pos = sAlgAbr.find('=');
   if ( pos != std::string::npos){ // promotion
      prom = sAlgAbr.substr(pos+1);
      if ( prom.size() != 1 ){
          LogIt(logFatal) << "Bad promotion string " << prom << " (" << s << ")";
          return "";
      }
      sAlgAbr = sAlgAbr.substr(0,pos);
   }

   switch(sAlgAbr[0]){
   case 'K':
       pp = (c == Co_White ? P_wk : P_bk);
       sAlgAbr = sAlgAbr.substr(1);
       break;
   case 'Q':
       pp = (c == Co_White ? P_wq : P_bq);
       sAlgAbr = sAlgAbr.substr(1);
       break;
   case 'R':
       pp = (c == Co_White ? P_wr : P_br);
       sAlgAbr = sAlgAbr.substr(1);
       break;
   case 'B':
       pp = (c == Co_White ? P_wb : P_bb);
       sAlgAbr = sAlgAbr.substr(1);
       break;
   case 'N':
       pp = (c == Co_White ? P_wn : P_bn);
       sAlgAbr = sAlgAbr.substr(1);
       break;
   default:
       pp = (c == Co_White ? P_wp : P_bp);
       break;
   }

   sAlgAbr.erase(std::remove(sAlgAbr.begin(), sAlgAbr.end(), 'x'), sAlgAbr.end());

   switch(sAlgAbr.size()){
   case 2: // Ng3
   {
       to = sAlgAbr;
       from = getFrom(to,p,pp);
   }
       break;
   case 3: // Nfg3
   {
       to = sAlgAbr.substr(1);
       std::string helper = sAlgAbr.substr(0,1);
       from = getFrom(to,p,pp,helper);
   }
       break;
   case 4: // Rb3b5
   {
       from = sAlgAbr.substr(0,1);
       to   = sAlgAbr.substr(2);
   }
       break;
   default:
       LogIt(logFatal) << "Bad move string " << sAlgAbr << " (" << s << ")";
       return "";
   }

   if ( from.empty() ){
       LogIt(logFatal) << "Cannot get from square " << to;
       return "";
   }

   return from + " " + to + (prom.empty()?"":std::string("=")+prom);

}

bool add(const std::string & move, Position & p, std::ofstream & binFile){
   std::string moveStr = move;
   if ( move == "0-0" || move == "0-0-0" || move == "O-O" || move == "O-O-O") moveStr = move;
   else {
       moveStr = getAlgAlt(moveStr,p); // convert from short algebraic notation to internal notation
       moveStr.insert(2," ");
   }
   Square from = INVALIDSQUARE;
   Square to   = INVALIDSQUARE;
   MType mtype = T_std;
   readMove(p,moveStr,from,to,mtype);
   const Move m = ToMove(from,to,mtype);
   if ( ! apply(p,m) ) return false;
   binFile << bits(m);
   return true;
}

void addLine(const std::string & line, std::ofstream & binFile){
   std::stringstream str(trim(line));
   std::string moveStr;
   Position ps;
   readFEN(startPosition,ps,true);
   Position p = ps;
   while( str >> moveStr ){ if ( ! add(moveStr, p, binFile)) break; }
}

bool readBook(const std::string & bookFileName){
    std::ifstream bookFile(bookFileName);
    const size_t lines = countLine(bookFile);
    size_t l = 0;
    size_t count = 0;
    const std::string bookFileNameBin = bookFileName.substr(0, bookFileName.find_last_of('.')) + ".bin";
    if (fileExists(bookFileNameBin)) std::remove(bookFileNameBin.c_str());
    std::ofstream bookFileBinary(bookFileNameBin, std::ios::out | std::ios::binary);
    std::string line;
    while (std::getline(bookFile, line)) {
        ++l;
        if (l % 10000 == 0) std::cout << int(100.*l / lines) << "%" << std::endl;
        ++count;
        addLine(line, bookFileBinary);
        bookFileBinary << bits(INVALIDMOVE);
    }
    bookFileBinary.close();
    bookFile.close();
}
