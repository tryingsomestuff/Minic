# Evaluation and NNUE

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md) | [Previous: tables and history](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/tables-and-history.md) | [Next: time and parallelism](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/time-and-parallelism.md)

Primary sources: [eval.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/eval.cpp),
[evalDef.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/evalDef.hpp), [evalTools.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/evalTools.hpp),
[evalConfig.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/evalConfig.hpp), [score.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/score.hpp),
[material.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/material.cpp), [nnue.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/nnue.hpp),
[nnue/weight.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/nnue/weight.hpp) and
[correctionHistory.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/correctionHistory.cpp).

## Score conventions and evaluation data

`eval()` returns a score for the **side to move**. The handcrafted evaluation
(HCE) accumulates white-positive *middlegame/endgame pairs* in `EvalScore`: the
same feature is often worth something different when many pieces are on the board
(middlegame, MG) and when few remain (endgame, EG). A *tapered evaluation*
blends the two components with a **game phase** $g \in [0, 1]$, where 1 means
full material and 0 a bare endgame. `ScaleScore()` combines them:

```text
score = gamePhase * MG + (1 - gamePhase) * endgameScaling * EG
```

```cpp
[[nodiscard]] FORCE_FINLINE ScoreType ScaleScore(const EvalScore s, const float gp, const float scalingFactorEG = 1.f) {
   return static_cast<ScoreType>(gp * s[MG] + (1.f - gp) * scalingFactorEG * s[EG]);
}
```

`endgameScaling` is a factor in the endgame component only; it is 1 unless a
drawish or hard-to-win situation is recognized (see the
[classical evaluation reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/classical-evaluation-reference.md)).

The phase is the total non-king phase weight of both armies divided by the
starting total, capped at 1. With the phase weights of
[definition.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/definition.cpp)
(pawn 93, knight 290, bishop 301, rook 518, queen 1076) the denominator is
$2 \cdot 1076 + 4 \cdot 518 + 4 \cdot 301 + 4 \cdot 290 + 16 \cdot 93 = 8076$.
Pawns therefore account for less than a fifth of the phase: trading all
queens lowers it by about 0.27, trading all pawns by about 0.18.
`gamePhase()` in
[positionTools.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/positionTools.cpp)
computes it from the material counters:

```cpp
const float totalMatScore = 2.f * absValueGP(P_wq) + 
                            4.f * absValueGP(P_wr) + 
                            4.f * absValueGP(P_wb) + 
                            4.f * absValueGP(P_wn) +
                            16.f * absValueGP(P_wp); 
...
return std::min(1.f,(matScoreW + matScoreB) / totalMatScore);
```

The material table stores this value truncated to an 8-bit integer
(`setGamePhase`/`gamePhase()` divide by 255); positions outside the table use the
floating-point computation. The two values can differ slightly, which matters only
close to a threshold such as the NNUE bucket boundary.

`EvalData` additionally carries phase, per-color danger, mobility and threat
booleans, `evalDone`, and an uncertainty field. A returned score does not imply
every auxiliary field is populated. A cached static score or an early NNUE
return can lack HCE attack data; `evalFeatures()` fills what search needs.

## Choosing specialized knowledge, NNUE or HCE

The important selection order is:

```text
variant / terminal special cases
        |
material classification and eligible endgame helpers
        |
NNUE first gate: small enough lazy material score, or force request
        |
full HCE if the first gate did not return NNUE
        |
NNUE second gate: HCE result within a smaller threshold
```

Bare kings and antichess have early behavior: with nothing but kings on the board
the result is `drawScore()`, and antichess uses its own material-only function.
Recognized low-material positions can use specialized helpers, including KPK,
when eligible and not obscured by a legal capture. Material classifications also
determine scaling/drawishness. NNUE does not erase those layers.

