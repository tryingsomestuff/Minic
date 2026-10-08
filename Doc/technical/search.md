# Search and selectivity

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md) | [Next: move ordering](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/move-ordering.md)

Primary sources: [searcherDriver.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherDriver.cpp),
[searcherPVS.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherPVS.hpp),
[searcherQSearch.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherQSearch.cpp),
[searcherDraw.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherDraw.cpp),
[searcherDraw.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherDraw.hpp) and
[searchConfig.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.hpp).

## Negamax, alpha/beta and bounds

Search scores are from the current side's point of view. Descending a ply
negates both result and window:

```text
score = -search(child, -beta, -alpha)
```

Alpha is the best established lower threshold; beta is the cutoff threshold.
A fail-high proves a lower bound for this window, not a globally exact score.
`B_alpha` is an upper bound, `B_beta` a lower bound, and `B_exact` an exact
window result. Many returns are fail-soft, but some heuristics deliberately
return alpha/beta or a margin-adjusted estimate.

The template `pvs<pvnode>()` specializes PV/non-PV behavior. The `cutNode` hint
affects selectivity; a PV flag, cut-node hint and TT-PV flag are related but
distinct inputs.

## The driver: iterative deepening and aspiration

The driver keeps the latest completed first-PV result and progressively searches
depths 1, 2, ... up to its target. Skill settings can limit depth or nodes.
Nodes limits are temporarily disabled for depth 1 to obtain a usable initial
result. The depth ceiling reserves headroom below `MAX_DEPTH`.

After the configured aspiration minimum depth, search starts with a narrow
window around the prior iteration's score. The initial half-width is:

```text
aspirationInit + max(0, aspirationDepthInit - aspirationDepthCoef * depth)
```

On fail-low the window expands downward and the search depth is restored.
On fail-high beta expands and the retry may be a ply shallower. Delta growth
also depends on game phase; sufficiently wide windows become mate-range
windows. These retries are not the adaptive LMR re-search experiment in TODO.

MultiPV repeats searches with previously selected root moves excluded.
Separate scores/PVs are retained per line; only the first line updates the main
reported best result. Output during failing aspiration windows is provisional.
Interrupt handling must preserve an already valid result instead of publishing
`STOPSCORE`.

After iterative deepening, local SMP vote selection can replace the main
worker's result with another worker's supported/deeper line. Strength-limiting
move selection is a separate path.

## PVS control flow

The broad order inside `pvs()` is:

1. Periodic stop/time checks; node context initialization.
2. Height/depth limit, interior draws, and qsearch at depth <= 0.
3. Node count, mate-distance window clipping and continuation-context lookup.
4. Hash construction, endgame classification, TT probing/cutoffs and Syzygy.
5. Static evaluation, optional TT estimate substitution and correction.
6. Attack/mobility data, improving state and forward pruning.
7. Null-move/ProbCut attempts, IIR and threat bookkeeping.
8. Root tablebase list or TT move, including singular probing.
9. Remaining move generation, scoring, legality, pruning and PVS/LMR searches.
10. Terminal/fifty handling, correction learning and TT storage.

Early returns are intentional. Histories or TT entries are not necessarily
updated on every path.

## Evaluation values are not interchangeable

The raw static score is stored in `stack[p.halfmoves].eval` before TT
search-score substitution. `staticScore` backs up that value.

`evalScore` can subsequently become a TT search result when its bound supports
using it as a better estimate. `evalScoreIsHashScore` records that distinction.
`correctedStaticScore` adds the pawn/non-pawn correction to `staticScore`;
`pruningEval` uses that correction unless a TT search result has replaced it.

Never apply evaluation correction on top of a TT search result. Conversely,
do not use a TT search bound as the raw evaluation of an ancestor for improving.
The intended separation is important even where existing TT writes use the
mutable `evalScore` variable; see [tables](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/tables-and-history.md) for that caveat.

`improving` compares the raw static stack score with the same side's score two
position plies earlier, outside check and only when `height > 1`. The asserted
root-relative height invariant guarantees that this ancestor is not before
the root.
It is not currently a corrected-evaluation or NNUE-uncertainty comparison.

### Threats, danger and inactive context signals

