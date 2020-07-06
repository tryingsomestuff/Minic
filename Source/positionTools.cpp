#include "positionTools.hpp"

#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "moveGen.hpp"
#include "pieceTools.hpp"
#include "tools.hpp"

#include <cstdlib>

std::string GetFENShort(const Position &p ){ // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR"
    std::stringstream ss;
    int count = 0;
    for (int i = 7; i >= 0; --i) {
        for (int j = 0; j < 8; j++) {
            const Square k = 8 * i + j;
            if (p.board_const(k) == P_none) ++count;
            else {
                if (count != 0) { ss << count; count = 0; }
                ss << PieceTools::getName(p,k);
            }
            if (j == 7) {
                if (count != 0) { ss << count; count = 0; }
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
    ///@todo Wrong if FRC !
    if (p.castling & C_wks) { ss << "K"; withCastling = true; }
    if (p.castling & C_wqs) { ss << "Q"; withCastling = true; }
    if (p.castling & C_bks) { ss << "k"; withCastling = true; }
    if (p.castling & C_bqs) { ss << "q"; withCastling = true; }
    if (!withCastling) ss << "-";
    if (p.ep != INVALIDSQUARE) ss << " " << SquareNames[p.ep];
    else ss << " -";
    return ss.str();
}

std::string GetFEN(const Position &p) { // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d5 0 2"
    std::stringstream ss;
    ss << GetFENShort2(p) << " " << (int)p.fifty << " " << (int)p.moves;
    return ss.str();
}

std::string SanitizeCastling(const Position & p, const std::string & str){
    if (( str == "e1g1" && p.board_const(Sq_e1) == P_wk ) || ( str == "e8g8" && p.board_const(Sq_e8) == P_bk )) return "0-0";
    if (( str == "e1c1" && p.board_const(Sq_e1) == P_wk ) || ( str == "e8c8" && p.board_const(Sq_e8) == P_bk )) return "0-0-0";
    return str;
}

Move SanitizeCastling(const Position & p, const Move & m){
    if ( !VALIDMOVE(m) ) return m;
    const Square from = Move2From(m); assert(squareOK(from));
    const Square to   = Move2To(m); assert(squareOK(to));
    MType mtype = Move2Type(m); assert(moveTypeOK(mtype));
    // convert GUI castling input notation to internal castling style if not FRC
    if ( !DynamicConfig::FRC ){
       bool whiteToMove = p.c == Co_White;
       if (mtype == T_std && from == p.kingInit[p.c] ) {
           if      (to == (whiteToMove ? Sq_c1 : Sq_c8)) return ToMove(from, to, whiteToMove ? T_wqs : T_bqs);
           else if (to == (whiteToMove ? Sq_g1 : Sq_g8)) return ToMove(from, to, whiteToMove ? T_wks : T_bks);
       }
    }
    return m;
}

Square kingSquare(const Position & p) {
  return p.king[p.c];
}

bool readMove(const Position & p, const std::string & ss, Square & from, Square & to, MType & moveType ) {
    if ( ss.empty()){
        Logging::LogIt(Logging::logFatal) << "Trying to read empty move ! " ;
        moveType = T_std;
        return false;
    }
    std::string str(ss);
    str = SanitizeCastling(p,str);
    // add space to go to own internal notation (if not castling)
    if ( str != "0-0" && str != "0-0-0" && str != "O-O" && str != "O-O-O" ) str.insert(2," ");

    std::vector<std::string> strList;
    std::stringstream iss(str);
    std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));

    moveType = T_std;
    if ( strList.empty()){ Logging::LogIt(Logging::logError) << "Trying to read bad move, seems empty " << str ; return false; }

    // detect castling
    if (strList[0] == "0-0" || strList[0] == "O-O") {
        moveType = (p.c == Co_White) ? T_wks : T_bks;
        from = kingSquare(p);
        to = (p.c == Co_White) ? Sq_g1 : Sq_g8;
    }
    else if (strList[0] == "0-0-0" || strList[0] == "O-O-O") {
        moveType = (p.c == Co_White) ? T_wqs : T_bqs;
        from = kingSquare(p);
        to = (p.c == Co_White) ? Sq_c1 : Sq_c8;
    }
    else{
        if ( strList.size() == 1 ){ Logging::LogIt(Logging::logError) << "Trying to read bad move, malformed (=1) " << str ; return false; }
        if ( strList.size() > 2 && strList[2] != "ep"){ Logging::LogIt(Logging::logError) << "Trying to read bad move, malformed (>2)" << str ; return false; }
        if (strList[0].size() == 2 && (strList[0].at(0) >= 'a') && (strList[0].at(0) <= 'h') && ((strList[0].at(1) >= 1) && (strList[0].at(1) <= '8'))) from = stringToSquare(strList[0]);
        else { Logging::LogIt(Logging::logError) << "Trying to read bad move, invalid from square " << str ; return false; }
        bool isCapture = false;
        // be carefull, promotion possible !
        if (strList[1].size() >= 2 && (strList[1].at(0) >= 'a') && (strList[1].at(0) <= 'h') &&  ((strList[1].at(1) >= '1') && (strList[1].at(1) <= '8'))) {
            if ( strList[1].size() > 2 ){ // promotion
                std::string prom;
                if ( strList[1].size() == 3 ){ // probably e7 e8q notation
                    prom = strList[1][2];
                    to = stringToSquare(strList[1].substr(0,2));
                }
                else{ // probably e7 e8=q notation
                    std::vector<std::string> strListTo;
                    tokenize(strList[1],strListTo,"=");
                    to = stringToSquare(strListTo[0]);
                    prom = strListTo[1];
                }
                isCapture = p.board_const(to) != P_none;
                if      ( prom == "Q" || prom == "q") moveType = isCapture ? T_cappromq : T_promq;
                else if ( prom == "R" || prom == "r") moveType = isCapture ? T_cappromr : T_promr;
                else if ( prom == "B" || prom == "b") moveType = isCapture ? T_cappromb : T_promb;
                else if ( prom == "N" || prom == "n") moveType = isCapture ? T_cappromn : T_promn;
                else{ Logging::LogIt(Logging::logError) << "Trying to read bad move, invalid to square " << str ; return false; }
            }
            else{
                to = stringToSquare(strList[1]);
                isCapture = p.board_const(to) != P_none;
                if(isCapture) moveType = T_capture;
            }
        }
        else { Logging::LogIt(Logging::logError) << "Trying to read bad move, invalid to square " << str ; return false; }
        if (PieceTools::getPieceType(p,from) == P_wp && to == p.ep) moveType = T_ep;
    }
    if ( DynamicConfig::FRC ){
       // In FRC, some castling way be encoded king takes rooks ... Let's check that, the dirty way
       if ( p.board_const(from) == P_wk && p.board_const(to) == P_wr ){ moveType = (to<from ? T_wqs : T_wks); }
       if ( p.board_const(from) == P_bk && p.board_const(to) == P_br ){ moveType = (to<from ? T_bqs : T_bks); }
    }
    if (!DynamicConfig::FRC && !isPseudoLegal(p, ToMove(from, to, moveType))) { ///@todo FRC !
        Logging::LogIt(Logging::logError) << "Trying to read bad move, not legal " << ToString(p) << str;
        return false;
    }

    return true;
}

float gamePhase(const Position & p, ScoreType & matScoreW, ScoreType & matScoreB){
    const float totalMatScore = 2.f * *absValues[P_wq] + 4.f * *absValues[P_wr] + 4.f * *absValues[P_wb] + 4.f * *absValues[P_wn] + 16.f * *absValues[P_wp]; // cannot be static for tuning process ...
    const ScoreType matPieceScoreW = p.mat[Co_White][M_q] * *absValues[P_wq] + p.mat[Co_White][M_r] * *absValues[P_wr] + p.mat[Co_White][M_b] * *absValues[P_wb] + p.mat[Co_White][M_n] * *absValues[P_wn];
    const ScoreType matPieceScoreB = p.mat[Co_Black][M_q] * *absValues[P_wq] + p.mat[Co_Black][M_r] * *absValues[P_wr] + p.mat[Co_Black][M_b] * *absValues[P_wb] + p.mat[Co_Black][M_n] * *absValues[P_wn];
    const ScoreType matPawnScoreW  = p.mat[Co_White][M_p] * *absValues[P_wp];
    const ScoreType matPawnScoreB  = p.mat[Co_Black][M_p] * *absValues[P_wp];
    matScoreW = matPieceScoreW + matPawnScoreW;
    matScoreB = matPieceScoreB + matPawnScoreB;
    return (matScoreW + matScoreB ) / totalMatScore; // based on MG values
}

bool readEPDFile(const std::string & fileName, std::vector<std::string> & positions){
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