NNUE requires successful loading (`useNNUE` becomes true only after the network
file was read) and must not be forbidden for the position (`forbidNNUE()` currently
returns false for every position; its body is commented out).
The first threshold compares a lazy material-based score against phase-dependent
`NNUEThreshold`/`NNUEThresholdEG` (default 638/638). The second compares the
completed HCE score with `NNUEThreshold2`/`NNUEThreshold2EG` (default 344/315).
All four are UCI spin options in the range 0..1500, registered in
[option.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/option.cpp).
The thresholds are blended with the game phase like any MG/EG pair. At the first
gate the feature groups contain only the material score (material-table value
including imbalance, or the plain piece-value fallback), hence "lazy". The lazy
score is **not** tapered: `isLazyHigh()` compares the plain average of its MG
and EG components, in absolute value, with the tapered threshold:

```cpp
bool isLazyHigh(ScoreType lazyThreshold, const EvalFeatures &features, EvalScore &score) {
   //if (DynamicConfig::FRC) return false;
   score = features.SumUp();
   return Abs(score[MG] + score[EG]) / 2 > lazyThreshold;
}
```

```cpp
const bool forbiddenNNUE = forbidNNUE(p) && !DynamicConfig::forceNNUE;
if (DynamicConfig::useNNUE && !forbiddenNNUE) {
const ScoreType nnueThreshold = ScaleScore(EvalScore(DynamicConfig::NNUEThreshold, DynamicConfig::NNUEThresholdEG), data.gp);
      // we will stay to classic eval when the game is already decided (to gain some nps)
      ///@todo use data.gp inside NNUE condition ?
      if (EvalScore score; DynamicConfig::forceNNUE || forceNNUE ||
        !isLazyHigh(nnueThreshold, features, score)) {
         ...
         ScoreType nnueEval = NNUEEVal(p, data, context, features, height);
```

A worked example, ignoring imbalance: with all other material on the board,
being a rook up gives a lazy score of $(538 + 518)/2 = 528 \le 638$, so NNUE
evaluates the position; being a queen up gives $(1103 + 1076)/2 = 1089 > 638$, so
the full HCE runs. If that HCE score then lies within the second threshold
(between 344 at full material and 315 in a bare endgame), NNUE is still called.

Runtime `forceNNUE` overrides the position prohibition in the current
expression and the first threshold; the function's force argument bypasses
the threshold, not every earlier special case or prohibition. Read both
conditions before asserting "forced" means exclusively neural evaluation.

The developer `qsearchNoPruning()` requests forced NNUE. Production qsearch
uses the normal `eval()` selection; it does not universally force NNUE despite
a broader comment in the evaluation API.

After the full HCE score is computed, the second gate can still switch to NNUE:

```cpp
const ScoreType nnueThreshold2 = ScaleScore(EvalScore(DynamicConfig::NNUEThreshold2, DynamicConfig::NNUEThreshold2EG), data.gp);
if ( DynamicConfig::useNNUE && !forbiddenNNUE && Abs(hceScore) <= nnueThreshold2 ){
   ...
   const ScoreType nnueScore = NNUEEVal(p, data, context, features, height, true);
```

The second call reuses the same `features` object, so its endgame scaling factor
may already include the scaling refined by the HCE (opposite bishops, pawn count...),
whereas the first gate only sees the factor coming from the material table.
The rationale for mixed evaluation is to keep exact/specialized endgame
knowledge and avoid some neural work in strongly material-dominated positions,
while using NNUE where positional effects can outweigh a lazy material view.
The thresholds and score scaling are tuning decisions, not universal constants.

## Handcrafted evaluation

For feature equations, attack-mask definitions, tuned examples and cache
dependencies, see the
[classical evaluation reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/classical-evaluation-reference.md).

Features are grouped into material, positional, development, mobility, attack,
pawn structure and complexity. Each group keeps its own white-positive MG/EG
pair so that the optional style settings can rescale a whole group before the
sum. `eval()` applies the style factors; `EvalFeatures::SumUp()` only adds the
groups, with the complexity group multiplied by the sign of the middlegame sum:

