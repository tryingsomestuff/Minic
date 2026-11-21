# Search and selectivity

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md) | [Previous: positions and moves](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/positions-and-moves.md) | [Next: move ordering](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/move-ordering.md)

Primary sources: [searcherDriver.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherDriver.cpp),
[searcherPVS.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherPVS.hpp),
[searcherQSearch.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherQSearch.cpp),
[searcherDraw.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherDraw.cpp),
[searcherDraw.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherDraw.hpp),
[searcher.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcher.hpp),
[searchConfig.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.hpp) and
[searchConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.cpp).

This chapter explains the search concepts and how Minic implements each of
them. Exact predicates, depth arithmetic, coefficient tables and the full list
of disabled switches are in the
[search policy reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md);
move scoring is described in
[move ordering](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/move-ordering.md).

## Negamax, alpha-beta and bounds

A chess engine looks for the best move by exploring the game tree: the root is
the current position, each edge a legal move. *Minimax* assumes both sides pick
their best move. *Negamax* is the same idea written with a single function:
scores are always from the point of view of the side to move, so the value of
a node is the maximum of the negated values of its children.

*Alpha-beta* adds a window `[alpha, beta]`. `alpha` is the score the side to
move is already guaranteed elsewhere in the tree; `beta` is the score above
which the opponent will avoid this line. A move scoring at least `beta`
produces a *beta cutoff* (fail high): the remaining moves need not be searched.
If no move exceeds `alpha` the node *fails low*. Descending one ply negates and
swaps the window:

```text
score = -search(child, -beta, -alpha)
```

A fail-high only proves a lower bound, a fail-low only an upper bound. The
transposition table (TT) records which: `B_beta` is a lower bound, `B_alpha` an
upper bound and `B_exact` a score strictly inside the window. Minic is mostly
*fail-soft* (it returns the best score found even when outside the window), but
several heuristics deliberately return `alpha`, `beta` or a margin-adjusted
estimate.

The recursive function is the template `pvs<pvnode>()` in
[searcherPVS.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherPVS.hpp):

```cpp
template<bool pvnode>
ScoreType Searcher::pvs(ScoreType                    alpha,
                        ScoreType                    beta,
                        const Position&              p,
                        DepthType                    depth,
                        DepthType                    height,
                        PVList&                      pv,
                        DepthType&                   seldepth,
                        DepthType                    extensions,
                        bool                         isInCheck_,
                        bool                         cutNode_,
                        const std::vector<MiniMove>* skipMoves) {
   assert(height == p.halfmoves - rootHalfmoves);
```

| Argument | Role |
|---|---|
| `alpha`, `beta` | Current window; a null window has `beta == alpha + 1`. |
| `depth` | Remaining main-search budget, in plies. Reductions and extensions change it for the child. |
| `height` | Distance from the root of this search, used for mate scores, killers and `seldepth`. |
| `pv`, `seldepth` | Outputs: the principal variation found below this node and the deepest ply reached. |
| `extensions` | Cumulative extensions along the path; the double singular extension checks it. |
| `isInCheck_`, `cutNode_` | Hints passed by the parent so that they need not be recomputed or guessed. |
| `skipMoves` | Moves to exclude: this turns the call into an exclusion search of the same position (singular probe, MultiPV). |

Three node types are usually distinguished. A *PV node* is searched with an
open window and may return an exact score; it is the template parameter
`pvnode`. A *cut node* is expected to fail high, an *all node* to fail low.
`cutNode` is only a hint passed down the tree, not a proven type. A third,
related flag, `ttPV`, says that the TT remembers this position as having been
on a PV.

The `assert` states the contract between the two coordinate systems described
in [positions and moves](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/positions-and-moves.md):
`height` is the absolute ply minus the root's absolute ply. Searches of the
*same* position (singular probe, null-move verification) keep `height`; a real
child receives `height + 1`.

## The driver: iterative deepening and aspiration

