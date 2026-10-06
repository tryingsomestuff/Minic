# Tables, hashes and game history

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md) | [Next: development](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/development.md)

Primary sources: [transposition.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/transposition.hpp),
[transposition.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/transposition.cpp),
[searcher.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcher.hpp), [tables.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/tables.hpp),
[correctionHistory.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/correctionHistory.hpp),
[material.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/material.hpp), [com.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/com.cpp) and
[com.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/com.hpp).

## State inventory

| State | Scope | Purpose |
|---|---|---|
| TT | Shared among local workers | Search bounds, move hints and evaluation information. |
| Pawn-and-king cache | Per worker | Reuse structural HCE work. |
| Move histories | Per worker | Learn promising moves/contexts. |
| Killers and counter moves | Per worker | Short-lived cutoff/refutation hints. |
| Correction tables | Per worker | Learn evaluation residuals. |
| Search stack | Per worker | Position copies, raw eval/hash/threat and NNUE accumulators. |
| `GameInfo` | COM/game | Actual played positions and optional searched-root scores. |
| Material table and KPK | Global initialized knowledge | Material classification, helpers and endgame knowledge. |
| NNUE weights | Shared read-mostly parameters | Inference network. |

None of these "histories" is interchangeable with another. In particular,
`StackData::eval` is not the result of a completed game-root search.

## TT layout and indexing

An entry comprises three 32-bit groups:

```text
fingerprint: 32
score s: 16 | evaluation e: 16
move: 16 | bound/flags/generation: 8 | depth: 8
```

This is normally 12 bytes on the desktop ABI; allocation uses `sizeof(Entry)`,
not a hard-coded assumption. The capacity is the largest power of two fitting
the requested byte budget:

```text
index = fullHash & (capacity - 1)
```

The array is aligned to 1024 bytes. There is **one entry per index**, not a
cluster with multiple candidates.

Every store replaces that entry. Depth, generation and exactness do not select
a victim. A 3-bit generation advances modulo 8 each search and is stored in
flags, but is not currently an aging replacement policy.

`hashFull()` samples nonzero stored fingerprints and reports per mille.
It does not measure the proportion of useful entries from the current
generation.

## Compressed-key validation

The stored fingerprint is XORed with the two payload words. A read copies the
entry and verifies:

```text
storedFingerprint XOR payload1 XOR payload2 == Hash64to32(requestedHash)
```

A stored valid move is also checked for pseudo-legality in the current
position. A mismatch invalidates the returned entry.

`getEntry()` returns true only when the validated entry has enough depth.
A shallower but valid entry can return false while leaving `e.h != nullHash`;
PVS can still use its move/evaluation/context. Distinguish **hit** from
**depth-sufficient cutoff candidate**.

XOR validation helps reject key mismatches/inconsistent snapshots. It neither
eliminates all compressed-key collisions nor makes non-atomic concurrent
access formally race-free.

## Bounds, flags and mate scores

The bound byte contains upper/lower/exact bound bits, TT-PV, gives-check,
in-check and generation flags. Test the relevant bits rather than comparing
the entire byte with an unflagged bound constant.

For mate scores, storage adds search height to winning mates and subtracts it
from losing mates; retrieval reverses that adjustment. This makes the mate
distance reusable from a root with a different height.

TT cutoffs additionally depend on node type, depth, bound direction, mate and
fifty-move conditions. Some late cuts allow slightly shallower entries with
margins. Excluded-move searches use perturbed keys.

`TT::getPV()` follows move hints with legality/repetition safeguards.
The main search maintains its own PV; this helper is not a promise that
every TT chain forms an exact principal variation.

## The evaluation-field caveat

The intended distinction is `s = search result` and `e = uncorrected static
evaluation`. Eval-only writes implement that distinction.

However, several PVS/qsearch cutoff/final writes pass the mutable `evalScore`,
which may already have been replaced by a TT search result. Therefore the
current source does not justify the blanket invariant "every stored `e` is
always the raw evaluator output".

