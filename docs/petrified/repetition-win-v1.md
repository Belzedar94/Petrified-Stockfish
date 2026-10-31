# Points repetition win v1

One owner-authorized successor from champion PR18, after parking PR23 on2026-10-31.
PR19/20/21/22/23 engine changes are excluded. No training or coefficient tuning.

PR18 deliberately disables orthodox upcoming-repetition draw detection for points:
the same repeated board can settle a win or loss according to the earned score.
Actual terminal detection remains correct, but at a branching quiet quiescence
leaf the move that produces a winning third occurrence may never be examined.

This candidate recognizes that one concrete winning option before the TT lookup
and selective pruning in non-root main search and quiescence. It first requires
a strictly positive earned-score lead. A historical prefilter looks for a repeated
opposite-side position within the reversible history, bounded by rule50 and the
last null move. Only if that exists does it enumerate legal quiet, non-pawn,
normal moves, excluding the singular-search excluded move. It makes and undoes
each candidate and accepts only an actual third repetition whose existing outcome
is a strict points win. Checks are allowed if the move is legal. Existing outcome
precedence remains authoritative: mate, stalemate, insufficient material and
fifty-move endings are not relabelled as repetition.

The result is handled like PR18's existing winning resignation option, with its
real legal move in the PV. It certifies an available win, not the globally optimal
mate distance. It is not a twofold-after-root shortcut, a forced-cycle proof or a
claim that the current position is already terminal. The helper does not write TT
entries; it rechecks the real history before TT use. Tied/losing repetitions and
legacy Walls behavior retain PR18 search. Rules, persistent state, NNUE bytes,
coefficients, ordering, reductions and every other search policy stay fixed.

Required evidence: full Debug/Release inherited references and exact state/NNUE/
undo/null; independent canonical-string history counts and all legal children;
second/third occurrences, score offsets, both colors, walls, special moves,
excluded moves, null isolation and late clocks. The pinned Chess.com client must
play histories and audit legal choices and settlements without terminal exclusions.
Depth-one and depth-four searches must expose quiet winning repetitions at a
branching leaf, with both colors, T1/T2, cold TT and TT warmed with the same board
without repetition history. Independent PVs, native UCI/CECP, ordinary/forced
searches, twice-repeatable candidate bench, champion167330, legacy179335 and five
exact-source CI jobs are required before strength testing.

One frozen independent-referee panel:40–400 games each at VSTC2+0.02, STC10+0.1,
LTC30+0.3; T1/Hash512/concurrency4. Displayed LOS100.0%/0.0% requires draining
pending reversed-color pairs and rechecking; resume within cap if lost. A cap
without boundary is NEUTRAL_SCREEN, never equivalence or statistical LOSS. Only
three audited wins plus native/WASM/interface/Fairyground acceptance permit
promotion. PR18's historical owner exception is not inherited. Bench and node
counts are identity/correctness evidence, not Elo. There is no automatic successor.

Retain raw results, independent transcripts, every failure and frozen input;
prove exact PID+CreationDate exits for every owned process, including auditors
and packaging, before release. Both Fairyground profiles remain closed/preserved
and T8 manually paused. No OpenBench workload, AtomicDB, remote workers, other
applications or Misere activity is authorized.
