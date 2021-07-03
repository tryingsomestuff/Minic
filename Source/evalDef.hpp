#pragma once

#include "definition.hpp"

struct EvalData;
struct Position;
struct Searcher;

[[nodiscard]] ScoreType eval(const Position &p, EvalData &data, Searcher &context, bool safeMatEvaluator = true, bool display = false);
