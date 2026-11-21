# Time management and parallel search

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md) | [Previous: evaluation](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/evaluation.md) | [Next: disabled techniques](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/disabled-features.md)

Primary sources: [timeMan.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/timeMan.cpp),
[timeMan.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/timeMan.hpp), [searcher.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcher.cpp),
[searcherDriver.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherDriver.cpp),
[searcherPVS.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherPVS.hpp),
[threading.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/threading.cpp),
[com.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/com.cpp),
[uci.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/uci.cpp),
[xboard.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/xboard.cpp),
[skill.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/skill.hpp),
[definition.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/definition.hpp) and
[distributed.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/distributed.cpp).

## Why an engine needs time management

A chess engine never "finishes" a search: deeper is almost always better, so the
question is how much of the clock to spend on the current move. Spending too
little wastes the advantage of thinking; spending too much risks losing on time
or leaving nothing for later critical positions. Engines therefore split the
problem in two:

- an *initial allocation*, computed once from the clock, increment and
  expected number of remaining moves;
- *in-search adjustments*, which spend more when the search looks unstable
  (the best score drops, the best move keeps changing) and less when the
  answer is obvious (only one legal move).

Minic follows this design with two layers. `TimeMan::getNextMSecPerMove()`
converts protocol clock information into an initial allocation;
`COM::thinkAsync()` stores it in `ThreadPool::currentMoveMs`, and
`ThreadPool::distributeData()` copies it into each worker's `currentMoveMs`.
During search, callers never read `currentMoveMs` directly but call
`Searcher::getCurrentMoveMs()`, which applies difficulty, game-score evolution,
variability and caps.

### Soft and hard limits

A *soft* limit decides whether a new iteration is worth starting: if the next
depth is unlikely to complete in time, starting it wastes time because an
unfinished iteration is usually discarded. A *hard* limit aborts a search that
is already running, to protect the clock. Minic compares both against the same
adjusted allocation `getCurrentMoveMs()` (itself bounded by `TimeMan::maxTime`),
at two places:

| Test | Where | Comparison |
|---|---|---|
| Soft (forecast) | Search driver, after each completed depth of the main worker (first MultiPV line only), when `TimeMan::isDynamic` | `elapsed * 1.2 * EBF > allowed` stops iterative deepening. |
| Hard (abort) | `timeCheck()` at PVS entry, sampled every 1024 calls | `elapsed > allowed` raises the stop flag, discarding the unfinished iteration. |

`maxTime` is a cap on the adjusted allocation, not a separate deadline, and a
final 20 ms floor can exceed it (see below).

## Protocol inputs

UCI `go` first resets `isDynamic`, `nbMoveInTC`, `msecPerMove`, `msecInTC`,
`msecInc`, `msecUntilNextTC` and `moveToGo` to "unset" (`false`/`-1`), then
parses:

| UCI parameter | Effect |
|---|---|
| `wtime` / `btime` | Only the side to move's value is kept, in `msecUntilNextTC`; sets `isDynamic`. |
| `winc` / `binc` | Only the side to move's value is kept, in `msecInc`; sets `isDynamic`. |
| `movestogo` | `moveToGo`; sets `isDynamic`. |
| `movetime` | `msecPerMove`. |
| `nodes` | `TimeMan::maxNodes`. |
| `depth`, `mate` | Depth limit (`mate` also enables `mateFinder`); `msecPerMove = INFINITETIME`. |
| `infinite` | Analysis state, `msecPerMove = INFINITETIME`. |
| `ponder` | Pondering state. |
| none of `infinite`, `depth`, `movetime`, `nodes`, `wtime`, `btime`, `movestogo` | Depth 10 search with infinite time (`winc`, `binc`, `ponder` and `mate` do not count as parameters here, so a bare `go mate N` also becomes a depth 10 search). |

