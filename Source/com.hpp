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

extern std::string       command; // see readLine, filled in COM, used in uci & xboard impl
extern RootPosition      position; // current analyzed position
extern DepthType         depth;

struct GameInfo {
    struct GameStateInfo {
       Position p;
       Move lastMove; // the move that has been played to reach this position
    };
    
    std::vector<GameStateInfo> _gameStates;
    
    Position initialPos; ///@todo shall be RootPosition ??? This is the first position after a new game

    void clear(const Position & initial);

    void append(const GameStateInfo & stateInfo);

    size_t size() const;

    [[nodiscard]] std::vector<Move> getMoves() const;

    [[nodiscard]] std::optional<Hash> getHash(uint16_t halfmove) const;

    [[nodiscard]] std::optional<Move> getMove(uint16_t halfmove) const;

    [[nodiscard]] std::optional<Position> getPosition(uint16_t halfmove) const;

    void write(std::ostream & os) const;
};

[[nodiscard]] GameInfo & GetGameInfo();

void init(const Protocol pr);

void readLine();

[[nodiscard]] bool makeMove(const Move m, const bool disp, const std::string & tag, const Move ponder = INVALIDMOVE);

void stop();

void stopPonder();

[[nodiscard]] bool receiveMoves(const Move bestmove, Move pondermove);

void thinkAsync(const State givenState);

[[nodiscard]] Move moveFromCOM(const std::string & mstr);

} // namespace COM
