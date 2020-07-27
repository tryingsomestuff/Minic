#pragma once

#include "definition.hpp"

struct EvalData;
struct Position;
struct Searcher;

template < bool display = false>
ScoreType eval(const Position & p, EvalData & data, Searcher &context,bool safeMatEvaluator = true, std::ostream * of = nullptr);
