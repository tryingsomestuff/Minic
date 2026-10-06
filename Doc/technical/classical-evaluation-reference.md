# Classical evaluation reference

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md)
| [Evaluation overview](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/evaluation.md)

This is a feature/data-flow reference to
[eval.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/eval.cpp),
[evalTools.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/evalTools.hpp),
[evalConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/evalConfig.cpp)
and [bitboardTools.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/bitboardTools.hpp).
Numerical pairs below are `(MG, EG)` internal scores at the documented snapshot.
Names such as "bonus" and "malus" express intent; tuned signs can disagree with
that intent.

## Numeric domains and material

Minic has separate material value sets in
[definition.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/definition.cpp):

| Piece | MG | EG | Game-phase weight | SEE |
|---|---|---|---|---|
| Pawn | 85 | 93 | 93 | 86 |
| Knight | 359 | 290 | 290 | 231 |
| Bishop | 393 | 301 | 301 | 296 |
| Rook | 538 | 518 | 518 | 571 |
| Queen | 1103 | 1076 | 1076 | 1048 |
| King | 8000 | 8000 | 8000 | 8000 |

Kings are not part of the ordinary phase numerator/denominator. This separation
prevents a SEE tuning change from silently being treated as a material-eval or
phase tuning change.

Fallback phase is remaining pawn/piece phase-weight sum divided by the starting
army total, capped at 1. A normal material-table hit supplies phase as an
8-bit quantized fraction and a precomputed white-minus-black score.

The material score includes second-degree imbalance from
[material.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/material.cpp):

```text
imbalance(c) =
  sum over m1 and m2 <= m1:
    ownCoeff[m1][m2] * ownCount[m1] * ownCount[m2]
  + opponentCoeff[m1][m2] * ownCount[m1] * opponentCount[m2]
  all divided by 16, separately for MG and EG
```

The table subtracts black's imbalance from white's. In an eval-tuning build
these precomputed imbalance contributions are set to zero; the out-of-table
material fallback also does not reproduce that polynomial. These are real
path differences.

## Evaluation construction order

HCE starts with material, then builds non-pawn PST/attack information, obtains
pawn/king cached terms, derives attack quality and dynamic pawn/piece features,
adds mobility and remaining placement/material corrections, applies style,
initiative, contempt and final scaling.

The order is dependency-driven: safe mobility needs both sides' attacks;
outposts need pawn holes and defenders; nonlinear king attack needs raw danger;
endgame scaling needs the strong side and passer counts.

Seven feature groups retain white-positive MG/EG pairs:

```text
material | positional | development | mobility
attack   | pawnStruct | complexity
```

`EvalData` contains selected persistent summaries; the local feature groups do
not survive the call. Search-side `evalFeatures()` is not the complete HCE.

## PST orientation and placement

Non-pawn squares use `relative_square(~color, square)` to address the stored
PST orientation. White-positive accumulation adds white and subtracts black;
pawns use the equivalent color-template helper.

`styleForwardness != 50` adds an MG-only rank adjustment:

```text
((styleForwardness - 50) * orientedRank) / 8
```

This is distinct from the later multiplicative style setting for the
positional feature group. NNUE input coordinates have a different convention;
do not reuse PST orientation helpers in its exporter.

## Attack-map construction

For each non-pawn piece, compute two coverages:

```text
shadow = coverage(square, occupancy XOR ownNonPawnOccupancy)
actual = coverage(square, occupancy)
```

The shadow board removes own non-pawn blockers, including the king, to estimate
aligned pressure into the opposing king zone. Actual attacks preserve the
real occupancy and populate:

| Name | Meaning |
|---|---|
| `att[c]` | Union of attacks. |
| `att2[c]` | Squares reached by overlapping attack sets. |
| `attFromPiece[c][kind]` | Union for a particular attacking kind. |
| `checkers[c][kind]` | Source squares of pieces actually attacking the enemy king. |

Before merging a new actual set `T`, `att2 |= att & T`, then `att |= T`.
Pawn targets from the cache later join `att`/`att2`.