Search separately backs up raw `staticScore`/stack evaluation and avoids
deliberately adding correction to a TT search result. If tightening the TT
invariant, audit every writer and reader together; changing only the header
comment cannot fix the data flow.

## Pawn cache

`Searcher::initPawnTable()` divides the configured total pawn-cache budget by
thread count, then rounds each worker's capacity down to a power of two.
Entries retain a compressed pawn-and-king key, cached structure bitboards,
score and danger. They are direct-mapped and overwritten on a miss.

The default desktop budget is 16 MiB before per-worker rounding; TT defaults
to 128 MiB. Actual allocated memory can be lower after power-of-two rounding.
Small-memory builds interpret size units differently through `SIZE_MULTIPLIER`.

The cache stores HCE features, not search bounds or move history. It does not
need the side-to-move search key because its stored score is white-positive
structural information.

## Move and correction tables

See [move ordering](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/move-ordering.md) for indices, weights and update equations.
The largest move-history structure is `counter_history`: a colored
piece/destination context by colored piece/destination candidate. Multiple CMH
distances use the same storage, so adding pointers is different from multiplying
the table by a distance dimension.

Correction tables have direct-mapped `[side][key & mask]` buckets without
full-key tags. Collisions intentionally blend observations. The active
pawn/non-pawn tables each use 16384 buckets per side; white and black non-pawn
keys address the same non-pawn table. Horizon correction has a separate
disabled gate.

A correction table learns eval error; a move-history table learns cutoff
utility. Their magnitudes cannot be substituted merely because both use
`ScoreType`.

## Material and KPK knowledge

The material index encodes piece counts in mixed bases, with separate
light/dark-square bishop counts. Precomputation supplies material score,
quantized phase, termination classification and optional helper function.
Promotion-heavy positions outside the supported count ranges use fallback
paths rather than indexing the ordinary table blindly.

The KPK bitbase and specialized helpers are separate from the TT. They supply
endgame knowledge determined by a position, not a noisy recent-search cache.
Loaded Syzygy files are another independent source with WDL/DTZ and rule-aware
mapping.

## `GameInfo`: the actual game

The initial position is stored separately; each subsequent `GameStateInfo`
contains the reached position, move that reached it and optional search score.
The initial position has its own optional score.

For a requested absolute ply:

```text
offset = requestedPly - initialPosition.halfmoves
offset 0       -> initial position / initial score
offset 1..size -> states[offset - 1]
```

The inclusive latest-entry bound is important. A zero search score is a valid
observed value; `nullopt` means no recorded score.

### Recording a search result

`setSearchScore(p, score)` requires:

1. A recorded position at `p.halfmoves`.
2. Matching full position hash.
3. Matching `fifty` counter, which the full hash does not encode.

It updates the existing entry; it does not append a searched position.
Failure logs a warning instead of silently associating a score with another
game/root.

The main driver stores the selected final score only for a real, completed,
nonempty-PV result with `postMove && !subSearch`. Publication occurs after helper
stop/wait and before move application. This is the root's search score, not a
search of the position after the chosen move.

Skill move selection can choose a move other than the first PV while the
recorded score still represents the driver's selected search result; do not
generalize it into a freshly evaluated score for every played move.

### Rebuilding, branching and undo

UCI reconstructs game moves on `position`; XBoard uses replay for undo/remove.
`replace(updated)` first compares the initial FENs. If they match, it carries
over the initial score and subsequent scores until the first move, hash or
fifty-counter mismatch.

This is prefix preservation, not a global "find any identical position and
reuse its score" cache. An abandoned branch's future scores are discarded.
A supplied midgame FEN establishes a new initial history; positions before
it remain unknown.

Starting a new game clears optional scores together with moves. The tests in
[cli_gameInfoTest.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/cli_gameInfoTest.cpp) cover zero/missing
scores, replay, divergence, FEN origins, rewind, fifty mismatch and reset.