If evaluation did not supply attack data, PVS reconstructs it with
`evalFeatures()`. `haveThreats` describes attacks on opposing pieces;
`goodThreats` emphasizes favorable attacker/victim classes, such as pawn
attacks on non-pawns or minor-piece attacks on rooks/queens.

Per-color danger contributes to the SEE pruning margins through
`dangerGoodAttack` and `dangerUnderAttack`. However, the summed-danger threshold
booleans currently record statistics rather than implementing a global
"dangerous position: reduce less" policy.

Historical booming/moobing flags are copied into `PVSData`, but
`situationDepthCorrection` is zero. Iteration-emergency flags are also forced
false there, and the relevant LMR toggle is disabled. The current historical
score repair therefore affects timing; it does not automatically enable all
the experimental history/emergency selectivity branches.

## Node-level forward pruning

These branches chiefly run at non-root, non-PV, non-check nodes, with additional
mate-finder, depth, threat and zugzwang-risk gates.

| Mechanism | Decision and motivation |
|---|---|
| Reverse futility / static null move | If evaluation exceeds beta plus a depth/phase margin, return a conservative margin-adjusted estimate without searching moves. |
| Opponent-threat pruning | Similar fail-high shortcut when no sufficiently good opponent threat is detected. |
| Razoring | If evaluation is well below alpha at shallow depth, use a qsearch verification; the no-own-threat path can return the razor threshold directly. |
| Null-move pruning | Pass the turn artificially; if the opponent still cannot bring the score below beta, try a cutoff. Verification protects selected deeper cases. |
| ProbCut | Test a limited number of promising captures against a raised beta, first in qsearch and then at reduced main-search depth. |

The coefficient helper computes:

```text
margin = (init[index1] + depth * slopeDepth[index1]
          + gamePhase * slopeGamePhase[index1]) * bonus[index2] / 256
```

Index meanings depend on the call: commonly TT-score substitution, improving
or cut-node state. Parameter names alone do not establish those meanings.
Depth activation ranges are checked separately.

Several margins use `marginDepth = max(0, depth - TTdepth)` when evaluation
has been replaced by a TT search score; others use the nominal depth. The
`depth` in the generic equation means the argument actually passed by that
heuristic, not necessarily the unchanged requested depth.

### Null move in detail

The current reduction is:

```text
R = nullMoveReductionInit
    + depth / nullMoveReductionDepthDivisor
    + min((pruningEval - beta) / nullMoveDynamicDivisor, 7)
nullDepth = max(0, depth - R)
```

The artificial child is searched with a null window. The node must satisfy
evaluation/baseline tests, cannot follow another null move, and uses material or
mobility to reduce zugzwang risk. `subSearch` and exclusion searches are gated.

A high result at sufficiently deep search can trigger a same-position
verification search. `nullMoveMinPly` and `nullMoveVerifColor` prevent recursive
verification behavior for a portion of the subtree. Unverified mate-range
null results can be capped to beta rather than asserted as actual mates.

The null child's TT move is also used as a refutation/threat hint for ordering.
In HCE-only search, the static score after a null move may be derived from the
parent plus tempo; this shortcut is not used with asymmetric NNUE.

### ProbCut and IIR

ProbCut raises beta by `probCutMargin + probCutMarginSlope * improving`,
generates captures and tries at most `probCutMaxMoves`. The child main-search
depth is `depth / probCutSearchDepthFactor`. Captures generated here can be
reused in the later main move list.

Its success-path TT write and capture-history bonuses are currently commented
out. Do not document them as active learning.

IIR reduces depth by `iirReduction` at sufficient depth when no TT hit is
available. It avoids spending the full nominal budget without a useful cache
context. Traditional IID, which performs a shallower search to discover a
move, exists but `doIID` is false.

## TT move, singular extensions and multi-cut

A pseudo-legal TT move is attempted before generating the remaining list,
unless excluded or constrained by the root tablebase list.

At sufficient depth, a non-root TT move with a useful lower/exact bound and
enough recorded depth can receive an exclusion search. Other moves are searched
around:

```text
betaC = TT score - 2 * depth
```

If they fail below `betaC`, the TT move is considered singular and gains a ply.
A stronger failure can grant a second ply, with cumulative-extension guards.
The same mechanism has multi-cut and negative-extension branches when the
evidence instead supports reducing work.

