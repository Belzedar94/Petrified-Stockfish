# Points settlement probe v1 results

PR26 is **REJECTED_STRENGTH** after its single frozen panel against PR18. VSTC and STC failed the preregistered boundary; LTC reached the 400-game cap without a boundary. Its raw INCONCLUSIVE_LIMIT is retained and selects NEUTRAL_SCREEN, not equivalence or statistical LOSS. PR18 remains champion. No tuning, replay, training, successor or OpenBench work is authorized.

| Control | Games | W/L/D | Raw Elo 95% | LOS | Time losses candidate/PR18 | Selection |
|---|---:|---:|---:|---:|---:|---|
| VSTC 2+0.02 | 88 | 27/57/4 | -123.38 +/-76.8 | 0.0% | 0/0 | LOSS |
| STC 10+0.1 | 338 | 132/191/15 | -61.27 +/-36.8 | 0.0% | 0/1 | LOSS |
| LTC 30+0.3 | 400 | 172/195/33 | -20.00 +/-32.7 | 11.4% | 0/0 | NEUTRAL_SCREEN |

The unchanged panel used T1/Hash512/concurrency4, minimum40/maximum400 per control, reversed-color pairs and the pinned independent Chess.com referee. Every boundary was rechecked after draining pairs. The original auditors reconciled all826 games,413 complete pairs and175286 independent plies with no incomplete pairs or excluded games, including PR18's one STC timeout. All1239 pair process instances have exact CreationDates. All396 frozen inputs,11 panel hashes and7 dependencies remained exact.

Session9608 returned actual exit0. Its controller ended PANEL_COMPLETE_AWAITING_AUDIT with panel_exit_code0 and no remaining owned children. Controller, panel/launcher alias and captured session ancestor have exit proofs. The original strict and journal audits matched1500 exited registered identities; later auditors and packaging helpers receive a separate final reconciliation. Access denial never counts as exit.

The supplementary intervals below are descriptive normal approximations, with correctly bounded score endpoints and a monotone Elo transform. Pair intervals use independent reversed-color pair means. Neither interval corrects for sequential stopping; they do not replace the frozen decisions or raw LOS.

| Control | Game-based Elo endpoints 95% | Pair-mean Elo endpoints 95% |
|---|---:|---:|
| VSTC | [-206.11, -52.37] | [-209.69, -49.77] |
| STC | [-98.86, -25.06] | [-88.83, -34.46] |
| LTC | [-52.94, 12.58] | [-43.44, 3.26] |

Retained engine output and game timelines reconcile for every best move, with one initial depth-one network probe per engine excluded from the descriptive search rows. These are searches along different game trajectories, not matched-position causal comparisons.

| Control | Median principal depth PR26/PR18 | Median seldepth PR26/PR18 | Median reported nodes PR26/PR18 |
|---|---:|---:|---:|
| VSTC | 14/14 | 20/20 | 28669/32367 |
| STC | 17/17 | 26/27 | 145378/173518 |
| LTC | 20/20 | 31/32 | 442019/534501 |

Equal median principal depth does not establish equal search quality. Finite legal rule probes still perform work even on a miss, and a one-ply terminal proof can change search choices, but these retained results do not isolate the cost or benefit of either mechanism. Correctness tests and repeated benches do not establish Elo. The PR25 matched-history diagnostic remains a separate experiment; it is not a causal measurement of PR26.

Frozen source d6eb5216664edfbc61ff1750e9c0c4a37c49c4f0 and validated revision9ffb80d77ccfe56cddf796ce7e316a7549eb811d have identical src. Candidate SHA256 be3754453fcb8776e3cd05717adcaaff69fc323cb4e418fa1d63f81dc8c0cdf5; PR18 SHA256 a94de9d923f70307f2ddee3664732d38cce4949b01332d05bfbd221acbbd943e; common network SHA256 ab8bc1e4bb57be21eab8af061259c2bbcf065b6dfcd13c848e2a62b52739941c. Points bench167330 twice for the candidate and167330 for PR18; legacy179335 both.

Full Debug/Release correctness, independent Chess.com, native interfaces and five CI jobs at the validated revision passed before this panel. They were not repeated. The no-move sentinel mismatch, failed Windows-path manifest repair and original FAILED state remain preserved alongside successful continuation002. The PR25 zero-time summary failure is also retained; no search was repeated to repair its summary.

Raw evidence and the original auditors are in .local/points-settlement-probe-v1 and .local/points-settlement-probe-panel-001. The verified local ZIP contains 8114 entries, including all4560 raw panel files, frozen inputs, failures, audits and raw inventory. Every entry passed SHA256/size/CRC verification. Archive SHA256: 5235b9b31062176ee4f1e5d71af8c8cda3e6f53ac572341b58ab1cbeb6f12f4f; size: 135639773 bytes. Completed packager logs and post-packaging exit proofs are retained adjacent and bound by .local/points-settlement-probe-v1/closure.json. That receipt is the authority for runtime release and is written only after all later helpers and the final native bookkeeping shell have exit proof. The archive intentionally preserves its earlier, unreleased snapshot.

Both Fairyground profiles remain closed and preserved; T8 stays paused. No unrelated applications, AtomicDB, remote workers or Misere activity.

The rejected source is preserved by tag candidates/points-settlement-probe-v1-rejected at validated revision9ffb80d77ccfe56cddf796ce7e316a7549eb811d. The branch and draft PR26 remain available; the engine was not promoted or merged.
