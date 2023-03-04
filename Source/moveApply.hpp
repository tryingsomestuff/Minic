#pragma once

#include "definition.hpp"

struct Position;
struct Searcher;

struct MoveInfo {
    MoveInfo(const Position& p, const Move _m);

    const Move   m         = INVALIDMOVE;
    const Square from      = INVALIDSQUARE;
    const Square to        = INVALIDSQUARE;
    const colored<Square> king = {INVALIDSQUARE, INVALIDSQUARE};
    const Square ep        = INVALIDSQUARE;
    const MType  type      = T_std;
    const Piece  fromP     = P_none;
    const Piece  toP       = P_none;
    const int    fromId    = 0;
    const bool   isCapNoEP = false;
};

void applyNull(Searcher& context, Position& pN);

bool applyMove(Position& p, const MoveInfo & moveInfo, const bool noNNUEUpdate = false);

void applyMoveNNUEUpdate(Position & p, const MoveInfo & moveInfo);