*Iterative deepening* repeats the whole search at depth 1, 2, 3, ... instead of
going directly to the target depth. Each iteration is much cheaper than the
next, and the shallow ones provide a best move at any time (needed when the
clock runs out), a score to center the next window on and a TT filled with good
moves that improve ordering in the next iteration.

`Searcher::searchDriver()` in
[searcherDriver.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherDriver.cpp)
iterates up to `min(requested depth, MAX_DEPTH - 6)`; skill settings can lower
the depth or impose a node budget. Before the loop, the node limit is set to
zero so that depth 1 always completes; the real value is restored after the
first iteration. Several shortcuts precede the loop: a forced "bongcloud" king
move when the option is set, a random mover (level 0, the first `randomPly`
plies, or almost no time), and, for a single-PV timed search on the main
thread, a depth-1 search whose only purpose is to detect a single legal move
and shorten the allotted time. Helper threads skip some depths using a
Stockfish-like `skipSize`/`skipPhase` schedule; see
[time and parallelism](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/time-and-parallelism.md).

An *aspiration window* is a gamble: a narrow window around the previous
iteration's score produces more cutoffs, but if the true score falls outside
it the iteration is repeated with a wider window. Minic uses one from depth
`aspirationMinDepth + 1` (depth 5) onward, with an initial half-width
`5 + max(0, 40 - 3 * depth)`. After every attempt the window grows:

```cpp
delta += static_cast<ScoreType>((delta / 4) * std::exp(1.f - gamePhase(p.mat,matW,matB))); // in end-game, open window faster
if (delta > std::max(128, 1024*4/depth) ) delta = matingScore(0);
if (alpha > matedScore(0) && score <= alpha) {
   windowDepth = depth;
   beta  = std::min(matingScore(0), static_cast<ScoreType>((alpha + beta) / 2));
   alpha = std::max(static_cast<ScoreType>(score - delta), matedScore(0));
   ...
}
else if (beta < matingScore(0) && score >= beta) {
   windowDepth = std::max(static_cast<DepthType>(1), static_cast<DepthType>(windowDepth - 1)); // from Ethereal
   beta = std::min(static_cast<ScoreType>(score + delta), matingScore(0));
   ...
}
else
   break;
```

On a fail-low, beta moves to the middle of the old window, alpha drops below
the returned score, and the full depth is restored. On a fail-high, beta rises
and the retry is one ply shallower (a fail-high is usually confirmed cheaply).
The growth factor $1 + e^{1-g}/4$ is larger in the endgame (game phase
$g \to 0$). Once `delta` exceeds `max(128, 4096 / depth)` the window becomes the
full mate range. On the main thread with a single PV, intermediate results are
printed with a `?` (fail-low) or `!` (fail-high) mark.

*MultiPV* reports several best lines: each line repeats the aspiration loop
with the moves already chosen at this depth passed as `skipMoves`. Each line
keeps its own score as aspiration center; only the first line updates the main
result. If the search is stopped, the interrupted iteration is discarded and the
last completed result is kept, so `STOPSCORE` is never published.

After iterative deepening the main thread selects the move by a depth-weighted
vote among all threads and takes the PV of the deepest supporter (unless skill
limitation picks a move from the MultiPV list instead). The driver also adds a
dynamic contempt term, `round(25 * tanh(score / 400))`, to the configured
contempt when the latter is non-negative; contempt biases the evaluation, not
`drawScore()`.

## PVS control flow

The order of operations inside `pvs()` matters because many steps return early:

1. Periodic stop/time check (main thread or stoppable co-searcher, every 1024
   nodes); node data initialization.
2. Static evaluation returned if height or depth reaches `MAX_DEPTH - 1`.
3. Non-root interior draw recognition (repetition, material, fifty-move rule).
4. Quiescence search when `depth <= 0`.
5. `seldepth` and node count update; non-root mate-distance pruning.
6. Continuation-history pointers, position hash (mixed with excluded moves),
   known-endgame classification.
