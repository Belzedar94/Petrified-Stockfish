# Petrified-Stockfish

On 2026-10-29 the owner adjudicated points-verified-null-v1 as a loss and closed
the whole panel. VSTC finished 400 games, 175/212/13, -32.23 +/-33.7 Elo,
measured LOS 2.9%; its raw automatic result remains INCONCLUSIVE_LIMIT. STC
stopped at 14 paired games (8/4/2); LTC was not run. This is LOSS_MANUAL, not
a displayed LOS 0.0% boundary. Preserve all raw records and rejected artifacts.
The owner then delegated early rejection: the agent may adjudicate neutral or
clearly losing candidates before the cap or remaining controls, recording the
reason and actual statistics separately. This discretion authorizes early
rejection only; promotion still requires three valid wins and completed audits
and interfaces. The owner requested the next substantial experiment against the
unchanged PR #7 PointsHead v1 champion. T8 remains manually paused.

The owner resumed the next experiment on 2026-10-29 after the baseline identity
clarification. The single new candidate is points-verified-null-v1, starting from
the qualified PR #7 PointsHead v1 source and using its exact winning network.
See docs/petrified/experiments/2026-10-29-points-verified-null-v1.json. It tests
guarded null-move pruning with mandatory non-null verification at every cutoff.
No new training or rejected capture changes are included. Validate the candidate,
then run all three controls at min40/max400 with the same boundary/pair policy;
only three audited wins and finished interfaces can replace PR #7's champion.
One candidate only; do not tune after viewing the panel. T8 remains manually
paused; do not touch other workers, OpenBench or the paused Misere chat.

On 2026-10-29 the owner authorized a new points-head-v1 experiment: retrain the
actual run4test dense head with scoreboard inputs, freeze the accumulators, improve
the data and use the local GPU. Test one selected candidate against the frozen
champion in all three controls, minimum 40 / maximum 400, displayed LOS 100.0% or
0.0%, complete pending pairs and recheck. See the points-head experiment record.
This authorization follows the closure of both search experiments; neither
rejected search change belongs in the new candidate. T8 remains manually paused.
The experiment is now complete: points-head-v1 won all three controls at
46/48/46 games, all displayed LOS 100.0%, with 70 complete color-reversed pairs
and independent referee audits. Local native/Node, CJS/ESM, isolated Python and
Fairyground backend checks passed. It is the new points champion; see
`docs/petrified/points-champion.json` for immutable binaries, weights and source.
The previous champion and all data/candidates are preserved. That request is closed.
The owner then explicitly requested the next idea and a panel against the new
baseline. The single successor is points-head-v2: continue the winning dense head
on fresh, deeper self-play from points-head-v1, with 25% replay of prior training
rows, frozen accumulators/PSQT and exactly the same engine binary. Its predeclared
recipe and immutable champion are in the 2026-10-29-points-head-v2 experiment record.
Train one selected candidate on the local GPU, then all three controls with the
same 40/400 and displayed LOS boundary gate. Do not promote from training loss or
start a further candidate. T8 stays manually paused and Misere is not contacted.
On 2026-10-29 the owner adjudicated the whole points-head-v2 experiment as neutral
and closed it manually. VSTC/STC finished at 400 games each (NEUTRAL_SCREEN);
LTC stopped at 22 games in 11 complete pairs, 12/10/0, measured LOS 66.6%
(NEUTRAL_MANUAL, below the automatic minimum). Four incomplete pairs are preserved
and excluded, including one completed but unpaired candidate win. All 822 paired
games and 144,807 referee plies passed audit. This is an explicit manual exception,
not statistical equivalence or a three-control pass. V2 is rejected for promotion.
The baseline remains the winner of today's PR #7, PointsHead v1, bench 177290;
the prior run4test + winning-mate-r2 baseline had bench 173124. The v2 panel used
the exact PR #7 champion binary and network as its opponent. The owner requested
a next idea, then asked to wait while this baseline identity was clarified. No
successor was launched during the v2 closure. Preserve all v2 artifacts and do
not restart its finished panel. PR #8 is closed, all panel processes exited,
the machine lease is released, and its follow-up is deleted. T8 remains manually
paused.

The approved implementation plan and immutable inputs are in
`docs/petrified/STATUS.md`, `baseline.json`, and `rules.md`. This repository is
dedicated to Petrified and starts from official SF's pinned SFNNv19 commit.
Use branches and pull requests against main. Preserve upstream history and GPL
attributions. Do not modify sibling repositories, shared runners or trainers.

Legacy `petrified` preserves the pinned Walls rules and reporting quirks.
The owner's current target is `petrifiedpoints`, Chess.com FFA / 2PC with
Deadwall EnPassant FatalCapture Sideways, including Game of Points. See
`docs/petrified/chesscom-points.md`. The independent points referee is the
hash-pinned public Chess.com client, accessed through our audit adapter; do not
commit that external bundle. Never use this engine or its binding as its own
independent match referee. run4test must pass its full hash before search.