`TimeMan::maxNodes` is not part of the reset list, so a node limit given by
one `go nodes` remains in force for later `go` commands until another value is
assigned. `TimeMan::overHead` is the time spent processing the last UCI
`position` command; the XBoard path never assigns it, so it keeps its initial
value 0 there.

XBoard sets the same variables: `level` stores a moves-per-session control
(`nbMoveInTC`, `msecInTC`, `msecInc`); `time` stores the remaining clock in
`msecUntilNextTC` and sets `isDynamic`; `st` gives a fixed move time and `sd`
a fixed depth with infinite time. `level` also writes its raw minutes field
into `msecUntilNextTC` as a placeholder, so the static repeating branch below
is only meaningful once `time` has arrived: before that, `maxTime` is
usually negative and `getCurrentMoveMs()` falls back to its 20 ms floor.
GUIs normally send `time` before every engine move, so this matters only for
hand-driven sessions.

The `MinMoveOverHead` option (`DynamicConfig::moveOverHead`, default 50 ms,
range 10 to 1000 ms) is the minimum safety margin. Out-of-range values are
rejected with a warning and leave the previous value in place.

## Initial allocation

`getNextMSecPerMove()` tests the controls in this order:

| Control | Initial behavior |
|---|---|
| Fixed nodes (`maxNodes > 0`) | `targetTime = maxTime = INFINITETIME` (30 days); node counting drives stopping. |
| Fixed move time (`msecPerMove > 0`) | `targetTime = maxTime = msecPerMove`, returned immediately (no overhead subtraction). This branch also covers `depth`, `mate`, `infinite` and `sd`, which use `INFINITETIME`. |
| Repeating control (`nbMoveInTC > 0`, XBoard `level` with moves) | Static: `(msecInTC + nbMoveInTC*inc - moveOverHead) / nbMoveInTC`. Dynamic (after `time`): `(remaining + k*inc - margin) / k` with `k = nbMoveInTC - ((p.moves - 1) % nbMoveInTC)` moves until the next control. |
| UCI moves-to-go (`moveToGo > 0`) | `(remaining + moveToGo*inc - margin) / moveToGo`, times the pondering correction. |
| Sudden death (otherwise) | Divide by an estimated move horizon and add an increment bonus, times the pondering correction (below). |

The dynamic margin is 1% of the remaining time, clamped between
`moveOverHead` and 3000 ms:

```cpp
auto getMargin = [&](TimeType remainingTime){
   return std::max(std::min(msecMarginMax, static_cast<TimeType>(msecMarginCoef * static_cast<float>(remainingTime))), msecMarginMin);
};
```

### Sudden death

With only a remaining clock, the engine acts "as if" a fixed number of moves
were left. An increment below `msecMinimal = 20` ms (including a missing
increment, stored as `-1`) is a *risky situation*: the horizon is fixed at 28
moves and the increment is ignored, because an engine cannot rely on an
increment that may not arrive or is eaten by communication delays. Otherwise
the horizon is `16 - max(gameLengthContrib, gamePhaseContrib)`, where
`gameLengthContrib = int(min(15, halfmoves/10))` and
`gamePhaseContrib = int((1 - gp) * 15)` grow as the game gets long or material
disappears. With $T$ the remaining clock, $M$ the margin, $n$ the horizon and
$I$ the increment:

$$
f = \frac{T-M}{n},\qquad
b = \begin{cases}0 & \text{risky}\\ \min(0.9\,I,\;T-M-f) & \text{otherwise}\end{cases},\qquad
ms = \operatorname{trunc}\big((f+b)\cdot c_{\mathrm{ponder}}\big).
$$

The cap $T-M-f$ prevents the increment bonus from spending clock that the base
fraction already consumed; it matters when the horizon shrinks to 1 late in a
game. A "Minic is in time trouble ..." comment is printed when
`nmoves * msecMinimal > msecUntilNextTC`.

### Pondering correction

*Pondering* is searching on the opponent's time, on the move the engine
expects the opponent to play. The UCI moves-to-go and sudden-death branches
multiply by `ponderingCorrection`, which is 1.5 when the main worker's
`isPondering` flag is set and 1 otherwise:

