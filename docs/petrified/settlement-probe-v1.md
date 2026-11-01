# Points settlement probe v1

The owner authorized one alternative from PR18 after the PR25 postmortem, with
no training and the same correctness validation and VSTC/STC/LTC panel. PR18 source
is `fb04cbca1a7584edd7a29fd5f0a19b28d551c217`; later experiment documents remain
historical. PR19–25 engine changes are excluded. See the [postmortem](pr25-postmortem-2026-11-01.md).

At a non-check points qsearch node immediately after an actual non-pawn
petrifying capture, enumerate legal non-capture-stage moves. Make and undo each
move using the real position and ask the existing outcome implementation whether
that child is terminal and strictly winning for the mover. Return the best such
one-ply proof, including its legal PV move, before TT lookup and stand-pat.
Honor an excluded move. Parent terminal, claim and maximum-ply handling precede
the probe. A null move or FEN reconstruction cannot invent a preceding capture.

This is a finite rule probe. It does not recursively search or evaluate quiet
children, remove stand-pat on a miss, change TT keys or write its proof to TT.
Tied, losing and ongoing children do not trigger the shortcut. It is not a proof
of the globally fastest mate, eventual liquidation, or a forced cycle. Qsearch
checks time at eligible nodes and propagates cancellation only with the position
restored. All ordinary PR18 search, move ordering, rules, state, scoring, scale,
coefficients, NNUE and legacy behavior stay fixed.

Before strength testing require full Debug/Release inherited reference/state/
NNUE/undo/null/SEE/capture/forced tests, exhaustive new legal-child and quiet-win
references, independent Chess.com histories/settlements with zero hidden terminal
exclusions, depth1/4 T1/T2 both-color cold/ordinary-warm/event-warm TT acceptance,
independent PVs, native UCI/CECP and stop, repeated benches and five source CI jobs.
Failures remain immutable; corrected tests may be repeated without changing the
competition engine. Record all hashes and exact PID/CreationDate identities.

One frozen panel against PR18: VSTC2+0.02, STC10+0.1, LTC30+0.3; T1/Hash512,
concurrency4, minimum40 and maximum400 games per control, reversed-color pairs and
the pinned independent Chess.com referee. Displayed LOS100.0%/0.0% drains pairs
and rechecks the boundary; continue within400 if it disappears. Run all three.
No boundary at the cap means NEUTRAL_SCREEN, not equivalence or statistical LOSS.
Only three audited wins plus native/WASM/interface distribution acceptance allow
promotion. Bench/nodes are not Elo. PR18's historical manual exception is not
inherited.

No post-result tuning, second candidate, training or OpenBench work is authorized.
Keep both Fairyground profiles closed, T8 paused, and other applications, AtomicDB,
remote workers and Misere untouched. The lease stays owned until a decision, all
owned process exits (including helpers/packager), original audits, exact process
reconciliation, raw inventory and verified evidence archive. No automatic successor.
