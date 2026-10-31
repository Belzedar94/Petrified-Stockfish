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

## Validated launch

points-repetition-win-v1 / PR24 is FROZEN and its sole panel is ACTIVE vs PR18.
Frozen source d9c1a0b3bc1ab25179064823ccb4b45ee5331ca0; candidate SHA256
f14a4f8f593e77d351f37d618da7688f324c89abd7046b6c473aab0866084e30.
Points bench167330 twice; PR18 rival167330; common network unchanged; legacy179335.
Only recognition of an available legal winning third repetition changes, before
TT/pruning in non-root main search and qsearch. The proof uses actual reversible
history, legal make/undo and the existing ThreefoldRepetition outcome, honors
excluded moves and emits the real PV move. No twofold shortcut or forced-cycle
claim. Rules/state/NNUE/coefficients and all other PR18 policies stay fixed.
No PR19/20/21/22/23 engine changes, tuning, training or further candidate.

Full Debug/Release inherited references/state/NNUE/undo/null passed:1318563 legal
queries,9338 undo states,2283 nulls,81210 reconstructions per build. Inherited
SEE56798 exchanges/795172 thresholds and177911 capture transactions per build.
New tests per build:8352 positions,96 winning repetitions,211440 reference
children,96 excluded moves,8234 null-isolation checks and24938 exact NNUE checks.
Independent Chess.com:1416 positions/legal lists,17656 children,247036 history
plies,48 winning repetitions and1162 terminal settlements; no terminal exclusions
in this new audit. Inherited restricted SEE/transaction audits retain their
explicit terminal exclusions.32 repetition searches per build passed depth1/4,
T1/T2,both colors,cold TT and TT warmed with another history; independent64 PV
plies and192 history plies passed. Inherited forced/reference checks,32 ordinary
searches/260 independent plies and native UCI/CECP passed.
Five CI jobs41209582743 passed at exact frozen source d9c1a0b3bc1ab25179064823ccb4b45ee5331ca0.
No PR24 validation failures. Build session49546 exited0, READY_FOR_PANEL_REVIEW;
61 development identities exited. Do not rebuild or repeat completed validation.
Correctness and bench/node counts are not strength evidence.

Panel .local/points-repetition-win-panel-001; session13965. Controller14564/
creation_filetime134379304310784183; launcher10556/134379304313128877;
real panel30136/134379304313361095. Verify PID+CreationDate, never PID alone
or access denial as exit. Use .local/points-repetition-win-v1/monitor_panel.py
and .local/points-repetition-win-followup.json; never duplicate/restart or change
active inputs.254 frozen inputs,11 panel hashes and7 dependencies passed launch
audit. Preflight10525196288 bytes free exceeded7583301632, concurrency4 fixed.
Atomic launch-handle/Python-start journal and fast-child reuse tests passed.

Same40/400 VSTC2+0.02/STC10+0.1/LTC30+0.3,T1/Hash512/concurrency4. Displayed
LOS100.0%/0.0% requires draining reversed-color pairs and rechecking; continue
within cap if boundary lost. Cap without boundary is NEUTRAL_SCREEN, never
equivalence or statistical LOSS. Run all three unless owner/delegated early
neutral or clearly losing rejection. Only three audited wins plus distribution/
interfaces permit promotion. PR18 stays champion; its manual exception is not
inherited. Lease remains owned until decision and proven exits of every owned
process, including auditors and packagers. Final original auditors must reconcile
pairs/colors/stats/referee logs/hashes, processes.json exact CreationDates and
journal identities, then preserve inputs, raw inventory and audits and verify
all archive entries. Normal controller finish must be PANEL_COMPLETE_AWAITING_AUDIT,
panel_exit_code0 and no children. STOP requires a separate manual audit while
preserving the original automatic auditor rejection. Never infer exit from denial.
Report all-three-TC tables on start/closure, changed decision/failure or200 new
games; read last_reported counters first and update only when reporting.
Follow-up petrified-repetici-n-ganadora-y-panel. No automatic successor.