```cpp
EvalScore EvalFeatures::SumUp() const {
   EvalScore score = scores[F_material] + scores[F_positional] + scores[F_development] + scores[F_mobility] + scores[F_pawnStruct] + scores[F_attack];
   score += scores[F_complexity] * sgn(score[MG]);
   return score;
}
```

Parameters and PSTs (piece-square tables: a bonus per piece type and square) are
defined in `evalConfig.*`; at startup `initEval()` precomputes the king-danger
table and `InitMaterialScore()` fills the material table.

| Family | Examples present in HCE |
|---|---|
| Material | Piece values, imbalance, pairs, pawn-count/piece interactions and specialized material classifications. |
| Piece placement | PSTs, development, bishop quality, outposts, king distances and rook/file relationships. |
| Activity | Mobility, safe/unsafe targets, pins and constrained pieces. |
| Tactical pressure | Hanging/undefended targets, piece threats and attacked king-zone squares. |
| King safety | Pawn shelter/storm, open/semi-open flanks, attack weights, defense and danger conversion. |
| Pawns | Passed/candidate/protected passers, backward, doubled, isolated and detached pawns, holes and file structure. |
| Endgame/complexity | Passer progress and king relations, initiative, drawish scaling and pawn-distribution effects. |

The two scores of a term can differ substantially. A king's shelter and
centralization, for example, need different middlegame and endgame incentives.
Style settings reshape HCE feature groups; they do not rewrite the network
weights.

### Pawn-and-king cache

Pawn structure changes rarely from one node to the next, so the HCE does not
recompute it each time. Each worker owns a direct-mapped **pawn hash table**
keyed by a Zobrist key of the pawns **and kings** (`computePHash()`; also stored
as `Position::ph`). Its size comes from the `PawnHash` option, divided among the
threads and rounded down to a power of two. A 32-bit tag detects a different
position in the slot, and a mismatch simply recomputes and overwrites the slot.
An entry holds:

```cpp
struct PawnEntry {
   colored<BitBoard>  pawnTargets   = {emptyBitBoard, emptyBitBoard};
   colored<BitBoard>  holes         = {emptyBitBoard, emptyBitBoard};
   colored<BitBoard>  semiOpenFiles = {emptyBitBoard, emptyBitBoard};
   colored<BitBoard>  passed        = {emptyBitBoard, emptyBitBoard};
   BitBoard           openFiles     = emptyBitBoard;
   EvalScore          score         = {0, 0};
   MiniHash           h             = nullHash;
   colored<ScoreType> danger        = {0, 0};
   ...
};
```

A cache miss constructs pawn attacks, passed pawns, holes, open/semi-open files,
the structural score (pawn PST, protected passers, candidates, backward, doubled,
isolated and detached pawns) and the pawn contributions to king danger. King
shelter, flank, storm and open-file terms are in the entry as well, which
explains the king dependency of the key.

The passed-pawn *evaluation* is deliberately not part of the entry. It needs the
side to move and the opposing non-pawn material (for the unstoppable-pawn test),
neither of which is in the key. `eval()` therefore fetches or builds the entry,
adds `pe.score` to the pawn-structure group, and only then evaluates the cached
passer bitboards:

```cpp
const Searcher::PawnEntry &pe = *pePtr; // let's use a ref instead of a pointer
// add computed or hash score for pawn structure
features.scores[F_pawnStruct] += pe.score;

// passer eval depends on some position stuff, can't be cached
evalPawnPasser<Co_White>(p, pe.passed[Co_White], features.scores[F_pawnStruct]);
evalPawnPasser<Co_Black>(p, pe.passed[Co_Black], features.scores[F_pawnStruct]);
```

Every field of the entry is thus a pure function of the pawns and kings. For
new terms, either keep dynamic dependencies (side to move, pieces, material)
outside the entry, as is done for passers, or extend the key. Most live tactical
geometry also remains outside the cache. The early `prefetchPawn()` call is
issued with the full position hash rather than the pawn key, so it does not
target the slot that is finally read. Evaluation-tuning builds bypass the table
and recompute a scratch entry every time.

