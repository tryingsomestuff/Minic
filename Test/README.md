# Minic pytest entry point

## Running the tests

The suite requires Python with `pytest` and the engine executable at
`Tourney/minic_dev_linux_x64`. From the repository root, build and copy the
engine there, then run all tests:

```sh
NOPROFILE=1 make dist
pytest -q -s Test/test.py
```

To run a single case, select its pytest ID:

```sh
pytest -q -s Test/test.py -k gameinfo_test
```

Each case launches a separate engine process and asserts that its exit code is
zero. Standard output is captured and printed (`-s` makes it visible), but neither
standard output nor standard error is asserted. The CLI smoke tests therefore
check successful execution, not exact scores, move lists, or protocol responses.
The developer commands require a build with `DEBUG_TOOL` enabled, as in the
default configuration.

## Test cases

### `perft_test`

**Engine arguments:** `-perft_test`

Runs the built-in perft comparisons on four positions: the starting position at
depth 5 (4,865,609 nodes), Kiwipete at depth 4 (4,085,603), a rook-and-pawn
endgame at depth 6 (11,030,083), and a promotion/castling position at depth 5
(15,833,292). Exercises move generation and application, including special moves.

### `see_test`

**Engine arguments:** `-see_test`

Runs the built-in static exchange evaluation (SEE) regression suite, checking
whether exchanges meet their specified score thresholds.

### `gameinfo_test`

**Engine arguments:** `-gameinfo_test`

Checks optional root-search scores, UCI history replay and branch
invalidation, XBoard rewind, new-game resets, and booming/moobing thresholds.
Also checks repetition-history reads with no move preceding the initial position,
and that the initial position still counts toward repetition, including threefold
repetition at PV nodes.
Scores belong to searched roots and use the side-to-move perspective; unsearched
positions remain unscored. Only completed search results are recorded, after SMP
result selection, separately from the search stack's static evaluations.

Booming/moobing compares scores two and four plies before the current root.
Missing or mate-range scores disable the heuristic; a rise or fall of at least
80 points triggers it. Rebuilding a history preserves scores only along its
unchanged prefix, including an identical initial FEN.

### `uci_test`

**Engine arguments:** `-uci`

Sends `uci`, `isready`, `go depth 10`, `wait`, and `quit` on standard input.
Exercises UCI initialization, readiness, a search from the starting position,
waiting for completion, and shutdown.

### `spsa_test`

**Engine arguments:** `-spsa`

Exercises exporting tunable engine options in SPSA format. Does not run a tuning
session or validate the exported values.

### `time_test`

**Engine arguments:** `-timeTest 1000 0 20 0`

Exercises the time-management simulation with an initial time of 1,000 ms, no
increment, 20 moves per time control, and no GUI lag.

### `qsearch_test`

**Engine arguments:** `-qsearch start`

Runs quiescence search without pruning from the starting position and reports
its score.

### `eval_test`

**Engine arguments:** `-eval start`

Evaluates the starting position using the active evaluator: NNUE when enabled
and loaded, otherwise handcrafted evaluation.

### `gen_test`

**Engine arguments:** `-gen start`

Generates, scores, sorts, and lists moves from the starting position. Does not
assert the move count or ordering.

### `see_cli_test`

**Engine arguments:** `-see start e2e4 0`

Exercises the individual SEE command, asking whether `e2e4` from the starting
position meets a score threshold of zero. Complements the built-in `see_test`
suite.

### `kpk_test`

**Engine arguments:** `-kpk "8/8/8/8/8/8/4k1P1/6K1 w - - 0 1"`

Probes the built-in king-and-pawn-versus-king evaluator with White's king on g1
and pawn on g2, Black's king on e2, and White to move. Does not assert the
reported score.
