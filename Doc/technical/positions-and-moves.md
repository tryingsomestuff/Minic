# Positions, moves and legality

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md) | [Next: search](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search.md)

Primary sources: [definition.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/definition.hpp),
[position.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/position.hpp), [position.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/position.cpp),
[moveGen.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/moveGen.hpp), [moveApply.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/moveApply.cpp),
[movePseudoLegal.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/movePseudoLegal.cpp) and
[hash.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/hash.cpp).

## Coordinate and type conventions

Squares are `a1 = 0` through `h8 = 63`; file is `square & 7`, rank is
`square >> 3`. White/black are 0/1 and `~color` flips them. Pieces use signed
color encoding; helpers such as `PieceIdx()` map them into table indices.
Do not remove color by taking an absolute piece value when the table expects
colored pieces.

| Type | Representation |
|---|---|
| `Square` | Signed 8-bit; negative values also serve as invalid squares/deltas. |
| `DepthType` | Signed 8-bit; `MAX_DEPTH = 127`. |
| `ScoreType` | Signed 16-bit; arithmetic often needs a wider intermediate. |
| `MiniMove` | Signed 16-bit move identity. |
| `Move` | Signed 32-bit identity plus ordering score. |
| `Hash`, `BitBoard` | Unsigned 64-bit. |
| `TimeType` | Signed 64-bit, generally milliseconds. |

The reserved move-type bits, not simply a sign test, distinguish invalid/null
moves. Use `isValidMove()`, `sameMove()` and conversion helpers instead of
inventing encodings from these typedefs.

## One board, several synchronized representations

`Position` contains:

- `_b`: a 64-square mailbox, convenient for piece identity.
- `_allB`: piece-kind bitboards, combined with `allPieces[color]` for color.
- Material counts and derived material fields.
- Full position hash `h`, pawn-and-king hash `ph`, and optional per-color
  non-pawn hashes `nph`.
- Side to move, kings, castling rights, en passant, last move and game counters.
- A non-owning `RootInformation*` and, with NNUE, an evaluator pointer.

Attack calculations use precomputed masks and occupancy-sensitive sliding
attacks. `WITH_MAGIC` selects the normal desktop magic-bitboard implementation;
alternative paths are available for other builds.

Mutating the mailbox alone is never enough. Board, bitboards, material, king
locations and hashes must describe the same position before the next search or
evaluation.

## Game ply versus search height

Reading a full FEN sets:

```text
halfmoves = 2 * (fullmove_number - 1) + 1 + (side_to_move == black)
```

Thus the starting position has `halfmoves = 1`. `moves` is the fullmove counter;
`fifty` counts reversible halfmoves since the last pawn move/capture.

The search stack is indexed by **absolute `halfmoves`**, whereas killers and
search limits use **relative `height`**. PVS and its pruning quiescence search
assert `height == halfmoves - rootHalfmoves`. Real moves and null moves advance
both coordinates by one; same-position searches (null-move verification, IID
and singular-extension probes) preserve height while changing depth or windows.
Relative-height guards therefore also enforce the corresponding stack boundary.
Standalone `qsearchNoPruning()` tools do not use this root-relative contract.

`initRootPositionOnStack()` records `rootHalfmoves`, clears pre-root stack slots
and initializes the root's evaluator. Real game history is in `GameInfo`, not in
those discarded slots.

## Copy/make and the stack

Minic does not normally make a move and then undo it on a single board.
`initChildPositionOnStack(parent)` copies the parent into the next absolute-ply
slot; `applyMove()` modifies the child. A rejected child can be discarded and
overwritten on the next attempt.

The important pattern in the main move loop is:

```text
copy parent into child stack slot
construct MoveInfo from the pre-move child
apply board move with NNUE update deferred
reject if illegal
apply pruning decisions
copy/associate child evaluator and update NNUE
search child
```

Deferring NNUE work avoids updating accumulators for moves that are never
searched. It does not permit searching a child with an evaluator still pointing
at the parent.

Same-position singular probes reuse stack slots. The TT-move path therefore
initializes its child **after** the singular probe, not before it.

## Move encoding

The low 16 bits contain six from bits, six to bits and four type bits:

```text
identity = (from << 10) | (to << 4) | type
Move     = identity plus a signed ordering score in the upper 16 bits
```

Types distinguish ordinary quiet moves, captures, en passant, four quiet
promotions, four capture promotions, four color/side castlings and a reserved
type. Use `Move2MiniMove()` when retaining identity without its transient score.

Ordering scores can change between searches. Equality of scored integers is
not generally the right way to decide whether two chess moves are the same.

## Castling is encoded king-to-rook

For castling, the stored destination is the **rook's initial square**, not the
king's landing square. This supports Chess960 positions where the initial king
and rook locations vary.

`RootInformation` records king/rook origins and castling permission masks.
`correctedMove2ToKingDest()` and `correctedMove2ToRookDest()` recover the landing
squares: c/g for the king and d/f for the rook.

Move generation checks occupancy of both paths and attacked king-path squares.
Application relocates both pieces and updates rights/hashes. Preserve cases
where a starting square is also another piece's landing square; orthodox-only
assumptions are unsafe.

Histories indexed by the moved king's destination must use the corrected king
square. A raw castling `to` can instead select the rook or an empty square after
application.

## Generation is not final legality

`MoveGen::generate<phase>()` supports:

| Phase | Actual generation |
|---|---|
| `GP_all` | All pseudo-legal move categories, including quiet promotions and castling. |
| `GP_cap` | Captures, capture promotions and en passant; not ordinary quiet promotion pushes. |
| `GP_quiet` | Non-captures, including quiet promotions and castling. |
| `GP_evasion` | King moves and candidate captures/blocks of checkers, including special handling paths. |

Evasion masks narrow the candidates, but application remains the legality
filter. En passant can expose a sliding attack when two pawns leave their
squares, so destination-only reasoning is insufficient.

`isPseudoLegal()` is especially important for moves recovered from a compressed
TT key. It is not a replacement for the post-move king-safety test.

## `applyMove()` and `MoveInfo`

`MoveInfo` snapshots the move's source/destination pieces, type, kings and en
passant state before mutation. `applyMove()` dispatches by type, updates
representations and checks whether the moving side's king is still attacked.
If illegal, it returns false **after modifying the child**; it does not restore
that object.

On success it removes/creates en passant state, flips side to move, updates
hashes/counters and optional NNUE state. Captures and pawn moves reset `fifty`;
other moves increment it. An en passant target is retained after a double push
only when an adjacent opposing pawn exists.

Promotions update both material and hash classes: the pawn disappears and a
non-pawn piece appears. En passant removes its victim from a square different
from `to`. Castling changes both king and rook representations.

`applyNull()` is not a legal game move. It flips side/hash, clears en passant,
records `NULLMOVE` and advances game-ply counters without moving pieces. It
does not run the ordinary capture/pawn/fifty update sequence.

## Hash domains

Full Zobrist hashing covers colored pieces, side, castling rights and en passant.
It does **not** encode the fifty-move counter. `computeHash()` normally returns
the incrementally maintained value; debug modes can verify it.

`computePHash()` covers pawns **and kings**, because cached pawn evaluation
contains king-location-dependent information. It is not a pawn-only key.
`nonPawnKey()` covers one color's non-pawn pieces with kings excluded.

Excluded searches perturb the PVS key with hashes of excluded moves. This keeps
ordinary and exclusion-specific bounds from being treated as interchangeable.

## NNUE ownership and invalidation

Ordinary moves insert/erase feature rows in both perspective accumulators.
A king move changes the feature coordinate system for its perspective and
requires a rebuild; castling must also account for the rook move.

`Position` copies keep a pointer to an evaluator; they do not deep-copy it.
Search code must use the evaluator stack helpers before incremental updates.
Standalone analysis/evaluation positions need their own associated evaluator.
The shared root metadata must also outlive all copied positions.

## State transition matrix

Here "non-pawn key" excludes kings as well as pawns:

| Transition | Full hash | Pawn-and-king hash | Non-pawn key | Additional state |
|---|---|---|---|---|
| Non-pawn non-king quiet | Erase/add moved piece, side, EP/castling effects. | Unchanged by the piece itself. | Erase/add mover. | Reversible count advances. |
| King quiet | Erase/add king plus control fields. | Erase/add king. | No king contribution. | King square, rights and NNUE frame. |
| Pawn quiet | Erase/add pawn plus control fields. | Erase/add pawn. | No pawn contribution. | Reversible count reset; possible EP. |
| Capture | Above plus victim removal. | Remove victim if pawn/king. | Remove victim if non-pawn/non-king. | Material counts; reversible count reset. |
| Promotion | Erase pawn, add promoted kind. | Remove source pawn without promoted contribution. | Add promoted kind. | Pawn/piece material classes. |
| EP | Move pawn and remove victim off destination. | Both pawn changes. | No pawn contribution. | Three changed squares. |
| Castling | King and rook relocation plus rights. | King relocation only. | Rook relocation only. | Root-defined king/rook starts and corrected destinations. |
| Null | Side toggle and EP removal. | No board change. | No board change. | Artificial history boundary; not a played move. |

Source:
[moveApply.cpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/moveApply.cpp).
Piece/bitboard/mailbox changes must match those key updates; recomputing only
`h` cannot repair an inconsistent material or evaluator representation.

## Why `MoveInfo` must precede mutation

After a capture the victim no longer exists on the destination; after EP the
old target may have been cleared; after a king move the old feature frame has
changed. `MoveInfo` captures that pre-move information once and is then passed
to board and NNUE updates. Reconstructing it from the completed child loses
exactly the information the incremental updater needs.

Failed `applyMove()` is not an undo operation. It can leave the disposable
child mutated before rejection; recopy the parent for a new candidate. Do not
reuse rejected child state as if it were unchanged.

## Attack generation backend

[attack.hpp](https://github.com/tryingsomestuff/Minic/blob/master/Source/attack.hpp)
provides the same `coverage`/`attack` API over magic bitboards or hyperbola
quintessence. Coverage is geometry under occupancy; attack intersects coverage
with a supplied target mask. It does not itself establish move legality.

Magic tables allocate 64-by-512 bishop and 64-by-4096 rook slots. With 64-bit
bitboards that is 256 KiB plus 2 MiB before masks/magic metadata. BMI2/PEXT is
selected only under the explicit architecture guards; otherwise the masked
occupancy is multiplied by a magic number and shifted.

`between[a][b]` excludes both endpoints. Using it to test blockers therefore
has a different contract from testing the ray's target square.

## Change checklist

When adding a move category or changing transition logic, check generation,
pseudo-legality, application, all hash domains, material, castling/EP counters,
NNUE updates, SEE, ordering, protocol conversion and tests. Perft should cover
legal transitions; special-move searches should also verify legal PV/bestmove
output and incremental evaluation consistency.
