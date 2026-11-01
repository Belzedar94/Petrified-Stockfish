# PR25 liquidation replies: rejected

The single frozen candidate from PR18 lost all three controls. PR18 remains the
champion. No network was trained; there will be no tuning, replay, OpenBench load
or automatic successor. The candidate branch, draft PR and validated binary are
retained as evidence.

The intended change searched all legal replies after a real non-pawn petrifying
capture at points quiescence, without stand-pat. A quiet reply consumed the
obligation and another petrification renewed it. This solved the constructed
quiet-mate horizon cases, but those acceptance cases did not predict strength.

| Control | Games | W/L/D | Raw Elo95% | LOS | Time losses candidate/PR18 | Result |
| --- | ---: | --- | --- | --- | --- | --- |
| VSTC 2+0.02 | 46 | 7/38/1 | -284.16 ±153.7 | 0.0% | 0/0 | LOSS |
| STC 10+0.1 | 46 | 5/39/2 | -329.56 ±172.1 | 0.0% | 0/0 | LOSS |
| LTC 30+0.3 | 48 | 10/35/3 | -200.64 ±117.7 | 0.0% | 0/0 | LOSS |

Each control reached the displayed LOS boundary after the minimum of 40 games,
drained pending reversed-color pairs and retained the boundary. T1, Hash512 and
concurrency4 remained fixed. The pinned independent Chess.com referee supplied
legal moves and endings. There are 23, 23 and 24 complete pairs respectively.

All four unchanged final auditors passed: 140 games, 70 pairs and 28,506
independent history/game plies. No incomplete pairs or games were excluded.
The 210 pair process instances reconcile with exact journal CreationDates.
The first closure audit proved exit of 315 registered identities; the strict and
retained-handle/journal identity sets agree. The controller and tool session
exited0, with PANEL_COMPLETE_AWAITING_AUDIT, panel_exit_code0 and no children.
Later auditor and packaging exits are recorded separately in the final closure.

The 313 frozen inputs, 11 panel hashes and 7 dependency hashes stayed exact.
Source a68084551828388053dd88d2d88821f6c7b581b4 and validated revision
cedf10cb5d104c5847fd46982197f36e15776f2f have identical engine source trees.
Candidate SHA256: 52f88a374e8520c896f70266ca360609c91ad17e1110cb43825c1a66dd41c586.
Full Debug/Release, inherited state/reference/NNUE/undo/null, new event/picker
tests, independent histories/settlements, UCI/CECP and five source CI jobs passed.
The competition binary was never rebuilt after the test-only DynStats correction.
Original test/CI, sealing and registration failures remain preserved.

The frozen statistics formatter has a separate endpoint defect: it maps a
nonpositive score probability to 0 Elo. At 18 VSTC games this produced a negative
confidence half-width. No active statistic, raw result or stopping decision was
changed. A supplementary calculation uses the same empirical game variance,
standard normal quantiles, bounds the score interval to [0,1], then transforms
its endpoints monotonically to Elo (0 maps to negative infinity).

| Control | Supplementary Elo95% interval | Exploratory pair-mean interval |
| --- | --- | --- |
| VSTC | [-482.86, -174.86] | [-450.28, -184.94] |
| STC | [-561.87, -216.87] | [-527.05, -225.02] |
| LTC | [-339.43, -103.70] | [-318.86, -114.00] |

These are approximate descriptive intervals, without adjustment for sequential
stopping. They are distinct from the preserved raw symmetric formatter output.
At the problematic 18-game snapshot the corrected interval is [-infinity,
-156.31], rather than a negative half-width.

The recorded search output also shows reduced principal depth:

| Control | Median depth candidate/PR18 | Median NPS candidate/PR18 |
| --- | --- | --- |
| VSTC | 13 / 14 | 1,186,383 / 1,174,000 |
| STC | 16 / 17 | 1,197,629.5 / 1,221,937 |
| LTC | 18 / 21 | 1,312,355 / 1,364,751.5 |

These summaries exclude the initial network probes and reconcile every remaining
bestmove with the game timelines. They describe the played games, not identical
positions under controlled budgets; they do not prove a unique cause of the
regression. The evidence rejects this broad reply extension. It does not establish
that training a new network is necessary, or that all liquidation-horizon ideas fail.

The failed Oracle browser advisory produced no answer. Its auxiliary Chrome
CreationDate was not captured; the preserved absent-PID/profile snapshot is not
represented as an exact identity exit proof. Compiler descendants completed
transitively through retained build waits. All registered experiment identities
are audited explicitly; these coverage limits remain visible.

The final closure proves exit of all 325 registered identities, including every
auditor, analysis helper, packager and supervisor. Its final native bookkeeping
shell also returned actual exit0. The 1,812-entry evidence ZIP includes all 787
raw panel files, frozen inputs, original failures and audits; every entry passed
SHA256, size and CRC checks. Completed packager output and later exit proofs stay
adjacent, hash-bound by closure.json. The lease is released.

Archive SHA256: 944a867aaa20a4ca56c8576b5a2b0e332d5df283818bfae2f521f9bc89d685d9.
The first final native tool receipt rounded its shell FILETIME during JavaScript
number transcription. That receipt remains preserved; receipt002 uses the exact
decimal string and original tool output. Original journals and audits were exact
and unchanged. The experiment JSON binds the final closure and both receipts.
