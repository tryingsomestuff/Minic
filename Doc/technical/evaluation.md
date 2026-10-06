# Evaluation and NNUE

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md) | [Next: time and parallelism](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/time-and-parallelism.md)

Primary sources: [eval.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/eval.cpp),
[evalDef.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/evalDef.hpp), [evalTools.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/evalTools.hpp),
[evalConfig.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/evalConfig.hpp), [score.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/score.hpp),
[material.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/material.cpp), [nnue.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/nnue.hpp),
[nnue/weight.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/nnue/weight.hpp) and
[correctionHistory.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/correctionHistory.cpp).

## Score conventions and evaluation data

`eval()` returns a score for the **side to move**. HCE accumulates white-positive
middlegame/endgame pairs in `EvalScore`; `ScaleScore()` combines them:

```text
score = gamePhase * MG + (1 - gamePhase) * endgameScaling * EG
```

Game phase is high with more material. The material table quantizes phase into
an 8-bit value; fallback phase computation derives it from piece/pawn material.

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

Bare kings and antichess have early behavior. Recognized low-material positions
can use specialized helpers, including KPK, when eligible and not obscured by
a legal capture. Material classifications also determine scaling/drawishness.
NNUE does not erase those layers.

NNUE requires successful loading and must not be forbidden for the position.
The first threshold compares a lazy material-based score against phase-dependent
`NNUEThreshold`/`NNUEThresholdEG`. The second compares the completed HCE score
with `NNUEThreshold2`/`NNUEThreshold2EG`.

Runtime `forceNNUE` overrides the position prohibition in the current
expression and the first threshold; the function's force argument bypasses
the threshold, not every earlier special case or prohibition. Read both
conditions before asserting "forced" means exclusively neural evaluation.

The developer `qsearchNoPruning()` requests forced NNUE. Production qsearch
uses the normal `eval()` selection; it does not universally force NNUE despite
a broader comment in the evaluation API.

The rationale for mixed evaluation is to keep exact/specialized endgame
knowledge and avoid some neural work in strongly material-dominated positions,
while using NNUE where positional effects can outweigh a lazy material view.
The thresholds and score scaling are tuning decisions, not universal constants.

## Handcrafted evaluation

For feature equations, attack-mask definitions, tuned examples and cache
dependencies, see the
[classical evaluation reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/classical-evaluation-reference.md).

Features are grouped into material, positional, development, mobility, attack,
pawn structure and complexity. `EvalFeatures::SumUp()` applies optional style
weights. Parameters and PSTs are defined in `evalConfig.*`; initialization
precomputes transformed tables.

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

Each worker has a direct-mapped cache keyed by pawns **and kings**. A cache miss
constructs pawn attacks, passers, holes, open/semi-open files, structural score
and pawn contributions to king danger. King shelter, flank and storm terms
explain the king dependency.

Most live tactical geometry remains outside this cache. There is an important
exception: cached `evalPawnPasser()` uses side to move and opposing non-pawn
material in its unstoppable test, neither of which is included in the key.
Therefore the present entry is not universally a pure function of pawns/kings.
For new terms, extend the key or keep those dynamic dependencies outside it.

`semiOpenFiles[color]` has a repository-specific convention: files containing
that color's pawn and no opposing pawn. Do not assume another engine's naming
convention.

### HCE finalization order

After summing feature groups, the implementation:

1. Adds sign-aware initiative and context contempt.
2. Refines endgame scaling for opposite bishops, queen-versus-no-queen and
   pawn distribution when no earlier specialized scaling applies.
3. Interpolates MG/EG with phase and endgame scaling.
4. Applies fifty-move scaling outside Armageddon.
5. Adds the side-dependent tempo in white-positive coordinates.
6. Clamps, converts to side-to-move coordinates and applies variant scoring.
7. Optionally switches to NNUE through the second threshold.

Armageddon suppresses ordinary drawish/fifty scaling in relevant paths and
uses variant-specific score shifts. Its draw semantics differ from normal chess.

## NNUE features and architecture

Features use 64 king squares, 64 piece squares and 12 colored piece categories:
**49152 possible indices per perspective**. Both king and piece squares are
horizontally flipped unconditionally; black's perspective does not add a
vertical flip. Ownership is us/them-relative, and white and black transformers
have separate input parameters.

