# Minic technical documentation

This is a source-oriented developer guide to Minic: its architecture, state
representation, search, evaluation, move selection, time allocation and tooling.
Start here, then follow the chapters rather than reading every source file in
alphabetical order.

## Scope

This guide documents Minic **3.47** in its usual desktop configuration, as defined by
[config.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/config.hpp). Small-memory targets, compiler/platform
conditions and runtime options can change it. The source links and named
functions are the authority for exact conditions and current constants.

Statements about why a mechanism is useful explain its algorithmic motivation,
not a claim that it produced a measured Elo gain. Where the original author's
reason is not recorded, a tradeoff is an interpretation of the implementation,
not an account of historical intent.

Each chapter names its primary source files. Code excerpts are shortened copies
of the real functions (omissions are marked `...`); function names are given so
that they can be found by searching rather than by line number.

## Chapters

The chapters are listed in reading order. Detailed references, indented under
their parent chapter, hold the exact arithmetic that the overview chapters only
summarize.

| Chapter | Main questions |
|---|---|
| [Architecture and lifecycle](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/architecture.md) | Who owns a position? How does a command become a search and a reported move? |
| [Positions, moves and legality](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/positions-and-moves.md) | How are boards, hashes, castling and NNUE state kept consistent? |
| [Search and selectivity](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search.md) | What happens in PVS and qsearch? Which pruning, reduction and extension mechanisms actually run? |
| &nbsp;&nbsp;↳ [Search policy reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md) | Every active/disabled depth-policy switch, exact LMR arithmetic, singular branches, pruning gates and retry windows. |
| [Move ordering and picking](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/move-ordering.md) | How are moves scored, picked and learned from? Why is this not a staged MovePicker? |
| [Tables, hashes and game history](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/tables-and-history.md) | What is cached or learned, who owns it, and when is it reset? |
| [Evaluation and NNUE](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/evaluation.md) | How are HCE, NNUE, endgame knowledge and correction history combined? |
| &nbsp;&nbsp;↳ [Classical evaluation reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/classical-evaluation-reference.md) | Attack masks, pawn classifications, passer races, king-danger sigmoid, mobility, tuned feature examples and final scaling. |
| &nbsp;&nbsp;↳ [NNUE topology and inference reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/nnue-reference.md) | Exact feature indices, tensor shapes, parameter/byte counts, quantization, incremental cases, exporter and training loss. |
| [Time management and parallel search](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/time-and-parallelism.md) | How are time limits, emergencies, pondering and Lazy SMP coordinated? |
| [Implemented but disabled techniques](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/disabled-features.md) | Which mechanisms exist in the code but are switched off by default, how are they gated and how can they be re-enabled? |
| [Building, diagnosing and modifying Minic](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/development.md) | How do you build, exercise, measure and safely change these mechanisms? |
| [Technical glossary](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md) | Definitions of engine terminology, Minic-specific names, mathematical symbols and easily confused concepts. |

The order goes from the engine's skeleton to its decisions: how the program is
organized, how a position is represented, how the tree is searched, how moves are
ordered, what the search remembers, how positions are evaluated, and how time
and threads are managed. The inventory of disabled techniques comes after all
the active mechanisms, since each entry refers back to the chapter that
explains its context. Tables precede evaluation because correction history
and the pawn cache, which evaluation relies on, are introduced there. The
development guide and glossary are meant to be consulted at any point.

For a first reading, follow architecture -> positions -> search -> move ordering
-> tables -> evaluation. For a heuristic experiment, also read time management
and the disabled-techniques inventory: these mechanisms interact even when their
code lives in different files, and a switched-off branch is often the natural
starting point of an experiment.
The reference chapters distinguish mathematical intent from literal code.
Disabled predicates, commented alternatives and known path differences are
documented explicitly; their existence is not evidence of strength or correctness.

## A search at a glance