### Synchronization scope

The game-history mutex protects `clear`, `append`, `replace`,
`setSearchScore` and `getSearchScore`. Legacy position/hash/move getters and
public members are not independently synchronized. A private temporary
`GameInfo` and the live singleton use the same mutex.

The existing API therefore provides protected score operations, not universal
thread-safe access to arbitrary history mutations. Position copies in history
also retain non-owning root/evaluator pointers; do not use a historical copy as
a fresh incremental NNUE position without proper association/lifetime handling.

## Reset policy

| Operation | Main effects |
|---|---|
| Worker `clearGame()` | Pawn cache, statistics, killers, counters, move/correction histories, previous best and stack reset. |
| Pool `clearGame()` | Shared TT clear plus every worker's game reset. |
| Normal `clearSearch()` | Statistics, killers, counters and previous best reset; learned histories/correction normally retained. |
| Forced history-clear search | Also clears move/correction histories. |
| `REPRODUCTIBLE_RESULTS` build | Clears extra search state, including TT/pawn cache/history; disabled normally. |
| Root stack initialization | Clears only pre-root slots and initializes the root position/evaluator. |
| New game | Resets protocol position and game history in addition to worker game state. |

Changing reset frequency can change search behavior/strength without changing
an individual heuristic formula. Do not treat a stateless benchmark and a
long game as automatically equivalent.

## Capacity rounding example

With a 12-byte TT entry and requested 128 MiB:

```text
raw capacity = floor(134217728 / 12) = 11184810
rounded capacity = 8388608
payload bytes = 8388608 * 12 = 100663296 = 96 MiB
```

Consequently configured megabytes are a budget, not a promise of exactly that
many allocated payload bytes. `sizeof(Entry)` and the power-of-two policy
determine the actual value; bucket masks rely on that rounding.

The material table has ten ternary counts (white/black knight, light bishop,
dark bishop, rook and queen) and two nine-valued pawn counts:

```text
TotalMat = 3^10 * 9^2 = 4782969
```

It is a deterministic mixed-radix index, not a Zobrist collision cache.
Unrepresentable promoted material takes the fallback path. The parallel
helper-pointer table also consumes memory; do not estimate startup footprint
using only the score-entry array.

## Correction-history gravity example

Correction update differs from move-history gravity:

```text
w = min(depth, 16)
change = clamp((bestScore - baselineEval) * w / 16, -maximum, maximum)
item += change - item*abs(change)/maximum
```

For a pawn-correction bucket at depth 8, residual +20 and old item +8,
`change = 10`, damping is `80/32 = 2` and the new item is 16. With residual
-20, damping remains 2 and the new item becomes -4. These integer divisions
truncate; they are not the signed right shifts used by move-history damping.

The two non-pawn-key queries reuse one table. Updating both keys does not
create two independent learned representations; an alias can receive two
updates. Optional horizon correction uses a separate context key and a
depth ramp, but is disabled in the normal configuration.

## Cache validity versus feature dependency

A correct compressed key says an entry matches the chosen key domain. It
does not prove every stored value depends only on that domain. The passer-race
dependency discussed in the
[classical evaluation reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/classical-evaluation-reference.md)
is an example. Similarly, TT `e` provenance depends on the writer, while
GameInfo scores require actual root membership beyond a hash match.

## Memory considerations

In addition to shared TT/network/material memory, every worker has a stack,
accumulators and histories. The raw two 384-element `int16_t` accumulators over
2048 slots alone occupy roughly 3 MiB before padding and other stack fields.
The continuation table adds over 1 MiB per worker.

The two quantized input matrices occupy about 72 MiB before biases/dense heads;
the serialized float matrices are about twice that. Increasing threads does
not duplicate those shared weights, but does duplicate local search structures.
Use startup size logs and actual process measurements for a specific build.
