# Classical evaluation reference

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md)
| [Evaluation overview](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/evaluation.md)

This is a feature/data-flow reference to
[eval.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/eval.cpp),
[evalTools.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/evalTools.hpp),
[evalConfig.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/evalConfig.cpp)
and [bitboardTools.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/bitboardTools.hpp).
Numerical pairs below are `(MG, EG)` internal scores with the current tuned
values. Each feature is stored as a *middlegame/endgame pair* and the final score
is a *tapered* blend of the two according to the game phase (see the
[evaluation overview](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/evaluation.md)).
Names such as "bonus" and "malus" express intent; tuned signs can disagree with
that intent. Most features are computed for white and black with opposite signs,
so every group is **white-positive**: a positive value favors white whoever is
to move.

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
army total (2 queens, 4 rooks, 4 bishops, 4 knights and 16 pawns), capped at 1.
A normal material-table hit supplies phase as an 8-bit quantized fraction and a
precomputed white-minus-black score.

The **material table** is indexed by the piece counts of both sides, and each
entry caches the phase, the material score (including imbalance) and a
classification used for endgame knowledge. It only covers counts up to two queens,
rooks, knights and bishops of each square color per side and up to eight pawns;
other material combinations use the fallback computation. The fallback sums plain
piece values and omits the imbalance polynomial below.

The material score includes second-degree imbalance from
[material.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/material.cpp)
(an idea from Tord Romstad), which lets the value of a piece depend on which other
pieces both sides own:

```cpp
EvalScore Imbalance(const Position::Material &mat, Color c) {
   EvalScore bonus = 0;
   for (Mat m1 = M_p; m1 <= M_q; ++m1) {
      if (!mat[c][m1]) continue;
      for (Mat m2 = M_p; m2 <= m1; ++m2) {
         bonus += EvalConfig::imbalance_mines[m1 - 1][m2 - 1] * mat[c][m1] * mat[c][m2] +
                  EvalConfig::imbalance_theirs[m1 - 1][m2 - 1] * mat[c][m1] * mat[~c][m2];
      }
   }
   return bonus / 16;
}
```

```text
imbalance(c) =
  sum over m1 and m2 <= m1:
    ownCoeff[m1][m2] * ownCount[m1] * ownCount[m2]
  + opponentCoeff[m1][m2] * ownCount[m1] * opponentCount[m2]
  all divided by 16, separately for MG and EG
```

The table subtracts black's imbalance from white's. In an eval-tuning build the
table's precomputed imbalance is set to zero and `eval()` adds the imbalance
difference at run time instead, for every material combination; in the ordinary
build the out-of-table material fallback has no imbalance at all. These are real
path differences.

## Evaluation construction order

HCE starts with material, then builds non-pawn PST/attack information, fetches
or builds the cached pawn/king entry, adds its pawn-structure score, evaluates
the passed pawns (which cannot be cached, see below), derives attack quality and
dynamic pawn/piece features, adds mobility and remaining placement/material
corrections, applies style, initiative, contempt and final scaling.

The order is dependency-driven: safe mobility needs both sides' attacks;
outposts need pawn holes and defenders; nonlinear king attack needs raw danger;
endgame scaling needs the strong side and passer counts.

Seven feature groups retain white-positive MG/EG pairs:

```text
material | positional | development | mobility
attack   | pawnStruct | complexity
```

The pawn-structure group receives the cached pawn-entry score first, then the
dynamic passer terms:

