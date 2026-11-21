# Implemented but disabled techniques

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md) | [Previous: time and parallelism](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/time-and-parallelism.md) | [Next: development](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/development.md)

Primary sources: [config.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/config.hpp),
[searchConfig.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.hpp),
[searchConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.cpp),
[searcherPVS.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherPVS.hpp),
[searcherQSearch.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherQSearch.cpp),
[moveSort.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/moveSort.cpp),
[correctionHistory.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/correctionHistory.cpp),
[eval.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/eval.cpp),
[dynamicConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/dynamicConfig.cpp),
[option.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/option.cpp).

Minic contains many search, evaluation and infrastructure ideas that are
written, compiled or at least kept in the source, but do not influence play in
the default desktop build. This chapter lists them. Each entry says what the
technique is meant to do, where its gate is, how to turn it on, and what the
code itself says about its limits. "Disabled by default" in this chapter always
means: in the default `make` build with the default runtime options.

An inventory like this has two uses. It gives a map of experiments that have
already been tried, so the same idea is not written twice. It also shows which
"obvious" switches are not enough on their own, because another signal they
depend on is fixed to zero.

## How code gets disabled

Minic uses several disabling layers. They have different costs, and each one
needs a different action to turn it back on.

| Layer | What it looks like | Effect on the binary | How to enable |
|---|---|---|---|
| Compile-time feature macro | `//#define WITH_...` in [config.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/config.hpp) | The guarded code is not compiled at all; data structures may change size | Uncomment the `#define` (or pass `-D...` to the compiler) and rebuild |
| Search boolean | `inline const bool doXxx = false;` in [searchConfig.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.hpp) | Compiled, but the compiler folds the constant condition and removes the branch | Edit to `true` and rebuild. These flags are `const` even in `WITH_SEARCH_TUNING` builds, so they are never UCI options |
| Local `constexpr` switch | `constexpr bool enableXxx = false;` inside `Searcher::depthPolicy()` | `if constexpr` drops the branch at compile time | Edit the switch and rebuild |
| Commented-out code | `/* ... */` blocks or `//` lines inside an active function | Not compiled; may have drifted from the surrounding code | Uncomment, check that it still compiles and that the names it uses still mean the same thing, rebuild |
| Inert default | Code is active, but a weight, factor or flag makes it a no-op (`CMHWeight = {8, 0, 0, 0}`, `situationDepthCorrection = 0`, `isEmergencyDefence = false`, style sliders at 50) | Runs (sometimes at a small cost) but changes nothing | Change the constant (rebuild) or, for tunable values, use a `WITH_SEARCH_TUNING`/`WITH_EVAL_TUNING` build that exposes it |
| Runtime option off by default | `bool xxx = false;` in [dynamicConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/dynamicConfig.cpp), registered in [option.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/option.cpp) | Fully compiled and available | `setoption name ... value true` (UCI) or a `-name value` command-line argument; no rebuild |
| Tuning-only build | `WITH_SEARCH_TUNING`, `WITH_EVAL_TUNING`, `WITH_PIECE_TUNING` | Constants become mutable variables and extra options or commands appear | Uncomment in `config.hpp` and rebuild; expect lower speed |

