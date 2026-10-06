# Time management and parallel search

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md) | [Next: tables and history](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/tables-and-history.md)

Primary sources: [timeMan.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/timeMan.cpp),
[timeMan.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/timeMan.hpp), [searcher.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcher.cpp),
[searcherDriver.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherDriver.cpp),
[threading.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/threading.cpp),
[definition.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/definition.hpp) and
[distributed.h](https://github.com/tryingsomestuff/Minic/blob/master/Source/distributed.h).

## Two time-management layers

`TimeMan::getNextMSecPerMove()` converts protocol clock information into an
initial allocation. The pool copies it to each worker's `currentMoveMs`.
During search, callers use `getCurrentMoveMs()`, which applies difficulty,
score evolution, variability and caps.

The public inputs include fixed move time, remaining time, increment,
moves-to-go/repeating control, processing overhead and node count. `targetTime`
is the nominal allocation; `maxTime` caps the later adjusted allocation.
The distinction is not a guarantee of exact real-time deadlines.

## Initial allocation

| Control | Initial behavior |
|---|---|
| Fixed nodes | Target/max time become effectively infinite; node counting drives stopping. |
| Fixed move time | Target and maximum both equal the requested move time. |
| XBoard repeating control | Average available time and future increments over the remaining moves, with static/dynamic variants. |
| UCI moves-to-go | Divide remaining clock plus expected increments minus margin by moves-to-go. |
| Sudden death | Estimate a remaining-move horizon from risk, game length and game phase, then add an affordable increment contribution. |

The dynamic margin is approximately 1% of remaining time, capped at 3000 ms,
with `moveOverHead` as the minimum. UCI/sudden-death allocation has a pondering
correction of 1.5 when the relevant pondering state is set.

For sudden death, increment below `msecMinimal = 20` is considered risky.
The risky horizon remains 28; the non-risky horizon is 16 minus the larger
game-length/phase contribution, at most 15. Risky allocation does not rely on
increment. Otherwise the increment bonus is capped at 90% of increment and
by available clock after the base fraction.

After the dynamic calculations:

```text
targetTime = max(initialAllocation - processingOverhead, 20)
maxTime    = min(remainingClock - margin, 7 * targetTime)
```

At severe time trouble, minimum-time fallbacks can exceed an otherwise tiny
remaining budget. They are best-effort behavior, not a proof against flagging.

## Adjusting time during search

With remaining-clock information, `getCurrentMoveMs()` considers:

| Signal | Current effect |
|---|---|
| Forced move | Divide allocation by 16. |
| Easy move | Divide by 8; the source notes this category is no longer used. |
| Iteration-score emergency | Multiply by 3, capped at one fifth of remaining clock. |
| Historical booming | Multiply by 1, with the same clock cap. |
| Historical moobing | Multiply by 2, with the same clock cap. |

Iteration emergencies take precedence over historical evolution; the factors
are not multiplied together. Then variability scales the allocation,
`maxTime` is applied and a final 20 ms floor is imposed.

Consequently, even historical factor 1 can cap an allocation, and the final
floor can exceed `maxTime` in extreme conditions. Do not simplify these rules
to "booming always gives extra time" or "maxTime is an absolute deadline".

## Within-search instability versus between-search evolution

The driver tracks completed iteration scores. In dynamic timing, beyond depth
14, a decline greater than 64 relative to depth - 2 can switch from standard
difficulty to an emergency category. A score above 130 selects the
attack-labelled category, otherwise defence.

The `IID` in these category names refers to iterative-deepening instability;
it is not evidence that the disabled traditional internal iterative deepening
heuristic runs.

Separately, at a real searched game root, historical evolution compares:

```text
previous = recorded final search score at root ply - 2
former   = recorded final search score at root ply - 4

booming: previous >= former + 80
moobing: previous <= former - 80
```

These are roots with the same side to move. Missing or mate-range values
disable the classification. The prior score selects attack/defence using
the same 130 threshold.

The recorded values are final search scores, not pre-root stack evaluations.
This restores meaning after the stack began clearing pre-root slots.
The old booming expression accepted stable/slightly decreasing scores; the
documented working tree requires an actual rise of at least 80.

The main driver consults history only with `!subSearch && postMove`.
The [GameInfo chapter](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/tables-and-history.md) explains preservation, validation
and publication.

## Variability and the next iteration

After depth 12, a score change greater than `emergencyMargin / 4` relative to
depth - 2 increases variability by a depth-dependent factor; otherwise it
decays by 0.98. Its time factor is:

$$
f(v)=\frac{2}{1+\exp(1-v)}=2\,\sigma(v-1),
$$

where $v$ is variability and $\sigma$ the logistic function.

Variability begins at 1, yielding factor 1. It is a heuristic instability
measure, not the NNUE uncertainty field.

The driver estimates effective branching factor from cumulative node counts
of consecutive completed depths. Before starting more work, dynamic timing
compares:

$$
1.2\,t_{\mathrm{elapsed}}\,\mathrm{EBF}>T_{\mathrm{allowed}},
$$

with $T_{\mathrm{allowed}}$ supplied by `getCurrentMoveMs()`.

This is a forecast, not an exact next-iteration duration. In the current order,
the stop test precedes recomputation of EBF for the latest depth, so it uses
the previously held estimate.

## Stop checks and output

`timeCheck()` is called from PVS on the main worker or a stoppable auxiliary
searcher, with a 1024-call countdown. It checks node limits, elapsed time
against the adjusted allocation and distributed stopping.

Qsearch checks `stopFlag` at entry but does not independently sample elapsed
wall time. Large qsearch excursions can delay time/node-limit detection.
The driver also checks node counts between iterations; depth 1 gets special
protection from the requested node cap.

`Clock` is `std::chrono::system_clock`, not `steady_clock`. The current
implementation should not be documented as immune to wall-clock adjustments.

For ponder/analysis, the adjusted allocation is effectively infinite.
If the search reaches its stopping depth early, the main worker waits for
`ponderhit` or `stop` before move emission. The protocol operation updates
the state/limits; workers are subsequently stopped and joined for that search.

## Lazy SMP lifecycle

Each worker has its own position stack, NNUE accumulators, move histories,
correction tables and pawn cache. They share the TT and global search settings.
Helpers receive effectively unlimited requested depth and rely on stop
coordination.

`ThreadPool::startSearch()`:

1. Waits for the previous main worker.
2. Updates `COM::state` late enough not to disturb the previous search.
3. Sets the atomic start lock and distributes copied `ThreadData`.
4. Wakes worker zero, which starts helpers.

Helpers wait on the start lock in the driver before the main iteration loop.
Per-worker depth-skipping schedules make the trees diverge. This is Lazy SMP,
not deterministic root-move splitting.

At the end, each nonempty PV contributes its completed depth as a vote for
its first move. The winning move's deepest supporter supplies the selected
`ThreadData`. This is not simply "take the deepest thread regardless of move".
Skill selection can bypass this voting path.

The driver then handles ponder/analysis waiting, stops helpers and waits for
them. It records the root search score before final reporting and best-move
application. Selection itself occurs before helper stop/wait, so do not imply
that every result read happened after all workers became idle.

## Synchronization: what exists and what is not guaranteed

Worker wake/wait uses a mutex and condition variable; helper release uses an
atomic start lock. GUI command/move operations use COM mutexes, and PV updates
have a dedicated mutex.

The implementation also shares ordinary `stopFlag` booleans and reads worker
data during voting. These facts do not establish a complete C++ memory-model
proof of race freedom.

The TT is shared without locks or atomic entry storage. XOR validation checks
snapshots, but is not formal synchronization. The new game-history mutex
protects modifying methods and score getter/setter, not all legacy readers or
public fields. Preserve the established lifecycle, and review synchronization
explicitly when adding shared state.

## Optional distributed search

`WITH_MPI` is disabled by default. When enabled, rank zero reads commands and
distributes input; processes maintain local searches and exchange TT entries,
reduce statistics and forward the master's stop state. Dedicated communicators
separate input, TT, statistics, moves and stopping.

Nonblocking broadcasts/reductions and a one-sided stop window accommodate
divergent trees. Local SMP and MPI can coexist, but they are not the same
memory-sharing mechanism. With MPI disabled, the distribution interface mostly
becomes no-op plumbing.

## Worked sudden-death allocation

Assume remaining clock 60000 ms, increment 500 ms, starting material, absolute
ply 1, no pondering, zero processing overhead and configured minimum margin
no greater than 600 ms. This is non-risky:

```text
margin = 600
gameLengthContrib = int(min(15, 1/10)) = 0
gamePhaseContrib = int((1-1)*15) = 0
horizon = 16
base = (60000-600)/16 = 3712.5
incrementBonus = min(450, 60000-600-3712.5) = 450
initialAllocation = int(4162.5) = 4162
targetTime = 4162
maxTime = min(59400, 7*4162) = 29134
```

With variability 1 and no other difficulty, current allowance is 4162 ms.
Historical moobing doubles it to 8324; an iteration emergency triples it but
the remaining-clock/5 cap yields 12000. A forced-move division yields 260 ms.
These categories are selected, not multiplied into one another.

At long game ply the horizon can shrink to 1. The increment-affordability
cap then matters: it prevents adding 90% of increment to a base fraction that
already consumes the available clock. This is why testing opening roots alone
does not characterize late-game timing.

## Forecast interpretation

If elapsed time is 500 ms and the held EBF estimate is 2.4, the next-depth
test compares `500 * 1.2 * 2.4 = 1440` ms with the adjusted allocation.
This uses cumulative elapsed/node history, not an exact measurement of one
future iteration. Aspiration retries, TT reuse, changed branching and worker
cooperation all make that forecast approximate.

Publication of a new variability/EBF estimate occurs at driver boundaries;
PVS periodically checks the current adjusted allowance and qsearch observes
the stop flag. Soft forecast, sampled elapsed check and final output latency
are three different stopping mechanisms.

## SMP result interpretation

Helpers retain local histories and can skip selected nominal depths, yet all
consult the shared TT. A fixed-depth request therefore does not imply identical
work or an identical trace on every worker.

Final voting is depth weighted, followed by selection of a deep supporting
result. Voting reads worker results before the later stop/join sequence, so it
must not be described as a snapshot proven synchronized merely by the
existence of `wait()`. Any publication refactor must separately specify result
ownership, the happens-before boundary and the score/PV/depth consistency of
the selected tuple.

## Measuring a timing change

Use both fixed-node/fixed-depth searches and real time controls. The former
help isolate tree changes; the latter expose overhead, emergencies, instability
and SMP timing. Compare actual move times, overruns, completed depths and
historical-score availability, not just nodes per second.

A timing repair can be Elo-neutral while correcting the signal it uses.
Only controlled matches can establish its strength effect.