```cpp
// add computed or hash score for pawn structure
features.scores[F_pawnStruct] += pe.score;

// passer eval depends on some position stuff, can't be cached
evalPawnPasser<Co_White>(p, pe.passed[Co_White], features.scores[F_pawnStruct]);
evalPawnPasser<Co_Black>(p, pe.passed[Co_Black], features.scores[F_pawnStruct]);
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

The *king zone* of a king is the set of the (up to eight) squares adjacent to it;
the king's own square is not part of it. For each non-pawn piece, compute two
coverages:

```text
shadow = coverage(square, occupancy XOR ownNonPawnOccupancy)
actual = coverage(square, occupancy)
```

The shadow board removes own non-pawn blockers, including the king, to estimate
aligned pressure into the opposing king zone (a rook behind a queen, for
instance, still counts). Actual attacks preserve the
real occupancy and populate:

| Name | Meaning |
|---|---|
| `att[c]` | Union of attacks. |
| `att2[c]` | Squares reached by overlapping attack sets. |
| `attFromPiece[c][kind]` | Union for a particular attacking kind. |
| `checkers[c][kind]` | Source squares of pieces actually attacking the enemy king. |

```cpp
const BitBoard shadowTarget = BBTools::pfCoverage[pp - 1](k, occupancy ^ nonPawnMat[c], c);
if (shadowTarget) {
   kdanger[~c] += countBit(shadowTarget & kingZone[~c]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack][pp - 1];
   const BitBoard target = BBTools::pfCoverage[pp - 1](k, occupancy, c); // real targets
   if (target) {
      attFromPiece[c][pp - 1] |= target;
      att2[c] |= att[c] & target;
      att[c] |= target;
      if (target & p.pieces_const<P_wk>(~c)) checkers[c][pp - 1] |= SquareToBitboard(k);
      kdanger[c] -= countBit(target & kingZone[c]) * EvalConfig::kingAttWeight[EvalConfig::katt_defence][pp - 1];
   }
}
```

Before merging a new actual set `T`, `att2 |= att & T`, then `att |= T`.
Pawn targets from the cache later join `att`/`att2`.

The current HCE loop begins at knight, not pawn. It merges pawn attacks into
aggregate maps and fills pawn checker sources, but does not populate the pawn
row of `attFromPiece` in this path, so the pawn term of the HCE `goodThreats`
is always false. The search-side reconstruction loops over
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

```cpp
const colored<BitBoard> weakSquare = { att[Co_Black] & ~att2[Co_White] & (~att[Co_White] | attFromPiece[Co_White][P_wk - 1] | attFromPiece[Co_White][P_wq - 1]),
                                       att[Co_White] & ~att2[Co_Black] & (~att[Co_Black] | attFromPiece[Co_Black][P_wk - 1] | attFromPiece[Co_Black][P_wq - 1])};
const colored<BitBoard> safeSquare = { ~att[Co_Black] | (weakSquare[Co_Black] & att2[Co_White]),
                                       ~att[Co_White] | (weakSquare[Co_White] & att2[Co_Black])};
