#pragma once

#include "definition.hpp"
#include "position.hpp"

/*!
 * Common tools for communication protocol (UCI and XBOARD)
 * Initialy made only for XBOARD 
 * UCI shall be state less but is not totally here
 */
namespace COM {

enum Protocol : uint8_t { p_uci = 0, p_xboard };
extern Protocol protocol;

enum State : uint8_t { st_pondering = 0, st_analyzing, st_searching, st_none };
extern State state;

extern std::string       command; // see readLine
extern RootPosition      position;
extern DepthType         depth;
extern std::vector<Move> moves;

void init(const Protocol pr);

void readLine();

[[nodiscard]] bool makeMove(const Move m, const bool disp, const std::string & tag, const Move ponder = INVALIDMOVE);

void stop();

void stopPonder();

[[nodiscard]] bool receiveMoves(const Move bestmove, Move pondermove);

void thinkAsync(const State givenState);

[[nodiscard]] Move moveFromCOM(std::string mstr);

} // namespace COM