Strength experiments remain local until the approved three-control gate and
initial interfaces pass. Use `scripts/local_campaign.py`, with all inputs frozen,
complete color-reversed pairs, no score-based adjudication, and `-T 4` after the
memory check. Do not extend a finished run because the result is disappointing.
The owner revised the minimum on 2026-10-28: at least 40 games per control,
with displayed LOS 100.0%, then finish pending pairs and check the final result.
The owner clarified for NNUE panels: 40 is a minimum, never a fixed quota.
Every TC continues to displayed LOS 100.0% or 0.0%, at most 64,000 games.
Drain pending pairs and recheck; if the boundary is lost, continue within that
same declared limit. Run all three TCs even if an earlier one loses. Promotion
requires all three to win; a limit without a boundary is inconclusive.
The owner then prioritized large Elo gains and asked to treat prolonged,
undecided comparisons as neutral. The adapter-v1 VSTC was closed manually
after 2,302 games as NEUTRAL_MANUAL, superseding an earlier manual loss request;
its measured LOS remains 11.6%, never 0.0%. The owner explicitly confirmed
the screening cap of 400 games per TC, minimum
40, retaining the LOS boundaries and pending-pair checks. At the cap without
a boundary, classify it as neutral for candidate selection, not statistical
equivalence. No promotion follows a neutral control. See the experiment record
for the current cap and any later owner clarification.
The owner then adjudicated the entire adapter-v1 experiment as neutral and
closed it: VSTC 2,302 games, STC 74 games (38/36/0, +9.39 +/-80.5 Elo,
LOS 59.2%), LTC not run. This is an explicit exception to finishing all three
controls, not an automatic gate result. Preserve interrupted raw records and
exclude unfinished pairs. The candidate is rejected for promotion; run4test
remains champion. All panel processes exited, the machine lease is released,
and Misere was explicitly notified to resume. No new local load may take
Misere's priority. The owner also allows reviewing 40-50 games per TC for
large gains; this alone does not establish a new automatic stopping rule.
The owner then explicitly authorized a sequential search-specialization loop:
capture ordering first, then a separate points-aware capture-pruning experiment.
Each candidate runs all three TCs with minimum 40, maximum 400 and displayed
LOS 100.0%/0.0%, draining pending pairs and rechecking. Promote only after three
valid wins; otherwise preserve and reject the candidate, keeping the champion.
Misere is paused by the owner; do not message its chat or wait on that project.
Use docs/petrified/points-champion.json and the current experiment record for
the frozen baseline and operational state. The manual T8 pause remains in force.
On 2026-10-29 the owner explicitly adjudicated capture-futility-v1 as neutral
and closed the remaining panel. Final counts: VSTC 400, STC 400, LTC 112;
LTC is NEUTRAL_MANUAL with measured LOS 23.6%, not an automatic boundary.
Four interrupted LTC pairs without completed games are preserved and excluded.
Both authorized search experiments are closed and rejected; run4test plus
winning-mate-r2 remains champion. No further experiment is authorized by that
sequence. The follow-up automation is deleted; T8 remains manually paused.
Use BMI2 and LTO for both opponents, with the same compiler and no PGO.
On 2026-10-28 the owner accepted campaign-003-walls-lto as a win and stopped
LTC at 16 games. Preserve this as a manual acceptance, never as 40+ games or
an automatic three-control pass. The next requested panel is FSF dev with the
official fa7e5810f172 NNUE versus this engine with run4test.
That secondary panel completed with PASS at 46/44/44 games (VSTC/STC/LTC),
including all pending pairs; see docs/petrified/experiments/2026-10-28-fsf-dev.json.
The wrapper preserves variantfishtest's reported-time clock accounting and logs
real elapsed time separately. A smoke run is never a strength-gate result.

The owner subsequently requested a manual T8 pause to use the engine locally.
That pause remains in force: DO NOT restart T8 on campaign exit. Preserve
`.local/worker-manual-pause.json`, do not start another worker or touch remote
workers, and never print worker command lines or secrets. No Petrified OpenBench
registration, test or datagen workload has been authorized by a points panel.

After the points panel and Fairyground are complete, use the available local
GPU for the smallest promising NNUE adaptation to points. Freeze the best
demonstrated points engine as champion. Each candidate must complete VSTC/STC/LTC
against that champion before promotion or trying the next version. Preserve
rejected candidates and results; lower training loss is not a strength result.

Use ignored `.local/` for external inputs, isolated Python environments, reference
checkouts, logs and campaigns; use ignored `build/` for generated distributions.
Do not replace global pyffish installations. Record source/binary/net/book hashes
and checks. Keep known failures and pending acceptance gates visible in STATUS.

For every local variantfishtest campaign, report partial and final results in a
table with one row per TC: games, W/L/D, Elo with its 95% interval, LOS, time
losses for candidate/opponent, and status. Include pending controls explicitly.
This is the owner's requested reporting format, including during a running test.

The engine advertises both rulesets in its UCI combo: default petrified var
petrified var petrifiedpoints. Fairyground maps its Petrified board to the points
engine and requires explicit scores for custom FENs. The local GUI
profile and lifecycle are documented in docs/petrified/fairyground.md; keep it
loopback-only and never replace the frozen campaign executable while testing.