The current HCE loop begins at knight, not pawn. It merges pawn attacks into
aggregate maps and fills pawn checker sources, but does not populate the pawn
row of `attFromPiece` in this path. The search-side reconstruction loops over
pawns too. Therefore their `goodThreats` construction must not be assumed
identical merely because the final formula has the same spelling.

## Weak, safe and protected squares

Let `A[c]` and `A2[c]` denote aggregate and double attacks, and `K[c]`, `Q[c]`
the king/queen attack maps. For one side:

```text
weak[c] = A[opponent] & ~A2[c] & (~A[c] | K[c] | Q[c])
safe[c] = ~A[opponent] | (weak[opponent] & A2[c])
protected[c] =
    pawnTargets[c]
  | (A[c] & ~A[opponent])
  | (A2[c] & ~A2[opponent])
```

These are tactical approximations, not proof that a king move or capture is
legal. A square defended only by a high-value piece can remain weak;
multiple favorable attackers can make a contested square count as safe.

`haveThreats[c]` tests attacks on any opposing piece. `goodThreats[c]` emphasizes
pawn attacks on non-pawns, minor attacks on rooks/queens and rook attacks on
queens. It is not computed using SEE for every threatened target.

## King safety: raw pressure to MG score

Raw `kdanger` combines shadow attacks on the enemy king zone, actual defense
of one's own zone, cached pawn/flank effects and safe-check rewards. Attack
weights are `(125, 189, 251, 153, 153, -39)` for pawn through king; defense
weights are `(41, 100, 96, 48, 26, 0)`.

Safe-check weights are `(122, 785, 569, 934, 776, 0)`. They apply to checker
source squares intersected with the relevant protection/safe set; this is a
reward for existing checker geometry, not generation of every possible future
checking move.

Absence of an opposing queen subtracts 585 from danger. The nonlinear table is:

$$
\begin{aligned}
i &= \operatorname{clip}\!\left(\operatorname{trunc}(D/32),0,63\right),\\
K(i) &= \operatorname{trunc}\!\left(
  \frac{492}{1+\exp((35-i)/11)}-5\right).
\end{aligned}
$$

$D$ is raw king danger, and $K(i)$ is the integer table value. Both casts
truncate toward zero rather than applying a mathematical floor.

White's table value is subtracted from white-positive MG attack score;
black's is added. EG gets zero from this conversion. Raw danger, rather than
the nonlinear score, is also exposed to search.

The sigmoid saturates high pressure and avoids an unbounded linear penalty.
Integer division, index clipping and conversion can create small plateaus;
threshold consumers should not pretend the final score is continuous.

## Cached pawn classification

The key includes both kings and all pawns. Useful primitives:

| Primitive | Implementation interpretation |
|---|---|
| `frontSpan<C>` | All squares ahead on the file, excluding the pawn's square. |
| `pawnPassed<C>` | Own pawn with no opposing pawn ahead on its file or adjacent files. |
| `pawnDoubled<C>` | Own pawn that lies ahead of another own pawn on its file. |
| `pawnIsolated` | Own pawn with no friendly pawn anywhere on either adjacent file. |
| `pawnHoles<C>` | Squares outside forward-filled friendly pawn attack coverage, then limited to the side's hole zone. |
| `pawnBackward<C>` | Pawn whose advance meets opposing pawn attacks and lacks forward-filled friendly pawn support, with the coded occupancy test. |
| `pawnCandidates<C>` | Semi-open pawns whose advance meets the coded single/single or double/double pawn-attack balance. |
| `pawnDetached<C>` | Advanced pawn deemed undefendable through occluded pawn advance and locally alone. |

These definitions are bitboard approximations. In particular, a candidate is
not a proven eventual passer and a hole is not necessarily an exploitable
outpost.

Backward, doubled, isolated and detached penalties use rank-indexed
`[rank][Close/SemiOpen]` MG/EG tables. For white they are subtracted; for black,
the rank is reversed and the same coefficient is added. Detached pawns already
classified as backward are excluded from the detached contribution.

`semiOpenFiles[c]` marks full files with own pawns but no opposing pawns.
`pawnSemiOpen<C>()`, used to classify individual pawn penalties, instead looks
for absence of an opposing pawn ahead along the file. Do not conflate these
two similarly named masks.

## Passed pawns and races