```cpp
const float ponderingCorrection = (ThreadPool::instance().main().getData().isPondering ? 3 : 2) / 2.f;
```

`thinkAsync()` computes the allocation before `startSearch()` distributes the
new `ThreadData`, so the flag read here is the one left by the previous search.
`ponderhit` clears it, whereas a ponder search ended by `stop` leaves it set;
the 1.5 factor therefore applies to the search that follows such a stopped
ponder search, not to the ponder search itself. With several threads,
`ponderhit` clears only the main worker's flag: if the final vote then copies a
helper's `ThreadData` into the main worker, the helper's still-set flag comes
back with it, and the next allocation is also multiplied by 1.5.

### Final targets

After the control-specific computation:

```cpp
targetTime = std::max(ms - overHead, msecMinimal);
// maxTime will be our limit even in case of trouble
maxTime = std::min(msecUntilNextTC - msecMargin, targetTime * 7);
```

The 20 ms floor can exceed an otherwise tiny remaining budget; it is
best-effort behavior, not a proof against flagging.

## Search start shortcuts

Before iterative deepening, `searchDriver()` can bypass the search:

- `bongCloud` (runtime option, disabled by default): if the side to move still
  has castling rights, play the first king step of a fixed list (e2, d1, f1, d2,
  f2 for White) found in the pseudo-legal move list. Legality is not checked,
  so this joke option can in principle step into check.
- `Level == 0`, `p.halfmoves < RandomPly` (default 0) or
  `currentMoveMs <= msecMinimal` (20 ms): play `randomMover()`'s random legal
  move. The last condition is how extreme time trouble is handled.

## Adjusting time during search

`getCurrentMoveMs()` returns `INFINITETIME` while the main worker is pondering
or analysing. Otherwise, only when a remaining clock is known
(`msecUntilNextTC > 0`), it applies the difficulty categories:

| Signal | Effect on `currentMoveMs` |
|---|---|
| `MD_forced` | `ret >> 4` (divide by 16). |
| `MD_easy` | `ret >> 3`; never produced by the current source (marked "not used anymore"; `easyMoveMargin = 180` is unused). |
| `MD_moobAttackIID` / `MD_moobDefenceIID` (iteration emergency) | `min(msecUntilNextTC / 5, ret * 3)`. |
| `PE_boomingAttack` / `PE_boomingDefence` | `min(msecUntilNextTC / 5, ret * 1)`. |
| `PE_moobingAttack` / `PE_moobingDefence` | `min(msecUntilNextTC / 5, ret * 2)`. |

An iteration emergency takes precedence: the position-evolution switch runs
only when no emergency extended the time, so the factors are never multiplied
together. Then, for every control:

```cpp
ret = std::min(TimeMan::maxTime, static_cast<TimeType>(static_cast<float>(ret) * MoveDifficultyUtil::variabilityFactor())); // inside [0.5 .. 2]
return std::max(ret, static_cast<TimeType>(20)); // if not much time left, let's try something ...;
```

Consequences: booming's factor 1 can still *reduce* the allocation through the
one-fifth-of-clock cap; the final floor can exceed `maxTime`; and because the
variability factor is applied even without a clock, a fixed `movetime` search
can stop before the requested time when variability has decayed below 1 (the
cap prevents it from exceeding the requested time).

### Forced moves

When only one legal move exists, searching deeply is pointless. The driver
detects this with a one-ply root search, performed only by the main worker of
a single-process, single-PV, finite-clock search (`isFiniteTimeSearch`: no node
or skill depth limit, `currentMoveMs < INFINITETIME`, a known remaining clock,
not pondering/analysing) with `currentMoveMs > 100`:

```cpp
_data.score = pvs<true>(matedScore(0), matingScore(0), p, 1, 0, _data.pv, _data.seldepth, 0, isInCheck, false); // depth 1 search to get real valid moves
// only one : check evasion or zugzwang
if (rootScores.size() == 1) {
   moveDifficulty = MoveDifficultyUtil::MD_forced;
}
```

