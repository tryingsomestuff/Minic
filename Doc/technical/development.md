# Building, diagnosing and modifying Minic

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md)

Primary sources: [Makefile](https://github.com/tryingsomestuff/Minic/blob/master/Makefile),
[Tools/build/common](https://github.com/tryingsomestuff/Minic/blob/master/Tools/build/common),
[Tools/build/build.sh](https://github.com/tryingsomestuff/Minic/blob/master/Tools/build/build.sh),
[Test/test.py](https://github.com/tryingsomestuff/Minic/blob/master/Test/test.py), [Test/README.md](https://github.com/tryingsomestuff/Minic/blob/master/Test/README.md),
[cli.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/cli.cpp), [option.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/option.cpp),
[stats.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/stats.hpp) and [TODO.md](https://github.com/tryingsomestuff/Minic/blob/master/TODO.md).

## Normal build

From the repository root:

```sh
make
```

This is the normal C++20 release-style build. It checks/downloads the expected
network into `Tourney/nn.bin` as needed, initializes/builds Fathom and invokes
the engine build script. The default script uses PGO: an instrumented build,
training run and optimized build. Output normally lands at:

```text
Dist/Minic3/minic_dev_linux_x64
```

For iteration without PGO:

```sh
NOPROFILE=1 make
```

Keep the same compiler/ISA/network/PGO conditions when comparing performance.
The native target is machine-specific; a binary built on one CPU is not
automatically portable to an older one.

The build gathers `Source/*.cpp`, `Source/nnue/*.cpp` and
`Source/nnue/learn/*.cpp`. Header-only templates such as PVS do not need an
independent source-list entry.

## Isolated outputs and network overrides

Avoid replacing an executable currently used by a tournament. For example:

```sh
NOPROFILE=1 EXE=Dist/Minic3/minic_experiment make
```

`EXE` becomes `FORCEDNAME`; supply a repository-relative output path with an
existing parent. Forced-name builds also disable FP contraction in the current
script, which is relevant to exact comparisons.

`EVALFILE=/absolute/path/to/network.bin` selects another embedded network;
`EVALFILE=none` disables embedding, not the compiled NNUE code:

```sh
NOPROFILE=1 EVALFILE=none EXE=Dist/Minic3/minic_noembed make
```

The `config` target still manages `Tourney/nn.bin`; `EVALFILE=none` is not a
promise that `make` requires no network/Fathom setup.
Runtime network loading is a separate option.

The CMake file in [Tools/build](https://github.com/tryingsomestuff/Minic/blob/master/Tools/build/CMakeLists.txt) is an
Include-What-You-Use harness, not the normal engine build system. Platform,
coverage and sanitizer scripts are specialized workflows: read their
assumptions before running them.

## Tests use the tournament copy

The pytest harness uses:

```text
Tourney/minic_dev_linux_x64
```

Build/copy a current default binary before running it:

```sh
NOPROFILE=1 make dist
pytest -q -s Test/test.py
```

`make dist` itself depends on the build and copies the default-named executable.
Do not assume it installs an `EXE` experiment under the pytest name.

Use a selector for a focused change:

```sh
pytest -q -s Test/test.py -k 'perft_test or see_test'
pytest -q -s Test/test.py -k 'gameinfo_test or uci_test'
```

The suite is a smoke/regression harness, not an Elo test. Inspect its
assertions: it does not establish correctness of every transient UCI message
or every possible search line.

## Focused engine diagnostics

With `DEBUG_TOOL`, useful direct commands include:

```sh
Dist/Minic3/minic_dev_linux_x64 -perft_test
Dist/Minic3/minic_dev_linux_x64 -see_test
Dist/Minic3/minic_dev_linux_x64 -gameinfo_test
Dist/Minic3/minic_dev_linux_x64 -eval start
Dist/Minic3/minic_dev_linux_x64 -qsearch start
```

Other CLI operations support perft/divide, fixed-depth analysis, SEE
threshold checks, KPK and time simulation. Read dispatch and the relevant
`cli*.cpp` file for their exact argument order; protocol and developer-helper
searches do not all have the same semantics.

For asynchronous UCI integration, drive the process interactively and wait
for `bestmove` before sending `quit`. A rapidly piped
`go ... / wait / quit` sequence is not a general synchronization contract.
Exercise multiple threads, MultiPV, stop/ponder, root histories and legal
bestmove/PV application as appropriate.

## Diagnostics and configuration

`WITH_STATS` is normally enabled and records main/qnodes, TT/cache hits,
cutoffs and heuristic activity. Statistics help distinguish a missing gate
from a merely unhelpful heuristic.

Relevant opt-in gates include:

| Gate | Diagnostic use |
|---|---|
| `WITH_TIMER` | Timing categories for engine work. |
| `WITH_BETACUTSTATS` | More detailed cutoff-source statistics. |
| `VERBOSE_EVAL` | HCE feature displays. |
| `DEBUG_HASH`, `DEBUG_PHASH`, `DEBUG_NONPAWNHASH` | Incremental key consistency. |
| `DEBUG_MATERIAL`, `DEBUG_BITBOARD` | Representation consistency. |
| `DEBUG_NNUE_UPDATE` | Incremental evaluator checks. |
| `DEBUG_APPLY`, `DEBUG_GENERATION`, `DEBUG_PSEUDO_LEGAL` | Move-path assertions/diagnostics. |
| `DEBUG_FIFTY_COLLISION` | Repetition hash comparison diagnostics. |
| `REPRODUCTIBLE_RESULTS` | Extra state clearing between searches. |

Some debug gates are mutually incompatible or require assertions. The current
`DEBUGMINIC` script path still includes `-DNDEBUG`; debug symbols alone do not
enable `assert`. Check actual compiler flags when expecting invariant failures.

There is no separate repository lint command. Format changed C++ with the
repository's clang-format configuration and preserve surrounding style.
Documentation-only edits do not require rebuilding the engine.

### Selected runtime defaults

These identifiers refer to [dynamicConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/dynamicConfig.cpp);
network initialization and option callbacks can change the active values.

| Setting | Initial desktop value |
|---|---|
| `threads`, `multiPV` | 1, 1. |
| `ttSizeMb`, `ttPawnSizeMb` | 128, 16; capacities are rounded and pawn budget is divided among workers. |
| `contempt`, `contemptMG` | 14, 9; the driver combines them for MG. |
| `moveOverHead` | 50 ms minimum timing margin. |
| `badCapLimit` | -80 for capture ordering. |
| `NNUEScaling` | 48, used as a multiplier divided by 64. |
| First NNUE thresholds MG/EG | 638 / 638. |
| Second NNUE thresholds MG/EG | 344 / 315. |
| `forceNNUE`, `limitStrength`, `withWDL` | False. |

`useNNUE` begins false and becomes true after successful loading. It is not
equivalent to the build gate or the availability of an embedded filename.
For the advertised protocol option names, ranges and callbacks, consult
the option registry rather than assuming they match these identifiers exactly.

### Coverage and assertions

CI's coverage path uses [Tools/build/docov](https://github.com/tryingsomestuff/Minic/blob/master/Tools/build/docov). It builds
its own executable and exercises analysis, move/state diagnostics and protocol
workloads; it does not simply reuse the pytest tournament copy.

Coverage checks that a path was exercised, not that its heuristic is beneficial.
For invariants, also use the corresponding debug checks with assertions enabled
and focused special-move/stack tests.

## Change-to-evidence matrix

| Change | Correctness evidence | Performance/strength evidence |
|---|---|---|
| Board/hash transition | Perft plus recomputed mailbox/bitboard/material/hash checks on special moves and FRC overlap cases. | Copy/update cost and matched games if search behavior changes. |
| NNUE feature/update | Reset versus incremental values for quiet/capture/EP/promotion/king/castle; exported versus trainer output. | Propagation/update timings, cache behavior and same network/options in matches. |
| LMR/extension/pruning | Exact child depths, predicates, retry windows, excluded-move behavior and narrow-type limits. | Candidate/prune/retry histograms, fixed-node results, then real-control matches. |
| Ordering/history | Score components, packed identity, picking order, available CMH contexts and update/reset behavior. | First-move cutoff rate plus downstream LMR/pruning effects. |
| TT format/policy | All readers/writers, bound/depth validation, mate coordinates, transport and collisions. | Actual capacity, hit usefulness and multiworker scaling. |
| Across-move timing | Missing scores, unchanged replay, branch replacement, undo, new game and mate-score filtering. | Root histories plus used/remaining clock over complete games. |

No single smoke test proves all columns. Perft does not exercise evaluation;
an eval comparison does not prove legality; successful protocol parsing does
not show that an auxiliary search was excluded from live-game score history.

## Designing a diagnostic for a disabled branch

First establish which gate prevents execution: compile-time feature, local
constexpr switch, runtime option, node type, depth, or producer signal.
Then assert its exact intended consequence, such as next depth and retry
count, on a deliberately qualifying position.

A disabled emergency modifier with a producer fixed false needs a producer
experiment too. An advanced-pawn modifier behind an advanced-pawn exclusion
needs an eligibility decision. Simply flipping every named switch obscures
these distinctions.

For numerical NNUE checks, include both buckets, both sides to move, feature
orientation, export order, quantization range, scalar/SIMD differences and
king-frame resets. This is a compatibility experiment before it is an Elo
experiment.

## Where a change belongs

| Change | Main surfaces to inspect |
|---|---|
| Runtime option | `dynamicConfig.*`, option registry/callback, protocol readiness/search safety. |
| Search tunable | `searchConfig.*`, consumers and optional search-tuning registration. |
| Static evaluation term | `evalConfig.*`, `evalDef.hpp`, evaluation feature/cache dependencies. |
| Move/state transition | Definition, generation, pseudo-legality, application, hashes, material, NNUE and protocols. |
| Search-stack use | Root/child/evaluator helpers, absolute-ply guards and same-position probes. |
| Ordering/history | Scorer, picker, update callers, LMR/pruning use and reset policy. |
| TT field/replacement | Every writer/reader, mate conversion, validation and SMP/MPI transport. |
| Game-score timing | Protocol replay, `GameInfo`, result selection/publication and TimeMan. |
| Network architecture | C++ features/layers/loader, Python features/model/export and format version. |

Do not put a runtime setting in `config.hpp`, or scatter a tunable margin across
multiple source files. Conversely, a compiled-out feature cannot be enabled
by merely adding a UCI option.

## Correctness, speed and strength are separate

**Correctness**: legal state transitions and lines, stable invariants, proper
stop/error handling and score/history semantics.

**Performance**: time per operation, nodes per second, memory traffic,
allocation overhead and SMP scaling.

**Strength**: quality of selected moves under the target limits, measured with
controlled games or suitable statistical tests.

A stack-safety repair can be Elo-neutral; fewer nodes can be a bad pruning
change; lower NPS can be a better tree. Do not use one metric as proof of the
other.

For search experiments, compare fixed-node/fixed-depth results first, then
matched real time controls with the same network, options, hardware and
opening policy. Preserve baseline binaries and record thread/hash/pondering
settings. Separate correctness regressions from measured Elo results.

## Common failure modes

| Symptom | First things to check |
|---|---|
| New branch never runs | Compile-time gate, runtime gate, node conditions and stats. |
| Illegal bestmove/PV | Child copy order, post-apply legality, castling conversion, TT hint validation. |
| Incremental eval drift | Evaluator association, deferred updates, king-frame rebuild, promotion/EP/castling. |
| History unexpectedly missing | Initial FEN, prefix divergence, new-game reset, optional score presence and publication gate. |
| Timing differs but fixed nodes do not | Clock parsing, adjusted budget, variability, history and overhead. |
| TT result looks inconsistent | Bound bits, depth sufficiency, mate adjustment, compressed-key validation and `e` field caveat. |
| Test exercises an old binary | `Dist` versus `Tourney`, custom `EXE`, copy/build timestamps. |
| Debug assertion does not fire | `-DNDEBUG`, gate definition and the actually selected build script. |

## Safe change checklist

Before publishing a change, inspect existing patterns and all dependent state,
use the smallest relevant diagnostics, then broaden coverage where the change
touches shared invariants. Keep unrelated fixes out of the experiment.

Update this guide when changing enabled heuristics, equations, state ownership,
reset behavior or file compatibility. Update TODO separately for unimplemented
ideas; do not silently turn an experimental proposal into a description of the
current engine.