```text
GUI / command line
        |
        v
UCI or XBoard -> COM::position + GameInfo + TimeMan parameters
        |
        v
ThreadPool -> per-searcher root and stack
        |
        v
iterative deepening + aspiration + MultiPV
        |
        v
PVS -> TT / draws / evaluation / selective pruning
        |
        v
TT move first -> generate and score remaining moves -> pick / apply
        |
        v
child PVS or qsearch -> bounds / PV / history updates
        |
        v
selected result -> game root score -> bestmove / XBoard move
```

This is an ownership and control-flow sketch, not a claim that every search
visits every stage. TT cutoffs, terminal positions and forward pruning can
return before move generation.

## Vocabulary and units

The [large glossary](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md)
expands this quick vocabulary into terminology families with chapter links,
equation notation and a table of similar names that must not be conflated.

| Term | Meaning in this guide |
|---|---|
| Root | The position at which this search started, not necessarily the game's initial position. |
| Depth | Remaining main-search budget, in plies; reductions and extensions change it. |
| Height | Search-relative ply argument, used for mate distances, killers and selective depth. |
| `halfmoves` | Position's absolute ply index; a standard initial FEN starts at **1**, not 0. |
| PV | Principal variation: the current best line maintained during search. |
| PV node | A node searched with the PV template specialization; usually a full window. |
| Cut node | A search hint that a beta cutoff is expected, not a guarantee. |
| Fail-low / fail-high | A result at or below alpha / at or above beta. |
| Bound | Information about a score: upper, lower or exact within the searched window. |
| Static evaluation | An evaluation without recursively searching the position. |
| Corrected evaluation | Static evaluation plus learned structural correction, with score guards. |
| Search score | A result from PVS/qsearch or a stored search bound; not interchangeable with static evaluation. |
| History | Can mean move-ordering statistics, correction tables or actual game positions. Always specify which. |
| SEE | Static exchange evaluation: an approximation of exchanges on a destination square. |
| HCE | Handcrafted/classical evaluation. |
| NNUE | Incrementally updated neural evaluation, with shared weights and per-position accumulators. |

Scores use Minic's internal scale, exposed as `cp` for ordinary UCI scores.
That does not make constants from another engine directly transferable.
Mate-range values have a separate meaning; move-ordering scores are another
quantity again.

## Status map

| Mechanism | Default desktop configuration |
|---|---|
| PVS, aspiration, LMR/LMP, null move, reverse futility, razoring, ProbCut | Enabled, with node-specific gates. |
| IIR | Enabled; traditional IID is disabled. |
| Singular extensions | Single/double, multi-cut and negative-extension branches exist; some refinements remain commented out. |
| Generic extensions in `depthPolicy()` | All its extension toggles are currently false. |
| Quiet/capture/pawn/continuation histories | Enabled. Continuation distances 1, 2, 4, 6 are supported, but the default weights 8, 0, 0, 0 use only distance 1. |
| Pawn/non-pawn correction | Enabled; optional horizon correction is disabled. |
| Partial move sorting | Enabled; not a capture/quiet staged generator. |
| NNUE | Compiled normally; actually used only after successful network loading. |
| NNUE uncertainty | Optional head disabled; the field exists but does not drive search. |
| Syzygy | Compiled on the usual non-MSVC desktop path; requires loaded tables. |
| MPI | Optional, disabled by default; distinct from local SMP. |
| Upcoming repetition, clustered TT, continuation correction | Roadmap candidates, not current implementations. |

The [disabled techniques](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/disabled-features.md) chapter lists every
mechanism that is present in the code but inactive by default, with its gate.
See [TODO.md](https://github.com/tryingsomestuff/Minic/blob/master/TODO.md) for experiments. A roadmap entry is not evidence
that a feature is enabled or strength-tested.

## Keeping the guide useful

When changing a feature, update the relevant chapter's status, data dependencies,
reset policy and source map. Check both compile-time and runtime gates. Prefer
named functions and equations over fragile line-number references.

The guide is broad, but is not an exhaustive API reference or a concurrency
proof. In particular, it documents existing synchronization limitations instead
of assuming that all shared state is race-free.