```

These are tactical approximations, not proof that a king move or capture is
legal. A square defended only by a high-value piece can remain weak;
multiple favorable attackers can make a contested square count as safe.

`haveThreats[c]` tests attacks on any opposing piece. `goodThreats[c]` emphasizes
pawn attacks on non-pawns, minor attacks on rooks/queens and rook attacks on
queens. It is not computed using SEE for every threatened target.

## King safety: raw pressure to MG score

*King danger* is an accumulated attack-pressure counter for each king: the more
(and the heavier) enemy pieces aim at the squares around it, the larger it is, and
the score penalty grows non-linearly with it.
Raw `kdanger` combines shadow attacks on the enemy king zone, actual defense
of one's own zone, cached pawn/flank effects and safe-check rewards. Attack
weights are `(125, 189, 251, 153, 153, -39)` for pawn through king; defense
weights are `(41, 100, 96, 48, 26, 0)`.

Safe-check weights are `(122, 785, 569, 934, 776, 0)`. For knights through
queens they apply to checker source squares intersected with `safeSquare`; for
pawns the checker squares are intersected with the side's own attack map instead.
This is a
reward for existing checker geometry, not generation of every possible future
checking move:

```cpp
kdanger[Co_White] += EvalConfig::kingAttSafeCheck[0] * countBit(checkers[Co_Black][0] & att[Co_Black]);
...
for (Piece pp = P_wn; pp < P_wk; ++pp) {
   kdanger[Co_White] += EvalConfig::kingAttSafeCheck[pp - 1] * countBit(checkers[Co_Black][pp - 1] & safeSquare[Co_Black]);
   ...
}
```

Absence of an opposing queen subtracts 585 from danger. The nonlinear table is:

$$
\begin{aligned}
i &= \operatorname{clip}\!\left(\operatorname{trunc}(D/32),0,63\right),\\
K(i) &= \operatorname{trunc}\!\left(
  \frac{492}{1+\exp((35-i)/11)}-5\right).
\end{aligned}
$$

$D$ is raw king danger, and $K(i)$ is the integer table value. Both casts
truncate toward zero rather than applying a mathematical floor. The table is
filled once at startup, with the constants `kingAttMax`, `kingAttTrans`,
`kingAttScale` and `kingAttOffset`:

```cpp
for (Square i = 0; i < NbSquare; i++) {
   EvalConfig::kingAttTable[i] =
       static_cast<ScoreType>(sigmoid(i, EvalConfig::kingAttMax, EvalConfig::kingAttTrans, EvalConfig::kingAttScale, EvalConfig::kingAttOffset));
}
```

White's table value is subtracted from white-positive MG attack score;
black's is added. EG gets zero from this conversion. Raw danger, rather than
the nonlinear score, is also exposed to search.

```cpp
features.scores[F_attack] -= EvalScore(dw, 0);
features.scores[F_attack] += EvalScore(db, 0);
data.danger[Co_White] = kdanger[Co_White];
data.danger[Co_Black] = kdanger[Co_Black];
```

The sigmoid saturates high pressure and avoids an unbounded linear penalty.
Integer division, index clipping and conversion can create small plateaus;
threshold consumers should not pretend the final score is continuous.

## Cached pawn classification

The pawn entry is keyed by all pawns and both kings; every classification below
and every cached term depends only on those pieces. Useful primitives:

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
outpost. A *passed pawn* has no opposing pawn in front of it on its own or an
adjacent file; a *hole* is a square that no friendly pawn can ever attack again;
a *backward* pawn has its advance square attacked by an enemy pawn and no friendly
pawn able to cover it; an
*isolated* pawn has no friendly pawn on a neighbouring file. For example:

```cpp
template<Color C> [[nodiscard]] constexpr BitBoard pawnDoubled(const BitBoard b) { return frontSpan<C>(b) & b; }
[[nodiscard]] constexpr BitBoard pawnIsolated(const BitBoard b) { return b & ~fillFile(_shiftEast(b)) & ~fillFile(_shiftWest(b)); }
...
template<Color C> [[nodiscard]] constexpr BitBoard pawnPassed(const BitBoard own, const BitBoard opp) { return own & ~pawnForwardCoverage<~C>(opp); }
```

Backward, doubled, isolated and detached penalties use rank-indexed
`[rank][Close/SemiOpen]` MG/EG tables. For white they are subtracted; for black,
the rank is reversed and the same coefficient is added. Detached pawns already
classified as backward are excluded from the detached contribution.

```cpp
pe.score -= EvalConfig::backwardPawn[r][EvalConfig::Close] * countBit(backwardCloseW & ranks[r]);
pe.score -= EvalConfig::backwardPawn[r][EvalConfig::SemiOpen] * countBit(backwardOpenW & ranks[r]);
...
pe.score += EvalConfig::backwardPawn[7 - r][EvalConfig::Close] * countBit(backwardCloseB & ranks[r]);
```

Besides those penalties the entry accumulates the pawn PST, the protected-passer
bonus (a passed pawn covered by a friendly pawn, rank table) and the candidate
bonus (rank table).

`semiOpenFiles[c]` marks full files with own pawns but no opposing pawns.
`pawnSemiOpen<C>()`, used to classify individual pawn penalties, instead looks
for absence of an opposing pawn ahead along the file. Do not conflate these
two similarly named masks.

## Passed pawns and races

The passed-pawn bitboards come from the cached entry (`pe.passed`), but their
score is computed on every evaluation, after the entry has been fetched,
because the race test depends on the side to move and on the opponent's
non-pawn material. The per-pawn score combines a rank bonus with two
Chebyshev-distance/rank tables: the distance from the friendly king indexes the
support table by the pawn's rank seen from its own side, and the distance from the
opposing king indexes the defend table by the rank seen from the opponent's side.
An unstoppable test (the pawn promotes before the opposing king can catch it) is:

```text
opponentNonPawnCount == 0
and distance(opponentKing, promotionSquare) - (sideToMove != pawnColor)
    > min(5, distance(pawnSquare, promotionSquare))
