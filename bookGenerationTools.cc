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

bool add(const std::string & move, Position & p, std::ofstream & binFile){
   std::string moveStr;
   if ( move == "0-0" || move == "0-0-0" || move == "O-O" || move == "O-O-O") moveStr = move;
   else {moveStr.push_back(move[0]); moveStr.push_back(move[1]); moveStr.push_back(' '); moveStr.push_back(move[2]); moveStr.push_back(move[3]);}
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
   readFEN(startPosition,ps);
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