Cached ordinary passer score combines rank bonus and two
Chebyshev-distance/rank tables for support and defense. An unstoppable test is:

```text
opponentNonPawnCount == 0
and distance(opponentKing, promotionSquare) - (sideToMove != pawnColor)
    > min(5, distance(pawnSquare, promotionSquare))
```

If true, add rook SEE-independent material value minus pawn value instead of
the ordinary passer expression. The code deliberately uses rook, not queen,
to keep promotion progress attractive.

Protected passers add a separate rank table when the pawn's square is covered
by friendly pawn attacks. Candidates have their own rank table.

Dynamic terms outside the cached entry include:

- A free-passer rank bonus when no opposing piece occupies its forward span.
  This does not check every intervening square for enemy attacks.
- Rooks behind passers, with own minus opposing rook counts along rear spans.
  The tuned pair is `(-19, 38)`, showing different MG/EG incentives.

**Cache dependency caveat:** `evalPawnPasser()` runs inside cache construction,
but its race test reads side to move and opponent non-pawn material. Those
fields are not part of the pawn-and-king key. The cache therefore cannot be
described as every cached term being a pure function of that key. This guide
records the actual dependency rather than asserting an idealized cache
invariant.

## Pawn shelter and king flanks

The cached entry also includes:

| Term | Computation outline |
|---|---|
| Shield | Count pawns in the relevant king-zone shield mask; multiply `(6, -8)` by `min(count^2, 9)`. |
| Pawnless flank | Add `(24, -39)` if no own pawn occupies the king's flank mask. |
| Storm | Subtract `(24, -27)` for enemy pawns in the coded near-flank rank zones. |
| Open files | Add raw danger for each flank file with no pawn, counting full-file bits and dividing by 8. |
| Semi-open files | Use own/opponent file classes with different raw danger weights. |
| Fawn patterns | Small a/h-file pawn-and-king masks with integer `/4` weighting. |

The last family is deliberately heuristic and its tuned signs are not a
general chess rule. Cached king placement explains why moving a king changes
`ph` even if no pawn moves.

## Development and outposts

| Feature | Exact dependency and typical intent |
|---|---|
| Piece in front of own pawn | Shift pawn squares forward and intersect own non-pawns; coefficient `(-17, 10)`. |
| Pawn in front of own minor | Shift pawns backward and intersect minors; rank table rather than one fixed bonus. |
| Center control | Protected-square count in the extended center, coefficient `(8, -1)`. |
| Unprotected holes | Own hole mask minus protected squares, coefficient `(-23, -1)` with division by 4. |
| Knight outpost | Knight on opponent hole and protected by own pawn, coefficient `(33, 14)`. |
| Bishop outpost | Equivalent bishop test, coefficient `(48, -2)`. |
| Minor on open file | Pawn-protected minor on an open-file mask, coefficient `(4, 0)`. |
| Knight king distance | Table indexed by the smaller distance to either king. |

The distance table's tuned values are not uniformly negative. Its comment
"knight too far penalty" does not imply that every indexed entry is a penalty.

## Threat and mobility features

Hanging pieces are own non-pawns on weak squares, with pair `(-29, -18)`.
Minor-piece threats target opposing non-pawns or weak pawns; rook/queen/king
threats use weak opposing squares and victim-kind-indexed tables.

Safe pawn attacks on opposing non-pawns use `(103, 36)`. Safe single pushes
into empty squares use `(-12, 13)` in mobility; attacks created from those
pushed squares use `(26, 20)` in attack score. Double pawn pushes are not
enumerated by this particular evaluation term.

For each N/B/R:

```text
mobilityCount = popcount(coverage & ~ownOccupancy & safeSquares)
feature += MOB[kind][mobilityCount]
```

Queen diagonal and orthogonal mobility have separate tables. King mobility
uses squares not attacked by the opponent, rather than the broader safe-square
mask. Pawn-push mobility does not simply join the persistent piece mobility
counter.

The tables need not be linear or monotone. Replacing them with "count times
weight" changes the evaluation model.

## Rooks, queens, pins and material refinements