```

```cpp
const bool unstoppable = (p.mat[~C][M_t] == 0) && ((chebyshevDistance(p.king[~C], PromotionSquare<C>(k)) - static_cast<int>(p.c != C)) >
                                                   std::min(static_cast<Square>(5), chebyshevDistance(PromotionSquare<C>(k), k)));
if (unstoppable)
   score += ColorSignHelper<C>() * (value(P_wr) - value(P_wp)); // yes rook not queen to force promotion asap
else
   score += (EvalConfig::passerBonus[ColorRank<C>(k)] + kingNearBonus) * ColorSignHelper<C>();
```

If true, add the middlegame piece-value difference rook minus pawn (538 - 85 = 453,
applied to both the MG and EG components) instead of the ordinary passer
expression. The code deliberately uses rook, not queen, to keep promotion
progress attractive.

Protected passers add a separate rank table when the pawn's square is covered
by friendly pawn attacks. This term and the candidate table are part of the
cached pawn entry, since they depend on pawns alone.

Other dynamic terms, also evaluated outside the cached entry, are:

- A free-passer rank bonus when no opposing piece occupies its forward span.
  This does not check every intervening square for enemy attacks.
- Rooks behind passers, with own minus opposing rook counts along rear spans.
  The tuned pair is `(-19, 38)`, showing different MG/EG incentives.

**Cache dependency note:** `evalPawnPasser()` is deliberately called after the
pawn entry is obtained and adds directly to the pawn-structure group (`F_pawnStruct`).
Terms inside the entry are therefore functions of the pawn-and-king key only;
anything that needs side to move, piece placement or material must be evaluated
outside it.

## Pawn shelter and king flanks

The *pawn shield* is the set of pawns directly around the king, and the *flank* is
the group of files near the king (`kingFlank[file]`). The cached entry also includes:

| Term | Computation outline |
|---|---|
| Shield | Count pawns in the king zone excluding the rank behind the king (relative to the king's color); multiply `(6, -8)` by `min(count^2, 9)`. |
| Pawnless flank | Add `(24, -39)` if no own pawn occupies the king's flank mask. |
| Storm | Subtract `(24, -27)` for enemy pawns in the coded near-flank rank zones (ranks 3-4 for white's king, 5-6 for black's king). |
| Open files | Add `283` raw danger for each flank file with no pawn, counting full-file bits and dividing by 8. |
| Semi-open files | Add `152` per flank file holding only a friendly pawn and `184` per file holding only an enemy pawn (same bit counting). |
| Fawn patterns | Small a/h-file pawn-and-king masks with integer `/4` weighting. |

```cpp
const colored<BitBoard> kingShield = {kingZone[Co_White] & ~BBTools::shiftS<Co_White>(ranks[SQRANK(p.king[Co_White])]),
                                      kingZone[Co_Black] & ~BBTools::shiftS<Co_Black>(ranks[SQRANK(p.king[Co_Black])])};