`semiOpenFiles[color]` has a repository-specific convention: files containing
that color's pawn and no opposing pawn. Do not assume another engine's naming
convention.

### HCE finalization order

The feature groups are filled in dependency order: material, per-piece PST and
attack maps, the pawn entry and then the dynamic passer terms, king danger,
development and outposts, threats, mobility, and the rook/queen/pin terms and
material corrections. After
that the implementation:

1. Applies the style factors to each group, then sums the groups with `SumUp()`.
2. Adds the sign-aware initiative and the context contempt.
3. Refines endgame scaling for opposite bishops, queen-versus-no-queen and
   pawn distribution when no earlier specialized scaling applies.
4. Interpolates MG/EG with phase and endgame scaling.
5. Applies fifty-move scaling outside Armageddon.
6. Adds the side-dependent tempo in white-positive coordinates.
7. Clamps, converts to side-to-move coordinates and applies variant scoring.
8. Optionally switches to NNUE through the second threshold.

```cpp
ScoreType ret = ScaleScore(score, data.gp, features.scalingFactor);
...
if (!DynamicConfig::armageddon){
   // apply scaling factor based on fifty move rule
   ret = fiftyScale(ret, p.fifty);
}
...
// apply tempo
ret += EvalConfig::tempo * (white2Play ? +1 : -1);
...
// clamp score
ret = clampScore(ret);
...
// take side to move into account
ret = (white2Play ? +1 : -1) * ret;
```

The *fifty-move scaling* shrinks scores toward zero as the halfmove clock grows
(the factor is $1 - \mathrm{fifty}^2 / 10000$), and the *tempo* is a small bonus
for having the move. Armageddon suppresses ordinary drawish/fifty scaling in
relevant paths and uses variant-specific score shifts. Its draw semantics
differ from normal chess.

## NNUE features and architecture

