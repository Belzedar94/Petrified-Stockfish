# Search specialization review: PR11 baseline

Date: 2026-10-29. Qualified engine source: `74ea5095e84656c7d7e5783888a3a003b23c0ad9`;
qualification branch reference: `edd51c4ab4c0fb111ea7a8b4e7994998c804ed2c`.
This review concerns Chess.com points rules, not legacy Walls or Atomic.
The owner's screenshot of Fabian's Atomic feedback motivates checking the
assumptions behind each heuristic. Explosion-specific suggestions do not transfer
literally: Petrified's important events are earned points, loss of the capturer,
permanent walls, surviving pawns and score-dependent settlement.

## What is already specialized

Rules and search are not entirely orthodox chess. `Position::do_move` applies
FatalCapture and points; the evaluator has a trained points head and an explicit
score term. `Petrified::outcome` settles mate, stalemate, repetition, insufficient
material and the fifty-move rule on final points. Main search and quiescence test
these outcomes. Non-losing resignation claims are represented in search. Ordinary
null-move pruning and qsearch's ordinary capture pruning are disabled for points.
Those protections must not be removed merely to resemble Stockfish again.

Correct terminal handling nevertheless does not make pre-move pruning, ordering,
reductions or the evaluation horizon appropriate for this game.

## Findings and priorities

| Priority | Verified baseline behavior | Why it matters here | Separate experiment to consider |
|---|---|---|---|
| 1, selected | `Position::see_ge` is a one-move formula; it never discovers an attacker or a recapture. Ordinary quiet moves have estimate zero. Every pawn capture subtracts a pawn regardless of whether it can be taken. | Quiet hanging pieces evade the existing SEE safety filter; safe pawn captures are understated; legal pins, wall-blocked rays, pawn exchanges and promotion choices are absent. | Replace only points SEE with a legal local-exchange minimax that understands fatal captures and points. |
| 2 | All qsearch move-count, futility and SEE pruning is inside `!pos.game_of_points()` (`search.cpp`, qsearch Step 5). Qsearch still uses stand-pat and mainly captures. | It can spend time examining clearly unprofitable self-petrification, yet fail to see a quiet sealing or settlement threat. More captures do not necessarily mean a more stable position. | After SEE is assessed, test conservative capture pruning with explicit check/settlement guards; separately test a bounded set of quiet threat moves. |
| 3 | Capture LMR `statScore` uses `873 * PieceValue[captured_piece] / 128`, and capture ordering uses `7 * PieceValue[victim]` plus history. | A queen taking a pawn disappears; a pawn taking a queen survives. Victim value alone does not describe either transaction, its tempo or the resulting wall. | A separate depth-allocation experiment based on net fatal-capture economics, not a bundle of ordering and pruning changes. |
| 4 | Capture/continuation histories describe moved piece and destination, without a fatal-versus-surviving flag, wall geometry or score state (`history.h`, `movepick.cpp`, `search.cpp`). Quiet threat ranking uses orthodox lower-valued attackers. | A piece that just disappeared is treated like a continuing attacker. A bishop attacking a pawn may be no threat at all; turning a square into a wall can be the decisive event. | Compact variant context in history, or threat ranking based on viable captures. Measure collision/memory cost before training/tuning. |
| 5 | The last-piece sacrifice exception in main-search SEE pruning tests `alpha >= VALUE_DRAW`. Quiet move-count, futility and history pruning have no explicit earned-score/settlement phase. | Stalemate is not intrinsically a draw: equal +10 bonuses leave the current score difference intact. A sacrifice or quiet move toward a blocked board can win for the leader and lose for the trailer. | A score-aware terminal-threat guard around selective pruning, with examples of winning, tied and losing settlement. |
| 6 | Terminal points are discontinuous (+20 for mate or resignation in the usual case; +10 each for other settlements), while local material/SEE weights remain smooth. The evaluation's fifty-move taper and material scaling descend from Walls. | Being materially ahead is not the same as being ahead on earned points. Preserving pieces, cashing them out, blocking play and avoiding termination can demand opposite behavior at different scores. | Targeted score/phase evaluation or training data for liquidation and wall formation, only after diagnosing actual search mistakes. |

These are code observations and hypotheses, not measured Elo gains. Candidate
selection should favor a clear rule-specific defect with an independently
checkable replacement over changes justified only by resemblance to another
variant engine.

## Why this is not a repeat of the earlier capture experiments

