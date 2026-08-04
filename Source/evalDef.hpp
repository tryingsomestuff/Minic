#pragma once

#include "definition.hpp"

struct EvalData;
struct Position;
struct Searcher;

// forceNNUE : bypass Gate 1's lazy-threshold classical fallback and trust NNUE directly.
// Used by qsearch, where every node is reached right after a capture (mid-exchange material
// noise), so the classical fallback is very often wastefully computed then reversed by Gate 2.
[[nodiscard]] ScoreType eval(const Position &p, EvalData &data, Searcher &context, bool forceNNUE = false, bool allowEGEvaluation = true, bool display = false);