Two practical tests tell the layers apart. If a symbol is missing from the
linked binary or an option is missing from the `uci` option list, it is a
compile-time gate. If the option is listed but the feature never triggers,
look for a producer signal that is fixed to false or to zero (several examples
follow). The three configuration layers themselves are described in
[architecture](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/architecture.md#three-configuration-layers),
and the build switches (`NOPROFILE=1 make`, `EXE=...`, `EVALFILE=...`) in
[development](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/development.md).

## Search: pruning, reductions and extensions

The search itself is described in
[search](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search.md); the exact
arithmetic of `depthPolicy()` is in the
[search policy reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md).

### Internal iterative deepening (IID)

**Idea.** Move ordering is best when the transposition table (TT) supplies a
move found by an earlier search. When a node has no TT move, *internal iterative
deepening* first runs a shallower search of the same node (here at `depth / 2`)
only to put a best move in the TT. The full-depth search then starts with that
move. The cost is the extra search. The benefit is better ordering at important nodes.

**Gate.** `SearchConfig::doIID = false` in
[searchConfig.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.hpp).
Disabled by default. The active replacement is `doIIR = true`, *internal
iterative reduction*. It does not search to find a move; it reduces the depth
by `iirReduction` when there is no TT hit at `depth >= iirMinDepth` (3).

From `Searcher::pvs` in
[searcherPVS.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherPVS.hpp):

```cpp
   // Classic IID
   if (SearchConfig::doIID && !pvsData.validTTmove /*|| e.d < depth-4*/) {
      if ((pvnode && depth >= SearchConfig::iidMinDepth) || (pvsData.cutNode && depth >= SearchConfig::iidMinDepth2)) { ///@todo try with cutNode only ?
         stats.incr(Stats::sid_iid);
         PVList iidPV;
         DISCARD pvs<pvnode>(alpha, beta, p, depth / 2, height, iidPV, seldepth, extensions, pvsData.isInCheck, pvsData.cutNode, skipMoves);
         if (stopFlag) return STOPSCORE;
         TT::getEntry(*this, p, pHash, 0, e);
         ...
```

**Caveats.**
- IID runs after IIR, so `iidMinDepth` (7, PV nodes) and `iidMinDepth2` (10,
  cut nodes) are compared with the already-reduced depth.
- Both IID and IIR increment the same `sid_iid` counter.
- `iidMinDepth3` (3) is defined and registered as a tuning option, but no code reads it.
- After the internal search, the block refreshes TT-derived flags and possibly
  `evalScore`/`pruningEval`. A re-enabled IID therefore also changes the
  pruning inputs for the rest of the node.

### Opponent-threat pruning variant: "own threats" pruning

**Idea.** Active *threat pruning* returns early when the opponent has no good
threats and the static score is already far above beta. The commented
mirror image would fail low when the side to move has **no** good threats of
its own and the score is far below alpha: without any threat, a big
improvement looks unlikely.

**Gate.** Commented-out block in `Searcher::pvs`
([searcherPVS.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherPVS.hpp)). The
coefficient it needs, `ownThreatCoeff`, is also commented out in both
`searchConfig.hpp` and `searchConfig.cpp`:

```cpp
/*
         // own threats pruning
         if ( SearchConfig::doThreatsPruning && !isMateScore(evalScore) && pvsData.lessZugzwangRisk && SearchConfig::ownThreatCoeff.isActive(depth, pvsData.evalScoreIsHashScore) &&
            !evalData.goodThreats[p.c] && evalScore < alpha - SearchConfig::ownThreatCoeff.threshold(depth, evalData.gp, pvsData.evalScoreIsHashScore, pvsData.improving) ){
            return stats.incr(Stats::sid_threatsPruning), alpha;
         }
*/
```

**Enable.** Uncomment the coefficient declaration and definition
(`{ {0, 0}, {256, 256}, {64, 64}, {0, 0}, {0, 0}, {2, 2}, "ownThreat" }`) and
the block, then rebuild. **Caveat:** the block tests `evalScore`, while the
neighbouring active heuristics now use `pruningEval`, the eval with correction
history applied. Bring it in line before testing. It also shares the
`sid_threatsPruning` counter with the active threat pruning.

### ProbCut side effects

**Idea.** *ProbCut* (active, `doProbcut = true`) tries good captures with a
raised beta (`betaPC`) and a shallow search. If one still beats `betaPC`, the node is
cut early. Two side effects are written but commented out at the success
point:

1. a capture-history bonus for the successful capture and a malus for the
   captures tried before it, so ordering learns from ProbCut;
2. a TT store of the ProbCut result as a lower bound (`TT::B_beta`) at depth
   `probCutSearchDepth-1`. A later visit could then cut immediately.

```cpp
               if (scorePC >= betaPC){
                  /*
                  const DepthType bonusDepth = probCutSearchDepth - 1 + (scorePC > (beta + SearchConfig::betaMarginDynamicHistory));
                  historyT.updateCap<1>(bonusDepth, *it, p);
                  ...
                  */
                  /*
                  TT::setEntry(*this, pHash, *it, TT::createHashScore(scorePC, height), TT::createHashScore(evalScore, height),
                  ...
                  */
                  return stats.incr(Stats::sid_probcut), scorePC;
               }
```

**Caveats.** The TT store would write `evalScore` into the static-eval field,
which can already be a TT-derived score at that point (see
[the evaluation-field caveat](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/tables-and-history.md#the-evaluation-field-caveat)).
The commented line `//if(!pvsData.cutNode) MoveSorter::sort(moves);` (full sort
at non-cut nodes) is a separate, also disabled, ordering idea in the same loop.

### Singular-extension variants

The active singular logic (see
[singular TT policy](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md#singular-tt-policy))
extends a "singular" TT move by one or two plies. Otherwise it uses multi-cut
or negative extensions. Three variants are commented out:

- **Triple extension.** One more ply when the reduced search fails very low
  (`score < betaC - 8 * depth`) for a non-capture TT move.
- **Reduced verification multi-cut.** In the `e.s >= beta` branch, a
  `depth - 4` search that would return `beta` (fail-hard) if another move also
  beats beta. The active code applies `extension = -2 + pvsData.pvnode`
  instead.
- **Cut-node negative extension.** `extension = -1` at cut nodes.

```cpp
               if (/*!pvsData.pvnode &&*/ score < betaC - 4 * depth && extensions <= 6) {
                  stats.incr(Stats::sid_singularExtension2);
                  ++extension;
                  /*
                  if (score < betaC - 8 * depth && !pvsData.ttMoveIsCapture && extensions <= 6){
                     stats.incr(Stats::sid_singularExtension6);
                     ++extension;
                  }
                  */
               }
            ...
            /*
            else if (pvsData.cutNode){
               extension = -1;
               stats.incr(Stats::sid_singularExtension7);
            }
            */
```

**Enable.** Uncomment and rebuild. The statistic counters
`sid_singularExtension6`/`7` still exist. The cut-node branch would sit before
the active `e.s <= alpha` branch and take priority over it.

### Generic extensions in `depthPolicy()`

**Idea.** An *extension* searches a move one ply deeper because it is forcing
or suspicious, so the horizon does not cut off a tactic. Nine generic
extensions are coded. All are disabled by default through local switches at the
top of `Searcher::depthPolicy()`:

```cpp
   constexpr bool enableCheckQuietExtension  = false;
   constexpr bool enableInCheckExtension     = false;
   constexpr bool enableBMExtension          = false;
   constexpr bool enableMateThreatExtension  = false;
   constexpr bool enableQueenThreatExtension = false;
   constexpr bool enableCastlingExtension    = false;
   constexpr bool enableRecaptureExtension   = false;
   constexpr bool enableGoodHistoryExtension = false;
   constexpr bool enableCMHExtension         = false;
```

| Switch | Would extend | Producer signal status |
|---|---|---|
| `CheckQuietExtension` | A checking early move, or a checking quiet with ordering score `>= HISTORY_MAX` | Active |
| `InCheckExtension` | An early move made while in check | Active |
| `BMExtension` | Botvinnik-Markoff: the null-move refutation is the same as two plies earlier, so the opponent's threat persists | `BMextension` is computed, but only when NNUE is in use |
| `MateThreatExtension` | Early move when the null-move search returned a mated score | `mateThreat` is set by null move |
| `QueenThreatExtension` | Early quiet queen move when the own queen is attacked, `SEE_GE(p, m, -80)` | Active |
| `CastlingExtension` | Castling moves | Active |
| `RecaptureExtension` | Non-bad move landing on the previous capture's square | Active |
| `GoodHistoryExtension` | Quiet with score in `(historyExtensionThreshold, HISTORY_MAX]` | `historyExtensionThreshold` = 512 |
| `CMHExtension` | Quiet with distance-1 continuation history `>= 3 * HISTORY_MAX / 4` | Active |

**Caveats from the code.**
- The whole block is guarded by `if (!pvsData.validTTmove)`, so these
  extensions only fire at nodes **without** a TT move.
- `tryExtension()` grants at most one ply. The first true predicate wins.
- For a *reducible* move, `if (extension - reduction > 0) reduction = extension;`
  cancels any net gain. In practice an extension only deepens moves searched
  through the non-reducible path (early moves) or nodes below `lmrMinDepth` (2).
- Details per predicate:
  [generic extension inventory](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md#generic-extension-inventory-all-disabled).

### Disabled LMR terms in `depthPolicy()`

**Idea.** *Late move reductions* (LMR) search late, probably poor moves at
reduced depth and re-search only if they unexpectedly raise alpha. Minic builds
the reduction from additive terms, each with its own `constexpr` switch. The
enabled terms are `StdBaseLMRReduction`, `StdNotImprovingReduction`,
`StdTTMoveIsCaptureReduction`, `StdFailHighReduction`, `StdHistoryReduction`,
`StdFormerPVPrudence`, `StdKnownEndgamePrudence`, `CapBaseLMRReduction`,
`CapHistoryReduction`, `CaptureBadCapExtraReduction`, `CapPVNodePrudence` and
`CapImprovingPrudence`. The following are disabled by default:

| Switch (`enable...`) | Quiet/capture | Term | Why enabling it alone may do nothing |
|---|---|---|---|
| `ThreadBaseReduction` | quiet | `+ id() % 2`: odd helper threads reduce more, which diversifies Lazy SMP | — |
| `AggressiveRandomReduction` | quiet | `+1` when `(nodes+qnodes) % 128 > 128 - factor * validQuietMoveCount` | The local `randomAggressiveReductionFactor` is `0`, so the test is `x > 128` with `x <= 127`: never true |
| `MobilityBasedReduction` | quiet | `+1` when `(opponentMobility - ownMobility) / 16 < 0`, i.e. own mobility exceeds the opponent's by at least 16 (integer division truncates toward zero); not when in check | — |
| `ReduceLessWhenInCheck` | quiet | `- isInCheck` | — |
| `ReduceLessWhenGivingCheck` | quiet | `- isCheck` | — |
| `ReduceLessAtPVNode` | quiet | `- pvnode` | Overlaps the enabled `formerPV || ttPV` prudence |
| `ReduceLessAfterAlphaUpdateAll` | quiet | `- (!cutNode && alphaUpdated)` | — |
| `ReduceLessOnSingularTT` | quiet | `- ttMoveSingularExt` | — |
| `ReduceLessOnTheirTurn` | quiet | `- theirTurn` (`height % 2`) | — |
| `ReduceLessOnEmergencyDefence` | quiet | `- isEmergencyDefence` | `isEmergencyDefence` is hard-wired to `false` (see [context signals](#iteration-emergency-and-game-history-depth-corrections)) |
| `ReduceLessOnAdvancedPawnPush` | quiet | `- 2 * isAdvancedPawnPush` | `isReductible` already requires `!pvsData.isAdvancedPawnPush`: unreachable |
| `CaptureReduceLessIfTTMoveIsCap` | capture | `- ttMoveIsCapture` | — |
| `CaptureReduceLessOnFormerPV` | capture | `- (formerPV || ttPV)` | — |
| `CaptureReduceLessOnAdvancedPawn` | capture | `- 2 * isAdvancedPawnPush` | Unreachable for the same reason |

Exact formulas:
[disabled reduction inventory](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md#disabled-reduction-inventory).

### Smaller commented search tweaks

| Location (`Searcher::pvs`) | Commented code | Idea |
|---|---|---|
| Quiet SEE pruning | `//else if (seeValue < 0 && nextDepth > 1) --nextDepth;` | Reduce quiets with negative SEE one more ply instead of only pruning very bad ones |
| After the LMR re-search | `historyT.update<±1>` / `historyT.updateCap<±1>` blocks | Update history from the outcome of the full-depth re-search of a reduced move ("LMR feedback") |
| Null move | `//!pvsData.isKnownEndGame &&` | Forbid null move in recognised endgames (zugzwang protection is currently `lessZugzwangRisk` only) |
| ProbCut | `//&& !pvsData.isKnownEndGame` | Same for ProbCut |
| Forward pruning guard | `/*&& !isMateScore(beta)*/` | Skip forward pruning near mate bounds; the comment says removing it helps mate finding |
| Move eligibility | `/*pvsData.isNotPawnEndGame &&*/` in `isPrunable`/`isReductible` | Stop pruning/reducing in pawn endgames |
| Root first moves | `/*+2*pvsData.rootnode*/` in `earlyMove` | Treat more root moves as "early" (not reduced) |

Also, `nullMoveMargin2` (0) and `lmrLateExtensionMargin` (64) are defined in
[searchConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.cpp)
but no search code reads them. Only `nullMoveMargin2` is registered as a
tuning option; changing it has no effect.
`probCutThreshold` and `randomAggressiveReductionFactor` exist only as
commented declarations.

### Iteration-emergency and game-history depth corrections

**Idea.** Two "this position is unstable" signals exist:

- the *iteration emergency*: in iterative deepening, the score drops by more
  than `emergencyMargin` (64) between depth `d-2` and `d` once
  `depth > emergencyMinDepth` (14), giving `MD_moobAttackIID`/`MD_moobDefenceIID`;
- the *game-history evolution*: the search score moved by at least 80 between
  the engine's last two moves ("booming"/"moobing").

Both feed time management (active, see
[within-search instability versus between-search evolution](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/time-and-parallelism.md#within-search-instability-versus-between-search-evolution)).
The search-side use, pruning less in unstable positions, is neutralised.
From `Searcher::pvs`:

```cpp
   pvsData.isEmergencyDefence = false;//moveDifficulty == MoveDifficultyUtil::MD_moobDefenceIID;
   pvsData.isEmergencyAttack  = false;//moveDifficulty == MoveDifficultyUtil::MD_moobAttackIID;
   ...
   // when score is fluctuating a lot in the main ID search, let's prune a bit less
   const DepthType emergencyDepthCorrection = pvsData.isEmergencyDefence || pvsData.isEmergencyAttack; 
   // take asymetry into account, prune less when it is not our turn
   const DepthType asymetryDepthCorrection = 0; //pvsData.theirTurn; ///@todo
   // when score is fluctuating a lot in the current game, let's prune a bit less
   const DepthType situationDepthCorrection = 0; /*pvsData.isBoomingAttack
                                            || pvsData.isMoobingAttack
                                            || pvsData.isBoomingDefend
                                            || pvsData.isMoobingDefend;*/
   
   ///@todo this is not used for now (==0)
   // take current situation and asymetry into account.
   const DepthType depthCorrection = emergencyDepthCorrection || asymetryDepthCorrection || situationDepthCorrection;
```

`depthCorrection` has one consumer: the LMP index
`SearchConfig::lmpLimit[pvsData.improving][depth + depthCorrection]`. A
non-zero correction would allow more quiet moves before late move pruning.
`pvsData.isBoomingAttack`, `isBoomingDefend`, `isMoobingAttack` and
`isMoobingDefend` are computed at each node but read nowhere else.

**Caveat.** `lmpLimit` has `lmpMaxDepth + 1` = 11 columns and LMP is active
for `depth <= lmpMaxDepth` (10). With `depthCorrection == 1` at depth 10 the
index would be 11, **out of bounds**. Clamp the index before you re-enable any
of these corrections.

### Danger-based pruning thresholds

`evalData.danger` (king-attack pressure) is computed at every node. Its
per-color values do feed the SEE pruning margins (`dangerGoodAttack -
dangerUnderAttack`). The summed-danger booleans, however, only update
statistics:

```cpp
   const bool isDangerPrune        = dangerFactor >= SearchConfig::dangerLimitPruning;
   const bool isDangerForwardPrune = dangerFactor >= SearchConfig::dangerLimitForwardPruning;
   const bool isDangerRed          = dangerFactor >= SearchConfig::dangerLimitReduction;
   ///@todo this is not used for now
   if (isDangerPrune)        stats.incr(Stats::sid_dangerPrune);
```

The intended "sharp position: prune and reduce less" policy (thresholds 16,
`dangerDivisor` 183) is therefore not implemented. Only the statistics exist
to measure how often it would trigger.

## Move ordering

Background: [move ordering](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/move-ordering.md).

### Second killer from two plies earlier

**Idea.** *Killer moves* are quiet moves that caused a beta cutoff at the
same height in a sibling node. Minic keeps two per height. It also gives a bonus to
the first killer of height `h-2`, the previous position with the same side to
move. The second killer of `h-2` would get `previousPlyKiller1Bonus` (1400), but
the branch is commented out in `MoveSorter::computeScore`
([moveSort.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/moveSort.cpp)):

```cpp
         else if (height > 1 && sameMove(m, context.killerT.killers[height - 2][0]))
            s += SearchConfig::previousPlyKiller0Bonus; // quiet killer
         //else if (height > 1 && sameMove(m, context.killerT.killers[height - 2][1]))
         //   s += SearchConfig::previousPlyKiller1Bonus; // quiet killer
```

The parameter is still defined and registered as a tuning option. In the same
spirit, `KillerT::isKiller()` in
[tables.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/tables.cpp)
has a commented `isFormerKiller` term. With it, `h-2` killers would also be
exempt from move pruning. With `WITH_BETACUTSTATS`, the `k3`/`k4` counters
already measure how often these moves cause cutoffs.

### Deeper continuation-history plies (inert weights)

**Idea.** *Continuation history* (CMH) scores a quiet move by its success
after a given earlier move: the move 1 ply back (counter-move history), 2 plies
back (follow-up history), and so on. Minic collects pointers for distances
`CMH_DISTANCE = {1, 2, 4, 6}`
([tables.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/tables.hpp)).
It weights them with `SearchConfig::CMHWeight = {8, 0, 0, 0}`
([searchConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searchConfig.cpp)),
so only distance 1 counts. Distances 2, 4 and 6 are inert by default.

`Searcher::getCMHScore` in
[searcher.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcher.cpp)
normalises by the total weight, and `HistoryT::update` in
[tables.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/tables.hpp)
scales the bonus by `CMHWeight[i] / CMHWeight[0]`:

```cpp
         const int cmhWeightBase = SearchConfig::CMHWeight[0];
         for (int i = 0; i < SearchConfig::MAX_CMH_PLY; ++i) {
            if (cmhPtr[i]) {
               ScoreType& CMHitem = (*cmhPtr[i])[PieceIdx(p.board_const(from)) * NbSquare + to];
               const int CMHbonus = s * SearchConfig::CMHWeight[i] / cmhWeightBase;
               CMHitem = HISTORY_UPDATE(CMHitem, CMHbonus);
            }
         }
```

**Caveats.**
- With a zero weight, the deeper tables receive a zero bonus. This is not
  free: `HISTORY_UPDATE` is still called, and the pointers are still computed.
- `CMHWeight[0]` must stay non-zero (division). The tuning option enforces a
  minimum of 1 for index 0.
- `isCMHGood`/`isCMHBad`, used by CMH pruning and the disabled CMH extension,
  read only `cmhPtr[0]` whatever the weights.
- **Enable.** Change the array and rebuild, or, in a `WITH_SEARCH_TUNING`
  build, set the options `CMHWeight1`..`CMHWeight3` (0..16).
- Details: [continuation history](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/move-ordering.md#continuation-history).

### Lazy sorting in quiescence

**Idea.** `MoveSorter::pickNextLazy` stops the selection sort once the last
picked score falls below a threshold. The remaining moves are probably bad, so
ordering them is not worth the cost
([picking](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/move-ordering.md#picking-full-sort-versus-partial-sort)).
PVS uses it with `lazySortThreshold` (-491). Quiescence calls plain `pickNext`,
and `lazySortThresholdQS` (-512) is never used:

```cpp
   while ((it = ms.pickNext/*Lazy*/(moves, offset, false/*, SearchConfig::lazySortThresholdQS*/))) {
```

Enable by restoring `pickNextLazy(moves, offset, false, SearchConfig::lazySortThresholdQS)`
in [searcherQSearch.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherQSearch.cpp) and rebuilding.

### Other ordering remnants

- King evasion bonus: `//if (isInCheck && from == p.king[C]) s += 10000;` in
  `MoveSorter::computeScore`.
- The bad-capture test has a commented `see + pstScore < 0` variant.
- `MoveDifficultyUtil::easyMoveMargin` (180) and the `MD_easy` time case
  (marked "not used anymore") remain from a removed easy-move detector.
  Nothing sets `MD_easy`.

## Quiescence search

Background: [quiescence](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search.md#quiescence-search).

### Quiescence futility pruning

**Idea.** In *quiescence search*, only captures (and evasions) are searched
until the position is quiet. *Futility pruning* there skips a capture when
even an optimistic result cannot reach alpha: the static eval, plus the captured
piece value, plus a margin, plus the promotion gain.

**Gate.** `SearchConfig::doQFutility = false`. Disabled by default. From
`Searcher::qsearch`:

```cpp
         if (SearchConfig::doQFutility && validMoveCount &&
             qsearchEval + SearchConfig::qfutilityMargin[evalScoreIsHashScore]
                         + (isPromotionCap(*it) ? (value(P_wq) - value(P_wp)) : 0) 
                         + (Move2Type(*it) == T_ep ? value(P_wp) : PieceTools::getAbsValue(p, Move2To(*it))) <= alphaInit) {
            stats.incr(Stats::sid_qfutility);
            continue;
         }
```

**Caveats.** `qfutilityMargin = {1024, 1024}` is very large, so even when
enabled it would rarely prune. The active SEE-based delta pruning
(`doQDeltaPruning`, Seer-style) covers most of the same ground. The
`validMoveCount` condition never prunes the first legal capture.

### Unused classic delta margin

`qDeltaMargin()` and `bestCapture()` in
[searcherQSearch.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherQSearch.cpp)
implement the classic node-level *delta pruning* margin: a queen if a pawn of
the side to move is on its seventh rank, otherwise a pawn, plus the value of
the opponent's best piece. A stand-pat score below `alpha - margin` would then
return immediately. `qDeltaMargin()` has no caller; the finer per-piece
fallback in `bestCapture()` is commented out as well.

### Null-move evaluation shortcut (HCE only)

After a null move, the static eval can be derived from the parent's as
`2 * tempo - parentEval` instead of calling `eval()`. Both `pvs` and `qsearch`
implement this, but only under `!DynamicConfig::useNNUE`. The comment says this
"won't work with asymetric HalfKA NNUE". In the default build an NNUE net is
embedded and loaded, so the shortcut is inactive. It becomes active
automatically with `EVALFILE=none` builds, `NNUEFile none`, or the variants
that switch NNUE off.

## Transposition table

Background: [tables and history](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/tables-and-history.md).

| Item | Gate / location | What it does | Enable |
|---|---|---|---|
| Disable the TT | `DynamicConfig::disableTT = false`; command-line only (`-disableTT 1`), **not** a UCI option | `TT::getEntry` returns false and `TT::setEntry` returns early. Used internally by tuning and by quiet-position extraction for data generation | CLI argument |
| Reproducible results | `//#define REPRODUCTIBLE_RESULTS` in `config.hpp` | `ThreadPool::clearSearch` also calls `TT::clearTT()`; `Searcher::clearSearch` clears the pawn table and forces history (and correction-history) reset before **every** search, not only every new game | Uncomment and rebuild. Determinism is still limited to single-thread, fixed-depth or fixed-node searches |
| Depth bonus for non-exact bounds | `/*+ (e.b != B_exact)*/` in `TT::getEntry` | Would require one extra ply of draft before trusting a bound-only entry | Uncomment, rebuild |
| Score damping | `//   score *= 0.999f;` in `createHashScore`/`adjustHashScore` | Would shrink stored non-mate scores slightly at each store/load, a crude decay against stale optimism | Uncomment, rebuild. As written the factor is applied on both store and load, so a score shrinks twice per round trip |
| Hash entry fuzzing | `//#define DEBUG_HASH_ENTRY` | Randomises entry data and skips the key check, to test that every TT consumer validates its move | Debug only |

From `ThreadPool::clearSearch` and `Searcher::clearSearch`:

```cpp
void ThreadPool::clearSearch() const {
#ifdef REPRODUCTIBLE_RESULTS
   TT::clearTT();
#endif
```

```cpp
void Searcher::clearSearch(bool forceHistoryClear) {
#ifdef REPRODUCTIBLE_RESULTS
   clearPawnTT();
   forceHistoryClear = true;
#endif
```

## Evaluation (HCE)

Background: [evaluation](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/evaluation.md),
[classical evaluation reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/classical-evaluation-reference.md).

### Complexity and forwardness terms (inert style defaults)

**Idea.** The *complexity* term is an "anti-human" trick: the side that is
ahead prefers to keep material, and hence complexity, on the board.
*Forwardness* rewards pieces on advanced ranks. Both are computed in
`eval()` ([eval.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/eval.cpp)),
but with the default style value 50 they contribute nothing:

```cpp
   features.scores[F_complexity]  = features.scores[F_complexity] .scale((DynamicConfig::styleComplexity      - 50) / 50.f, 0.f);
```

The factor is `(50 - 50) / 50 = 0`. The other style factors are `1 + (style - 50) / 50`,
so 50 gives 1 and those terms keep their tuned weight. Forwardness is
skipped completely unless `styleForwardness != 50` (`withForwadness`).

**Enable.** UCI options `StyleComplexity`, `StyleForwardness`, and also
`StyleAttack`, `StyleDevelopment`, `StyleMaterial`, `StyleMobility`,
`StylePositional`, all 0..100.

**Caveats.**
- `stylePawnStruct` is read by `eval()` but has **no** UCI option or
  command-line key, so it stays 50 without a source edit.
- `DynamicConfig::stylized` is recomputed by `EvalFeatures::callBack()` and
  read nowhere.
- Style only affects the HCE path. When NNUE evaluates the position, it has no
  effect. Details:
  [style, initiative and final scaling](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/classical-evaluation-reference.md#style-initiative-and-final-scaling).

### Other HCE remnants

- `forbidNNUE()` contains a commented rule that would force HCE for
  opposite-coloured-bishop endings with few pawn differences. It always returns
  `false` (disabled by default). Enabling it also requires `forceNNUE` to stay
  false, because `forceNNUE` overrides it.
- `MaterialHash::Ter_Draw` ("real FIDE draw") is handled in `eval()`, but the
  material table does not currently assign it: `///@todo some Ter_MaterialDraw are Ter_Draw (FIDE)`
  in [material.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/material.cpp).
  Material draws go through `Ter_MaterialDraw` instead.
- `scalingFactorQueenNoQueenSlope` is 0, a tuned value that removes the
  minor-piece dependence of the queen-versus-no-queen scaling.
- `VERBOSE_EVAL` (compile-time) prints intermediate scores of the HCE when
  `-eval`-style displays are requested.

## NNUE

Background: [NNUE reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/nnue-reference.md).

### Uncertainty head (`WITH_NNUE_UNCERTAINTY`)

**Idea.** The network can be trained with a second output, the
log-variance of its prediction, so that the engine knows how confident each
evaluation is. A search could then prune less where the net is uncertain.

**Gate.** `//#define WITH_NNUE_UNCERTAINTY` in `config.hpp`. Disabled by default.
It adds a layer `fc3_uncertainty` to each bucket
([nnue/weight.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/nnue/weight.hpp)).
From `NNUEEval::propagate`
([nnue/evaluator.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/nnue/evaluator.hpp)):

```cpp
#ifdef WITH_NNUE_UNCERTAINTY
      const float log_var = layer.fc3_uncertainty.forward(x3).data[0];
      const float variance = std::exp(std::clamp(log_var, -10.0f, 10.0f));
#else
      constexpr float variance = 1.0f;
#endif
```

**Caveats.**
- The value is stored in `EvalData::uncertainty` and nothing in search reads
  it. Enabling the macro only costs time.
- Without the macro, the loader accepts both the plain (151049100 bytes) and
  the uncertainty-carrying (151049300 bytes) file sizes and ignores the tail.
  With the macro, both expected sizes are 151049300, so **plain nets are
  rejected**.
- See [uncertainty](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/evaluation.md#uncertainty-available-scaffolding-not-active-selectivity).

### Weight prefetching (`WITH_NNUE_PREFETCH`)

`//#define WITH_NNUE_PREFETCH` in `config.hpp`. Disabled by default. In the
incremental update helpers of
[position.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/position.hpp),
it prefetches every feature-transformer weight row that the following
`erase`/`insert` calls will touch: from, to or promotion, captured piece, and
en-passant victim, for both perspectives. A *prefetch* asks the CPU to start
loading a cache line early so the later access does not stall. Whether this
gains speed depends on the CPU and on how much other work separates the
prefetch from the use. Measure nps before and after (`-evalSpeed`, `bench`).

### Alternative bucket selection

`NNUEEVal()` in [eval.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/eval.cpp)
keeps two commented bucket selectors: a king-square bucket table and an
occupancy-count formula. The active selector uses the game phase
(`data.gp < 0.45f` gives bucket 0, otherwise 1). The weights have
`nbuckets = 2`, and the alternatives assume other bucket semantics (and, for
the king table, up to 7 buckets). They need a net trained for them. See
[bucket selection](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/nnue-reference.md#bucket-selection).

## Correction history: horizon table (`WITH_HORIZON_CORRHIST`)

**Idea.** *Correction history* learns, per hashed context, how far search
results differ from the static eval, and adds that average error to future
evals ([correction history](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/evaluation.md#correction-history)).
Pawn-structure and non-pawn-material tables are enabled. The *horizon* table
is a third, disabled context. Its key is not built from the board; it buckets
the node situation: TT/eval disagreement in steps of 24 (up to bucket 7) and
node type (PV / cut / all). It applies only at `depth > 8`, ramping in linearly
over 8 plies.

**Gate.** `//#define WITH_HORIZON_CORRHIST` inside the
`WITH_CORRECTION_HISTORY` block of `config.hpp`. Disabled by default. When off,
`Searcher::horizonCorrection` returns 0. From
[correctionHistory.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/correctionHistory.cpp):

```cpp
#ifdef WITH_HORIZON_CORRHIST
   if (depth <= horizonCorrMinDepth) return 0;
   // ramp up linearly instead of snapping to full weight right past the threshold
   const int ramp = std::min<int>(horizonCorrRampDepth, depth - horizonCorrMinDepth);
   const ScoreType raw = horizonCorrHist.score(c, horizonKey(baselineEval, ttRefScore, hasTTHint, pvnode, cutNode));
   return static_cast<ScoreType>(static_cast<int>(raw) * ramp / horizonCorrRampDepth);
#else
   return 0;
#endif
```

**Caveats.**
- The `config.hpp` comment mentions "depth/volatility/tt/node-type". The key
  actually has no volatility component; depth only enters through the ramp.
- `pvs` calls `horizonCorrection` only when `evalScoreIsHashScore` is false,
  and passes `evalScore` as `ttRefScore`. At that point `evalScore` equals the
  raw static score, so at probe time the "TT disagreement" bucket measures
  `|staticScore - correctedStaticScore|`, i.e. the size of the pawn plus non-pawn
  correction. Updates, by contrast, pass the possibly TT-derived `evalScore`.
  Probe and update contexts can therefore differ. Check this before judging
  the idea from match results.
- Its clamp `correctionHistoryMaxHorizon` (24) is a tuning option only in
  `WITH_SEARCH_TUNING` builds.

## Time management, threads and distributed search

Background: [time and parallelism](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/time-and-parallelism.md).

| Item | Gate | Behaviour / caveat |
|---|---|---|
| Search-side emergency and booming corrections | Hard-wired `false`/`0` in `pvs` | Time-side use is active; search-side use is disabled (see [above](#iteration-emergency-and-game-history-depth-corrections)) |
| Booming time bonus | `emergencyFactorBoomHistory = 1` in [definition.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/definition.hpp) | `PE_boomingAttack/Defence` multiply the move time by 1. The branch can only cap time at `msecUntilNextTC / maxStealDivisor`; it never extends it |
| Easy move | `MD_easy`, `easyMoveMargin` | Never set (see ordering remnants) |
| Thread count clamp | `//#define LIMIT_THREADS_TO_PHYSICAL_CORES` | Clamps `Threads` to `std::thread::hardware_concurrency()`. **Caveat:** that function reports logical CPUs (hyper-threads included), not physical cores as the name says |
| Thread-parity LMR | `enableThreadBaseReduction` | See LMR table; the active diversification is depth skipping (`skipSize`/`skipPhase`) |
| Distributed (multi-process) search | `//#define WITH_MPI` in `config.hpp` | See below |

From `ThreadPool::setup` in
[threading.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/threading.cpp):

```cpp
#ifdef LIMIT_THREADS_TO_PHYSICAL_CORES
   unsigned int maxThreads = std::max(1u, std::thread::hardware_concurrency());
   if (DynamicConfig::threads > maxThreads) {
```

### `WITH_MPI`

**Idea.** Lazy SMP shares one TT among threads in a process. With MPI, several
processes (possibly on several machines) run Lazy SMP independently and
exchange TT entries asynchronously. Only rank 0 reads input, which is
broadcast to the others. Stop flags and statistics are synchronised as well.

**Gate and enable.** Uncomment `WITH_MPI`. `Tools/build/build.sh` detects the
uncommented define (`grep "^#define WITH_MPI"`) and switches to the Intel MPI
compiler wrappers `mpiicpx`/`mpiicx`. Run with `mpirun`. When the macro is
off, every `Distributed::` function is a stub, so the rest of the code needs no
`#ifdef`.

**Caveats** ([distributed.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/distributed.cpp)):
- Only entries with depth `> _ttMinDepth` (3) are shared.
- They go through a double buffer of `distributedTTBufSize` (16) entries.
- If the previous `MPI_Iallgather` has not completed, the buffer is reused and
  entries are lost (counted as overruns).
- Received entries always replace local ones.
- `//#define DEBUG_DISTRIBUTED` in `distributed.h` enables per-rank traces.
- Energy reporting (`energyMonitor.cpp`) also reduces power figures across ranks.
- See [optional distributed search](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/time-and-parallelism.md#optional-distributed-search).

## Play style and variant runtime options

These are compiled in and only need a runtime setting. Disabled by default
means the default value in
[dynamicConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/dynamicConfig.cpp).
Option names are those registered in
[option.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/option.cpp).
Most also accept `-name value` on the command line, using the `DynamicConfig`
member name.

| UCI option (default) | Effect | Caveats from the code |
|---|---|---|
| `MateFinder` (false) | Disables node-level forward pruning, move pruning and LMR (`!DynamicConfig::mateFinder` guards) to search for mates | Each UCI `go` resets it to false and only `go mate N` sets it, so the checkbox is effectively overridden. Also `-mateFinder` CLI |
| `UCI_LimitStrength` (false) + `UCI_Elo` (1500) | Converts Elo to `level` through a fitted cubic (`Skill::convertElo2Level`) at search start | Overwrites `Level` |
| `Level` (100) | Below 100: depth-limited play plus MultiPV-based weakened move picking; 0 = random mover | `Skill::enabled()` is true for 1..99 |
| `nodesBasedLevel` (false) | With `Level` 1..99, limits nodes (`Skill::limitedNodes()`) instead of depth | Main process only |
| `RandomOpen` (0) | Adds a random term in `[-RandomOpen, RandomOpen]` to the eval in the first 10 plies | Uses the non-seeded random device: not reproducible |
| `Armageddon` (false) | Draws count as a loss for White; scores shifted by `armageddonScore` | Setting it through `UCI_Variant armageddon` also turns NNUE off; the checkbox alone does not |
| `AntiChess` (false) | Captures become mandatory in move generation; `evalAntiChess` (material minimisation) replaces the eval | `UCI_Variant antichess` also disables NNUE; selecting `chess` afterwards does **not** re-enable it (reload `NNUEFile`) |
| `anarchy` (false) | En passant becomes mandatory when available (move list filtered) | Marked "will be slow"; filtering after full generation |
| `bongCloud` (false) | While castling rights remain, plays the first legal king step from a fixed list (`Ke2`, `Kd1`, `Kf1`, `Kd2`, `Kf2`) without searching | Score reported as 0 |
| `WDL_display` (false) | Appends `wdl` statistics to UCI score output | Output only |
| `Style*` (50) | See [HCE style](#complexity-and-forwardness-terms-inert-style-defaults) | HCE only |
| `UCI_Opponent`, `UCI_RatingAdv` | Scale contempt by a `ratingFactor` in `[-2, 2]` | Applied on `isready` by `Opponent::init()`; parsing the rating from the `UCI_Opponent` string is commented out, only the name lookup in a built-in table is used |
| `GenFen`, `PgnOut` (false), `GenFenDepth`, `GenFenDepthEG`, `RandomPly` | Training-data generation (`WITH_GENFILE`, on by default) | Only used by the `-selfplay` CLI path |
| `forceNNUE` (false) | Uses NNUE even where the lazy-HCE threshold would skip it | Overrides `forbidNNUE()` |

Not registered as UCI options but readable from the command line:
`disableTT`, `debugMode`, `fullXboardOutput`, `DFRC` and `maxNodes`.

## Tooling and diagnostic builds

All of these are compile-time macros in
[config.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/config.hpp),
disabled by default. Enable them by uncommenting and rebuilding.
`NOPROFILE=1 make` avoids the PGO step for quick experiments.

| Macro | What it adds | Notes |
|---|---|---|
| `WITH_SEARCH_TUNING` | `CONST_SEARCH_TUNING` and `CONSTEXPR_SEARCH_TUNING` become empty, so search constants become mutable. `option.cpp` registers them as UCI spin options (all `Coeff` members, margins, bonuses, divisors, CMH weights, correction-history limits) for SPSA-style tuning; `-spsa` prints the list (`Options::displayOptionsSPSA`) | `do...` flags and `depthPolicy` switches are not exposed. Slower because constants are no longer folded |
| `WITH_EVAL_TUNING` | Texel tuning: `-tuning <file>` runs `evaluationTuning()` (logistic fit of eval to game results, `K` = 0.23 start). Eval parameters become mutable (`CONST_EVAL_TUNING`) | Forces `#undef WITH_EVALSCORE_AS_INT`. Material-table imbalance is zeroed and recomputed live in `eval()`; contempt is zeroed in the driver; the pawn cache is bypassed; the TT is disabled during tuning |
| `WITH_PIECE_TUNING` | Piece values (MG, EG, game-phase weight, SEE value) become UCI options; changing one re-symmetrises the tables and rebuilds material scores | |
| `WITH_TIMER` | `START_TIMER`/`STOP_AND_SUM_TIMER` become `rdtsc` cycle counters per section (eval, move generation, scoring, NNUE...); `Timers::Display()` on exit of CLI tools | Adds overhead to every timed section |
| `WITH_PGN_PARSER` | `-pgn <file>`: parses games and extracts quiet positions (qsearch-checked, Elo >= 1600) for eval tuning | Historical tuning-data path |
| `WITH_FMTLIB` | Coloured "pretty" console output through `{fmt}` | Requires the fmt library headers |
| `WITH_EVALSCORE_AS_INT` | Packs the MG/EG pair into one `int` (the Stockfish trick) instead of `array1d<ScoreType,2>` | The `config.hpp` comment says it is "just as slow" as the default; incompatible with eval tuning |
| `WITH_NNUE_PREFETCH` | See [NNUE](#weight-prefetching-with_nnue_prefetch) | |
| `WITH_NNUE_UNCERTAINTY` | See [NNUE](#uncertainty-head-with_nnue_uncertainty) | Rejects plain nets |
| `WITH_HORIZON_CORRHIST` | See [correction history](#correction-history-horizon-table-with_horizon_corrhist) | |
| `WITH_MPI` | See [above](#with_mpi) | |
| `WITH_BETACUTSTATS` | `updateStatBetaCut()` classifies each beta cutoff (good/bad capture, promotion, killer 1-4, counter, other quiet); `bench`/`benchBig` print percentages | Costs a little per cutoff |
| `REPRODUCTIBLE_RESULTS` | See [TT](#transposition-table) | |
| `LIMIT_THREADS_TO_PHYSICAL_CORES` | See [threads](#time-management-threads-and-distributed-search) | Counts logical CPUs |
| `WITHOUT_FILESYSTEM` | Removes `std::filesystem` use: no file-size check in the NNUE loader, no `WITH_GENFILE`, no `WITH_DATA2BIN` | Set with `-D` by the Raspberry Pi and WASM build scripts, not for desktop |

### Debug gates

All are `//#define` lines in the "Debug" section of `config.hpp` (except where
noted). They add consistency checks that are too slow for play.

| Macro | Checks (file) |
|---|---|
| `VERBOSE_EVAL` | Prints intermediate HCE scores (`eval.cpp`) |
| `DEBUG_NNUE_UPDATE` | Compares incremental accumulators with a full refresh (`nnue/*`, `moveApply.cpp`, `cli.cpp`, `cli_perft.cpp`, `moveGen.cpp`) |
| `DEBUG_BACKTRACE` | Prints a backtrace on errors (`logging.cpp`) |
| `DEBUG_HASH`, `DEBUG_PHASH`, `DEBUG_NONPAWNHASH` | Recomputes Zobrist, pawn and non-pawn keys from scratch and compares them with the incremental ones (`hash.cpp`) |
| `DEBUG_MATERIAL` | Verifies incremental material after each move (`moveApply.cpp`) |
| `DEBUG_APPLY` | Checks pseudo-legality of applied and TT moves (`moveApply.cpp`, `searcherPVS.hpp`) |
| `DEBUG_GENERATION` | Validates squares and piece types during generation (`moveGen.hpp`) |
| `DEBUG_GENERATION_LEGAL` | Checks every generated move with `isPseudoLegal` (`moveGen.hpp`). Not compatible with `DEBUG_PSEUDO_LEGAL` |
| `DEBUG_PSEUDO_LEGAL` | Each move accepted by `isPseudoLegal` must also appear in a `GP_all` generation (`movePseudoLegal.cpp`). Not compatible with `DEBUG_GENERATION_LEGAL` (each would call the other) |
| `DEBUG_BITBOARD` | Compares bitboard and mailbox occupancy after each move (`moveApply.cpp`) |
| `DEBUG_HASH_ENTRY` | TT entry fuzzing (`transposition.cpp`) |
| `DEBUG_KING_CAP` | Detects king captures in eval and search (`eval.cpp`, `searcherPVS.hpp`, `tools.cpp`) |
| `DEBUG_PERFT` | Perft enumerates every `MiniMove` value and keeps those accepted by `isPseudoLegal`, instead of using move generation, which tests the pseudo-legality checker against perft counts (`cli_perft.cpp`) |
| `DEBUG_STATICEVAL` | Recomputes and checks every static eval obtained from TT or the null-move shortcut (`searcherPVS.hpp`, `searcherQSearch.cpp`) |
| `DEBUG_EVALSYM` | Checks colour symmetry of the eval and disables fifty-move scaling (`tools.cpp`, `score.hpp`) |
| `DEBUG_TT_CHECK` | Verifies the TT "is check" flag after applying the TT move (`searcherPVS.hpp`) |
| `DEBUG_FIFTY_COLLISION` | Detects hash collisions in repetition detection with full position comparison (`searcherDraw.cpp`, `position.hpp`) |
| `DEBUG_DISTRIBUTED` | MPI traces (`distributed.h`, not in `config.hpp`) |
| `DEBUG_MTRACE` | Memory tracing in the data converter (`nnue/learn/convert.cpp`, not in `config.hpp`) |

## Evaluating a re-enabled technique

Disabled code is not "free Elo waiting to be switched on". Most of these
branches were disabled because they did not pass a test, or because the
surrounding code changed after them. A sound procedure, also described in
[development](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/development.md#designing-a-diagnostic-for-a-disabled-branch):

1. **One change per experiment.** Flip one switch (or one coherent group with
   its producer signal) so a result can be attributed to it.
2. **Check that it actually triggers.** Many entries above depend on a signal
   fixed to false or zero, on an eligibility test that excludes them, or on
   NNUE versus HCE. Use the existing statistics counters (`sid_...`, shown with
   search statistics) or add one.
3. **Correctness first.** Build, run `pytest -q -s Test/test.py` (perft, SEE,
   eval, KPK, UCI smoke tests) after `make dist`. For anything touching
   indices, run a debug or sanitizer build (`Tools/build/dosanitize`). The LMP
   index above is an example of a latent out-of-bounds access.
4. **Fixed-node comparison.** Compare `bench` node counts and fixed-node
   matches. This isolates the effect on search shape from speed effects.
5. **Timed matches.** Only then play time-controlled games (short and longer
   time controls) against the unchanged binary, with enough games for a
   statistically meaningful result. Pure speed items (`WITH_NNUE_PREFETCH`,
   `WITH_EVALSCORE_AS_INT`) are better judged first by nps.
6. **Clean up.** If the idea is kept, move its tunables into
   `searchConfig.*`/`evalConfig.*` or `DynamicConfig` as appropriate. If it is
   rejected, leave the gate as documented here so the result is not
   rediscovered from scratch.
