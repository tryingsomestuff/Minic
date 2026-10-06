# Technical glossary

[Index](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/README.md)

This glossary combines chess-engine terminology with the meanings used in
Minic. A term describing a general technique does **not** imply that Minic
implements or enables it. Snapshot-specific exceptions and inactive mechanisms
are identified explicitly.

Use the section links below or browser search to find a term. The chapter
links beside each family lead to equations, source maps and implementation
details rather than duplicating the complete implementation here.

## Find a terminology family

| Family | Main topics |
|---|---|
| [Coordinates and representations](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#coordinates-and-representations) | Ply, depth, height, boards, move encodings and ownership. |
| [Chess geometry and special moves](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#chess-geometry-and-special-moves) | Attacks, pins, castling, en passant and promotions. |
| [Search and bounds](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#search-and-bounds) | Negamax, PVS, windows, node types and root results. |
| [Selectivity](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#selectivity) | Pruning, reductions, extensions and verification. |
| [Ordering and learned move statistics](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#ordering-and-learned-move-statistics) | SEE, killers, history, CMH and gravity. |
| [Classical evaluation](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#classical-evaluation) | MG/EG, phase, mobility, danger and pawn features. |
| [Neural evaluation and training](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#neural-evaluation-and-training) | NNUE, features, accumulators, tensors and uncertainty. |
| [Hashes, tables and history](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#hashes-tables-and-history) | Zobrist domains, TT entries, correction and game replay. |
| [Time management](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#time-management) | Allocation, variability, emergencies and forecast. |
| [Parallelism and synchronization](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#parallelism-and-synchronization) | SMP, MPI, local/shared state and publication. |
| [Protocols and variants](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#protocols-and-variants) | UCI, XBoard, FRC and Armageddon. |
| [Builds and diagnostics](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#builds-and-diagnostics) | PGO, perft, assertions, coverage and reproducibility. |
| [Experiments and statistical language](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#experiments-and-statistical-language) | Elo, SPRT, confidence and controlled comparisons. |
| [Equation notation](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/glossary.md#equation-notation) | Symbols, indicators, casts, clipping and score units. |

## Coordinates and representations

Details: [positions and moves](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/positions-and-moves.md)
and [architecture](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/architecture.md).

| Term | Meaning |
|---|---|
| Square | A board coordinate; Minic uses a1 = 0 through h8 = 63. The compact `Square` type is signed. |
| File / rank | A vertical/horizontal board coordinate. Minic's numerical file/rank indices are zero based. |
| Bitboard | A 64-bit set of squares. AND intersects sets; OR unions them; population count measures set size. |
| Mailbox | An array-like square-to-piece board representation. Minic keeps it alongside bitboards rather than choosing only one representation. |
| Occupancy | Squares occupied by pieces, either for one color or both. It determines slider blockers. |
| Piece kind | Pawn, knight, bishop, rook, queen or king without ownership; distinct from a colored piece. |
| Colored piece | Piece kind plus side. Minic encodes colors using signed piece values and indexes tables through helpers. |
| Material count | Counts of pieces/pawns, including light/dark bishops and aggregate classes; not the material evaluation score. |
| Position | Board state, control fields, material, keys and non-owning associations required to search a position. |
| RootPosition | A position that owns `RootInformation`; copy construction duplicates that information, while assignment is deleted. |
| RootInformation | Shared root-specific geometry, particularly king/rook starts and castling permissions. Ordinary child copies do not own it. |
| Root | The position where the current search begins. It may be deep into a played game. |
| Ply | One move by one side, not a full white-plus-black turn. |
| `halfmoves` | Absolute position ply index. A standard starting FEN has value 1, not 0. |
| `moves` | Full-move counter, distinct from `halfmoves` and recursive search height. |
| `fifty` | Reversible halfmove count since the last pawn move/capture; not the game's absolute move number. |
| Depth | Remaining main-search budget, modified by reductions/extensions. It does not directly count every recursive call. |
| Height | Search-relative recursive ply argument used by mate scores, killers and selective depth. |
| Selective depth / seldepth | Deepest reported search-relative exploration; it can exceed nominal depth through extensions and qsearch. |
| `rootHalfmoves` | Absolute root boundary for the current stack; guards prevent treating pre-root slots as valid local history. |
| Stack slot | Worker-local storage indexed by absolute game ply, holding position/evaluation/search context. Not simply indexed by height. |
| Copy/make | Copy the parent position into a child slot, then apply a move. Minic normally discards/replaces children instead of unmaking them. |
| Make/unmake | Modify a board in place and restore it after recursion. A common alternative, not Minic's normal main-search model. |
| Move identity / MiniMove | The low 16-bit from/to/type encoding, excluding the move-ordering score. |
| Scored Move | Move identity plus packed ordering score; comparing identity must not accidentally compare the attached score. |
| Corrected destination | Actual king landing square for castling, rather than its encoded rook-origin destination. |
| MoveInfo | Pre-move facts needed for board and NNUE mutation. Constructing it after mutation loses victim/EP/king-frame information. |
| Association | A non-owning link from a position to its evaluator or root information. Copying the pointer is not copying ownership. |
| Invariant | A relationship that must stay true, such as mailbox/bitboard/material/hash consistency after a legal transition. |

## Chess geometry and special moves

Details: [generation, legality and transitions](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/positions-and-moves.md).

| Term | Meaning |
|---|---|
| Coverage | Squares reached by a piece under supplied occupancy, before restricting to a target set or proving move legality. |
| Attack map | Union of reached squares. An evaluation attack map can contain attacks that do not correspond to legal moves. |
| Double attacks | Squares covered by overlapping attack sets, not necessarily two pieces that can both legally capture there. |
| X-ray | Slider pressure exposed through a removed blocker. SEE and shadow king pressure use modified occupancies for this purpose. |
| Slider | Bishop, rook or queen; its coverage depends on intervening occupancy. |
| Leaper | A piece such as a knight whose geometric attacks do not depend on intervening blockers. |
| Check | Attack on the side-to-move king. Check handling changes generation, evaluation and pruning eligibility. |
| Checker | A piece delivering check. In Minic's evaluation checker masks, set bits mark attacking source squares, not king squares. |
| Evasion | A move that resolves check. Generation is only an initial filter; application still verifies legality. |
| Pin | A piece constrained by an exposed attack behind it. Evaluation pin features are approximations, not a universal legal-move oracle. |
| Pinner | The sliding attacker that creates pin geometry. |
| Discovered attack / check | Moving a blocker exposes another piece's line of attack, possibly against a king. |
| Pseudo-legal | Satisfies preliminary move/board conditions but may leave the mover's king attacked. |
| Legal | Survives the actual post-application legality test and applicable variant rules. |
| Quiet move | Context-dependent. Ordinary Minic search quiets are `T_std`; castling and quiet promotions are separate types. |
| Capture | Removes an opposing piece. Ordinary capture, EP and capture promotion have distinct representations and some different consumers. |
| En passant / EP | Pawn capture with the victim off the mover's destination. Removing two old squares can expose a slider attack. |
| EP target | The square used to encode potential en passant; it is not the victim pawn's square. |
| Promotion | Replace a pawn by another piece kind at the last rank. Both material classes and NNUE feature kinds change. |
| Underpromotion | Promotion to rook, bishop or knight rather than queen. It is not inherently a bad move. |
| Castling | Simultaneous king/rook relocation. Minic encodes king-to-rook-origin and translates to landing squares. |
| Chess960 overlap | Castling geometry where a start/landing square overlaps another role or a king remains stationary; fixed orthodox assumptions fail. |
| Chebyshev distance | Maximum absolute file/rank separation: the unobstructed king-move distance between two squares. |
| Front span / rear span | File squares ahead of/behind a pawn or reference square for a specified color, excluding the reference square. |
| `between` mask | Squares strictly between two aligned endpoints; endpoints themselves are excluded. |
| Magic bitboards | Precomputed slider-attack tables indexed by masked occupancy multiplication/shift, or the selected PEXT backend. |
| PEXT | BMI2 parallel bit extraction, used under explicit architecture guards for attack-table indexing. |
| Hyperbola quintessence / HQ | Arithmetic slider-attack generation alternative to the magic-table path. |

## Search and bounds

Details: [search](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search.md).

| Term | Meaning |
|---|---|
| Node | One searched position/context. Same-board exclusion or verification searches can create different contexts for that board. |
| Edge | Move connecting a parent search position to a child. |
| Negamax | Minimax written using side-to-move scores: a parent's move score is the negative of its child's returned score. |
| Alpha / beta | Lower and upper endpoints of the current search window; not material values or move-ordering ranks. |
| Alpha-beta | Search that omits work once the window proves it irrelevant to the parent's choice, subject to any selective approximations. |
| Window | Interval supplied to a search. The implementation's comparisons, not a drawing's endpoint notation, define boundary behavior. |
| Null window / zero window | Integer-score window of width one, commonly `[alpha, alpha+1]`; despite the name its numeric width is not zero. |
| Fail-low | Result at or below the original alpha; an upper-bound outcome. |
| Fail-high | Result at or above beta; a lower-bound outcome and potential cutoff. |
| Exact bound | Result inside the searched window. In a selective engine it is not proof of exhaustive game-theoretic minimax. |
| Upper / lower bound | Search evidence that the score is at most / at least a stored value, subject to the searched context. |
| Fail-soft | Returning a value outside the original window rather than only its boundary. Always retain the matching bound interpretation. |
| Beta cutoff | An adequate move reaches beta and stops the remaining move loop. |
| Alpha update | A move raises the best score above alpha; it need not cause a cutoff. |
| Principal variation / PV | Best line currently maintained by the search; incomplete/retried lines are not necessarily the final reported line. |
| PV node | Minic's PV template specialization, normally associated with full-window work and PV maintenance. |
| Cut node | A hint that a cutoff is expected. It is predictive context, not a property proved before search. |
| All node | Traditional expected node where every move must fail low; do not infer a separate Minic specialization from this generic term. |
| TT-PV / former PV | Context derived from the current PV flag and TT metadata, used for prudence; not a guarantee the old line remains best. |
| PVS | Principal Variation Search: search an early move normally, challenge later moves with a narrow window, then retry promising results. |
| Re-search / retry | Another search attempt with a different depth, window or cut-node hint; distinguish which quantity changed. |
| Iterative deepening | Search successive root depths, retaining completed results and ordering/timing information between iterations. |
| Aspiration window | Initially narrow root window around an earlier score; fail-low/high causes widened retries. |
| MultiPV | Multiple root variations requested for output. Additional root work is not identical to one ordinary best-line search. |
| Quiescence / qsearch | Search beyond the main depth horizon to reduce tactical instability. Minic searches evasions when checked and captures otherwise. |
| Stand pat | Static score used as a qsearch baseline when not in check. It is not literally a legal pass move. |
| Horizon effect | Apparent safety caused by a consequential event lying beyond the explored budget. |
| Terminal node | Position where search returns without ordinary continuation, for example mate, draw or another early recognized ending. |
| Mate-distance score | Reserved score near `MATE`, adjusted by relative height to favor shorter wins and delay losses. |
| STOPSCORE | Internal stop sentinel; not a valid ordinary evaluation to publish as centipawns. |
| Root result | Selected move/score/PV/depth tuple for reporting, potentially selected through worker voting. |
| Completed iteration | Finished root-depth result; distinguish it from partially searched or failed aspiration attempts. |

The negamax relationship, abstracting the engine's selective policies, is:

$$
V(p,d)=\max_{m\in\mathcal{M}_{\mathrm{legal}}(p)}
       \left[-V(p_m,d-1)\right].
$$

This notation does not claim that Minic visits every legal move at depth $d$.

## Selectivity

Details: [search policy reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/search-policy-reference.md).

| Term | Meaning |
|---|---|
| Selectivity | Spending unequal search effort across moves/nodes using heuristic evidence. Can reduce cost and miss useful lines. |
| Pruning | Skip a candidate or return early without completing its otherwise normal search. |
| Reduction | Search less deeply initially. A promising reduced result may trigger an unreduced retry. |
| Extension | Increase a move's depth budget. Generic extensions are disabled; the singular TT path has active extension behavior. |
| Negative extension | Remove plies through the singular-policy extension variable; distinct from ordinary LMR arithmetic. |
| LMR | Late Move Reductions, using depth/move rank and context to select a smaller initial child budget. |
| LMP | Late Move Pruning, skipping eligible later quiets after a depth-dependent legal-candidate count. |
| Early move | In the generated loop, `validMoveCount < 2`; a successful TT move affects that count before ordinary candidates. |
| Improving | Local raw static-evaluation trend against a guarded two-ply-earlier stack value; not proof of a favorable game trajectory. |
| Advanced pawn push | Current rank/piece-kind predicate protecting some moves from selectivity; not synonymous with a passed pawn. |
| Futility pruning | Omit eligible moves judged unlikely to raise alpha given a static estimate and margin. |
| Reverse futility / static null | Node-level static margin test against beta; distinct from actually making a null move. |
| Razoring | Shallow low-estimate node checked using qsearch before an early return. |
| Null move | Artificial pass used as a search probe. It is neither a legal played move nor a null-window search. |
| Null-move pruning / NMP | Reduced search after an artificial pass; a fail-high can support pruning the original node. |
| Zugzwang | Position where the obligation to move worsens the outcome. It undermines the premise behind null-move pruning. |
| Verification search | Extra work testing a selective result, such as a null-move fail-high; not verification of the entire engine. |
| Mate threat | Signal from the artificial null-move search returning a mated score; selective evidence, not necessarily a proved forced mate. |
| Threat pruning | Node policy using own/opponent threat summaries and margins; not blanket proof that every threatened position is tactical. |
| ProbCut | Search selected promising captures against a raised threshold to support an earlier cutoff. |
| IIR | Internal Iterative Reduction: reduce nominal depth without a sufficiently useful TT context; enabled. |
| IID | Internal Iterative Deepening: internal preliminary search to discover a move; traditional IID is disabled. |
| Singular move | A TT candidate whose excluded alternatives fail below a verification threshold. |
| Exclusion / skip move | Search the same position while disallowing selected moves. Its context/key cannot be treated as ordinary unrestricted search. |
| Singular extension | Additional budget for the singular TT candidate after excluded-alternative testing. |
| Multi-cut | Early return when alternative-move verification indicates the original threshold is already exceeded. |
| Delta pruning | Qsearch policy comparing expected gain/margins before searching eligible captures. |
| Quiet SEE pruning | Skip an eligible quiet whose exchange estimate is too poor for its calculated child budget. |
| `skipQuiet` / `skipCap` | Loop state suppressing subsequent eligible quiets or bad captures; not unconditional suppression of every move type. |
| BM extension | Botvinnik-Markoff threat-evolution extension branch; its local enable switch is false even when its signal is computed. |
| Emergency-defense modifier | Optional depth-policy term whose switch is false and whose PVS producer is currently forced false. |

## Ordering and learned move statistics

Details: [move ordering](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/move-ordering.md).

| Term | Meaning |
|---|---|
| Move ordering | Predict which candidates deserve early search; it affects cutoffs, selectivity and learning, not only speed. |
| Move picking | Selecting the next candidate from the scored list. Minic's partial sorting is not a separately staged capture/quiet generator. |
| TT move | Move hint retrieved from the transposition table, subject to validation and actual legality. |
| Previous best | Retained root choice used to prioritize candidates; distinct from a generic TT move. |
| Killer | Recent quiet cutoff move at a search-relative height; a history hint, not a tactical guarantee. |
| Counter move | One remembered response indexed by the preceding move's from/corrected-to identity. |
| Continuation history / CMH | Learned response scores conditioned on earlier moved-piece/destination contexts. Not the counter-move identity table. |
| Continuation distance | Number of moves back for a context. Active distances 1/2/4/6 have weights 8/6/4/2. |
| Available context | Valid, root-guarded CMH row pointer. Unavailable weights are omitted from normalization. |
| Context alias | Two contextual references address the same row/bucket; their learning effects need not be independent. |
| From/to history | Score keyed by side, source and destination. |
| Piece/to history | Score keyed by colored moving piece and destination. |
| Capture history | Score keyed by colored moving piece, destination and captured kind; EP is excluded by its update helper. |
| Pawn history | Proposed additional contextual ordering signal; not the active pawn cache or pawn correction history. |
| Gravity update | Bonus plus damping toward a bounded scale; the exact integer expression matters and is not always an explicit clamp. |
| HSCORE | Depth-dependent move-history bonus, `4*min(depth,32)^2`. |
| HISTORY_DIV | Arithmetic right shift by 10. Negative rounding differs from ordinary integer division toward zero. |
| MVV-LVA | Most Valuable Victim / Least Valuable Attacker capture-ordering heuristic; not a full exchange evaluation. |
| SEE | Static Exchange Evaluation: estimate local alternating exchanges using attackers and x-rays. |
| SEE_GE | Threshold wrapper that currently computes the full SEE, then compares; not an optimized threshold-only evaluator. |
| Recapture | Capture aimed at a recently used destination. Particular dormant Minic predicates are looser than that verbal definition. |
| Bad capture | Ordering classification based on capture score and configured limits, later consumed by pruning/reduction. |
| PST hint | Positional table improvement used in move ordering; not a full child evaluation. |
| Escape bonus | Ordering bonus for safely leaving a refutation-threatened square under the coded gates. |
| Lazy partial sorting | Stop finding the exact highest entry after the threshold condition and consume the tail without a full sort. |
| Cutoff malus | Negative learning applied to earlier eligible list entries; they are not exclusively moves that received full searches. |

## Classical evaluation

Details: [classical evaluation reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/classical-evaluation-reference.md).

| Term | Meaning |
|---|---|
| HCE | Handcrafted/classical evaluation, combining explicit material, pawn, placement, activity and attack features. |
| Static evaluation | Position estimate without recursive continuation search; may include HCE, NNUE, endgame helpers and context. |
| Raw static score | Static baseline before correction and TT search-score substitution. Preserve its provenance. |
| Corrected evaluation | Static baseline plus structural correction under score guards; not a correction applied blindly to TT searched scores. |
| White-positive score | Positive favors white regardless of who moves. HCE accumulates most feature pairs in these coordinates. |
| Side-to-move score | Positive favors the player moving. Negamax consumes this orientation. |
| Centipawn / cp | Ordinary UCI score unit. Minic's internal tuned pawn values are not all exactly 100. |
| MG / EG | Middlegame/endgame components of an evaluation pair; independent feature weights, not separate search phases. |
| Game phase / gp | Remaining weighted material fraction; pawns contribute, and table hits can quantize it to an 8-bit fraction. |
| Tapered evaluation | Blend MG and EG using phase, with possible additional EG scaling. |
| PST | Piece-Square Table, rewarding/penalizing piece placement in a specified coordinate orientation. |
| Material imbalance | Nonlinear piece-count interactions in addition to linear material totals. |
| Bishop pair | Presence-based material feature for bishops covering both square colors, with pawn-count-dependent parameters. |
| Bad bishop | Tuned interaction with own pawns on the bishop's square color; not a legal restriction on the bishop. |
| Mobility | Coverage reaching eligible non-own/safe squares, evaluated using nonlinear MG/EG tables. |
| Scalar mobility | Auxiliary count exposed to search; not the same value as the summed mobility evaluation score. |
| Shadow attacks | Coverage with own non-pawn blockers removed to estimate king pressure; not real legal attacks. |
| Weak square | Evaluation mask identifying vulnerable/poorly defended contested squares according to aggregate attack quality. |
| Safe square | Evaluation mask combining unattacked squares and favorable multiple-attack relationships; not legality proof. |
| Protected square | Pawn protection or favorable aggregate/double-attack relations used by development/placement features. |
| Hanging piece | Own non-pawn on an evaluation-weak square; the feature is approximate rather than exact SEE loss. |
| Good threat | Favorable attacker/victim classes, such as pawn versus non-pawn, summarized without SEE on every target. |
| King zone | Precomputed neighborhood used to score pressure, defense and pawn shelter. |
| King danger | Raw pressure summary combining attacks, defenders, checks and cached pawn effects; distinct from the final nonlinear MG penalty. |
| Safe-check weight | Reward applied to eligible checker-source masks; not a complete count of legal future checking moves. |
| Sigmoid | Saturating S-shaped function, used for danger conversion, time variability or training probabilities in different domains. |
| Pawn shield / shelter | Pawn coverage near the king, including a capped squared-count score. |
| Pawn storm | Opposing pawns in the coded near-flank/rank zones used to estimate king pressure. |
| Open file | File with neither side's pawns. Other pieces may still occupy it. |
| Semi-open file | Convention-dependent: Minic's cached mask marks files containing own pawns but no opponent pawns. |
| Passed pawn / passer | Pawn with no opposing pawn ahead on its own/adjacent files under the implemented mask. |
| Candidate passer | Pawn qualifying for a coded support/attack-balance predicate; not proven to become passed. |
| Protected passer | Passed pawn on a square attacked by a friendly pawn. |
| Free passer | Current bonus uses absence of enemy pieces in its forward span; it does not prove a completely safe route. |
| Unstoppable passer | Heuristic king-race test with no opposing non-pawns; it reads side to move and material beyond the pawn-cache key. |
| Doubled pawn | Pawn ahead of another friendly pawn on the same file according to the forward-span mask. |
| Isolated pawn | Pawn with no friendly pawn anywhere on either adjacent file. |
| Backward pawn | Pawn identified by the coded advance/support/enemy-pawn-attack test; heuristic structural classification. |
| Detached pawn | Advanced, locally alone pawn classified as undefendable by the pawn helper; overlaps are filtered for scoring. |
| Pawn hole | Square outside the forward-filled friendly pawn attacks within a designated hole zone. |
| Outpost | Minor on an opponent pawn hole defended by an own pawn; not every strategically useful square receives this feature. |
| Development | Feature group containing central protection and pawn/minor placement relationships, not only opening piece deployment. |
| Initiative | Sign-aware second-order adjustment from pawns/distribution/material class, limited not to reverse a phase's score alone. |
| Complexity | HCE feature group whose style multiplier is zero at neutral setting 50; not neural uncertainty. |
| Contempt | Context score favoring/penalizing draws or sides under configured policy; not a universally fixed constant. |
| Tempo | HCE side-to-move adjustment after fifty scaling; NNUE has no separate tempo term. |
| EG scaling | Material/distribution-dependent multiplier on EG only; some tuned cases amplify rather than suppress it. |
| Fifty scaling | Quadratic attenuation from the reversible count, `1-fifty^2/10000`, outside relevant variant/debug exceptions. |
| Lazy evaluation threshold | Cheap score test used to decide whether expensive evaluation is needed; not move picking's lazy-sort threshold. |
| Style option | Runtime feature-group/rank weighting; it modifies HCE terms, not the learned network parameters. |

## Neural evaluation and training

Details: [NNUE reference](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/nnue-reference.md).

| Term | Meaning |
|---|---|
| NNUE | Efficiently updatable neural evaluation: maintain sparse-input affine sums incrementally, then run the dense head. |
| HalfKA | King-conditioned, all-piece feature family including kings. Match Minic's actual mapping, not another engine's similarly named layout. |
| Perspective | One side's king and us/them ownership convention. White/black have separate learned input matrices. |
| Feature index | Encodes king square, piece kind/ownership and piece square; 49152 possibilities per perspective. |
| Active feature | An occupied piece-square conjunction in the current king frame; inactive features contribute zero. |
| Sparse input | Store/process occupied feature indices instead of multiplying a dense 49152-element input at each node. |
| Transformer | Input affine layer, forming 384 sums for each perspective from active feature rows and biases. |
| Accumulator | Per-position vector of those sums. This is local state, not the shared matrix of network weights. |
| Bias | Learned additive parameter independent of current input; initializes an affine sum. |
| Incremental update | Erase/add feature rows when pieces change, preserving the unchanged affine contributions. |
| Reset / rebuild | Reinitialize from biases and all active features, needed when a perspective's king frame changes. |
| Dirty evaluator | Validity marker requiring explicit update/reset; propagation asserts clean state, rather than generally repairing it automatically. |
| STM-first concatenation | Put the side-to-move accumulator first, then the other perspective; null moves change this order without changing board features. |
| Dense layer | Affine matrix/bias computation using all values of the preceding vector. |
| Clipped ReLU | Activation bounded between 0 and 1 in the normal dense/SIMD input path; scalar input clipping has a documented difference. |
| Linear head | Final affine scalar without clipped activation, producing evaluation or log variance. |
| Skip connection | Earlier values bypass an intermediate transformation. Minic uses concatenation, not residual addition. |
| Bucket | One of two phase-selected dense parameter sets; not a king bucket or piece-count bin in the active inference path. |
| Tensor shape | Number/order of dimensions. Actual dense inputs are 768, 8, 16 and 24, not merely repeated 8-wide layers. |
| Quantization | Convert input floats to rounded int16 weights/biases with scale 512; dense parameters remain floats. |
| Dequantization | Convert int16 sums to float using reciprocal scale before neural activation. |
| Saturation / clipping | Restrict a value's range. Per-weight load clipping does not prove all accumulator sums remain in int16 range. |
| SIMD | Single Instruction Multiple Data execution; instruction set, layout and reduction order can change rounding and throughput. |
| Alignment | Storage address boundary needed by selected vector loads/stores; NNUE uses 64-byte alignment. |
| FMA | Fused multiply-add with one rounding step; can differ numerically from separate multiply and add. |
| Transposed weight layout | Loader converts disk input-major dense weights to output-major rows for SIMD dot products. |
| Factorizer | Training-only shared piece-position component folded into king-conditioned weights at export. |
| Export / serialization | Write parameters/version in the loader's required layer/bucket ordering and numeric format. |
| Embedded network | Network bytes included in the executable; distinct from loading an external runtime file. |
| Inference | Evaluation using fixed loaded weights; it does not perform online backpropagation in the search. |
| Target | Desired training output, currently blending score-derived probability and game outcome by lambda. |
| Outcome | Win/draw/loss label mapped to a probability-like target; orientation must match the training loader. |
| Powered error | Training uses absolute sigmoid-target error to power 2.4, not ordinary squared error. |
| Sample weighting | Adjust each training example's contribution; active surprise weighting differs from the calculated but unused zone weights. |
| Dropout | Training regularization that randomly suppresses activations; the model's dropout calls are commented out. |
| Log variance | Optional head output transformed by exponential after clipping; not already a standard deviation. |
| Variance / standard deviation | Usually dispersion and its square root. Minic's optional training target is not automatically calibrated in engine-score units. |
| Uncertainty head | Optional additional neural output; disabled in the normal C++ build and not currently a search selectivity signal. |
| Calibration | Empirically checking that predicted confidence/dispersion matches observed error in a specified domain. |
| Correction versus uncertainty | Correction estimates signed bias; uncertainty concerns dispersion/error risk. Their magnitudes are not interchangeable. |

## Hashes, tables and history

Details: [tables and history](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/tables-and-history.md).

| Term | Meaning |
|---|---|
| Zobrist hash | XOR of randomized feature keys enabling cheap reversible key updates; not cryptographic identification. |
| Full key / `h` | Position-domain key including pieces/control fields, but not the reversible fifty counter. |
| Pawn-and-king key / `ph` | Pawns plus kings, used because cached pawn/shelter evaluation depends on king placement. |
| Non-pawn key / `nph` | Color-specific non-pawn pieces excluding kings; used by structural correction. |
| Hash collision | Different inputs produce the same full key, table index or compressed signature; distinguish those three events. |
| Bucket index | Masked key bits selecting a bounded table location. An equal index alone does not imply an equal position. |
| Fingerprint / signature | Compressed key information used to validate a stored entry; TT signature also XORs payload words. |
| Direct-mapped table | One storage slot per index. Colliding indexed items replace one another. |
| TT | Transposition Table storing move, search score, eval field, depth, bound/flags and compressed identity. |
| Transposition | Different move orders reach equivalent board/control state, allowing useful search-information reuse under suitable context. |
| TT hit | Validated entry exists; the API's boolean additionally distinguishes sufficient-depth hits from usable shallower hints. |
| TT cutoff | Return supported by an adequately usable stored bound, depth and node conditions. |
| TT `s` / `e` | Search score versus evaluation field; current writers do not universally preserve raw-static provenance in `e`. |
| Generation | Three-bit modulo-eight TT search age tag; currently stored but not used to choose a replacement victim. |
| Replacement policy | Decide which entry a store overwrites. Current TT always replaces its one indexed slot. |
| Clustered TT | Several candidate entries per index with victim selection; a roadmap alternative, not the current one-entry layout. |
| Hashfull | Sampled occupied compressed-key fraction in per mille; not current-generation usefulness or allocated bytes. |
| Pawn cache | Worker-local cached HCE structure/danger data; not pawn move history or a table of search bounds. |
| Cache dependency | Inputs needed to determine a cached value. Key validation alone does not prove every value depends only on key fields. |
| Material table | Precomputed mixed-radix material classifications/scores/phases; not a probabilistic Zobrist cache. |
| KPK bitbase | Compact exact king-pawn-versus-king knowledge after normalization, within its supported rules. |
| Syzygy | External endgame tables accessed through the optional tablebase path; requires actual files and successful initialization. |
| WDL | Win/draw/loss result domain; not a numeric centipawn score. |
| DTZ | Distance to zeroing move in tablebase terminology, distinct from mate distance or full-move number. |
| Correction history | Learned structural static-eval residuals; separate from cutoff-trained move ordering. |
| Residual | Observed searched score minus the designated static baseline; sign and baseline provenance matter. |
| Horizon correction | Optional depth/context-dependent correction with a ramp beyond its threshold; disabled normally. |
| GameInfo | Actual game-position ledger plus optional searched-root scores, rather than the recursive tree's stack. |
| Optional search score | Missing unless a validated final searched-root result was recorded; missing is not score zero. |
| Replay prefix | Already played position sequence rebuilt by a protocol command; unchanged prefixes preserve their recorded scores. |
| Divergent branch | Replay departs from the old sequence; abandoned future scores are discarded rather than retained as a transposition cache. |
| Undo | Remove/revert actual game history through the supported protocol path; not the recursive child-discard operation. |
| Upcoming repetition | Proposed prospective repetition technique; do not confuse it with the implemented retrospective draw checks. |

## Time management

Details: [time and parallelism](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/time-and-parallelism.md).

| Term | Meaning |
|---|---|
| Time control / TC | Rule governing available clock, increments and possible periodic replenishment. |
| Sudden death | Remaining clock must cover the rest of the game, possibly with increments; no explicit moves-to-go replenishment. |
| Repeating control | Replenishment after a specified number of moves, common in XBoard timing input. |
| Moves to go | Protocol-supplied number of moves to the next control, not necessarily the number left in the game. |
| Increment | Added clock per played move; distinguish it from current remaining clock. |
| Fixed movetime | Requested per-move time; different from dynamic allocation from a remaining game clock. |
| Fixed nodes | Stop based on node count, not dynamic clock allocation; limits are sampled and can overshoot. |
| Margin | Reserved portion of remaining clock, with configured minimum and cap. |
| Processing overhead | Measured/control-path time subtracted from initial dynamic allocation; not identical to the configured margin minimum. |
| Target time | Base allocation computed before subsequent difficulty/variability adjustments. |
| Maximum time | Additional allocation cap; final floor and sampled checks mean it is not a formal hard real-time guarantee. |
| Minimum time floor | Best-effort lower allowance, normally 20 ms, which can exceed tiny remaining/max budgets. |
| Allocation horizon | Estimated number of future moves to divide available clock across; not searched ply depth. |
| Risky situation | Small/missing increment triggers the conservative timing branch. |
| Forced move | Difficulty category reducing allowance, not proof that the game is forced to its conclusion. |
| Easy move | Historic time category still represented; current source notes it is no longer used. |
| Iteration emergency | Completed-depth score deterioration within the current search; separate from between-root game evolution. |
| Booming | Genuine historical rise by at least 80 between same-side recorded roots; current time multiplier is 1, with a clock cap. |
| Moobing | Historical fall by at least 80 between same-side recorded roots; current multiplier is 2. |
| Attack / defense difficulty | Time-management category labels using score thresholds; not necessarily the actual position's tactical plan. |
| Variability | Heuristic root-score instability state controlling a logistic time factor; not neural variance. |
| Effective branching factor / EBF | Node-count-derived search growth estimate used to forecast another iteration. |
| Forecast | Predictive soft-stop calculation from elapsed time/EBF, not a measured future duration. |
| Pondering | Search while the opponent is thinking, with distinct protocol/time-state behavior. |
| Ponderhit | Notification that predicted opponent play occurred; changes how ongoing work is timed/reported. |
| Stop sampling | Periodic PVS checks rather than a wall-clock check on every node; qsearch mainly observes the stop flag. |

## Parallelism and synchronization

Details: [architecture](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/architecture.md)
and [parallel search](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/time-and-parallelism.md).

| Term | Meaning |
|---|---|
| Protocol thread | Control/input loop; not worker zero, even with `Threads = 1`. |
| Worker / Searcher | Search thread plus its own position stack, histories, correction and pawn cache. |
| Main worker | Searcher index zero, coordinating root work/output; "main" does not mean the protocol thread. |
| ThreadPool | Singleton managing local workers and their search lifecycle. |
| SMP | Shared-memory parallel search; local workers can consult the same TT. |
| Lazy SMP | Workers explore related trees with local state and TT-mediated cooperation rather than explicit centralized move splitting. |
| Depth skipping | Worker scheduling diversity omitting selected nominal iterations; distinct from per-move LMR. |
| Shared state | Objects accessible by multiple threads, requiring specific consistency/lifetime rules. |
| Worker-local state | Owned separately by one searcher, such as its move histories; sharing weights does not share accumulators. |
| Start lock | Coordination delaying helper search until root setup is released. |
| Mutex | Mutual exclusion for a protected operation; its presence does not protect unrelated unguarded readers. |
| Condition variable | Wake/sleep coordination used alongside a lock and predicate; not a data container. |
| Atomic | Operation with specified cross-thread semantics; it does not automatically make surrounding non-atomic data safe. |
| Happens-before | Ordering relation needed to establish legal synchronized observation of writes across threads. |
| Publication | Making a result available to readers with suitable lifetime and consistency, not simply assigning a field. |
| Snapshot | Mutually consistent observed state at a defined boundary; ordinary multi-field reads need not form one. |
| Stop / wait / join | Request termination versus wait for completion. Requesting stop alone does not establish that the worker has finished. |
| Worker voting | Depth-weighted root-result selection followed by choosing a supporting worker result. |
| Data race | Unsynchronized conflicting non-atomic access; XOR TT validation does not provide a formal C++ race-freedom proof. |
| MPI | Optional multi-process distributed communication, separate from local shared-memory threads. |

## Protocols and variants

Details: [lifecycle](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/architecture.md).

| Term | Meaning |
|---|---|
| UCI | Universal Chess Interface, the GUI/engine command and output protocol. |
| XBoard / CECP | Another GUI/engine protocol with different clocks, game-state ownership and play/force modes. |
| `position` | UCI command establishing a FEN/start position and replayed moves; not merely a request to clear search history. |
| `go` | UCI search request carrying depth/nodes/clock/pondering constraints. |
| `bestmove` | Final UCI move publication; distinct from intermediate `info` scores/PVs. |
| `info` | UCI analysis/status output; can expose partial or completed depth data before final move selection. |
| `isready` / `readyok` | Protocol readiness exchange; do not assume it joins every ongoing search without checking implementation semantics. |
| New-game signal | Protocol instruction resetting game-related state according to the implementation's reset policy. |
| FEN | Forsyth-Edwards Notation for board, side, castling, EP and counters; not a complete list of prior game positions. |
| SAN / coordinate move text | Algebraic notation versus source/destination notation; neither is the internal scored-move representation. |
| FRC / Chess960 | Fischer Random Chess, particularly relevant to castling starts, overlap and output conventions. |
| Armageddon | Variant with asymmetric draw outcomes and altered evaluation/search draw scoring. |
| Antichess | Losing-chess variant with different capture/king assumptions; ordinary chess legality/evaluation assertions need variant guards. |
| `subSearch` | Marks auxiliary search work; not every `searchDriver()` call belongs to the live game root. |
| `postMove` | Controls reporting/publication behavior; combined with other gates and root validation for persistent game scores. |

## Builds and diagnostics

Details: [development](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/development.md).

| Term | Meaning |
|---|---|
| Compile-time feature gate | Define/constexpr choice controlling compiled behavior; adding a runtime UCI option cannot enable absent code. |
| Runtime option | Registered setting applied while the program runs, subject to its lifecycle/consumer constraints. |
| C++20 | Engine language level, including rules relevant to signed shifts, templates and type behavior. |
| Narrow integer type | Compact score/depth/square representation; intermediate arithmetic and final conversion need range discipline. |
| Assertion | Development invariant check; `NDEBUG` can remove it from execution. |
| Debug diagnostic gate | Enables additional state/NNUE/hash checks; distinct from merely using a debug compiler optimization setting. |
| Perft | Count legal move-tree leaves at specified depth; checks generation/application, not evaluation or playing strength. |
| Divide | Perft broken down by root move, helping localize count mismatches. |
| Smoke test | Small integration check showing a common path works; not complete correctness or strength evidence. |
| Regression test | Persistent case guarding a previously fixed behavior, ideally its exact output/state invariant. |
| Coverage | Which paths were exercised; neither correctness proof nor evidence a heuristic gains Elo. |
| PGO | Profile-Guided Optimization using representative execution data to optimize the compiled binary. |
| Non-PGO build | Iterative build using `NOPROFILE=1`; useful for development but potentially different performance. |
| Distribution copy | Tournament/test executable in `Tourney`, separate from the freshly built output in `Dist`. |
| Stale binary | Tests/matches run an older executable than the edited/built code. Build success alone does not rule this out. |
| NPS | Nodes per second; affected by tree shape, evaluation, hardware and counters, not equivalent to Elo. |
| Fixed-depth comparison | Equal nominal root depth, not necessarily equal nodes, selective depth or time. |
| Fixed-node comparison | Equal requested node cap; isolates some timing policy but is not a complete real-time strength comparison. |
| Determinism / reproducibility | Repeat behavior under controlled environment/state; multiworker scheduling and shared TT can vary traces. |
| Include-What-You-Use / IWYU | Header dependency diagnostic harness; the repository CMake harness is not its normal engine build system. |

## Experiments and statistical language

These terms describe evaluation methodology, not additional search features.
See [correctness, speed and strength](https://github.com/tryingsomestuff/Minic/blob/master/Doc/technical/development.md#correctness-speed-and-strength-are-separate).

| Term | Meaning |
|---|---|
| Baseline | Unchanged comparison configuration/binary. Record network, options, hash, threads and control as well as commit. |
| Ablation | Remove one component to understand its contribution under a controlled setup. |
| Confounder | Another differing factor that can explain results, such as changed network, openings or timing. |
| Paired games | Replay an opening with colors exchanged to reduce opening/side bias; pairs can have dependent outcomes. |
| Opening suite | Selected starts/lines defining the match sample; results need not generalize to another suite. |
| Draw rate | Fraction of drawn games, affecting statistical power and interpretation of score differences. |
| Score rate | `(wins + draws/2) / games`; not the raw win fraction when draws occur. |
| Elo difference | Rating-model expression of expected score difference, not a centipawn margin or a direct strength proof. |
| SPRT | Sequential Probability Ratio Test comparing specified hypotheses as evidence accumulates; setup and stopping assumptions matter. |
| Null / alternative hypothesis | Competing statistical claims about performance, not the engine's null-move operation. |
| Confidence interval | Uncertainty interval under a stated sampling/model procedure; not the NNUE uncertainty output. |
| Statistical significance | Evidence relative to a chosen model/test criterion; different from practical size or generality of a gain. |
| Neutral / inconclusive patch | Results show little gain or insufficient evidence under tested conditions; not proof the code has no effect. |
| Overfitting | Tuning too closely to the sampled positions/controls so apparent gains do not generalize. |
| Multiple testing | Trying many candidates increases false-positive opportunities; selecting the best observed result alone can mislead. |
| Correctness versus strength | A safety repair can be Elo-neutral; a stronger-looking selective patch can still be incorrect. Measure both separately. |

For the common rating-model conversion from score rate $p$:

$$
\Delta E = 400\log_{10}\!\left(\frac{p}{1-p}\right),
\qquad 0<p<1.
$$

This is a rating-model transformation, not Minic's neural probability
conversion. It is undefined at exact sample rates 0/1 without an estimation
procedure and does not supply a confidence interval on its own.

## Equation notation

| Symbol / operation | Meaning and domain |
|---|---|
| $p$, $p_m$ | Position and child after move $m$ in search equations; in probability equations $p$ instead denotes probability. Read the local definition. |
| $d$, $h$ | Remaining depth and search-relative height, not absolute game ply. |
| $n$ | Legal-candidate rank in the LMR reference, including later-pruned candidates. |
| $E$, $R$ | Per-move extension and reduction. Extension can be negative in the singular TT path. |
| $\alpha$, $\beta$ | Search window boundaries in internal search-score units. |
| $g$ | Game phase, with high values toward initial material; not number of moves left. |
| $\mathbf{1}[C]$ | Indicator: 1 if condition $C$ is true, otherwise 0. It is not a tunable multiplier. |
| $\operatorname{clip}(x,a,b)$ | Clamp $x$ into the closed range from $a$ to $b$. |
| $\operatorname{trunc}(x)$ | Convert toward zero, matching ordinary floating-to-integer conversion for representable values. |
| $\lfloor x\rfloor$ | Round downward. Differs from truncation for negative inputs and should not replace arbitrary C++ casts. |
| $\ln$, $\log_{10}$ | Natural/base-ten logarithms. The LMR helper named `my_log2` approximates natural log despite its name. |
| $\sigma(x)$ | Logistic function, $1/(1+e^{-x})$, whose input/output domain must be specified. |
| $\odot$, $W x$ | Componentwise multiplication versus matrix/vector multiplication; dimensions differ. |
| $[u;t]$ | Concatenation, not sum; the output width is the sum of the two widths. |
| $A_{c,j}$ | Component $j$ of perspective $c$'s NNUE accumulator. |
| $\mathcal{F}_c$ | Active feature indices for perspective $c$, not every allocated input index. |

## Similar names that must not be conflated

| Names | Distinction |
|---|---|
| Null move / null window | Artificial pass versus narrow score interval. |
| Depth / height / absolute ply | Remaining budget versus relative recursion coordinate versus game position index. |
| Move score / static score / search score | Ordering priority versus nonrecursive evaluation versus returned/stored search evidence. |
| Counter move / continuation history | One remembered response identity versus contextual response scores. |
| Pawn cache / pawn history / pawn correction | Cached HCE features versus proposed ordering statistics versus static-bias learning. |
| Raw danger / king penalty / uncertainty | Attack summary versus nonlinear MG contribution versus optional neural dispersion-related output. |
| Iteration emergency / moobing | Deterioration within one search versus deterioration across earlier same-side game roots. |
| Valid entry / sufficient-depth TT hit | Usable hint versus bound eligible under depth checks. |
| Hash match / root membership | Key agreement versus validated actual-game position/index/counter context. |
| Dirty flag / lazy evaluator | Validity marker versus a general automatic-repair/cache policy; the latter must not be assumed. |
| Exact bound / exact game value | Inside-window search outcome versus exhaustive or applicable tablebase proof. |
| Coverage / attack / legal move | Geometric reach versus target relation versus accepted state transition. |
| Variability / variance | Timing instability heuristic versus neural/statistical dispersion terminology. |
| Stop requested / joined | Notification versus completed execution with an appropriate synchronization boundary. |