The search still runs, but with one sixteenth of the allocation.

## Within-search instability versus between-search evolution

Iterative deepening (below) produces one score per completed depth. If the
score stays flat, deeper search is unlikely to change the decision and time can
be saved. If it drops, the engine has probably just discovered a problem with
its intended move and needs time to find an alternative: this is when extra
time has the largest payoff. Minic uses three signals of this kind.

### Iteration emergency (within one search)

In dynamic timing, after a completed depth above 14 whose score is more than 64
below the score of depth - 2, the main worker switches from standard
difficulty to an emergency category, at most once per search (the test
requires `MD_std`, so a forced move never becomes an emergency):

```cpp
if (TimeMan::isDynamic && 
    depth > MoveDifficultyUtil::emergencyMinDepth && 
    moveDifficulty == MoveDifficultyUtil::MD_std &&
    _data.score < getSearchData().scores[depth - 2] - MoveDifficultyUtil::emergencyMargin) {
   moveDifficulty = _data.score > MoveDifficultyUtil::emergencyAttackThreshold ? MoveDifficultyUtil::MD_moobAttackIID
                                                                               : MoveDifficultyUtil::MD_moobDefenceIID;
```

A score above `emergencyAttackThreshold` (130) selects the attack-labelled
category, otherwise defence; both currently use factor 3. Comparing with
depth - 2 rather than depth - 1 compares iterations of the same parity, which
damps the odd/even oscillation typical of alpha-beta scores. The `IID` in the
category names refers to iterative-deepening instability; it is not evidence
that the disabled internal iterative deepening heuristic runs.

### Booming and moobing (between game moves)

At a real game root (`isMainThread() && !subSearch && postMove`), the driver
compares the final search scores recorded for the two previous roots with the
same side to move:

```cpp
bool Searcher::isBooming(uint16_t halfmove){
   if (halfmove < 4) { return false; }  // no booming at the beginning of the game of course
   const auto previous = COM::GetGameInfo().getSearchScore(halfmove - 2);
   const auto former   = COM::GetGameInfo().getSearchScore(halfmove - 4);
   if (!previous || !former || isMateScore(*previous) || isMateScore(*former)) return false;
   constexpr ScoreType boomMargin = 80;
   return *previous >= *former + boomMargin;
}
```

`isMoobing()` is symmetric with `*previous <= *former - moobMargin` (80).
"Booming" means the score rose by at least 80 over the last two own moves,
"moobing" that it fell by at least 80; booming is tested first. Missing or
mate-range scores, or a root before ply 4, disable the classification. The
previous score selects attack/defence with the same 130 threshold.