const int pawnShieldW = countBit(kingShield[Co_White] & pawns[Co_White]);
...
pe.score += EvalConfig::pawnShieldBonus * std::min(pawnShieldW * pawnShieldW, 9);
```

The last family is deliberately heuristic and its tuned signs are not a
general chess rule. Cached king placement explains why moving a king changes
`ph` even if no pawn moves.

## Development and outposts

An *outpost* is a minor piece on a square that enemy pawns can no longer attack
(an opposing hole) and that a friendly pawn protects.

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

```cpp
features.scores[F_positional] += EvalConfig::outpostN * countBit(pe.holes[Co_Black] & knights[Co_White] & pe.pawnTargets[Co_White]);
```

The distance table's tuned values are not uniformly negative. Its comment
"knight too far penalty" does not imply that every indexed entry is a penalty.

## Threat and mobility features

Hanging pieces are own non-pawns on weak squares, with pair `(-29, -18)`.
Minor-piece threats count a side's non-pawn pieces, or its pawns standing on weak
squares, that are attacked by an enemy knight or bishop; rook, queen and king
threats count any piece of that side on a weak square attacked by the
corresponding enemy piece. All of them use tables indexed by the victim kind.

Safe pawn attacks on opposing non-pawns use `(103, 36)`. Safe single pushes
into empty squares use `(-12, 13)` in mobility; attacks created from those
pushed squares use `(26, 20)` in attack score. Double pawn pushes are not
enumerated by this particular evaluation term.

*Mobility* is the number of squares a piece can move to; counting only *safe*
squares avoids rewarding moves that lose the piece. For each N/B/R:

```text
mobilityCount = popcount(coverage & ~ownOccupancy & safeSquares)
feature += MOB[kind][mobilityCount]
```

```cpp
const uint16_t mob = BB::countBit(BBTools::pfCoverage[T - 1](k, occupancy, C) & ~p.allPieces[C] & safe);
mobility += mob;
score += EvalConfig::MOB[T - 2][mob] * ColorSignHelper<C>();
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

A *pin* attaches a piece to a more valuable one behind it. King/queen pin terms
use a helper that identifies opposing sliding pinners (with only the opponent's
pieces as blockers) and intersects own pieces along the rays. The present
helper's final `between()` call uses `p.king[c]` even when called with a queen
square. Describe these as implemented pin features, not a general-purpose
exact pinned-to-arbitrary-square API.

```cpp
const BitBoard pinner = BBTools::attack<P_wb>(s, p.pieces_const<P_wb>(~C) | p.pieces_const<P_wq>(~C), p.allPieces[~C]) |
                        BBTools::attack<P_wr>(s, p.pieces_const<P_wr>(~C) | p.pieces_const<P_wq>(~C), p.allPieces[~C]);
applyOn(pinner, [&](const Square & k){
   pinned |= BBTools::between(k,p.king[C]) & p.allPieces[C]; 
});
```

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

```cpp
features.scores[F_positional]  = features.scores[F_positional] .scale(1 + (DynamicConfig::stylePositional  - 50) / 50.f, 1.f);
...
features.scores[F_complexity]  = features.scores[F_complexity] .scale((DynamicConfig::styleComplexity      - 50) / 50.f, 0.f);
```

Complexity instead uses `(styleComplexity - 50)/50` for MG and zero for EG.
At default 50 its contribution is zero, even though a raw complexity term was
calculated: the number of non-pawn pieces plus half the pawn count, signed by the
side to move. `SumUp()` then multiplies the scaled group by the sign of the
middlegame sum of the other groups.

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

```cpp
EvalScore initiativeBonus = EvalConfig::initiative[0] * countBit(allPawns) +
                            EvalConfig::initiative[1] * ((allPawns & queenSide) && (allPawns & kingSide)) +
                            EvalConfig::initiative[2] * (countBit(occupancy & ~allPawns) == 2) - EvalConfig::initiative[3];
initiativeBonus = EvalScore(sgn(score[MG]) * std::max(initiativeBonus[MG], static_cast<ScoreType>(-Abs(score[MG]))),
                            sgn(score[EG]) * std::max(initiativeBonus[EG], static_cast<ScoreType>(-Abs(score[EG]))));
score += initiativeBonus;
```

This prevents the initiative adjustment alone from reversing that phase's
sign, while permitting either amplification or contraction. Contempt follows:
`score += context.contempt`, an MG/EG pair set by the search driver.

When no prior material scaling applies and Armageddon is off, the strong side
is white if the current EG score is positive and black otherwise. EG scale is:

| First qualifying case | Scale |
|---|---|
| Opposite bishops, no other non-pawns | `(34 + 20*strongSidePassedPawnCount)/128`. |
| Opposite bishops with other material | `(80 + popcount(strongSideOccupancy))/128`; occupancy includes pawns and king. |
| Exactly one queen on the whole board | `(79 + 0*queenlessSideMinorCount)/128`, currently constant. |
| Otherwise | `min(1, (75 + 25*strongSidePawnCount)/128)`. |