PR21/22/23 are parked outside the OpenBench queue. PR9/12/13/15/16/19/20 remain
separately retained; PR17 standalone remains excluded. Both Fairyground profiles
stay closed/preserved; T8 manually paused. No OpenBench workload, AtomicDB, remote
workers, other applications or Misere activity.

## VSTC closure

VSTC for points-repetition-win-v1 / PR24 closed at400 games:200/173/27,
+23.49+/-33.0 Elo95%,LOS92.0%,time0/0,200 complete reversed-color pairs.
Raw INCONCLUSIVE_LIMIT is preserved; selection NEUTRAL_SCREEN, never equivalence
or statistical LOSS. Automatic promotion is ineligible; PR18 stays champion.
STC is active and LTC pending in the SAME frozen panel. Continue without changing
inputs or relaunching. Full final audits and proven process exits remain required;
the lease is not released. All exact identities and frozen restrictions below apply.

## STC closure and LTC start

VSTC and STC for points-repetition-win-v1 / PR24 closed at400 games each:
VSTC200/173/27,+23.49+/-33.0 Elo95%,LOS92.0%,time0/0;
STC175/205/20,-26.11+/-33.3 Elo95%,LOS6.1%,time0/1.
Each has200 complete reversed-color pairs. Both raw INCONCLUSIVE_LIMIT results
are preserved; both selections are NEUTRAL_SCREEN, never equivalence or
statistical LOSS. Automatic promotion is ineligible; PR18 stays champion.
LTC is active in the SAME frozen panel. Continue without changing inputs or
relaunching. Full final audits and proven process exits remain required; the
lease is not released. All exact identities and frozen restrictions below apply.

## Final manual closure

PR24 points-repetition-win-v1 is CLOSED_NEUTRAL_MANUAL_OWNER after cooperative STOP.
The owner requested ending this test and a whole-project rethink, not a successor.
VSTC400:200/173/27,+23.49+/-33.0 Elo95%,LOS92.0%,time0/0;
STC400:175/205/20,-26.11+/-33.3 Elo95%,LOS6.1%,time0/1;
LTC6:2/4/0,raw -120.41+/-77.8,LOS19.3%,time0/0; interrupted below minimum40.
The two raw INCONCLUSIVE_LIMIT results and LTC INTERRUPTED are preserved.
VSTC/STC selections remain NEUTRAL_SCREEN; LTC has no strength conclusion.
This is not equivalence or statistical LOSS. PR18 remains champion.

Manual closure passed806 games/403 reversed-color pairs/160100 independent plies.
Four incomplete pairs and two completed unpaired games are preserved and excluded.
All1286 registered development/panel identities exited;1221 pair instances reconcile
with exact CreationDates. Journal and strict identity sets match. The controller
session exited0 with PANEL_STOPPED_AWAITING_MANUAL_AUDIT,panel_exit_code1 and no
children. Original automatic auditor exit1/STOP rejection and script are preserved;
the separate manual auditor and closure supervisor exited0. Six direct closure
identities exited; four inner Python CreationDates were not separately journalled.
Their completion is proven transitively through retained waits and the exact signed
CPython3.12 redirector; this limitation is explicit in the separate exit proof.
The packager exited0 and both of its exact identities exited.

All254 frozen inputs,11 panel hashes and7 dependencies remain exact. The7349-entry
ZIP includes every4489 raw panel files, inputs, original rejection, audits and
inventory; every entry was hash-verified. The lease is released. Branch/draft and
validated source tag candidates/points-repetition-win-v1-neutral are preserved.
No replay, tuning, training, new candidate or OpenBench queue entry. Whole-project
analysis is authorized; implementation of a new program requires a separate request.
Both Fairyground profiles remain closed/preserved; T8 paused. No AtomicDB, remote
workers, other applications or Misere activity. Prior retained/parked PRs unchanged.