The recorded values are the final root scores of earlier searches, stored by
`GameInfo::setSearchScore()` only when the searched root matches the game
history at that ply (hash and fifty counter), not evaluations read from the
search stack. The
[GameInfo section](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/tables-and-history.md#gameinfo-the-actual-game)
explains preservation, validation and publication.

### Variability

After every completed depth above 12 with a nonempty PV (main worker, first
MultiPV line, whatever the control), a score change greater than
`emergencyMargin / 4` (16) relative to depth - 2 multiplies the global
`MoveDifficultyUtil::variability` by `1 + depth / 100`; otherwise it is
multiplied by 0.98. The main worker resets it to 1 at the start of each
search. Its time factor is

$$
f(v)=\frac{2}{1+\exp(1-v)}=2\,\sigma(v-1),
$$

where $\sigma$ is the logistic function: $f(1)=1$, $f$ tends to 2 for very
unstable searches and stays above $2/(1+e)\approx 0.54$ when $v$ decays toward
0. It is a heuristic instability measure, not the NNUE uncertainty output.

### Inactive search-side uses of these signals

The same signals were meant to make the *search* more prudent, not only
longer. In `pvs()` the code computes `pvsData.isBooming*`/`isMoobing*`
flags, but nothing reads them; `isEmergencyDefence`/`isEmergencyAttack` are
forced to `false` (their producer `moveDifficulty == MD_moob...IID` is
commented out); `situationDepthCorrection` is the constant 0 with the
booming/moobing expression commented out; and the LMR toggle
`enableReduceLessOnEmergencyDefence` is `false`. They are disabled by default.
Re-enabling the depth correction means restoring the commented expressions;
note that `depthCorrection` is added to the depth index of `lmpLimit`, whose
second dimension has `lmpMaxDepth + 1 = 11` entries, so a correction of 1 at
depth 10 would index past the table. See
[disabled features](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/disabled-features.md#iteration-emergency-and-game-history-depth-corrections).

## Iterative deepening and aspiration windows

*Iterative deepening* searches depth 1, then 2, 3, and so on. A finished depth
always leaves a usable best move, which is what makes time-based stopping
possible, and it fills the TT and history tables that make the next depth
cheaper. The full driver is described in the
[search chapter](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search.md#the-driver-iterative-deepening-and-aspiration);
only its timing-relevant aspects are summarized here.

Each depth after `aspirationMinDepth` (4) is searched in an *aspiration window*:
a narrow interval around the previous score instead of the full range. A
narrow window causes more cutoffs, hence a faster search, but the result is only
trustworthy if it falls strictly inside it. Otherwise the search is repeated
with a wider bound:

```cpp
ScoreType delta =
    (SearchConfig::doWindow && depth > SearchConfig::aspirationMinDepth)
        ? SearchConfig::aspirationInit + std::max(0, SearchConfig::aspirationDepthInit - SearchConfig::aspirationDepthCoef * depth)
        : matingScore(0); // MATE not INFSCORE in order to enter the loop below once
ScoreType alpha       = std::max(static_cast<ScoreType>(currentScore[multi] - delta), matedScore(0));
ScoreType beta        = std::min(static_cast<ScoreType>(currentScore[multi] + delta), matingScore(0));
```

With the default values the initial half-width is `5 + max(0, 40 - 3 * depth)`.
On each failure `delta` grows by `delta / 4 * exp(1 - gamePhase)` (faster in
endgames) and becomes the full window once it exceeds `max(128, 4096 / depth)`.
After a fail-low (`score <= alpha`) the retry depth is restored, `beta` is
pulled to `(alpha + beta) / 2` and `alpha` lowered to `score - delta`; after a
fail-high `beta` is raised to `score + delta` and the retry depth is reduced by
one (but not below 1). With a single PV the main worker prints such retries
with a `?` (fail-low) or `!` (fail-high) mark. A window failure repeats work at
the same depth, so the cost of an iteration depends on score stability, which
is one reason the forecast below is only approximate.

## Forecasting the next iteration

The *effective branching factor* (EBF) is the growth ratio of the tree from one
depth to the next. Minic estimates it from cumulative node counts of
consecutive completed depths, starting from 1.7 and recomputing only after
depth 12. Because the stop test precedes the recomputation, it uses the
previously held estimate:

```cpp
if (TimeMan::isDynamic && static_cast<TimeType>(static_cast<double>(getTimeDiff(startTime))*1.2*EBF) > getCurrentMoveMs()) {
   stopFlag = true;
   ...
   break;
}

// compute EBF
if (depth > 12) {
   EBF = getSearchData().nodes[depth] / static_cast<double>(asLeastOne(getSearchData().nodes[depth - 1]));
```

Since the work up to depth $d$ took $t$, depth $d+1$ is expected to finish
near $t\cdot\mathrm{EBF}$; the factor 1.2 adds a safety margin. With an
elapsed time of 500 ms and an EBF of 2.4 the test compares
$500\times1.2\times2.4=1440$ ms with the adjusted allocation. Aspiration
retries, TT reuse, changed branching and helper cooperation all make this an
approximation. The soft test only exists for dynamic controls; fixed
`movetime` searches rely on the hard limit alone.

## Stop checks

`timeCheck()` is called at PVS entry by the main worker or by a stoppable
co-searcher (auxiliary searchers used by data-generation tools). Its countdown
`periodicCheck` is a function-local `static`, shared by every caller, reloaded
with `min(maxNodes, 1024)` when a node limit is set and 1024 otherwise:

```cpp
if (TimeMan::maxNodes > 0 && nodeCount > TimeMan::maxNodes) {
   stopFlag = true;
   ...
}
if (getTimeDiff(startTime) > getCurrentMoveMs()) {
   stopFlag = true;
   ...
}
Distributed::pollStat();
```

The main worker's node count is the pool-wide `nodes + qnodes` (the reduced
MPI counter when several processes run). A secondary MPI process additionally
pulls rank zero's stop flag here. Helper workers never test time: they stop
when `ThreadPool::stop()` sets their flag.

Qsearch checks `stopFlag` but does not sample elapsed time, so a large qsearch
excursion delays detection. The driver also checks the node limit after each
depth. `TimeMan::maxNodes` is zeroed during depth 1 and restored afterwards on
the main process, so at least one iteration always completes.

`Clock` is `std::chrono::system_clock`, not `steady_clock`, so a wall-clock
adjustment during a search can distort the measured elapsed time.

During pondering and analysis the allocation is infinite. If such a search
reaches its depth limit early, the main worker sleeps in 1 ms steps until
`ponderhit` (UCI clears `isPondering`) or a stop. After a `ponderhit` the
allocation computed at `go ponder` applies, and elapsed time is still measured
from the start of the ponder search, so time already spent pondering counts
against it.

## Strength limitation

The `Level` option (0 to 100, default 100) weakens play; `Skill::enabled()` is
true for levels 1 to 99, and level 0 is the random mover. With
`UCI_LimitStrength`, each search overwrites `Level` with
`Skill::convertElo2Level()` applied to `UCI_Elo` (500 to 2800, default 1500), a
cubic fit followed by `max(0, (elo' - 500) / 29 - 10)`. An enabled skill
either:

- limits nodes, when `nodesBasedLevel` is set: `maxNodes = exp((elo + 840) / 240)`
  with `elo = 29 * (Level + 10) + 500`; or
- limits depth to `1 + 2 * sqrt(max(0, Level - 20))`, forces at least 4 PVs
  and picks a move with `Skill::pick()`, which adds a level-dependent bonus
  plus a random term to each non-mate PV score and keeps the best result (a
  Stockfish-derived formula). This path bypasses thread voting and sends no
  ponder move.

## Lazy SMP

### The idea

Splitting an alpha-beta tree among threads is hard: the tree is very
unbalanced, and most of its value comes from cutoffs that are only known
after the first move of a node has been searched. Historic schemes (YBWC, DTS)
split nodes explicitly and need much synchronization. *Lazy SMP* takes the
opposite approach: every worker runs its own complete iterative-deepening
search of the same root position, with no work splitting, and the workers
communicate only through the shared transposition table (TT). A worker that
finishes a subtree stores the result in the TT, and the others find it there
instead of searching it. Since threads are not perfectly synchronized and
have different histories, they explore slightly different trees and fill
the TT ahead of each other. It scales well in practice because there is
almost no locking or idle time, at the cost of duplicated work, which makes
the speedup sublinear.

### Lifecycle

Each worker (`Searcher`) has its own position stack, NNUE accumulators, move
histories, correction tables and pawn cache. They share the TT and global
search settings. `ThreadPool::distributeData()` gives helpers the requested
depth `MAX_DEPTH`; they rely on stop coordination.

`ThreadPool::startSearch()`:

1. Waits for the previous main worker.
2. Updates `COM::state` late enough not to disturb the previous search.
3. Sets the atomic `Searcher::startLock` and distributes copied `ThreadData`
   and `currentMoveMs`.
4. Wakes worker zero, whose `searchLauncher()` starts the helpers before
   entering its own driver.

Helpers spin on `startLock` in the driver before the iteration loop. The main
worker releases it when it begins depth 2 (or at the end of the search if it
finishes earlier), so helpers do not compete with the cheap first iteration.
`startThread()` also clears each worker's `stopFlag`.

### Depth skipping

A helper with index `i = (id - 1) % 20` skips depth `d` when
`((d + skipPhase[i]) / skipSize[i]) % 2` is odd, a scheme taken from
Stockfish:

```cpp
constexpr array1d<int,threadSkipSize> skipSize  = {1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
constexpr array1d<int,threadSkipSize> skipPhase = {0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7};
...
const auto i = (id() - 1) % threadSkipSize;
if (((depth + skipPhase[i]) / skipSize[i]) % 2){
   ...
   continue; // next depth
}
```

For example, helper 1 searches depths 2, 4, 6, ...; helper 2 searches 1, 3,
5, ...; helper 3 searches 1, 4, 5, 8, 9, ...; helper 4 searches 3, 4, 7, 8, ...
The main worker never skips. Helpers are thus spread over different depths,
which diversifies the trees and makes some helpers reach deeper iterations
earlier. The skip sits inside the MultiPV loop, so with several PVs every line
of a skipped depth is skipped.

### Result voting

When the main worker leaves its loop it selects the result by a depth-weighted
vote. With $W$ the workers whose PV is nonempty, $m_w$ the first move of
worker $w$'s PV and $d_w$ its last completed depth:

$$
V(m)=\sum_{w\in W,\;m_w=m} d_w,\qquad m^\star=\arg\max_m V(m).
$$

The map is scanned in increasing `MiniMove` order with a strict `>`, so ties go
to the smallest `MiniMove` value. Among the supporters of $m^\star$, the
deepest worker (lowest index on equal depth) supplies the whole `ThreadData`
(score, PV, depth):

```cpp
std::map<MiniMove, int> voteCount;
for (const auto& s : ThreadPool::instance()) {
   const auto& d = s->getData();
   if (!d.pv.empty()) {
      const int w = static_cast<int>(d.depth);
      voteCount[Move2MiniMove(d.pv[0])] += w;
   }
}
```

Voting makes the choice robust against one helper that went deep on a narrow,
possibly wrong line; it is not simply "take the deepest thread". An enabled
depth-based skill level uses `Skill::pick()` instead.

The driver then waits for `ponderhit`/stop if needed, stops helpers and waits
for them, records the root search score in `GameInfo`, synchronizes MPI
statistics/TT, prints the final line and emits the move. Selection happens
before the helper stop/wait, while helpers may still be updating their data.

### Synchronization: what exists and what is not guaranteed

Worker wake/wait uses a per-worker mutex and condition variable; helper release
uses the atomic start lock. GUI command operations use COM mutexes. Each
worker owns a `_mutexPV` that it takes while publishing its best line; the
second voting loop takes only the main worker's `_mutexPV` (the first loop takes
none), so it does not exclude concurrent helper updates. The `stopFlag` members are ordinary booleans, and the TT is shared
without locks or atomic entry storage; its XOR validation rejects torn entries
but is not formal synchronization. The game-history mutex protects modifying
methods and the score getter/setter, not all readers. These facts do not
amount to a C++ memory-model proof of race freedom; review synchronization
explicitly when adding shared state.

### Disabled thread-related features

- `LIMIT_THREADS_TO_PHYSICAL_CORES` (commented out in
  [config.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/config.hpp)):
  disabled by default. When defined, `ThreadPool::setup()` caps `Threads` at
  `std::thread::hardware_concurrency()`, which counts logical hardware threads
  (including SMT siblings), not physical cores as the name suggests. Without
  it the `Threads` option accepts 1 to 1023.
- `enableThreadBaseReduction` (`constexpr false` in the LMR code of
  `searcherPVS.hpp`): disabled by default. It would add `id() % 2` to the
  reduction of quiet moves, so odd-index workers reduce one ply more, another
  way to diversify helper trees. `threadReductionIdx` is a related placeholder
  fixed to 0.
- `REPRODUCTIBLE_RESULTS` (commented out): disabled by default. When defined,
  `ThreadPool::clearSearch()` also clears the TT before every search, and
  `Searcher::clearSearch()` clears the pawn table and forces the history and
  correction-history reset, removing cross-search state; multi-threaded runs
  remain timing dependent.

See [disabled features](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/disabled-features.md)
for the full inventory.

## Optional distributed search

Distributed search runs several Minic processes, possibly on different
machines, that cooperate through the Message Passing Interface (MPI) library.
`WITH_MPI` is commented out in `config.hpp`, so it is disabled by default;
`distributed.h` then provides mostly empty inline stubs, a world size of 1 and
`isMainProcess() == true`. Enabling it requires uncommenting `WITH_MPI`;
`Tools/build/build.sh` detects the define and switches to the `mpiicpx`/`mpiicx`
compiler wrappers, and the engine is started through `mpirun`.
`MPI_Init_thread` must provide `MPI_THREAD_MULTIPLE`, otherwise the engine
stops with a fatal error.

The design extends Lazy SMP across processes:

- Rank zero reads commands and broadcasts each input line and the played move;
  other ranks silence their output.
- Secondary processes search with `MAX_DEPTH` and `INFINITETIME`: they never
  decide to stop by themselves, but pull rank zero's main-worker `stopFlag`
  through a one-sided MPI window, in `timeCheck()` and after each depth.
- TT entries written by a main worker (`TT::setEntry()` passes
  `isMainThread()` as its `distribute` argument) with depth greater than 3
  are copied (if a `try_lock` succeeds)
  into a send buffer of `distributedTTBufSize` entries (default 16; exposed
  as an option, range 4 to 8192, only in `WITH_SEARCH_TUNING` builds). When the buffer is full and the previous nonblocking
  all-gather has completed, received entries are written into the local TT
  (always replacing) and the buffer is sent; otherwise the buffer is
  overwritten and those entries are lost (overruns are counted and logged).
  `syncTT()` completes outstanding exchanges at the end of each search.
- Node counters are summed with nonblocking all-reductions polled from
  `timeCheck()`.
- Dedicated communicators separate input, TT, statistics, moves and stopping.

With more than one process, the engine name reported to UCI and XBoard (base
version, commit and an optional `-dirty` marker) gets a `_<N>proc` suffix.

## Worked sudden-death allocation

Assume remaining clock 60000 ms, increment 500 ms, starting material,
`halfmoves = 1`, no pondering correction, zero processing overhead and
`MinMoveOverHead` no greater than 600 ms. This is non-risky:

```text
margin = min(3000, 0.01*60000) = 600
gameLengthContrib = int(min(15, 1/10)) = 0
gamePhaseContrib = int((1-1)*15) = 0
horizon = 16
base = (60000-600)/16 = 3712.5
incrementBonus = min(450, 60000-600-3712.5) = 450
initialAllocation = int(4162.5) = 4162
targetTime = 4162
maxTime = min(59400, 7*4162) = 29134
```

With variability 1 and standard difficulty, the allowance is 4162 ms.
Historical moobing doubles it to 8324; an iteration emergency triples it, but
the remaining-clock/5 cap yields 12000. A forced move gives `4162 >> 4 = 260`
ms. These categories are selected, not multiplied.

## Measuring a timing change

Use both fixed-node/fixed-depth searches and real time controls. The former
isolate tree changes; the latter expose overhead, emergencies, instability and
SMP timing. Compare actual move times, overruns, completed depths and
historical-score availability, not just nodes per second. `TimeMan::simulate()`
can replay an allocation sequence with random moves to check that a control
never runs out of clock. A timing repair can be Elo-neutral while correcting
the signal it uses; only controlled matches establish its strength effect.