```cpp
// moreover scaledown if pawn on only one side
if (allPawns && !((allPawns & queenSide) && (allPawns & kingSide))) { features.scalingFactor -= EvalConfig::scalingFactorPawnsOneSide / 128.f; }
```

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

```cpp
[[nodiscard]] constexpr float fiftyMoveRuleScaling(const uint8_t fifty){
   return 1.f - std::max(0, fifty*fifty) / 10000.f;
}
```

## Endgame knowledge and scope

With fewer than five total non-pawns and eligible material classification,
evaluation checks for an actual legal generated capture before using helpers.
KXK pushes the weaker king to edges and brings kings closer; KmmK uses
corner geometry; KPK probes the normalized bitbase. KBPK can approximate a
wrong-color bishop ending using KPK knowledge.

```cpp
const ScoreType sc       = 12 * pushToEdges[losingK] + pushClose[chebyshevDistance(winningK, losingK)];
return clampScore(s + ((winningSide == Co_White) ? (sc + WIN - height + 1) : (-sc - WIN + height)));
```

(`helperKXK`, where `s` is the EG material score). In the same quiet,
low-material situation the other classifications set the endgame scaling factor
(divided by 128) instead of returning a score: a known win uses
`scalingFactorWin` = 384 (a factor 3), a hard win `scalingFactorHardWin` = 64 and
a likely draw `scalingFactorLikelyDraw` = 39. Real draws return `drawScore()` when
the side to move is not in check.

These helpers are not Syzygy DTZ, nor a complete replacement for mate/stalemate
search. Material-table classifications can amplify or suppress EG scaling
without claiming an exact win proof.

The KPK *bitbase* ([kpk.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/kpk.cpp),
after public-domain code by H.G. Muller) is a precomputed table of one bit per
king-and-pawn-versus-king position: won or not. It is built at startup by
sweeping all positions repeatedly, deriving each unknown result from its
successors, until nothing changes. It is indexed by side to move, both king
squares and a pawn
restricted to files a-d and ranks 2-7:
`KPKmaxIndex = 2 * 24 * 64 * 64` = 196608 positions, stored in 6144 32-bit
words (24 KiB). `normalizeSquare()` mirrors the board horizontally when the pawn
is on files e-h and vertically when the strong side is black. A won probe
returns `WIN + valueEG(P_wp) + 10 * pawnRank` for the strong side; a drawn
probe returns 0 (or a mate score in Armageddon). KBPK with a bishop that does
not control the promotion square reuses the KPK probe; otherwise it keeps the
material score. The whole bitbase is compiled out with `WITH_SMALL_MEMORY`.

When adding a feature, specify its score group, orientation, cache dependency,
MG/EG meaning, style interaction, narrow-type range and whether search consumes
its auxiliary data. A term's intuitive chess label is only the starting point.

## Disabled and inert terms

Some HCE code is present but has no effect with the default settings; the
complete inventory is the
[implemented but disabled techniques](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/disabled-features.md#evaluation-hce)
chapter:

- The complexity group is multiplied by 0 at the default `StyleComplexity` 50,
  and forwardness is skipped while `StyleForwardness` is 50 (both disabled by
  default, enabled through those UCI options).
- `stylePawnStruct` is used by the style scaling but has no UCI option.
- `scalingFactorQueenNoQueenSlope` is tuned to 0, so the queen-versus-no-queen
  scale ignores the minor-piece count.
- The `Ter_Draw` classification is handled but never assigned; material draws
  use `Ter_MaterialDraw`.
- The opposite-bishop rule of `forbidNNUE()`, which would keep such endings on
  the HCE, is commented out.
- `VERBOSE_EVAL` (compile time, disabled by default) prints intermediate HCE
  scores when the evaluation is displayed.

The
[evaluation overview](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/evaluation.md#disabled-and-inert-parts)
summarizes these together with the NNUE ones.
