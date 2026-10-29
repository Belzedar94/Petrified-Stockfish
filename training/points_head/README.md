# Frozen run4test transformer, score-conditioned dense heads

This is one bounded experiment authorized on 2026-10-29. It starts from champion
`7dd7c827b41e9460184730227fed4222929dbbaf` (winning-mate-r2), without either
rejected capture-order/futility experiment. It is not a new transformer or a
claim of an Elo gain. See the experiment JSON under `docs/petrified/experiments`.

## Architecture and exact arithmetic

The original 52,046,752 run4test bytes are embedded unchanged and must hash to
`345d67b733991ae5beb2d3a6ea40d54ea01abb1fb699aa39e6f6406495f4d933`.
The 49,152 x 512 feature transformer, biases and eight PSQT columns are frozen.
Only the eight dense 1056/16/32/1 heads train. They start with the original
1024/16/32/1 weights and zero weights on 32 additional score inputs. The zero
extension must reproduce the champion exactly before training.

The inputs after the 1,024 clipped accumulator values are: positive/negative
score difference (clipped to 63, times two), own/opponent points (same clipping),
rule-50 counter (at most 100), twice the wall count, and 26 clipped ramps
`clamp(64 + 16 * (difference - threshold), 0, 127)`. Thresholds are
`-40,-32,-24,-22,-21,-20,-19,-16,-12,-8,-4,-2,0,2,4,8,12,16,19,20,21,22,24,32,40,64`.
The difference is clipped to +/-200, matching the existing fixed skip. These
features expose the mate bonus boundary without teaching the terminal rules;
exact outcomes remain rules/search code.

The ordinary run4test material bucket, two clipped-ReLU layers, shifts by six,
PSQT blend, material scale and rule-50 scale are preserved. The fixed
`100 * score difference` term remains outside the head, providing the champion's
initial score value. QAT uses rounded integer weights, floored activations and
truncating output divisions with straight-through gradients. Float64 is needed
only for final GPU scaling to preserve exact C++ integer truncation. Deployment
uses int8 weights, int32 biases and the existing AVX2/scalar kernels.

Only `petrifiedpoints` activates the extension. Legacy play uses the embedded
original heads. Existing PTADPT01 residual files remain readable.

## File extension

After the unchanged base: magic `PTHD0001`, five little-endian uint32 fields
`1,32,16,32,8` (version, score inputs, first hidden, second hidden, buckets).
Each bucket contains the original head layout without its old hash field:
16 int32 biases, 16x1024 int8 weights, 32 int32 biases, 32x32 int8 weights
(last 16 inputs zero padding), one int32 bias, 32 int8 weights, then 16x32 int8
score weights. End with 64 ASCII hex bytes containing SHA-256 of the entire
preceding payload. The full network is 52,192,028 bytes. Exact length, embedded
base hash, extension hash, dimensions, zero padding and bias bounds are checked.
Serialization and loading are not tied to native struct padding.

## Data and fixed training procedure

Generate 1,024 fresh games with champion/run4test at 8,192 nodes/move, T1/Hash64,
four concurrent games, seed 29102901. Prefixes have 8/12/16/20 random legal plies.
Every move and result is checked by the pinned external Chess.com client, and
sampled scores are cross-checked. The engine binding only proposes board moves
and FENs; it is not the independent referee. Sample one in three positions,
excluding checks, retaining searched scores and final points outcomes. Censored
games are excluded, not labeled draws. No speculative score-counter relabeling.

SHA-256 of the first eight opening plies modulo ten fixes the whole-family split:
0 test, 1 validation, 2..9 train. Exclude match-opening families. Deduplicate FENs
including the scoreboard and rule-50 state; remove any cross-split duplicates.
The test is untouched until final checkpoint selection. C++ exports frozen
activations once, and independent NumPy recomputation checks their exact values.

One 24-epoch CUDA run, batch512, Adam initial lr0.04 in quantized parameter units,
cosine decay to 15%, fixed seed29102901. BCE target is 50% actual final outcome,
50% sigmoid of searched internal evaluation / 600. The ordinary UCI cp teacher
is converted by 2.08; mate and exact terminal scores saturate the probability.
Add 0.001 squared distance to baseline (in units of 600). Select the minimum
validation blended loss, then evaluate test once and verify serialized integer
inference. Better loss is diagnostic, never a strength gate or promotion.

## Validation and match

`validate.py` checks independent NumPy features, exact integer C++ evaluation,
zero-head identity, legacy behavior, multiple instances/two threads, export
round-trip and malformed files. `petrified-state-test NETWORK` checks scalar
versus incremental/SIMD with make/undo. Existing points and interface tests cover
the user's mating game, 19/20/21 deficits and the losing 25-24 mate.

Freeze one selected network and BMI2/LTO/no-PGO binary before matches. The existing
champion binary/run4test is the opponent. VSTC2+0.02, STC10+0.1, LTC30+0.3,
T1/Hash512/concurrency4 after a memory check. Each control: minimum40, maximum400,
displayed LOS100.0%/0.0%, complete pending pairs and recheck. All three run. A cap
without a boundary is NEUTRAL_SCREEN, not equivalence. Only three valid wins
permit promotion; keep rejected artifacts. T8 stays manually paused.

Runtime inputs, external client, logs, weights and checkpoints are kept in
ignored `.local/points-head-v1/`. No trainer, shared runner or sibling repository
is modified. The existing GPU-enabled Python installation is used without
changing its packages or the global pyffish installation.
