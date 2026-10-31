# Points threat evasion v1

One owner-authorized candidate from PR18, after parking PR22 on 2026-10-31.
No PR19, PR20, PR21 or PR22 engine changes are included.

The hypothesis is that a quiet quiescence leaf can look stable while a live
piece is exposed to a profitable Petrified capture. A single explicit layer of
legal responses may prevent that horizon error, at the cost of more nodes and
legal/SEE queries. This is a strength hypothesis, not a claimed improvement.

## Fixed policy

At entry to points quiescence, after terminal/claim handling and the usual TT
probe, a position qualifies only if it is not in check, has more than one legal
move and has no capture-stage move. Capture stage includes queen promotions.
Construct a hypothetical pass using the existing null-move implementation:
change side to move, clear EP and isolate repetition, then restore the original
state. If any legal enemy capture has existing points SEE at least one engine
unit, suppress stand-pat and use PR18's full move picker to search legal replies.
There is no additional quiet pruning in this layer. Claims remain available.

Every recursive qsearch call receives no threat budget, including after a forced
move or a check evasion. PR18's forced-choice and check-evasion continuations
remain. Entry points TT depth is zero and recursive points TT depth is minus
one, above unsearched entries at minus two, so a capture-only bound cannot cut
off a richer entry query. Legacy qsearch depths and behavior remain unchanged.

The hypothetical pass is not a legal game action or a forced-capture certificate.
The existing SEE prices only optional legal recaptures on the local target,
live material and capture points. It does not value wall placement, tempo,
terminal bonuses or forced replies elsewhere. There is no new coefficient.
Rules, scoring, position updates, NNUE bytes, all other PR18 search policies and
weights remain fixed. No training, result-driven tuning or further candidate.

## Required evidence

Full Debug/Release inherited references and exact state/NNUE/undo/null tests;
new predicate comparisons against a separately reconstructed opposite-side FEN
and slow legal make/undo exchange oracle; full-picker completeness; special
moves, both colors, score offsets and random walks. Independent Chess.com checks
compare legal lists and local capture economics; its terminal settlements are
explicit exclusions from the restricted exchange objective, never zero values.
EP fixture-converter limitations must be reported. Depth-one searches must
include quiet responses, with independent PV legality, cold/warm TT and T1/T2.
Native UCI/CECP, ordinary searches, repeatable points benches and unchanged
legacy 179335 are required, followed by five exact-source CI jobs.

Then one frozen independent-referee panel: VSTC 2+0.02, STC 10+0.1, LTC 30+0.3;
T1, Hash512, concurrency4, minimum40/maximum400 games per control. Displayed
LOS100.0%/0.0% requires draining pending pairs and rechecking. A cap without a
boundary is NEUTRAL_SCREEN, not equivalence. Only three audited wins plus full
distribution/interface acceptance permit promotion. PR18's historical owner
exception is not inherited. Process identities, actual exits, pair/color checks,
independent logs, frozen hashes and preservation inventory are mandatory before
lease release. Both Fairyground profiles stay closed and T8 remains paused.

## Owner parking on2026-10-31

PR23 points-threat-evasion-v1 is PARKED_MANUAL_OWNER. Its sole panel stopped cooperatively.
VSTC400:174/204/22,-26.11+/-33.2 Elo95%,LOS6.1%,time0/0; STC274:114/136/24,
-27.96+/-39.5,LOS8.1%,time0/0; LTC not run. Raw INCONCLUSIVE_LIMIT/INTERRUPTED
are preserved. Parking is an owner selection, not statistical LOSS or equivalence.
PR18 stays champion. Manual closure passed674 games/337 reversed-color pairs/
129307 independent plies. Four incomplete pairs and two completed unpaired games
are preserved and excluded. All1103 development/panel identities exited;1023 pair
process instances reconcile with exact CreationDates. Journal and strict exit
sets match. All closure auditor and packager identities exited. The closure
supervisor, manual auditor and packager exited0; the original automatic auditor
exited1 on STOP. That rejection and its original script remain preserved.
All289 frozen inputs,11 panel hashes and7 dependencies remain exact. The6293-entry
ZIP contains all3762 raw panel files, inputs, original failures, audits and inventory;
every entry was hash-verified. Lease released, follow-up deleted. Branch/draft/tag
candidates/points-threat-evasion-v1-parked retained, outside the OpenBench queue.
