#pragma once

#include "definition.hpp"

struct EvalData;
struct Position;
struct Searcher;

[[nodiscard]] ScoreType eval(const Position &p, EvalData &data, Searcher &context, bool allowEGEvaluation = true, bool display = false);