Triple extension and a separate cut-node negative-extension branch are
commented out. Furthermore, every generic extension toggle in `depthPolicy()`
is false: check, Botvinnik-Markoff threat evolution, mate threat, queen threat,
castling, recapture, good-history and CMH extensions are not currently active
there. The active singular path is separate.

## Per-move pruning

After generation/scoring and successful board application, later moves may be
skipped. Counters distinguish legal candidates from actually searched moves.
The first eligible move receives stronger protection; later selectivity has
additional advanced-pawn, check, killer and mate guards.

| Mechanism | Current use |
|---|---|
| Futility | Skip later ordinary quiet non-check moves when static evidence is too far below alpha. |
| LMP | Skip further eligible quiets after an improving/depth-dependent move-count limit. |
| Quiet history pruning | Compare the scored move with a configured threshold; can set `skipQuiet`. |
| CMH pruning | Test only the distance-1 context; missing context is not a bad-history signal. |
| Capture history pruning | Filter eligible ordinary captures with poor piece/victim history. |
| Capture SEE filtering | Drop badly scored captures using depth and danger-adjusted thresholds. |
| Quiet SEE filtering | Reject unsafe quiets with a threshold based partly on their planned child depth. |

`skipQuiet` does not make move generation deferred: the full remaining list
already exists. Root, checking, promotion, en passant and castling behavior
must be read through their actual move-type gates, not a generic quiet/capture
description.

## LMR and re-search

The precomputed base reduction approximates a logarithmic function:

```text
Rbase = int(my_log2(depth * 1.6) * my_log2(moveNumber) * 0.4)
```

`my_log2()` is the repository's numerical approximation. The name is not a
reason to substitute a different log function without retuning.

For ordinary quiet moves, active adjustments include not-improving, TT capture
context, likely fail-high, move-ordering history score, former TT-PV and known
endgame prudence. Capture reductions use capture history, bad-capture status,
PV-node and improving adjustments. Many other named switches are false.

`depthPolicy()` returns:

```text
nextDepth = depth - 1 + extension - reduction
```

It constrains reductions to avoid an accidental net extension on reducible
moves and prevents a reduced child depth below zero.

Later moves first receive a reduced null-window search. If it exceeds alpha
and there was a reduction, search again at `depth - 1 + extension`.
A promising result then gets a full-window PV search when appropriate.
This re-search target is fixed, not adaptively deeper/shallower according to
the amount of improvement.

Good ordering affects both which moves are reduced and how much: changes to
CMH weights are therefore not purely cosmetic list permutations.

## Quiescence

Production `qsearch()` resolves tactical instability beyond the main depth.
Outside check it uses a corrected stand-pat evaluation. In check, stand pat is
not a legal option; an initial mate-range score and evasion generation handle
the node.

It probes TT entries with qsearch depth conventions: eval-only entries use
`-2`; ordinary and special qsearch results use `-1` or `0`. A capture TT move
or any suitable evasion TT move can be tried before generation.

Outside check, the generated phase is `GP_cap`, which includes capture
promotions and en passant but not quiet promotion pushes. In check it is
`GP_evasion`. Bad exchanges below `seeQThreshold` are filtered. Delta pruning
can reject near-neutral captures or return beta on sufficiently favorable
exchange evidence; `doQFutility` is currently false.

Past qply 5, non-check nodes can restrict captures to the previous capture's
destination when such a recapture square exists. It is not an unconditional
capture-depth limit.

Qsearch checks `stopFlag`, height, mate windows and the post-loop fifty rule;
it does not run the full PVS repetition/material recognizer at every entry or
perform wall-clock checks itself. Terminal/draw handling is not identical to
PVS: no legal evasion establishes mate, but no capture outside check is not
a full stalemate test. The auxiliary `qsearchNoPruning()` is also different:
it generates captures even in check and must not be described as the production
evasion path.

## Draws, mate distances and endgames

PVS's interior recognizer checks repetition, material draws and the pre-loop
fifty threshold. `isRep()` starts four plies back, examines matching-side stack
history down to the root, then continues in `GameInfo`. Captures/pawn moves stop
the history walk. It uses a count limit of 2 at PV nodes and 1 otherwise;
there is no upcoming-repetition detector.