7. TT probe: direct cutoffs and margin-based "late" cutoffs at non-PV nodes.
8. Syzygy WDL probe at non-root nodes.
9. Static evaluation, possibly replaced by a TT score; correction history.
10. Threat and danger data, `improving`, zugzwang risk.
11. Non-PV node pruning: static null move, threat pruning, razoring, null move,
    ProbCut.
12. Internal iterative reduction; killer reset for the children; per-move
    pruning gates.
13. Root tablebase move list; TT move search, including the singular probe.
14. Generation of the remaining moves, scoring, and the move loop with per-move
    pruning, LMR and PVS re-searches.
15. Checkmate/stalemate and fifty-move handling, correction-history learning,
    TT store.

Histories, the TT and correction history are therefore not updated on every
path. The [reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md#pvs-step-by-step)
gives the exact condition of every step.

## The transposition table inside the search

The same position is often reached through different move orders
(*transpositions*). The TT stores, per position hash, the best move, a score
with its bound type, the search depth and the static evaluation (see
[tables and history](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/tables-and-history.md)).
In `pvs()` the entry serves four purposes:

- *Cutoff*: at a non-PV node, an entry at least as deep as the current depth
  whose bound proves the window can be returned directly, provided the
  fifty-move counter is below 92 and the material is not a known endgame.
  Minic also returns `alpha`/`beta` from an upper/lower-bound entry that is at
  least `depth - 1` deep when the stored score lies beyond the window by a
  depth-proportional margin (an idea from Ethereal).
- *Evaluation cache*: the stored static evaluation avoids calling `eval()`.
  On a miss, Minic immediately writes an "evaluation only" entry at depth -2.
- *Better estimate*: a deeper search score whose bound points in the right
  direction replaces the static score for pruning decisions.
- *Move ordering*: the TT move is searched first, before any move generation.

Mate scores are stored relative to the node (`createHashScore()` adds the
height to a mating score) and converted back on reading (`adjustHashScore()`),
because the distance to mate differs for each path reaching the position.

## Evaluation values are not interchangeable

Several "evaluations" coexist in `pvs()`:

- The raw static score is stored in `stack[p.halfmoves].eval` (in check, the
  value `matedScore(height)` is stored instead) and copied into `staticScore`.
- `evalScore` may then be replaced by a TT search score when the entry's bound
  supports it; `evalScoreIsHashScore` records that substitution.
- `correctedStaticScore` adds the pawn and non-pawn correction-history terms to
  `staticScore`. `pruningEval` uses that corrected value, unless a TT search
  score has replaced it, in which case no correction is added (a search score is
  already free of the static-evaluation bias that correction history learns).

*Improving* compares the stored raw static score with the one of the same side
two plies earlier, only when `height > 1` and not in check. When the side to
move is improving, quiet reductions are smaller and razoring, futility, LMP
and history pruning are less aggressive. The static null move and ProbCut
are the exceptions: their margins are smaller when improving, because a node
that is both improving and above beta is very likely to stay there. This is
not a corrected-evaluation comparison.

### Threats, danger and inactive context signals

If evaluation did not provide attack data (NNUE or TT evaluation),
`pvs()` reconstructs it with `evalFeatures()`. `haveThreats[c]` says that color
`c` attacks some enemy piece; `goodThreats[c]` restricts this to favorable
attacks (pawn on a piece, minor on a rook or queen, rook on a queen).
King-danger sums, divided by `dangerDivisor` (183), enlarge the SEE pruning
thresholds when the side to move is attacking.

Several context signals are computed but have no effect by default: the
"danger" thresholds only increment statistics; the booming/moobing game-history
flags feed only time management (`situationDepthCorrection` is zero); the
iteration-emergency flags are forced false; and the asymmetry correction is
zero. As a result `depthCorrection`, added to the LMP depth index, is always 0.

## Node-level forward pruning

*Forward pruning* abandons a node or a move before searching it, on the basis
of static evidence. It is unsound by nature: it saves a large amount of work
at the cost of occasionally missing a tactic. Minic applies the following
node-level tests only at non-PV, non-root nodes that are not in check, and
never in `MateFinder` mode (a runtime option, disabled by default, which turns
off forward pruning and LMR for mate search).

| Mechanism | Idea | Minic implementation |
|---|---|---|
| Static null move (reverse futility) | If the static score is far above beta, a real move will almost certainly keep it there. | Depth 0..6, low zugzwang risk: return `pruningEval - margin` if `pruningEval > beta + margin`. |
| Threat pruning (from Koivisto) | Same test when the opponent has no good threat. | Depth 0..2, `!goodThreats[opponent]`, smaller margin. |
| Razoring | If the static score is far below alpha at shallow depth, only tactics can save the node. | Depth 0..2: without own threats return the razor bound directly, else verify with qsearch. |
| Null move | Pass the turn; if the opponent still cannot reach beta, cut. | Depth >= 2, verification above depth 6. |
| ProbCut | A capture that beats `beta + margin` at reduced depth very probably beats beta at full depth. | Depth >= 10, at most 5 captures, qsearch then depth/3 search. |

Most margins are produced by a shared helper, `Coeff::threshold()`, linear in
depth and game phase and scaled by a contextual factor (for instance improving
or not); see the
[coefficient table](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md#parameterized-margins).
An own-threat pruning variant (return alpha when the side to move has no good
threat and is far below alpha) exists in commented-out code, with its
coefficient `ownThreatCoeff` also commented out: it is disabled by default and
needs both to be restored to be enabled.

### Null move

The null-move idea is that passing is almost always worse than playing the best
real move. If the side to move is still above beta after giving the opponent a
free move, searched at a strongly reduced depth, the node is cut. The exception
is *zugzwang*, where every move worsens the position (typical of pawn
endgames); Minic therefore requires `lessZugzwangRisk` (the side to move has a
piece other than king and pawns, or a mobility above 4) and, for deep searches,
re-verifies the cutoff with a normal reduced search of the same position. With
`R = 5 + depth / 6 + min((pruningEval - beta) / 147, 7)`, the null child at
depth 6 or below is directly a quiescence search. A mate score from the null
search sets `mateThreat`, and the opponent's TT move after the null move is
kept as a *refutation*: it gives the threat-escape ordering bonus and is
compared with the previous refutation by the (disabled) Botvinnik-Markoff
extension. The code and its verification logic are quoted in the
[reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md#null-move-pruning-and-verification).

Without NNUE, the static evaluation of the position after a null move is
derived from the parent: `2 * tempo - parentEval`. This shortcut assumes a
symmetric evaluation and is not used with NNUE.

### ProbCut, IIR and IID

ProbCut raises beta by `124 - 53 * improving`, tries the best non-bad captures
(skipping the TT move), first with a qsearch and then at `depth / 3`, and
returns the score of the first one that confirms. It requires the side to move
to have a threat and either a cut node or a static score already above the
raised beta, and is skipped when a sufficiently deep TT entry says the score is
below it. Captures generated by ProbCut are kept and reused by the main loop.
Two learning steps after a ProbCut success, a capture-history bonus and a TT
store, are commented out (disabled by default).

*Internal iterative deepening* (IID) is the classical cure for a node without
TT move: run a shallower search first to find one. *Internal iterative
reduction* (IIR) is a cheaper alternative: a node without any TT entry is
probably not important, so it is simply searched one ply shallower. Minic uses
IIR (`doIIR`, depth >= 3, reduction 1). Note that an evaluation-only entry
counts as a TT hit, so IIR applies essentially on the first visit of a
position. IID is implemented but disabled by default (`doIID` is false): at PV
nodes from depth 7 or cut nodes from depth 10 without a TT move, it would
search at `depth / 2` and re-read the TT entry. Enabling it means setting
`doIID` to true in
[searchConfig.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.hpp)
and rebuilding.

## TT move, singular extensions and multi-cut

An *extension* searches a move deeper than normal because a shallow horizon
would misjudge it. A *singular extension* targets a TT move that is clearly
better than all alternatives: if every other move, searched at reduced depth
with a window just below the TT score, fails low, the TT move is the only
reasonable choice and deserves more depth. Conversely, if the alternatives also
reach that window while it is itself above beta, several moves beat beta and
the node can be cut at once (*multi-cut*).

In Minic the probe runs at non-root nodes of depth >= 8 with strength level
above 80, a non-mate lower-bound or exact TT score and a TT depth of at least
`depth - 5`. The core, from `pvs()`:

```cpp
const ScoreType betaC = e.s - 2 * depth;
PVList sePV;
DepthType seSeldepth = 0;
std::vector<MiniMove> skip{e.m};
const ScoreType score = pvs<false>(betaC - 1, betaC, p, depth / 2, height, sePV, seSeldepth, extensions, pvsData.isInCheck, pvsData.cutNode, &skip);
if (stopFlag) return STOPSCORE;
if (score < betaC /*&& extensions <= 6*/) { // TT move is singular
   stats.incr(Stats::sid_singularExtension);
   pvsData.ttMoveSingularExt=true;
   ++extension;
   // TT move is "very singular" and depth is small : kind of single reply extension
   if (/*!pvsData.pvnode &&*/ score < betaC - 4 * depth && extensions <= 6) {
      stats.incr(Stats::sid_singularExtension2);
      ++extension;
      ...
   }
}
// multi cut (at least the TT move and another move are above beta)
else if (betaC >= beta) {
   return stats.incr(Stats::sid_singularExtension3), betaC; // fail-soft
}
// if TT move is above beta, we try a reduce search early to see if another move is above beta (from SF)
else if (e.s >= beta) {
   ...
   extension = -2 + pvsData.pvnode;
   stats.incr(Stats::sid_singularExtension4);
}
...
else if (e.s <= alpha){
   extension = -1;
   stats.incr(Stats::sid_singularExtension5);
}
```

The probe keeps `height` and does not initialize a child slot, because it
searches the same position with the TT move excluded; the child position for
the TT move is copied only after the probe. The excluded move is mixed into the
hash, so the probe has its own TT entry. When the excluded TT move is the only
legal move, the move loop of the exclusion search ends by returning `alpha`
(`betaC - 1`), which counts as singular. When the test fails without a
multi-cut, a *negative extension* searches the TT move shallower: by 2 plies
(1 at PV nodes) when the TT score is already above beta, by 1 ply when it is at
or below alpha.

A third extension ply, an additional verification search before the negative
extension and a cut-node negative extension are commented out (disabled by
default). The nine generic extensions of `depthPolicy()` (check, in-check,
Botvinnik-Markoff, mate threat, queen threat, castling, recapture, good history,
continuation history) are compiled-out `constexpr` toggles, all false: the
singular path is the only active extension. See the
[extension inventory](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md#generic-extension-inventory-all-disabled).

## The move loop: PVS and late move reductions

*Principal variation search* (PVS) relies on good move ordering: the first
move is searched with the full window because it is expected to be best;
later moves are only asked a cheap question, "is it better than alpha?", with a
null window. Only if the answer is yes is the move re-searched with the full
window. *Late move reduction* (LMR) makes the question cheaper still by
searching late, poorly ordered moves at reduced depth; a reduced search that
unexpectedly beats alpha is repeated at normal depth.

Minic follows three paths: the TT move (searched before generation, never
reduced by LMR), the first generated move when there was no legal TT move
(full window, no LMR), and all later moves (pruning, then reduced null window
and retries). The later-move code in `pvs()` is:

```cpp
score = -pvs<false>(-alpha - 1, -alpha, child, nextDepth, height + 1, childPV, seldepth, static_cast<DepthType>(extensions + extension), pvsData.isCheck, true);
if (reduction > 0 && score > alpha) {
   stats.incr(Stats::sid_lmrFail);
   childPV.clear();
   score = -pvs<false>(-alpha - 1, -alpha, child, depth - 1 + extension, height + 1, childPV, seldepth, static_cast<DepthType>(extensions + extension), pvsData.isCheck, !pvsData.cutNode);
   ...
}
if ( score > alpha && (pvsData.rootnode || score < beta)) {
   stats.incr(Stats::sid_pvsFail);
   childPV.clear();
   // potential new pv node
   score = -pvs<true>(-beta, -alpha, child, depth - 1 + extension, height + 1, childPV, seldepth, static_cast<DepthType>(extensions + extension), pvsData.isCheck, false);
}
```

`nextDepth = depth - 1 + extension - reduction` comes from `depthPolicy()`.
The base reduction is a precomputed table
$\lfloor 0.4\,\ln(1.6d)\,\ln(n)\rfloor$ over depth $d$ and move number $n$
(the helper is called `my_log2()` but computes a natural logarithm). Quiet
moves are reduced one ply more when not improving, when the TT move is a
capture and when a cut node looks likely to fail high; the reduction shrinks for
a good ordering score (and grows for a very negative one), at TT-PV nodes and
in known endgames. Captures use capture history, a
bad-capture increment and PV/improving prudence. The re-search depth is fixed
(`depth - 1 + extension`); it does not vary with how much the reduced result
exceeded alpha. Exact terms and the fourteen disabled ones are listed in the
[reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md#base-lmr-table).

Because ordering scores feed LMR directly, history divisors, bonuses and
continuation weights change which tree is searched, not just its order. By
default only the distance-1 continuation history contributes
(`CMHWeight = {8, 0, 0, 0}`).

A root shortcut meant to return `alpha - failLowRootMargin` (118) when the
first root move scores that far below alpha is written so that it never fires;
see the [reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md#root-specifics).

### Per-move pruning

Before the reduced search, a later move can be skipped:

| Mechanism | Idea | Minic gate |
|---|---|---|
| Futility pruning | Near the leaves, a quiet move cannot recover a static score far below alpha. | Depth <= 10; skips the remaining quiets. |
| Late move pruning (LMP) | Late quiets in a well-ordered list rarely matter. | Depth <= 10; legal-move count above a table limit. |
| History pruning | Quiets that never caused cutoffs are unlikely to now. | Depth <= 2 (1 when improving); ordering score below a threshold. |
| Continuation-history pruning | Same, in the context of the opponent's last move. | Depth < 4; distance-1 entry below -256; skips only this move. |
| Capture-history pruning | Captures with a poor history. | Depth <= 4 (3 when improving). |
| SEE pruning | *Static exchange evaluation* estimates the material balance of the exchange sequence on the destination square; losing exchanges are skipped. | Bad captures and quiets, thresholds growing with depth and attacking danger. |

These tests apply only at non-root nodes, to moves that are not advanced pawn
pushes or current-height killers, when alpha is not a mate score and neither
the parent nor the child is in check. Futility, LMP and
history failures set `skipQuiet`, after which every later prunable quiet is
skipped without testing; the move list itself is already fully generated. The
formulas are in the
[reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md#per-move-pruning-equations).

## Quiescence search

Stopping the search at a fixed depth creates the *horizon effect*: a position
in the middle of an exchange can be evaluated as if the hanging piece were
safe. A *quiescence search* (qsearch) continues with captures only until the
position is quiet. At each node the side to move may *stand pat*, i.e. accept
the static evaluation, because it is not forced to capture; it searches
captures only to try to improve on it. In check, standing pat is not legal, so
all evasions are searched.

Minic's `qsearch()` in
[searcherQSearch.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherQSearch.cpp):

- uses the corrected static score as stand pat, and returns it at once if it
  reaches beta; in check the initial score is `matedScore(height)`;
- probes the TT with qsearch depths and cuts at non-PV nodes (entries written at depth 0 in check or at
  the qsearch root, -1 elsewhere; evaluation-only entries at -2 are never
  cutoffs) and tries a capture or evasion TT move first;
- generates captures (including capture-promotions and en passant, but not
  quiet promotions) outside check and evasions in check, ordered by MVV-LVA and
  capture history rather than SEE;
- outside check, skips captures with SEE below -50, applies *delta pruning*
  (skip a near-neutral capture when the score is far below alpha, return beta
  when a clearly winning capture starts close to beta) at non-TT-PV nodes, and
  once the qsearch ply counter exceeds 5 only searches recaptures on the
  square of the last capture;
- returns a mate score when no evasion exists, and a draw when the fifty-move
  counter reaches 100 after the loop.

It does not check the clock (the main search does) nor repetitions, and an
empty capture list outside check is not a stalemate test. Capture-based
qsearch futility (`doQFutility`) is implemented but disabled by default. The
auxiliary `qsearchNoPruning()`, used by developer tools (quiet-position
extraction for data generation, evaluation tuning, PGN processing), has no
pruning and no TT, stands pat and generates only captures even in check (a
commented line would generate all moves when in check). Exact thresholds are in the
[qsearch reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md#quiescence-reference).

## Draws, mate distances and endgames

At every non-root node, `interiorNodeRecognizer()` returns a draw score for a
repetition, an insufficient-material draw or a fifty-move counter of at least
101. The repetition test, `isRep()` in
[searcherDraw.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherDraw.cpp),
walks back two plies at a time (only positions with the same side to move can
be equal), first through the search stack, then through the game history:

```cpp
bool Searcher::isRep(const Position& p, bool isPV) const {
   // handles chess variants
   const int limit = isPV ? 2 : 1;
   if (p.fifty < (2 * limit - 1)) return false;
   int count = 0;
   const Hash h = computeHash(p);
   int k = p.halfmoves - 4;
   bool irreversible = false;
   ...
   // look in stack first
   for ( ; k >= rootHalfmoves; k-=2) {
      ...
      if (stack[k].h == nullHash) break; // no more "history" in stack (will look at game state later)
      if (stack[k].h == h){
         ...
         ++count;
         if (count >= limit){
            ...
            return true;
         }
      }
      ...
   }
   // Continue through pre-root game history
   while(!irreversible && k>=0){
      const std::optional<Hash> curh = COM::GetGameInfo().getHash(k);
      if (!curh.has_value()) break;
      ...
      k-=2;
   }
   return false;
}
```

The omitted lines stop the walk at an irreversible move (a capture or a pawn
move), checking both moves between two compared positions. The two limits are
a compromise: inside the tree, one earlier occurrence is treated as a draw
(the side that repeated can repeat again, so searching further is wasted),
whereas a PV node requires two earlier occurrences, a real threefold
repetition, so that a reported PV draw is genuine. There is no
upcoming-repetition (cycle) detection.

The fifty-move rule is tested at 101 before the move loop and at 100 after it,
so that a checkmate delivered on the hundredth reversible ply is preferred to
the draw. No legal move means checkmate when in check and stalemate otherwise;
an exclusion search without any remaining move returns `alpha` instead.

`drawScore()` returns +1 or -1 depending on the parity of the node counter, a
small pseudo-random perturbation that keeps draw scores from being exactly
equal; under the Armageddon option, a draw is a loss for White.
`matingScore(h) = MATE - h` and `matedScore(h) = -MATE + h` (with
`MATE = 10000`) encode the distance to mate; any score within `MAX_DEPTH`
(127) of $\pm$`MATE` counts as a mate score (`isMateScore()`), while
`eval()` clamps its positional score with `clampScore()` to
$\pm$(`MATE - 2 * MAX_DEPTH`), so that an evaluation is not mistaken for a
mate. *Mate-distance pruning* clips the
window to `[matedScore(height), matingScore(height + 1)]`: no line from this
node can do better than mating at the next ply.

With Syzygy tablebases, an interior WDL probe returns ±2000 plus the static
evaluation for a win/loss (to keep making progress), a draw score for cursed
wins and blessed losses, and 0 for a draw. At the root, `probe_root()` replaces
the move list with the single move selected by Fathom; code for a list of all
moves preserving the result, and for storing interior tablebase hits in the TT,
is commented out (disabled by default).

## Learning during the search

On a beta cutoff outside check, a quiet move updates the killer, counter-move
and history tables, and every earlier quiet in the list receives a malus; a
capture updates capture history likewise. The bonus depth is increased by one
when the cutoff exceeds beta by more than 170. TT cutoffs at non-PV nodes also
reward or penalize the stored move. Earlier list entries are penalized even if
they were pruned rather than searched. The update rules are in
[tables and history](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/tables-and-history.md).

*Correction history* learns how much the static evaluation tends to be wrong
for a given pawn structure or non-pawn material configuration. Minic updates it
at the end of `pvs()` for non-check nodes whose best move is quiet and whose
score is not a mate, skipping bounds that carry no information relative to the
corrected static score. A third, depth-context ("horizon") correction is
compiled out by default (`WITH_HORIZON_CORRHIST` is commented out in
[config.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/config.hpp)),
so `horizonCorrection()` returns 0.

## Disabled techniques at a glance

Many search ideas are implemented but switched off. They are documented with
their exact arithmetic in the
[reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md#other-disabled-or-inert-search-code)
and inventoried across the whole engine in
[implemented but disabled techniques](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/disabled-features.md).

| Technique | Gate (all disabled by default) |
|---|---|
| Nine generic extensions | `constexpr bool enable...Extension = false` in `depthPolicy()` |
| Fourteen extra LMR terms | `constexpr bool enable... = false` in `depthPolicy()` |
| IID | `SearchConfig::doIID = false` |
| Qsearch futility | `SearchConfig::doQFutility = false` |
| Own-threat pruning | Commented-out block and coefficient |
| Third singular ply, extra singular verification, cut-node negative extension | Commented-out branches |
| ProbCut history bonus and TT store; post-LMR history updates | Commented-out blocks |
| PV-node TT cutoff; TT store of tablebase hits; multi-move tablebase root list | Commented-out blocks |
| Reducing bad-SEE quiets one more ply | Commented-out line |
| Danger-, emergency-, asymmetry- and game-history-based selectivity | Flags computed but unused or forced to 0 |
| Horizon correction history | `WITH_HORIZON_CORRHIST` not defined |
| Check extension at the horizon (search checked leaves at depth 1) | Commented-out line before the qsearch call |
| No pruning/reduction in pawn endgames, no null move/ProbCut in known endgames, more "early" root moves | Commented-out eligibility terms |
| Classic node-level qsearch delta margin (`qDeltaMargin()`) | Function without caller |
| Mate finder mode | Runtime option `MateFinder`, false by default |

## Selected current defaults

These are starting values in
[searchConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.cpp)
and [searchConfig.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.hpp),
not guarantees that a heuristic runs at the corresponding depth.

| Parameter/family | Value |
|---|---|
| Aspiration | From depth 5; initial half-width `5 + max(0, 40 - 3 * depth)`. |
| Null move | Minimum depth 2; margin 18; verification above depth 6; reduction `5 + d/6 + min(excess/147, 7)`. |
| ProbCut | Minimum depth 10; at most 5 legal captures; margin `124 - 53 * improving`; child depth `d/3`. |
| IIR | Minimum depth 3; reduction 1. |
| Singular extension | Minimum depth 8; TT depth at least `d - 5`; double extension below `betaC - 4d` while cumulative extensions <= 6. |
| LMR | Minimum depth 2. |
| LMP | Depth <= 10, table indexed by improving and depth. |
| SEE pruning | Captures `-(65 + 180 * (d - 1 + danger/8))`; quiets `-(32 + 61 * (nd - 1) * (nd + danger/11))`; qsearch -50. |
| CMH pruning | Depth below 4 for both improving states; margin -256. |
| Direct TT cutoff fifty gate | Counter below 92. |
| Late TT cutoffs | Depth allowance 1; margins 60 (alpha) and 64 (beta) per ply. |
| Qsearch delta pruning | Skip if SEE <= 20 and score + 169 < alpha; return beta if SEE >= 162 and score + 47 > beta. |
| PVS lazy-sort threshold | -491. |