NNUE ("efficiently updatable neural network") is a small network whose first
layer is *sparse*: its input is a long vector of 0/1 features, only a few of
which are active. Minic uses a **HalfKA**-style input: every feature combines the
square of one king (the *perspective's* own king), the square of a piece, and the
kind and owner of that piece. The network is evaluated once per perspective
(white and black), and each perspective has its own parameters.

Features use 64 king squares, 64 piece squares and 12 colored piece categories:
**49152 possible indices per perspective**. Both king and piece squares are
horizontally flipped unconditionally (`square ^ 7`); black's perspective does not
add a vertical flip. Ownership is us/them-relative, and white and black
transformers have separate input parameters.

The [NNUE reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/nnue-reference.md)
gives exact indexing examples, matrix layouts, parameter counts, inference
equations, update cases and the training/export contract.

The result of the first layer for one perspective is an **accumulator**: a vector
of 384 values equal to the input bias plus the sum of one weight row per active
feature. Because moving a piece only adds or removes a few features, the
accumulator can be updated *incrementally* by adding and subtracting rows
instead of being recomputed from the whole board.

The two accumulators each contain **384** values. At propagation they are
ordered side-to-move first, concatenated and fed to one of two dense buckets
(a *bucket* is an independent set of dense-layer parameters selected by a
property of the position):

```text
white features -> accumulator 384 --\
                                     -> STM-first concat 768
black features -> accumulator 384 --/

fc0: 768 -> 8, activation
fc1:   8 -> 8, activation; concatenate with fc0 -> 16
fc2:  16 -> 8, activation; concatenate with previous 16 -> 24
fc3:  24 -> 1 evaluation
optional fc3_uncertainty: 24 -> 1 log-variance
```

Bucket 0 is selected for `gamePhase < 0.45`, bucket 1 otherwise. The active
code uses phase, not the commented king-bucket or piece-count alternatives:

```cpp
int bucket = 0;
if (data.gp < 0.45f)
   bucket = 0;
else
   bucket = 1;

// call the net
const auto evalResult = p.evaluator().propagate(p.c, bucket);
```

**Quantization** stores the first-layer weights as 16-bit integers instead of
floats so that accumulator updates are cheap integer additions: each float read
from the network file is multiplied by a scale (512) and rounded, and
the accumulator is divided by that scale again when it feeds the dense layers.
The input weights and accumulators are therefore `int16_t` (quantized at load
time). Dense layers remain floating point and use a *clipped ReLU* activation,
$\min(\max(x, 0), 1)$, which bounds every activation to [0, 1]. SIMD and scalar
paths have the same layer topology, including concatenation skip connections.
The neural output is multiplied by `outFactor = 600`.

```cpp
template<> struct Quantization<true> {
   using WT = float;
   using WIT = int16_t;
   using BT = float;
   using BIT = int16_t;
   static constexpr WT scale {512}; // 32*512 = 16384 and |weight| & |bias| < 0.6
   static constexpr float outFactor {600.f};
   static float round(const float& x) { return std::round(x); }
};
```

Do not assume numerical equivalence merely from that topology: the SIMD path
clips/dequantizes input activations together, whereas the scalar fallback calls
`activationInput` after dequantizing. That helper's upper bound depends on the
quantization scale. Platform comparisons should check actual outputs as well
as the dense-layer structure.

### Incremental updates

Network weights are shared; each search-stack evaluator owns two accumulators
and a dirty state. Initial construction starts from input biases and inserts
occupied-square features.

Ordinary moves erase the old piece feature and insert the new one; captures
erase the victim. Promotions and en passant need their correct piece/square
changes. Moving a king changes its perspective's feature frame and requires
a rebuild; the other perspective can still use incremental changes. The
dispatcher in `applyMoveNNUEUpdate()` makes that distinction:

```cpp
if (Abs(moveInfo.fromP) != P_wk || (getBlock(moveInfo.from) == getBlock(moveInfo.to))) {
   // ***be carefull here***, p.c has already been updated !!!
   if (p.c == Co_Black) updateNNUEEvaluator<Co_White>(p.evaluator(), moveInfo);
   else updateNNUEEvaluator<Co_Black>(p.evaluator(), moveInfo);
}
// if a king was moved (including castling!!), reset nnue evaluator
// this is based on new position state !
else { 
   p.resetNNUEEvaluator(p.evaluator(), ~p.c);
   ...
}
```

Dirty state, explicit reset and evaluator association are part of correctness,
not optional performance hints. Propagation asserts a clean evaluator rather
than automatically repairing a dirty one. Row prefetching before these updates
(`WITH_NNUE_PREFETCH`) is implemented but disabled by default (see
[disabled and inert parts](#disabled-and-inert-parts)).

### Converting a neural output into an engine score

`NNUEEVal()`:

1. Propagates the selected bucket and converts the evaluation to `ScoreType`.
2. Applies phase/endgame scaling to that scalar.
3. Multiplies by `NNUEScaling / 64` (default 48, adjustable between 32 and 256).
4. Adds phase-scaled contempt, clamps and applies fifty scaling if appropriate.
5. Applies variant scoring using absolute ply and explicit search height.

```cpp
nnueScore = ScaleScore({nnueScore, nnueScore}, data.gp, features.scalingFactor);
// NNUE evaluation scaling
nnueScore = static_cast<ScoreType>((nnueScore * DynamicConfig::NNUEScaling) / 64);
// take contempt into account (no tempo with NNUE, 
// the HalfKA net already take it into account but this is necessary for "dynamic" contempt from opponent)
nnueScore += ScaleScore(context.contempt, data.gp);
// clamp score
nnueScore = clampScore(nnueScore);
if (!DynamicConfig::armageddon){
   // apply scaling factor based on fifty move rule
   nnueScore = fiftyScale(nnueScore, p.fifty);
}
```

The `NNUEScaling` factor maps the network output to the scale of the classical
evaluation so that the search parameters (margins, thresholds) remain meaningful
with either evaluator.

Unlike HCE, it does not add the separate HCE tempo term. The side-to-move
accumulator ordering already participates in the learned evaluation.

The uncertainty field is copied from propagation separately; these score
transformations do not automatically rescale it.

Contempt is a search-context bias (a small offset that makes the engine avoid or
seek draws against a given opponent), not an attribute learned by the network.
The driver constructs an MG/EG pair from the root side, configured contempt,
additional MG contempt and opponent rating factor. When the EG contempt is not
negative, a score-dependent dynamic adjustment
`round(25 * tanh(previousIterationScore / 400))` is added to both components
([searcherDriver.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/searcherDriver.cpp)).
Ordinary draw returns still use `drawScore()`, rather than returning that pair.

### File compatibility

Current network version is `0xc0ffee03`; it is unrelated to the engine version
string (base version, short revision identifier and an optional `-dirty` suffix).
The file comes from the `NNUEFile` option (`none` or empty selects the HCE, and
`embedded` selects a network compiled into the binary when the build provides one).
Expected file sizes are 151049100 bytes
without the optional head and 151049300 with it. A head-disabled build accepts
either and ignores the trailing head; a head-enabled build expects the larger
format. Filesystem-disabled builds omit the disk file-size check.

The on-disk parameters are floats, quantized during loading. The large input
matrices account for most of the file size; the inference representation is
smaller. This is Minic's format, not a generic Stockfish NNUE file.

## Correction history

Correction history learns systematic evaluation **bias** from selected search
outcomes: when the search result at a node repeatedly differs from the static
evaluation in the same kind of structure, the static evaluation of similar
positions is shifted by that average difference. It is applied in PVS/qsearch,
not baked into network weights, and each search thread has its own tables:

```text
correction = pawn+king table[side][pawnKey]
           + nonPawn table[side][whiteNonPawnKey]
           + nonPawn table[side][blackNonPawnKey]
correctedStatic = clamp(rawStatic + correction)
```

```cpp
ScoreType Searcher::correctionScore(const Position& p) const {
   int correction = 0;
#ifdef WITH_PAWN_CORRHIST
   correction += pawnCorrHist.score(p.c, p.ph);
#endif
#ifdef WITH_NONPAWN_CORRHIST
   correction += nonPawnCorrHist.score(p.c, nonPawnKey(p, Co_White));
   correction += nonPawnCorrHist.score(p.c, nonPawnKey(p, Co_Black));
#endif
   return static_cast<ScoreType>(correction);
}
```

Both compile-time gates, `WITH_PAWN_CORRHIST` and `WITH_NONPAWN_CORRHIST`, are
enabled in [config.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/config.hpp). The same non-pawn table is queried for
both color-specific keys, which exclude the kings. Each active
table has 16384 buckets per side. Mate scores are not shifted.

At learning time:

```text
residual = bestScore - correctedBaseline
change   = clamp(residual * min(depth, 16) / 16, -maximum, +maximum)
item    += change - item * abs(change) / maximum
```

```cpp
FORCE_FINLINE void update(Color c, Hash key, DepthType depth, int evalDiff, int correctionMax) {
   ScoreType & item = table[c][key & (size - 1)];
   const int depthWeight = std::min(static_cast<int>(depth), SearchConfig::correctionHistoryDepthCap);
   const int change = std::clamp(evalDiff * depthWeight / SearchConfig::correctionHistoryDepthScale,
                                  -correctionMax, correctionMax);
   item = static_cast<ScoreType>(item + change - (item * Abs(change)) / correctionMax);
}
```

The pawn maximum is 32; each non-pawn contribution uses 44. The subtracted
term pulls the item back toward zero, which keeps each table value within
its maximum, so the three active contributions can total 120 in
magnitude. Hash collisions make this contextual learning, not an exact
per-position correction.

PVS learns only under appropriate non-check/non-mate, quiet-result and bound
conditions (not in check, a valid best move exists and is not a capture or
promotion, the score
is not a mate score, and a fail-high result whose score is not above the corrected
evaluation, or a fail-low result not below it, is ignored because such a bound
does not reveal a bias).
Quiescence search reads the corrected evaluation but never updates the tables.
A third, horizon-context correction table (`WITH_HORIZON_CORRHIST`) is
implemented but disabled by default. Correction magnitude does not currently
alter LMR by itself.

## Uncertainty: available scaffolding, not active selectivity

`WITH_NNUE_UNCERTAINTY` is disabled by default. Without it, propagation returns a placeholder
variance of 1; `EvalData::uncertainty` is not a calibrated signal. Nothing in the
search currently reads that field. The Python exporter always writes the
head, so its files have the larger size.

With the optional head:

```text
variance = exp(clamp(logVariance, -10, 10))
```

The current Python trainer's uncertainty loss operates on the transformed
prediction/target domain. It mixes search-score-derived sigmoid targets and
game outcomes; its variable named `mse` is currently a powered absolute error
with exponent 2.4. Do not assume the head already predicts variance in squared
engine centipawns or implements a calibrated Gaussian-error model.

Before using it for selectivity, establish meaningful targets, calibration and
units; propagate a presence/validity signal through NNUE, HCE and TT paths;
then measure a conservative search response.

Correction magnitude is a different signal: learned signed bias is not
prediction variance. Both experiments can be interesting independently, but
neither guarantees a tactical-safety interpretation. The proposed bounded LMR
experiments and eventual combination are described in [TODO](https://github.com/tryingsomestuff/Minic/blob/master/TODO.md).

## Disabled and inert parts

Several evaluation and NNUE mechanisms are compiled in, or present in the
source, without affecting play in the default configuration. The complete
inventory, with code excerpts and caveats, is the
[implemented but disabled techniques](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/disabled-features.md)
chapter; the evaluation-related items are:

| Item | Why it is inactive by default | How to enable |
|---|---|---|
| Complexity term (HCE) | Computed, but its MG style factor is `(styleComplexity - 50) / 50 = 0` at the default 50. | UCI `StyleComplexity` other than 50. |
| Forwardness term (HCE) | Skipped unless `styleForwardness != 50`. | UCI `StyleForwardness`. |
| Pawn-structure style | `stylePawnStruct` is read but has no UCI option, so it stays 50. | Source edit. |
| `forbidNNUE()` | Its opposite-bishop rule is commented out; it always returns false. | Uncomment the rule (runtime `forceNNUE` still overrides it). |
| `Ter_Draw` material class | Handled in `eval()`, but no material-table entry is assigned it. | Classify entries in `material.cpp`. |
| Uncertainty head | `//#define WITH_NNUE_UNCERTAINTY` in `config.hpp`; nothing in search reads the value. | Define the macro and use a net that contains the head. |
| NNUE row prefetch | `//#define WITH_NNUE_PREFETCH`. | Define the macro, then measure speed. |
| King-square or occupancy buckets | Commented selectors in `NNUEEVal()`; the weights have two phase buckets. | Needs a net trained for those buckets. |
| Horizon correction table | `//#define WITH_HORIZON_CORRHIST`. | Define the macro. |

All the style settings only reshape the HCE, so they have no effect on
positions that NNUE evaluates. The details are in the
[classical evaluation reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/classical-evaluation-reference.md#disabled-and-inert-terms)
and the [NNUE reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/nnue-reference.md#disabled-and-inactive-nnue-code).

## Training and serialization

[Train/README.md](https://github.com/tryingsomestuff/Minic/blob/master/Train/README.md) documents the separate Python/PyTorch
pipeline. It is not part of the normal C++ engine build.

`train.py` loads binary datasets (by default through the C++ sparse loader
`training_data_loader.cpp`, which builds the feature indices and the phase
bucket; `halfka.py` is the equivalent dense path used with `--py-data`), and
`model.py` implements the network and loss. `serialize.py` converts `.ckpt`/`.pt`
models into the engine's binary parameter order. Training-only factorization must
be folded into the exported transformer.

`run_games.py` and `plot.py` support candidate evaluation. If changing features,
dimensions, buckets, activation or output heads, update trainer, exporter,
loader version/size and both inference paths together. Compare engine outputs
with the exported model; successful loading alone does not prove equivalence.
