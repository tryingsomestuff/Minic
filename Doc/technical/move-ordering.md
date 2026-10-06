# Move ordering and picking

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md) | [Next: evaluation](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/evaluation.md)

Primary sources: [moveSort.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/moveSort.cpp),
[moveSort.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/moveSort.hpp), [tables.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/tables.hpp),
[tables.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/tables.cpp), [searcher.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcher.cpp)
and [searcherSee.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherSee.cpp).

## Why ordering affects more than speed

An early good move raises alpha or cuts off the node before expensive later
moves. In Minic, ordering also determines move number, history-pruning evidence
and LMR reduction. A move's score can therefore alter which tree is searched,
not just the order in which an unchanged tree is visited.

Separate three operations:

```text
generation -> assigning scores -> selecting the next list entry
```

The PVS TT-first path can avoid the first operation entirely on a cutoff.
Once the remaining list is built, partial sorting avoids work on its unused
tail, but does not avoid generating that tail.

## Score composition

`MoveSorter::computeScore()` starts from the move-type base in `MoveScoring`.
A nonzero existing score is left unchanged, allowing reuse of ProbCut-scored
captures. `score()` skips lists with fewer than two entries.

| Category | Base/bonus in the documented source |
|---|---|
| Ordinary quiet | Base 0. |
| Castling | Base 256. |
| Capture / en passant | Base 7000. |
| Quiet promotions Q/R/B/N | Bases 3950 / 3500 / 3350 / 3300. |
| Capture promotions Q/R/B/N | Bases 7950 / 7500 / 7350 / 7300. |
| Previous best root move | Add 15000 at height 0. |
| TT move | Add 14000 if it was not the previous-best case. |

The bonuses are additive: there is no single universal score for "the TT move".
For example, a TT capture keeps its type base. Source comments that give older
bonus values are not the current implementation.

### Ordinary captures and en passant

Captures that are not promotions receive a phase-interpolated PST hint:
the moving piece's improvement plus the captured piece's removed PST value,
divided by `capPSTScoreDivisor`.

With SEE enabled outside check, add the exchange score and a 512 recapture
bonus when applicable. If SEE is below `DynamicConfig::badCapLimit`, subtract
twice the capture base, moving the capture towards the end of the list.

Without SEE, or while in check, add MVV-LVA, recapture and capture-history
bonuses instead. Thus capture history is not directly added by this scorer's
normal SEE branch, although it is still used for capture pruning/reduction.
Promotion ordering uses its own bases rather than this ordinary-capture path.

`SEE()` estimates successive least-valued captures with x-ray attackers;
`SEE_GE()` currently calls the full `SEE()` and compares its result with the
threshold; it is not a specialized early-exit implementation. Neither substitutes for
the legal move application test or a full tactical search.

### Killers and counter moves

For ordinary quiets and castling, the scorer takes the first matching branch:

| Match | Bonus |
|---|---|
| Current height killer 0 | 1900 |
| Current height killer 1 | 1300 |
| Killer 0 at height - 2 | 1700 |
| Counter to the previous move | 1500 |

These branches bypass generic quiet-history/PST scoring. This is not a strictly
descending category ladder: the earlier-side killer's numeric bonus exceeds
killer 1, but branch matching still tests killer 1 first.

Killers are two remembered moves per search-relative height. Counter moves map
the previous move's from/corrected-to squares to one move identity. The
counter-move table is not the continuation-history table.

### Generic quiet score

Without a killer/counter match:

```text
score += from/to history / quietHistoryDivisor1
       + piece/to history / quietHistoryDivisor2
       + weighted CMH / quietHistoryDivisor3
```

All three divisors currently equal 3. Non-castling moves also receive half the
PST improvement. This provides a positional prior while learned histories are
small.

A safe move leaving the square threatened by the null-move refutation can gain
512, provided it is not in check and `SEE_GE(..., -80)` succeeds. Castling
does not use this escape/PST branch. Its destination for histories is the
corrected king landing square.

## Picking: full sort versus partial sort

With `USE_PARTIAL_SORT`, `pickNext()` finds the highest-scored entry in the
remaining suffix, swaps it into the selected prefix and advances `begin`.
The implementation uses `min_element` with a descending comparator; this is
why it still selects the largest score.

PVS uses `pickNextLazy()`, which permanently sets `skipSort` for that sorter when
the previous picked score drops below its threshold, or when the caller says
the list is already sorted. Once set, later entries are consumed in their
current order. Production qsearch instead calls `pickNext()` repeatedly without
the lazy cutoff; `lazySortThresholdQS` exists but its use is commented out.

Without partial sorting, the list is fully sorted in descending order.
ProbCut separately picks from its capture-only list before the main search
adds quiets.

This is **not** a state machine that generates good captures, quiets and bad
captures on demand. Implementing a staged MovePicker would require preserving
scoring, legality, duplicate avoidance, list-prefix learning and pruning
semantics, not merely replacing `pickNextLazy()`.

## History tables and updates

The worker-local tables are:

| Table | Key |
|---|---|
| `history` | Side to move, from square, to square. |
| `historyP` | Colored moved piece, destination. |
| `historyCap` | Colored moved piece, destination, captured piece kind. |
| `counter_history` | Context moved piece/destination, current moved piece/destination. |

