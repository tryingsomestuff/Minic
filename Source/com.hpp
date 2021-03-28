#pragma once

#include "definition.hpp"

#include "position.hpp"

/*!
 * Common tools for communication protocol (UCI and XBOARD)
 * Initialy made only for XBOARD some things here are not used in UCI...
 * ///@todo to be cleaned a little more
 */
namespace COM {
    enum State : unsigned char { st_pondering = 0, st_analyzing, st_searching, st_none };
    extern State state; // this is redundant with Mode & Ponder...

    enum Ponder : unsigned char { p_off = 0, p_on = 1 };
    extern Ponder ponder;

    extern std::string command;
    extern Position position;
    extern DepthType depth;
    enum Mode : unsigned char { m_play_white = 0, m_play_black = 1, m_force = 2, m_analyze = 3 };
    extern Mode mode;
    enum SideToMove : unsigned char { stm_white = 0, stm_black = 1 };
    extern SideToMove stm; ///@todo isn't this redundant with position.c ??
    extern Position initialPos;
    extern std::vector<Move> moves;

    void init();

    void readLine();

    [[nodiscard]] SideToMove opponent(SideToMove & s);

    bool sideToMoveFromFEN(const std::string & fen);

    bool makeMove(Move m, bool disp, std::string tag, Move ponder = INVALIDMOVE);

    void stop();

    void stopPonder();

    void receiveMoves(Move bestmove, Move pondermove);

    void thinkAsync();

    [[nodiscard]] Move moveFromCOM(std::string mstr);
}
