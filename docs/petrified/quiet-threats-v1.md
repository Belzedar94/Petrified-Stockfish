# Points quiet threats v1

One candidate from PR18. Main-search quiet ordering currently rewards escaping
attacks by lesser pieces and penalizes entering them. In Petrified, an equal
piece capture can earn points despite petrifying its attacker. Conventional
lesser-piece sets omit that cost to the victim.

For each potential victim P/N/B/R/Q, union the existing opponent attack maps
whose immediate capture transaction is strictly positive:

`PieceValue[victim] + 100 * capture_points(victim) - fatal_attacker_value > 0`.

The fatal attacker value is zero for a pawn (which survives) and its live
material value otherwise. The resulting attacker sets are P, PNB, PNBR, PNBR,
PNBRQ respectively. Royal commoners neither capture nor receive this heuristic.
The existing `20 * PieceValue[victim]` escape/arrival weight stays unchanged.

These are pseudo-attack maps on the current board, including existing king
x-rays and wall blockers. They do not prove legal, profitable exchanges: pins,
replies, pawn promotions, en-passant, settlement bonuses and positional wall
value are outside this local ordering estimate. No move is removed. Legality,
capture order, TT, checks bonus, histories, pruning, reductions, state, rules,
NNUE and legacy ordering stay PR18. PR19/20/21 engine changes are excluded.

Before strength testing: hand-enumerated capture matrix, independent coordinate
maps in both modes, all legal child maps and undo/cache restoration, the full
Debug/Release state/NNUE/reference suite, all inherited component fixtures,
hash-pinned independent Chess.com capture economics and board geometry,
native interfaces/forced and ordinary search PVs, reproducible candidate and
champion benches, unchanged legacy179335, and five exact-source CI jobs.

One frozen VSTC2+0.02/STC10+0.1/LTC30+0.3 panel against PR18, T1/Hash512/
concurrency4, minimum40/maximum400 games per control. Displayed LOS100.0%/0.0%
boundaries require finishing pending pairs and rechecking. A limit without a
boundary is NEUTRAL_SCREEN for selection, never equivalence. Only three audited
WIN results plus distribution/interfaces permit automatic promotion. No tuning,
training, OpenBench workload or automatic successor.
