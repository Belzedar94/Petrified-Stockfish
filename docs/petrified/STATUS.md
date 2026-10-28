# Petrified-Stockfish implementation status

This branch starts at official Stockfish `ff7cee4244ec96ea488c402669e3674c0574dce6`.
It is an implementation in progress, **not a strength-qualified release**.

Acceptance order: reproducible baseline; rules/state differential tests; exact
Legacy Walls evaluation; native and packaged interfaces; the three local strength
controls; only then OpenBench registration and specialization/modern NNUE work.

The reference rules are Fairy-Stockfish Walls, without Chess.com scoring. The
mandatory opponent is Walls plus run4test. The first local campaign is running;
VSTC has passed, while STC/LTC remain pending completion. No OpenBench workload
is authorized before the entire local gate and initial interface validation pass.

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
Linux release/debug/sanitizers, full distribution/reproducibility checks and
OpenBench integration remain outstanding. Modern NNUE/training specialization
has not started; it is gated behind the initial benchmark.

## First local campaign (in progress)

Candidate binary SHA-256:
`4664ec97c4b7a6f8ac5c05df2111668906812eb41c900466c28b138afdb47b7c`.
Unmodified Walls binary:
`cb7e28bcbae155f6234acf730a2116678301b07e957cb08f7dc0833a775a9f76`.
Independent diagnostic referee:
`8a5b7157a6332d4e5e5d57003714a8e08c5f75c408e6ac04054ee1fc146d8a27`.
Both engines were built with GCC 16.1.0 for x86-64 BMI2 and use the frozen run4test.
The four-game concurrency preflight failed with 5.73 GiB free against a 6.58 GiB
requirement, and T8 was restored. The next campaign fixed concurrency at three;
its preflight passed. Threads=1 and Hash=512 are unchanged for both players.

| Control | Games | W/L/D | Elo ± 95% interval | LOS | Time losses (candidate/Walls) | Status |
|---|---:|---|---|---|---|---|
| VSTC 2+0.02 | 106 | 85/2/19 | +365.89 ± 81.9 | 100.0% | 0/0 | Passed after pending pairs |
| STC 10+0.1 | — | — | — | — | — | Running |
| LTC 30+0.3 | — | — | — | — | — | Pending |

Local raw records are under `.local/campaign-002/` with separate engine streams,
every move and clock, referee results, full pairs, manifests and the worker lease.
This is not yet a complete three-control pass or a claim of world leadership.

## Frozen inputs

See `baseline.json`. External binaries, networks, books, generated data and build
directories belong under ignored `.local/`; never commit them accidentally.
Preserve sibling projects and their local modifications.

## Local strength gate

Sequential VSTC 2+0.02, STC 10+0.1, LTC 30+0.3, each with Threads=1,
Hash=512 and the same run4test. Start with four concurrent games only after a
memory check. Each control must finish with Total > 100 and displayed LOS 100.0%,
including pending pairs, with a maximum of 64,000 games per control. Independent
Walls adjudication must validate moves and results. The rounded LOS is an
operational criterion, not certainty.

During these campaigns, gracefully pause the existing local OpenBench worker,
then restore that same single instance to T8 on completion, failure or interruption.
Do not alter remote workers or publish Petrified workloads before the gate.