Rook open-file pair is `(41, 24)`; the two semi-open classes use `(24, -2)`
and `(28, 17)`. Queen/rook file alignment uses `(-1, 35)`.
Facing enemy rooks are tested with front-span geometry, not a full legal
exchange calculation. Queen proximity uses `7 - ChebyshevDistance` to the
enemy king and coefficient `(10, 6)`.

King/queen pin terms use a helper that identifies opposing sliding pinners
with selected occupancy and intersects own pieces along rays. The present
helper's final `between()` call uses `p.king[c]` even when called with a queen
square. Describe these as implemented pin features, not a general-purpose
exact pinned-to-arbitrary-square API.

Queen-square terms select the first queen in the bitboard; the source itself
notes the multi-queen limitation. Pawn-count-dependent knight/rook adjustments,
same-square-color bishop pawn penalties and pair adjustments then refine
material. Pair "malus" names also do not guarantee negative tuned coefficients:
the knight pair is `(14, -7)`, rook pair `(58, -30)`.

## Style, initiative and final scaling

The ordinary style multipliers affect **MG only**:

```text
MGgroup *= 1 + (style - 50) / 50
EGgroup *= 1
```

Complexity instead uses `(styleComplexity - 50)/50` for MG and zero for EG.
At default 50 its contribution is zero, even though a raw side-to-move
piece/pawn-count complexity term was calculated.

Initiative is computed after summation from pawn count, pawns on both flanks,
a pawn-only ending flag and an offset:

```text
rawInitiative =
    (-1, 6) * totalPawnCount
  + (70, 43) * pawnsOnBothFlanks
  + (115, 65) * exactlyTwoNonPawnOccupiedSquares
  - (59, 83)
```

In ordinary chess the last boolean recognizes kings and pawns only.
Each phase then uses:

```text
initiative = sign(score) * max(rawInitiative, -abs(score))
```

This prevents the initiative adjustment alone from reversing that phase's
sign, while permitting either amplification or contraction. Contempt follows.

When no prior material scaling applies and Armageddon is off, the strong side
is white if the current EG score is positive and black otherwise. EG scale is:

| First qualifying case | Scale |
|---|---|
| Opposite bishops, no other non-pawns | `(34 + 20*strongSidePassedPawnCount)/128`. |
| Opposite bishops with other material | `(80 + popcount(strongSideOccupancy))/128`; occupancy includes pawns and king. |
| Exactly one queen on the whole board | `(79 + 0*queenlessSideMinorCount)/128`, currently constant. |
| Otherwise | `min(1, (75 + 25*strongSidePawnCount)/128)`. |

If pawns exist on only one flank, subtract the one-flank term afterwards.
Its coefficient is **-20**, so this increases the scale by 20/128; it is not
necessarily capped back to 1. A comment saying "scale down" is not the actual
arithmetic.

Phase interpolation is:

$$
S=\operatorname{trunc}\!\left(
g\,S_{\mathrm{MG}}+(1-g)\,s_{\mathrm{EG}}\,S_{\mathrm{EG}}\right).
$$

$g$ is game phase and $s_{\mathrm{EG}}$ the specialized endgame scale.

Only EG is multiplied by this specialized scaling. For MG 100, EG 200,
phase .25 and EG scale .5, the fused score is 100, not 125 or 175.

Fifty scaling in
[score.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/score.hpp)
is `1 - fifty^2 / 10000`, not a linear fade. At counts 50/80/90 the factors
are .75/.36/.19. HCE applies this before tempo 15; NNUE adds contempt before
its own fifty scaling and has no separate tempo. Those orders are observable.

## Endgame knowledge and scope

With fewer than five total non-pawns and eligible material classification,
evaluation checks for an actual legal generated capture before using helpers.
KXK pushes the weaker king to edges and brings kings closer; KmmK uses
corner geometry; KPK probes the normalized bitbase. KBPK can approximate a
wrong-color bishop ending using KPK knowledge.

These helpers are not Syzygy DTZ, nor a complete replacement for mate/stalemate
search. Material-table classifications can amplify or suppress EG scaling
without claiming an exact win proof.

When adding a feature, specify its score group, orientation, cache dependency,
MG/EG meaning, style interaction, narrow-type range and whether search consumes
its auxiliary data. A term's intuitive chess label is only the starting point.
