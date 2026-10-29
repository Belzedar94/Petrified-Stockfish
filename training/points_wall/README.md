# Points wall transformer experiment

This is a separate experiment from head retraining. It starts with the exact
qualified PR #7 PointsHead v1 network and trains only 4,096 king-oriented wall
rows of 512 signed integer deltas. Every delta is bounded to [-8, 8]. Live-piece
features, transformer biases, PSQT, all dense-head bytes, score inputs, rules and
search are unchanged. The legacy ruleset always uses the original transformer.

The input is the previously audited v2 **dataset**, with its existing game and
position splits. No v2 network weights are loaded. Its holdout has already been
used in the v2 experiment, so it is not a fresh external generalization test;
the independent match panel remains the strength decision. Preparation rebuilds
the unclipped int16 accumulator from original run4test and checks all 73,400
rows against the frozen exported features. Training first checks exact zero
delta identity against all stored PR #7 evaluations.

The preregistered run uses seed 29102904, 24 epochs, batches of 512, Adam with
learning rate 0.04 and cosine factor ending toward 0.15, gradient clipping at
1.0, integer rounding with straight-through gradients, and no data augmentation.
Loss is BCE of evaluation/600 against a 50/50 blend of game result and teacher
probability, plus 0.001 times squared evaluation deviation from v1/600, plus
0.0001 times the mean squared delta/8. Select the lowest validation blended loss,
then evaluate the test loss. There is one run and one selected candidate, never
selection or retuning from match results. Lower loss is not an Elo claim.

`PTWF0001` is the original 52,046,752-byte run4test, the version/dimensions and
eight unchanged v1 dense heads, a little-endian int16 matrix [4096,512], and a
64-byte ASCII SHA-256 of everything preceding the checksum. The eight-byte
extension magic replaces `PTHD0001`; the five dimensions remain (1,32,16,32,8).
The original base hash must match in full. Loading combines the wall deltas
with original wall rows once, rejecting out-of-range values. Switching points
and legacy rules invalidates affected accumulator entries. Save preserves the
exact validated file bytes.

Run preparation, one CUDA training, then independent NumPy/C++ validation via
the scripts in this directory. The zero network is a correctness fixture only,
not a second strength candidate. Invalid-file and mixed-rules cache tests are
required. The selected candidate still needs Debug/Release state restoration,
rules, UCI/CECP, deterministic bench, the frozen three-control panel, and (only
if it wins) distribution/WASM/Fairyground qualification before promotion.