The fifty rule uses a pre-loop threshold of 101 and a post-loop threshold of
100, allowing the move loop to resolve mate cases first. No legal moves means
mate when checked and stalemate otherwise; exclusion searches have distinct
no-move behavior.

`matingScore(height) = MATE - height` and
`matedScore(height) = -MATE + height`. Window clipping implements mate-distance
pruning. TT storage converts between root-relative and position-relative mate
scores; never treat these values as ordinary centipawns.

Normal `drawScore()` is a small node-parity-dependent +1/-1, not the configured
contempt value. Contempt instead biases evaluation. Armageddon has a different
draw mapping.

Syzygy interior WDL probes can return mapped wins/draws; some winning results
also incorporate static evaluation as progress guidance. Root probing currently
supplies the single selected Fathom root move; commented code for a wider
acceptable-move set is not active.

## Learning and experimental boundaries

Beta cutoffs reward quiet/capture histories and penalize preceding list entries.
TT cutoffs can also update histories. Existing penalties are list-prefix based,
not a separately recorded list of exclusively searched moves.

Correction history learns selected non-check, non-mate, non-capture/promotion
best results, with reliable-bound gates, against the corrected static baseline.
Not every early return learns it.

Adaptive LMR retry depth, correction-magnitude selectivity, NNUE-uncertainty
selectivity, continuation correction and upcoming repetition are separate
experiments in [TODO](https://github.com/tryingsomestuff/Minic/blob/master/TODO.md). None should be inferred merely from an
unused field or disabled branch.

## Parameterized margins are context-indexed

`Coeff<N,M>` evaluates:

```text
threshold(d, gp, i, j) =
  ScoreType((init[i] + d*slopeDepth[i] + gp*slopeGamePhase[i])
            * bonus[j] / 256)
active(d, i) = minDepth[i] <= d <= maxDepth[i]
```

`bonus` is a multiplier, not an additive centipawn bonus. Depth eligibility
and the numerical threshold are separate methods. Index semantics belong to
each caller: the common eval-versus-TT/improving interpretation does not apply
universally to history/capture-history calls.

For example, static-null margin with `i=0`, `j=0`, depth 4 and phase .5 is:

```text
(-15 + 4*82 + .5*149) * 404/256 = 611.5234375 -> 611
```

The underlying cast truncates rather than rounding to the nearest score.
Any modified formula must also respect the narrow `ScoreType` assertions.

## A budget change has more than one effect

IIR changes nominal depth before the ordinary move loop. Pruning gates then
use that depth and contextual margins. LMR changes `nextDepth`, and quiet SEE
uses that reduced budget in its threshold. A fail-high can then trigger two
different retries. These are successive policies, not one additive
"selectivity score".

Do not attribute a changed tree solely to a new LMR coefficient without
checking which moves were removed before recursion and how many unreduced
retries it caused. Disabled extra reduction/extension predicates likewise
need upstream signal and caller-eligibility checks.

For an executable-style specification of all generic reduction/extension
switches, TT singular alternatives, clamps, eligibility and retry windows,
see the [search policy reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md).

## Selected current defaults

These are starting values in [searchConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.cpp),
not unconditional guarantees that a heuristic runs at the corresponding depth.

| Parameter/family | Value |
|---|---|
| Aspiration | Enabled after depth 4; initial delta `5 + max(0, 40 - 3 * depth)`. |
| Null move | Minimum depth 2; verification threshold 6; reduction constants 5, 6 and 147. |
| ProbCut | Minimum depth 10; at most 5 tested legal captures; margin `124 - 53 * improving`; child depth divisor 3. |
| IIR | Minimum depth 3; reduction 1. |
| Singular probing | Minimum depth 8; separate TT-depth adequacy parameter 5. |
| LMR | Minimum depth 2, plus move/node eligibility gates. |
| Qsearch SEE | Reject exchanges below -50, outside check. |
| CMH pruning | Depth below 4 for either improving state; margin -256. |
| Direct TT cutoff fifty gate | Counter below 92 on applicable paths. |
| Late TT bounds | Depth allowance 1; alpha/beta margins 60/64. |
| PVS lazy-sort threshold | -491. |

Depth/phase coefficient tables separately govern futility, razoring, threat
and history thresholds. Use their call-site indices and inclusive activation
range; copying a single slope is not reproducing the heuristic.