The [NNUE reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/nnue-reference.md)
gives exact indexing examples, matrix layouts, parameter counts, inference
equations, update cases and the training/export contract.

The two accumulators each contain **384** values. At propagation they are
ordered side-to-move first, concatenated and fed to one of two dense buckets:

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
code uses phase, not the commented king-bucket or piece-count alternatives.

The input weights and accumulators are quantized to `int16_t` at load/inference
time with scale 512. Dense layers remain floating point and use clipped ReLU
in [0, 1]. SIMD and scalar paths have the same layer topology, including
concatenation skip connections. The neural output is multiplied by
`outFactor = 600`.

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
a rebuild; the other perspective can still use incremental changes.

Dirty state, explicit reset and evaluator association are part of correctness,
not optional performance hints. Propagation asserts a clean evaluator rather
than automatically repairing a dirty one. `WITH_NNUE_PREFETCH` is currently disabled.

### Converting a neural output into an engine score

`NNUEEVal()`:

1. Propagates the selected bucket and converts the evaluation to `ScoreType`.
2. Applies phase/endgame scaling to that scalar.
3. Multiplies by `NNUEScaling / 64`.
4. Adds phase-scaled contempt, clamps and applies fifty scaling if appropriate.
5. Applies variant scoring using absolute ply and explicit search height.

Unlike HCE, it does not add the separate HCE tempo term. The side-to-move
accumulator ordering already participates in the learned evaluation.

The uncertainty field is copied from propagation separately; these score
transformations do not automatically rescale it.

Contempt is a search-context bias, not an attribute learned by the network.
The driver constructs an MG/EG pair from the root side, configured contempt,
additional MG contempt and opponent rating factor. A score-dependent dynamic
adjustment uses `25 * tanh(previousIterationScore / 400)` on its eligible path.
Ordinary draw returns still use `drawScore()`, rather than returning that pair.

### File compatibility

Current network version is `0xc0ffee03`. Expected file sizes are 151049100 bytes
without the optional head and 151049300 with it. A head-disabled build accepts
either and ignores the trailing head; a head-enabled build expects the larger
format. Filesystem-disabled builds omit the disk file-size check.

The on-disk parameters are floats, quantized during loading. The large input
matrices account for most of the file size; the inference representation is
smaller. This is Minic's format, not a generic Stockfish NNUE file.

## Correction history

Correction history learns systematic evaluation **bias** from selected search
outcomes. It is applied in PVS/qsearch, not baked into network weights:

```text
correction = pawn+king table[side][pawnKey]
           + nonPawn table[side][whiteNonPawnKey]
           + nonPawn table[side][blackNonPawnKey]
correctedStatic = clamp(rawStatic + correction)
```

The same non-pawn table is queried for both color-specific keys. Each active
table has 16384 buckets per side. Mate scores are not shifted.

At learning time:

```text
residual = bestScore - correctedBaseline
change   = clamp(residual * min(depth, 16) / 16, -maximum, +maximum)
item    += change - item * abs(change) / maximum
```

The pawn maximum is 32; each non-pawn contribution uses 44. With bounded
individual table values, the three active contributions can total 120 in
magnitude. Hash collisions make this contextual learning, not an exact
per-position correction.

PVS learns only under appropriate non-check/non-mate, quiet-result and bound
conditions. The optional horizon-context correction table exists but its gate
is disabled. Correction magnitude does not currently alter LMR by itself.

## Uncertainty: available scaffolding, not active selectivity

`WITH_NNUE_UNCERTAINTY` is off. Without it, propagation returns a placeholder
variance of 1; `EvalData::uncertainty` is not a calibrated signal.

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

## Training and serialization

[Train/README.md](https://github.com/tryingsomestuff/Minic/blob/master/Train/README.md) documents the separate Python/PyTorch
pipeline. It is not part of the normal C++ engine build.

`train.py` loads binary datasets, `halfka.py` supplies features, and `model.py`
implements the network/loss. `serialize.py` converts `.ckpt`/`.pt` models into
the engine's binary parameter order. Training-only factorization must be folded
into the exported transformer.

`run_games.py` and `plot.py` support candidate evaluation. If changing features,
dimensions, buckets, activation or output heads, update trainer, exporter,
loader version/size and both inference paths together. Compare engine outputs
with the exported model; successful loading alone does not prove equivalence.
