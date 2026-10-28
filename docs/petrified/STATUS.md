# Petrified-Stockfish implementation status

This branch starts at official Stockfish `ff7cee4244ec96ea488c402669e3674c0574dce6`.
It is an implementation in progress, **not a version 1.0 release**.

Acceptance order: reproducible baseline; rules/state differential tests; exact
Legacy Walls evaluation; native and packaged interfaces; the three local strength
controls; only then OpenBench registration and specialization/modern NNUE work.

The original reference rules are Fairy-Stockfish Walls, without Chess.com scoring. The
mandatory opponent is Walls plus run4test. The owner accepted the BMI2+LTO
comparison as a win on 2026-10-28 after VSTC/STC passed and an early LTC result.
This is recorded as a manual acceptance, not an automatic three-control pass.
The secondary panel against FSF dev plus its official NNUE passed all three
controls with completed pending pairs. The existing worker was restored then,
but the owner subsequently paused T8 manually; it remains stopped.
No Petrified OpenBench workload has been created.

The owner subsequently requested Chess.com Game of Points and a local NNUE pilot.
`petrifiedpoints` implements that contract separately from the frozen legacy
mode. See [rules, source evidence and NNUE plan](chesscom-points.md). Native,
Python, CJS/ESM Board, Node engine and CECP checks pass locally with run4test;
Linux/MSVC Debug/Release and Linux sanitizers passed on `cd63a09b`
(Actions run 40733012514). Fairyground now uses points
analysis and shows the earned score plus final bonus next to the board.

The first points panel completed 40 games per control against the frozen legacy
binary: VSTC 39/0/1, STC 40/0/0, LTC 40/0/0 (W/L/D), no time losses. Every game
was independently arbitrated by the pinned Chess.com client. At these boundary
scores the shared runner's confidence interval/LOS routine is undefined; this
is recorded honestly, not converted into a fabricated LOS gate. Both binaries
use GCC 16.1, BMI2, LTO, no PGO, run4test, T1 and Hash512. See the
[frozen experiment](experiments/2026-10-28-points-vs-legacy.json). This points
baseline is the next NNUE pilot's opponent; it is not a world-leadership claim.

A later independently refereed NNUE pilot exposed a Chess.com rule difference:
a slider-pinned pawn may not move sideways, even along its pin ray. This is now
fixed in points mode, with five oracle fixtures including the exact `f7e7`
failure. Full legal sets also match the public client on 523 sampled positions.
Future A/B panels apply this mandatory correction to both opponents. The first
pilot's interrupted six-game result is invalid, not strength evidence.
The owner clarified the next panel's stopping rule: 40 games is the minimum,
then each TC continues to displayed LOS 100.0% or 0.0%, with a 64,000-game cap.
All pending pairs count; a lost boundary after draining resumes the same test.
All three controls run even after a losing result. Synthetic scheduler tests
cover this contract; they are not strength measurements.

## Implemented and checked on Windows

- Dedicated rules/state, wall occupancy and hashing, reversible lateral pawns,
  promotion/EP/castling and the pinned pseudo-royal/SAN/terminal contract.
- Legacy Walls loader with full SHA-256 validation, scalar reference, incremental
  caches and AVX2. Missing or incompatible weights cannot search or pass bench.
- UCI and CECP, native stable-ABI Python wheel, CJS/ESM Board/PGN WASM, and complete
  UCI/NNUE Node WASM. The interfaces share the C++ rules implementation.
- 17,952 rules comparisons (all 7,939 book positions plus random walks/fixtures);
  integer NNUE equality on all 7,939 book positions; 4,712 debug state/undo checks
  with incremental/SIMD versus scalar evaluation. A further 1,013 rules checks
  passed after the final search-history correction.
- Python/CJS/ESM agree with Walls on 513 positions and every legal SAN. Protocol
  tests cover MultiPV, two threads, stop, ponderhit, terminal roots, invalid nets,
  CECP edit/walls/fractional clocks/analysis and native/Node evaluation equality.
- The immutable shared-runner wrapper completed a four-game smoke run with an
  independent Walls referee. This smoke result is not a strength measurement.

These are targeted development checks, not the complete release acceptance.
Windows/MSVC and Linux release/debug CI, isolated wheels, and Linux ASan/UBSan
passed on `f2437e1660e658c72085c2f7f4d59ba11a089255` (Actions run 40700379298).
CI state tests do not include the external real network; those checks have run
locally on Windows. Full distribution/reproducibility checks, Linux validation
with run4test and OpenBench integration remain outstanding. Modern NNUE/training specialization
has not started; it is gated behind the initial benchmark.

## First local campaign (cancelled; LTO mismatch)

Candidate binary SHA-256:
`4664ec97c4b7a6f8ac5c05df2111668906812eb41c900466c28b138afdb47b7c`.
Unmodified Walls binary:
`cb7e28bcbae155f6234acf730a2116678301b07e957cb08f7dc0833a775a9f76`.
Independent diagnostic referee:
`8a5b7157a6332d4e5e5d57003714a8e08c5f75c408e6ac04054ee1fc146d8a27`.
Both engines were built with GCC 16.1.0 for x86-64 BMI2 and use the frozen run4test.
The candidate used LTO; the original Walls MinGW build did not. This was disclosed
and was corrected in the subsequent campaign with the same candidate binary.
The four-game concurrency preflight failed with 5.73 GiB free against a 6.58 GiB
requirement, and T8 was restored. The next campaign fixed concurrency at three;
its preflight passed. Threads=1 and Hash=512 are unchanged for both players.

