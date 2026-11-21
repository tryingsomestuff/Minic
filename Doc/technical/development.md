# Building, diagnosing and modifying Minic

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md) | [Previous: disabled techniques](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/disabled-features.md) | [Next: glossary](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md)

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

Every build identifies itself: the engine reports its base version, the
six-character Git commit and a `-dirty` suffix when the checkout has local
changes (`3.47-af45d8-dirty`, for instance). The build script writes this
information into a generated `minic_build_info.h` that it force-includes, so
the string reflects the checkout at build time, not at run time. Check it in the
`uci` output before trusting a comparison between two executables.

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

### Build variables

The [Makefile](https://github.com/tryingsomestuff/Minic/blob/master/Makefile) and [Tools/build/build.sh](https://github.com/tryingsomestuff/Minic/blob/master/Tools/build/build.sh)
read these environment variables:

| Variable | Effect |
|---|---|
| `NOPROFILE=1` | Single optimized compilation, no PGO training run. |
| `EXE=<path>` | Output path relative to the repository root (exported as `FORCEDNAME`); also adds `-ffp-contract=off`. |
| `EVALFILE=<path>` | Embed this network instead of `Tourney/nn.bin`, and skip the network checksum check. `EVALFILE=none` builds without an embedded network. |
| `DEBUGMINIC=1` | `-O3 -g -ggdb -fno-omit-frame-pointer`, still with `-DNDEBUG`, so assertions remain off. |
| `VTUNEMINIC=1` | Release flags plus `-g` (symbols for profilers) instead of stripping. |
| `CXX`, `CC` | Compilers; default `g++`/`gcc`. The build switches to Intel MPI compilers by itself when `WITH_MPI` is uncommented. |

Without `EVALFILE`, `make config` downloads the default network into
`Tourney/nn.bin` when it is missing or when its MD5 checksum is not the
expected one. The PGO training run is `bench 20` (`bench 10` in a `DEBUGMINIC`
build). `build.sh` also compiles the Fathom tablebase prober when
`Fathom/src/tbprobe.h` exists and links it into the engine.

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
pytest -q -s Test/test.py -k pawnhistory_test
```

The cases are `perft_test`, `see_test`, `gameinfo_test`, `pawnhistory_test`,
`uci_test`, `spsa_test`, `time_test`, `qsearch_test`, `eval_test`, `gen_test`,
`see_cli_test` and `kpk_test`.
Each case starts a separate engine process and asserts its exit code. The
[test README](https://github.com/tryingsomestuff/Minic/blob/master/Test/README.md) describes every case. The developer
commands behind the two most specific ones are:

| Test | What it checks |
|---|---|
| `gameinfo_test` | Searched-root scores, UCI replay and branch invalidation, XBoard rewind, new-game reset, booming/moobing thresholds and repetition detection that reads the game history. |
| `pawnhistory_test` | Bounded history update, accessor mapping, pawn-history learning and reset, and its contribution to quiet-move ordering. |

The suite is a smoke/regression harness, not an Elo test. Inspect its
assertions: it does not establish correctness of every transient UCI message
or every possible search line.

## Focused engine diagnostics

The default configuration defines `DEBUG_TOOL`, which makes developer commands
available as the first command-line argument. Commands that need a position
take a full FEN in one quoted argument, or one of the shortcuts `start`,
`fine70`, `shirov`, `shirov2` and `mate4`. The dispatch is in `cliManagement()`
in [cli.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/cli.cpp):

| Command | Arguments (defaults) | Purpose |
|---|---|---|
| `-perft_test`, `-perft_test_long`, `-perft_test_long_fischer` | none | Built-in perft regression positions, standard and Chess960. |
| `-perft` | `<fen> [depth=5]` | Count leaf nodes of the legal move tree; also reports speed. |
| `-see_test` | none | SEE regression suite. |
| `-see` | `<fen> <move> <threshold>` | Whether `SEE_GE(move, threshold)` holds. The threshold argument is required. |
| `-gameinfo_test`, `-pawnhistory_test` | none | Game-history and pawn-history unit checks. |
| `bench`, `-bench`, `benchBig` | `[depth=16]` | Fixed-depth search over a position set; total nodes identify search behavior. |
| `-analyze` | `<fen> [depth=15]` | Fixed-depth search with normal output. |
| `-mateFinder` | `<fen> [depth=10]` | Same with the mate-finder mode, which disables selective pruning. |
| `-qsearch` | `<fen>` | `qsearchNoPruning()` score, not the production quiescence search. |
| `-eval`, `-evalHCE` | `<fen>` | Active evaluator (NNUE when loaded) or handcrafted evaluation, with game phase. |
| `-evalSpeed` | `[epd file]` | Evaluation throughput, TT disabled. |
| `-gen` | `<fen>` | Generate, score and list the moves. |
| `-testmove` | `<fen>` | Apply the hard-coded move from square 8 to 16 (a2a3) and print the result. |
| `-attacked`, `-cov` | `<fen> <square index>` | Attack bitboard on a square, or coverage of the piece standing on it. |
| `-kpk`, `-probe` | `<fen>` | KPK bitbase or Syzygy probe. |
| `-timeTest` | `[initial ms=50000] [increment=0] [moves in TC=-1] [GUI lag=0]` | Simulate time allocation over a game. |
| `-selfplay` | `[depth=15] [games=1]` | Self-play games; with `-genFen 1` they also write training positions. |
| `-spsa` | none | Print numeric options in SPSA input format. |
| `-uci`, `-xboard` | none | Protocol loops; no argument at all also means UCI. |

For example:

```sh
Dist/Minic3/minic_dev_linux_x64 -perft start 6
Dist/Minic3/minic_dev_linux_x64 -analyze "r1bqkb1r/pp3ppp/1np1pn2/6N1/2BP4/8/PPP1QPPP/R1B1K1NR w KQkq - 1 1" 20
Dist/Minic3/minic_dev_linux_x64 -see start e2e4 0
Dist/Minic3/minic_dev_linux_x64 -timeTest 60000 500 -1 0
```

A FEN argument is checked against a regular expression before `readFEN()`;
a malformed one is reported and the command does nothing. Protocol and
developer-helper searches do not all have the same semantics: `-qsearch` in
particular is not the search path used in games.

For asynchronous UCI integration, drive the process interactively and wait
for `bestmove` before sending `quit`. A rapidly piped
`go ... / wait / quit` sequence is not a general synchronization contract.
Exercise multiple threads, MultiPV, stop/ponder, root histories and legal
bestmove/PV application as appropriate.

## Runtime options and their registration

A runtime option is a `DynamicConfig` global, declared in
[dynamicConfig.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/dynamicConfig.hpp), given its default in
[dynamicConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/dynamicConfig.cpp), and registered in
`registerCOMOptions()` in [option.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/option.cpp) with a protocol
name, a type, a widget, a range and an optional callback:

```cpp
   _keys.emplace_back(k_int,   w_spin,  "Hash"                        , &DynamicConfig::ttSizeMb                       , (unsigned int)1  , (unsigned int)256000                , &TT::initTable);
   _keys.emplace_back(k_int,   w_spin,  "PawnHash"                    , &DynamicConfig::ttPawnSizeMb                   , (unsigned int)1  , (unsigned int)4096                  , &ThreadPool::initPawnTables);
   _keys.emplace_back(k_int,   w_spin,  "Threads"                     , &DynamicConfig::threads                        , (unsigned int)1  , (unsigned int)(MAX_THREADS-1)       , std::bind(&ThreadPool::setup, &ThreadPool::instance()));
```

The registry produces both the UCI `option name ...` list and the XBoard
`feature option=...` list, so one registration serves both protocols. The
callback runs after a successful change; it is what makes `Hash` reallocate the
TT and `Threads` recreate the workers.

`Options::SetValue()` validates what it receives. A boolean must parse
completely (`true`/`false`, with `0`/`1` mapped), an integer must parse
completely and lie inside the range registered for the option, and an
unsigned value cannot be negative. A rejected value is logged as a warning and
the option keeps its previous value, so a typo in a GUI configuration or a
tuning script does not silently install an out-of-range parameter:

```cpp
      if (parsedValue < keyRef.vmin || parsedValue > keyRef.vmax) {
         Logging::LogIt(Logging::logWarn) << "Rejected out-of-range option " << key << "=" << parsedValue
                                          << " (expected " << keyRef.vmin << ".." << keyRef.vmax << ")";
         return false;
      }
```

Any change is also refused while the main worker is searching.

The same globals can be set on the command line with `-<identifier> <value>`,
using the `DynamicConfig` identifier rather than the protocol name, for
instance `-ttSizeMb 256 -threads 4`. `initOptions()` reads them with `GETOPT`
before any table is allocated:

```cpp
#define GETOPT(name, type)              Options::getOption<type>(DynamicConfig::name, #name);
```

`getOption()` extracts the value with `operator>>` and does **not** apply the
registered range: command-line values bypass the protocol validation, and a
boolean must be written `1` or `0` there. A few globals are only reachable
this way, for example `-disableTT`, `-debugMode` and `-minOutputLevel`.

To add a runtime option: declare and define the global, register it with the
type matching the C++ variable (`k_bool`, `k_depth`, `k_int`, `k_score`,
`k_ull`, `k_string`), add a callback if a structure must be rebuilt, add a
`GETOPT` line if it is useful on the command line, and read the global where
it is used.

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
| `forceNNUE`, `limitStrength`, `withWDL`, `mateFinder`, `disableTT` | False. |

`useNNUE` begins false and becomes true after successful loading. It is not
equivalent to the build gate or the availability of an embedded filename.
For the advertised protocol option names, ranges and callbacks, consult
the option registry rather than assuming they match these identifiers exactly.
`MateFinder` is registered as a UCI option, but every `go` resets it and
`go mate <n>` sets it again; in practice it is controlled by the `go` command,
or by `-mateFinder` on the command line.

## Tuning builds

Three gates, all **disabled by default**, turn compiled constants into
adjustable parameters. See also
[disabled techniques](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/disabled-features.md).

`WITH_SEARCH_TUNING` makes the `CONST_SEARCH_TUNING` parameters of
[searchConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.cpp) mutable and registers them as
UCI spin options (aspiration, null move, pruning coefficients, ordering
bonuses, history divisors, `CMHWeight0` to `CMHWeight3`, correction-history
limits...). Their registered ranges are checked like any other option.
`-spsa` then prints one line per numeric option, in the
`name, int, value, min, max, step, learning rate` format used by SPSA tuners:
the bounds are about plus or minus 50 % of the current value, clipped to the
registered range. [Tools/fit/sprt_tune.py](https://github.com/tryingsomestuff/Minic/blob/master/Tools/fit/sprt_tune.py)
discovers these options over UCI and tunes them one at a time with
cutechess-cli SPRT matches; [Tools/fit/ax_tune.py](https://github.com/tryingsomestuff/Minic/blob/master/Tools/fit/ax_tune.py) is
a Bayesian-optimization alternative.

`WITH_EVAL_TUNING` makes evaluation terms mutable and adds the
`-tuning <epd file>` command, a Texel-style optimizer that fits handcrafted
evaluation parameters to game results stored in an EPD file.
`WITH_PIECE_TUNING` additionally exposes piece values. These builds are slower
and are not meant for play.

## Debug gates

`WITH_STATS` is normally enabled and records main/qnodes, TT/cache hits,
cutoffs and heuristic activity. Statistics help distinguish a missing gate
from a merely unhelpful heuristic.

The following opt-in gates are commented out in
[config.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/config.hpp); enable one by uncommenting it and
rebuilding:

| Gate | Diagnostic use |
|---|---|
| `WITH_TIMER` | Timing categories for engine work. |
| `WITH_BETACUTSTATS` | Cutoff-source statistics, printed after `bench`. |
| `VERBOSE_EVAL` | HCE feature displays. |
| `DEBUG_HASH`, `DEBUG_PHASH`, `DEBUG_NONPAWNHASH` | Recompute keys and compare them with the incremental ones. |
| `DEBUG_MATERIAL`, `DEBUG_BITBOARD` | Representation consistency. |
| `DEBUG_NNUE_UPDATE` | Compare incremental and freshly rebuilt accumulators. |
| `DEBUG_APPLY`, `DEBUG_GENERATION`, `DEBUG_GENERATION_LEGAL`, `DEBUG_PSEUDO_LEGAL` | Move-path checks. The last two are mutually incompatible. |
| `DEBUG_STATICEVAL`, `DEBUG_EVALSYM` | Check evaluation shortcuts and color symmetry. |
| `DEBUG_HASH_ENTRY`, `DEBUG_TT_CHECK`, `DEBUG_KING_CAP`, `DEBUG_PERFT`, `DEBUG_BACKTRACE` | TT payload corruption tests, king-capture detection, perft and backtrace aids. |
| `DEBUG_FIFTY_COLLISION` | Report repetition hash matches between different positions. |
| `REPRODUCTIBLE_RESULTS` | Extra state clearing between searches. |

Some checks use `assert`. Every build script, including the `DEBUGMINIC` and
sanitizer paths, compiles with `-DNDEBUG`, so debug symbols alone do not
enable assertions: remove `-DNDEBUG` from the flags when an invariant failure
is expected.

There is no separate repository lint command. Format changed C++ with the
repository's clang-format configuration (Google-based, 3-space indentation,
150 columns) and preserve surrounding style.

## Continuous integration and coverage

The GitHub Actions workflow runs on pushes and pull requests to `master`. It
builds the engine with `make`, then runs
[Tools/build/docov](https://github.com/tryingsomestuff/Minic/blob/master/Tools/build/docov). That script builds
its own executable with GCC coverage instrumentation (`--coverage`, `-O3`,
`DEBUG_TOOL`), runs a fixed list of workloads (analysis, perft, `bench`,
self-play, the SEE and perft regression commands, time simulation, evaluation,
move generation, KPK, a scripted UCI session and a scripted XBoard session),
then produces an HTML report with `lcov`/`genhtml`. It does not reuse the
pytest tournament copy. Useful variables:

| Variable | Effect |
|---|---|
| `COV_PROFILE=full` | Also runs `benchBig` and the long perft suites. |
| `COV_FAIL_ON_ERROR=1` | Exit with an error if any step failed (otherwise failures are only counted). |
| `SYZYGY_PATH=<dir>` | Adds tablebase analysis and probing when the directory exists. |

On `master`, the report is published on the project's GitHub Pages site.

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
