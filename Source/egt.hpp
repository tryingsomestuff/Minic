#pragma once

#include "definition.hpp"

#ifdef WITH_SYZYGY

struct Position;
struct Searcher;

/*!
 * This EGT implementation is using SYZYGY TB
 * and is more or less a simple copy/paste from Arasan by Jon Dart
 * Fathom implementation (from https://github.com/jdart1/Fathom) must be cloned into "Fathom" directory
 * inside Minic root directory for this to compile/work.
 */
namespace SyzygyTb {

const ScoreType TB_CURSED_SCORE = 1;
const ScoreType TB_WIN_SCORE = 2000;
extern int MAX_TB_MEN;

bool initTB();

int probe_root(Searcher & context, const Position &p, ScoreType &score, MoveList &rootMoves);

int probe_wdl(const Position &p, ScoreType &score, bool use50MoveRule);

} // SyzygyTb
#endif