| Control | Games | W/L/D | Elo ± 95% interval | LOS | Time losses (candidate/Walls) | Status |
|---|---:|---|---|---|---|---|
| VSTC 2+0.02 | 106 | 85/2/19 | +365.89 ± 81.9 | 100.0% | 0/0 | Passed after pending pairs |
| STC 10+0.1 | 106 | 70/2/34 | +264.31 ± 59.3 | 100.0% | 0/0 | Passed after pending pairs |
| LTC 30+0.3 | 16 | 9/0/7 | +221.14 ± 135.8 | 100.0% | 0/0 | Cancelled by owner |

Local raw records are under `.local/campaign-002/` with separate engine streams,
every move and clock, referee results, full pairs, manifests and the worker lease.
This is not yet a complete three-control pass or a claim of world leadership.
The cancellation cleanup restored the existing local worker at T8 successfully.

## Matched BMI2/LTO campaign (accepted by owner)

Same candidate `4664ec97...`; clean Walls source `5b13915f...`, compiled with
GCC 16.1.0, `-O3 -flto -flto-partition=one`, BMI2/AVX2, no PGO. The Walls binary
SHA-256 is `757fe43a33624648f22fe99fc55976192fe86140080e333de27e422e51672da7`.
Both players used run4test, Threads=1, Hash=512 and concurrency three.

| Control | Games | W/L/D | Elo ± 95% interval | LOS | Time losses | Status |
|---|---:|---|---|---|---|---|
| VSTC 2+0.02 | 44 | 30/2/12 | +261.29 ± 104.4 | 100.0% | 0/0 | Passed, pending pairs completed |
| STC 10+0.1 | 44 | 23/0/21 | +201.54 ± 72.3 | 100.0% | 0/0 | Passed, pending pairs completed |
| LTC 30+0.3 | 16 | 8/0/8 | +190.85 ± 121.5 | 100.0% | 0/0 | Owner accepted; stopped early |

The exact owner instruction was: "Puedes adjudicarlo como win. Empieza el panel
vs fsf dev". LTC did not reach 40 games. Its original result remains INTERRUPTED;
the acceptance is a separate decision record. No unfinished games are counted.
The worker was restored at T8, then leased again for the new panel.
Records: `.local/campaign-003-walls-lto/`, including `manual-decision.json`.

The secondary opponent is clean FSF dev `9f778da667f6e07dae1e85d3e2ea204fc6dee94d`,
built with the same BMI2/LTO compiler settings, binary SHA-256
`84ad55602f1cffebdbfc707118a6d18437ac8fe63b95dd0c924a5199a84adf4f`.
Its official NNUE from the supplied Drive file is frozen in `baseline.json`;
the candidate still uses run4test. FSF dev passed 8,952 FEN/perft comparisons,
including all 7,939 openings, against the pinned Walls referee. Both players
positively identified their respective NNUE before the match.
Records: `.local/campaign-004-fsf-dev/`.

| Control | Games | W/L/D | Elo ± 95% interval | LOS | Time losses | Status |
|---|---:|---|---|---|---|---|
| VSTC 2+0.02 | 46 | 42/0/4 | +536.97 ± 302.2 | 100.0% | 0/0 | Passed, pending pairs completed |
| STC 10+0.1 | 44 | 33/0/11 | +338.04 ± 110.7 | 100.0% | 0/0 | Passed, pending pairs completed |
| LTC 30+0.3 | 44 | 31/1/12 | +289.24 ± 105.4 | 100.0% | 0/0 | Passed, pending pairs completed |

All controls used Threads=1, Hash=512 and concurrency three; no clock losses or
measured wall-clock overruns occurred. Public results and hashes are in
`experiments/2026-10-28-fsf-dev.json`. These small samples have wide Elo intervals;
the rounded LOS criterion does not establish absolute certainty or world leadership.

## Local GUI and protocol correction

[Fairyground instructions](fairyground.md) cover the pinned local installation,
automatic run4test profile, analysis, playing and bounded start/stop lifecycle.
The native binary advertises its only UCI variant with `var petrified`; the combo
parser accepts the repeated default. This resolves Fairyground's compatibility
failure. Native UCI/CECP and complete Node WASM lifecycle checks pass with the
real network after the correction. The frozen campaign binary is preserved and
the GUI build has the same deterministic default bench: **179335**.

## Frozen inputs

See `baseline.json`. External binaries, networks, books, generated data and build
directories belong under ignored `.local/`; never commit them accidentally.
Preserve sibling projects and their local modifications.

## Local strength gate

Sequential VSTC 2+0.02, STC 10+0.1, LTC 30+0.3, each with Threads=1,
Hash=512 and the same run4test. Start with four concurrent games only after a
memory check. Each control must finish with Total >= 40 and displayed LOS 100.0%,
including pending pairs, with a maximum of 64,000 games per control. Independent
Walls adjudication must validate moves and results. The rounded LOS is an
operational criterion, not certainty.

During these campaigns, gracefully pause the existing local OpenBench worker,
then restore that same single instance to T8 on completion, failure or interruption.
Do not alter remote workers or publish Petrified workloads before the gate.
