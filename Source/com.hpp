#pragma once

#include "definition.hpp"

#include "position.hpp"

/*!
 * Common tools for communication protocol (UCI and XBOARD)
 * Initialy made only for XBOARD 
 * UCI shall be state less but is not totally here
 */
namespace COM {

    enum State : unsigned char { st_pondering = 0, st_analyzing, st_searching, st_none };
    extern State state; 

    extern std::string command; // see readLine
    extern Position position; 
    extern DepthType depth;
    extern std::vector<Move> moves;

    void init();

    void readLine();

    [[nodiscard]] bool makeMove(Move m, bool disp, std::string tag, Move ponder = INVALIDMOVE);

    void stop();

    void stopPonder();

    [[nodiscard]] bool receiveMoves(Move bestmove, Move pondermove);

    void thinkAsync();

    [[nodiscard]] Move moveFromCOM(std::string mstr);
}