`capture-order-v1` changed ranking with victim value, earned points and the cost
of the disappearing mover, while intentionally preserving the old SEE boundary.
All three controls reached their cap without a win. `capture-futility-v1` added
earned points to one futility margin; its panel was also rejected for selection.
Neither implemented recapture analysis or quiet-move safety. Their neutral
selection outcomes do not prove equivalence, and do not establish that another
capture-based change will win.

PR9's verified null pruning, PR12's incremental state and PR13's pin-based
legality remain retained independently for future OpenBench. None belongs in
this candidate. PR14's sparse head failed the engineering speed screen before
playing any games. The subsequently sketched fused accumulator was archived
before compilation when the owner redirected the work toward search.

## Selected hypothesis: points-exchange-see-v1

Only the estimate returned by points SEE changes. Existing caller thresholds,
move ordering weights, LMR, history layouts, qsearch policy, rules, state and
network bytes remain fixed. Legacy Walls keeps its existing estimate.

The restricted objective is the change in live material plus 100 units per
earned point, retaining existing piece values and point scale. After the initial
legal move, either side may stop or make a legal capture of the piece being
exchanged. Non-pawn captures destroy their mover and leave a neutral wall, ending
that exchange. Pawn captures survive and may be recaptured. Promotions consider
all four choices. A double pawn push also considers en passant on the passed
square. King safety is checked on each resulting board, with walls in occupancy
and royal commoners excluded as capturers.

This is an exact minimax of that restricted objective, not exact chess search or
an exact NNUE prediction. It omits terminal bonuses, quiet replies, other-square
tactics, tempo and the positional value of walls. It allows declining even in
check, as a local heuristic. Existing pre-node terminal checks remain essential;
late-clock and score-sensitive pruning are distinct outstanding questions.

Recursion is small for structural reasons: a non-pawn ends it, and a fixed target
has at most two pawn origins of each color. An initial en passant reply can shift
the target once. We enumerate legal choices instead of assuming the cheapest
attacker is optimal.

Hand-calculated regression examples (engine units, not Elo or displayed cp):

| Local event | Old estimate | New local estimate |
|---|---:|---:|
| Safe pawn takes an undefended queen | 3230 | 3438 |
| Quiet queen move permits an undefended enemy pawn capture | 0 | -3438 |
| Quiet queen move permits an enemy knight to petrify on it | 0 | -2657 |
| Pawn takes a pawn, followed by an undefended pawn recapture | 100 | 0 |
| Pawn defender is absolutely pinned and cannot legally recapture | Still deducts the moving pawn | Rejects the illegal recapture |
| Rook takes a rook and becomes a wall | 500 | 500; no fictitious recapture of the wall |

## Acceptance and falsification

1. Compare the optimized exchange board with a slow enumerator using full legal
   move generation, actual make/undo and complete material/score recounts. Test
   pins, x-rays, walls, king adjacency, both colors, all promotions, en passant,
   quiet double pushes, threshold boundaries and random walks/undo.
2. Independently compare nonterminal exchange trees with the hash-pinned Chess.com
   client. Explicitly exclude and report terminal-settlement cases: that client
   freezes eliminated armies and adds settlement bonuses, outside this local
   objective. Terminal correctness is checked by the separate points/mate suite.
3. Debug/Release, exact NNUE/state invariants, native interfaces and independent
   searched PVs must pass. The new points search bench must reproduce twice;
   it need not equal PR11 because the search tree deliberately changes. Legacy
   must remain 179335 and the rival must remain points177290.
4. Freeze one candidate before VSTC/STC/LTC against PR11, T1/Hash512/concurrency4
   after a memory check. Minimum40/maximum400 per control, displayed LOS boundary
   and complete-pair recheck, with the existing delegated manual rejection.
   No parameter tuning after results. Three audited wins plus distribution and
   interfaces remain necessary for promotion.

An improved SEE could still lose strength through cost, unsuitable inherited
thresholds or missing positional effects. Faster depth completion, fewer nodes
or good tactical examples alone will not be treated as a strength result.

## Completed isolated hypothesis: points-capture-lmr-v1

After the owner retained PR15 for future OpenBench, the next candidate starts
again at PR11. It changes only capture depth allocation: the capture statScore
currently rewards the victim's board value even when the capturing piece is
lost. Use the immediate change in live material plus100 units per capture point,
including promotion gains. Preserve873/128, capture history and every other
reduction/ordering/pruning policy. This is not the retained exchange SEE.