From/to, piece/to and capture history initialize to zero; continuation entries
initialize to -1. Killers/counters reset every search, while history tables
normally persist through searches in the same game.

The depth bonus and gravity update are:

```text
HSCORE(depth) = 4 * min(depth, 32)^2
bonus        = sign * HSCORE(depth)
item        += bonus - ((item * abs(bonus)) >> HISTORY_POWER)
HISTORY_POWER = 10
```

The damping scale is `HISTORY_MAX = 1024`. It is not an explicit saturation
clamp on every possible update: use the formula and caller's bonus/depth
constraints rather than assuming arbitrary inputs cannot overshoot.

Quiet history updates require `T_std`: castling and quiet promotions do not
use that update function. Capture history accepts `isCaptureNoEP`, which includes
capture promotions but excludes en passant; caller conditions still matter.

On a quiet beta cutoff, the code updates the killer, counter and positive
histories, then applies negative history updates to earlier eligible list
entries. Capture cutoffs have their own capture-history updates. TT cutoffs
can also learn from their stored move.

Earlier entries are not an explicit record of exclusively searched moves:
some can have been skipped/pruned. Changing list layout can therefore change
learning as well as search order.

## Four-distance continuation history

Current constants:

| Prior-move distance | Context stack offset | Weight |
|---|---|---|
| 1 | Current position's `lastMove` | 8 |
| 2 | One position slot back | 6 |
| 4 | Three position slots back | 4 |
| 6 | Five position slots back | 2 |

The table row is selected from the colored piece on the prior move's corrected
destination in its resulting position. The column is the candidate moved
piece and candidate destination.

Distance 1 is the opponent's latest move; distances 2/4/6 are increasingly old
moves by the same side as the candidate. Four pointers reference the same
`counter_history` storage; there is no independent distance dimension.
Repeated contexts may therefore refer to the same row.

`getCMHPtr()` checks valid last moves and refuses contexts before
`rootHalfmoves`. A root can still supply distance 1 if its own `lastMove` is
valid; older game contexts are not reconstructed from `GameInfo`.

The ordering score normalizes only by available weights:

$$
H(m)=\operatorname{trunc}\!\left(
\frac{\sum_{i\in I}w_i\,h_i(m)}{\sum_{i\in I}w_i}\right),
\qquad (w_1,w_2,w_4,w_6)=(8,6,4,2).
$$

$I$ contains only available context distances, and $h_i(m)$ is the row's
history score for candidate $m$.

With no context it returns zero. Learning applies a bonus scaled by
`weight[i] / weight[0]` to each available row.

`isCMHGood()` and `isCMHBad()` inspect **only pointer 0**. An absent pointer
satisfies neither predicate. Multidistance ordering/learning does not make
the pruning test multidistance too.

## Worked continuation/history example

Suppose distances 1/2/4 have available entries `320, 100, -80`, while distance
6 is unavailable:

```text
weightedCMH = (8*320 + 6*100 + 4*(-80)) / (8+6+4)
            = 2840 / 18 = 157
```

The absent context contributes neither zero times its weight nor its weight
to the denominator. With from/to history 192, piece/to history 96 and PST
improvement 30, a generic quiet receives:

```text
192/3 + 96/3 + 157/3 + 30/2 = 64 + 32 + 52 + 15 = 163
```

This assumes no killer/counter/escape/root bonus. The divisions truncate
individually. Summing all numerators before dividing produces a different
score in general.

At learning depth 4, `HSCORE(4) = 64`. Continuation bonuses before damping
are 64/48/32/16 for distances 1/2/4/6, respectively. If contexts alias the
same row, the actual updates are successive gravity operations on that row,
not one update on four independent tables.

## SEE algorithm and scope

In
[searcherSee.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherSee.cpp),
SEE builds attackers to the corrected destination, removes the initial source,
updates slider x-rays under a shrinking occupancy mask and alternates
least-valued attackers. The stored swap list is folded backward to model
declining an unfavorable continuation.

Pawn capture priority changes on a promotion rank; a later pawn capture is
treated as queen promotion. Kings are selected only if the remaining attacker
set has no opposing piece. Pinned attackers are not filtered by recursively
applying each exchange as a fully legal position.

Thus SEE is a local exchange estimate, not a replacement for legality or an
oracle for forks/checking sequences. Its initial EP handling has its own
special occupancy expression; audit it separately rather than assuming ordinary
capture geometry covers EP.

## Ordering score as a selectivity input

The move-score value is packed above the 16-bit identity. A root-best or TT
bonus changes where a move is picked; killer matching changes whether generic
history is even consulted; the same resulting score then feeds quiet LMR,
history pruning and bad-capture classification.

For the exact downstream predicates, see the
[search policy reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md).
An ordering experiment is not isolated if those consumers retain the old
thresholds.

## Experimental implications

Keep CMH weights, score normalization and learning scale consistent. Separately
test ordering changes and pruning/reduction changes when possible.

Pawn history would be another contextual move-ordering signal, not the existing
pawn evaluation cache or pawn correction history. Likewise, continuation
correction would learn evaluation residuals, not beta-cutoff move quality.
