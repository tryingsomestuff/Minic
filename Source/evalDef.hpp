#pragma once

#include "definition.hpp"

struct EvalData;
struct Position;
struct Searcher;

template < bool display = false, bool safeMatEvaluator = true>
ScoreType eval(const Position & p, EvalData & data, Searcher &context);