For a queen taking a pawn, the value changes from208 to-2230; for a pawn taking
a queen, from2538 to3438. A fatal rook-for-rook transaction is500, reflecting the
earned points after both live rooks disappear. The existing reduction/research
logic decides depth from that value. Immediate terminal outcomes are still
resolved on entering the child, regardless of its reduced depth. Future wall
sacrifices can be undervalued; correctness of the transaction is not evidence
of a strength improvement. One frozen candidate tested this hypothesis.

The panel completed on2026-10-30 with400 games per control: VSTC181/199/20
(-15.65+/-33.3 Elo95%,LOS17.8%), STC195/175/30 (+17.39+/-32.8,LOS85.1%) and
LTC189/181/30 (+6.95+/-32.8,LOS66.1%), all time losses0/0. All three raw results
are INCONCLUSIVE_LIMIT, classified NEUTRAL_SCREEN for selection. This rejects
promotion without claiming equivalence or a statistical loss. All1200 games,
600 complete pairs and232064 independent referee plies passed final audit;
all1832 recorded development/build/panel identities exited. PR11 remains champion.
The experiment record preserves the source, binary, network, audits, raw results,
212-entry bundle and the resolved process-inspection failure. No retuning,
replayed panel, automatic next candidate or OpenBench workload is authorized.

## Selected successor: points-forced-qsearch-v1

PR16 is retained by the owner for future OpenBench, with its neutral selection
unchanged. The new authorization starts again at qualified PR11. A wall corridor
can leave one legal quiet move; stand-pat can then hide an unavoidable points
settlement beyond the nominal horizon. At non-check points qnodes with exactly
one legal move, suppress stand-pat and search that move. Captures are included
in the same forced-choice rule. In check, the existing evasion search applies.
At branching positions, the original quiescence policy resumes. Terminal checks,
resignation claims, repetition, rule50 and MAX_PLY remain unchanged.

A capped generator query stops at the second legal move; full generation and
ordering remain exact. No coefficient, threshold, main-search policy, SEE,
rule, state or network changes. No retained PR9/12/13/15/16 code is included.

Validate capped-query parity against the frozen full generator, special moves,
random walks and undo. Construct isolated king corridors with blocked rooks to
avoid insufficient-material settlement: depth one must follow quiet moves to
threefold or rule50, with leading, tied and trailing scores for both colors.
Check T1/T2 and repeated TT searches. The hash-pinned Chess.com client verifies
legal choices and plays complete PVs through terminal settlement. Keep the
existing Debug/Release, exact NNUE/state, points/mates, native interfaces,
independent PV, reproducible points/legacy bench and source CI gates.

The query costs time and forced positions may be rare. Exact tactical examples
are not evidence of Elo. Freeze one candidate for the existing three-control
40/400 screen and do not tune after results. Only three audited wins plus final
distribution and interface acceptance can replace PR11.

The new depth-one regression exposed an inherited reporting defect before any
strength game: Syzygy PV rewriting changed a points repetition win into a
displayed draw. The candidate also bypasses this orthodox display rewrite for
points scores; actual settlement rules and search values are unchanged. The
failed binary and original output are retained. CI initially attempted a search
without external weights; weighted searches remain local and source CI retains
its five original jobs, now including capped-query C++ tests.


## 2026-10-30: points-qsearch-see-v1 from PR18

Use the existing exact legal local material-plus-earned-points SEE to omit strictly negative voluntary captures in points quiescence. Protect check evasions, checks, promotions, unique legal moves, loss-valued nodes, and after the real move any terminal settlement, useful resignation claim, bare-king ending or unique legal reply.

SEE omits positional wall value and multi-square/quiet tactics. This is a selective-search hypothesis, never a proven upper bound. Guards preserve immediate settlement and forcing exceptions; only the strength panel can select it.

Variant-specific pruning must respect obstacle geometry and terminal tactics. Duck placement/refutation rules are not applicable literally to permanent Petrified walls. Wakwak reports failed null pruning and a reduction alternative; PR18 already has verified null pruning, which remains unchanged. Independent implementation from Petrified rules and existing local SEE; no Wakwak source code copied.

Read-only HTTP200. Test9102 (silly-see-idea) finished at882 games271/331/280 with LLR-1.20; tests9063/9064 finished without passing at the observed snapshot. Strength-option comparisons are not evidence for a Petrified search gain.

Sources: https://github.com/Silverrzz/wakwak/blob/main/docs/README.md, https://github.com/Silverrzz/wakwak/blob/main/docs/duck-tech/null-move-reductions.md, https://github.com/Silverrzz/wakwak/blob/main/docs/duck-tech/king-safety.md, https://github.com/Silverrzz/wakwak/blob/main/docs/duck-tech/duck-refutations.md, https://openbench.nocturn9x.space/
