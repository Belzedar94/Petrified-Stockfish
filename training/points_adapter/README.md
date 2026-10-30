# Points adapter pilot v1

This is a small learned residual on top of the frozen run4test NNUE and the
first points baseline. It is not PetrifiedNNUEV2/SFNNv19 and does not retrain the
49,152-input transformer. The experiment asks whether a score-conditioned head
can improve evaluation cheaply before a larger training campaign.

Champion: commit `a7d49c250192453810f05c283086d1a479f6a8a0`, binary SHA-256
`c79ab058c22c68f5e27b96c84e418bac09d77a527d1da5dfc92bacb6bd193da7`, run4test
`345d67b733991ae5beb2d3a6ea40d54ea01abb1fb699aa39e6f6406495f4d933`.
The binary was built from the corresponding source working tree before that
commit. It is preserved, including its pre-commit version metadata.

## Data and selection

`datagen.py` uses the frozen champion with 1,024 nodes/move, T1/Hash32, four
games concurrently. Histories begin with 12 seeded random legal moves, separate
from the fixed match openings. FENs retain scores, walls and counters. Final
labels follow points (including resignation claims); censored games get no
training rows. The first pilot consumes complete games 0..511. Generation was
initially provisioned for 2,048, then deliberately bounded to the first 512 for
a faster pilot; the quota controller preserves complete files and excludes
extra queued games. Original manifests and stop logs remain available.

Whole-game split by index modulo eight: 0 test, 1 validation, others training.
The trainer also rejects overlapping opening histories between splits. The
initial 16-game spot audit missed a rare pinned-pawn rule discrepancy. The first
strength attempt was therefore invalidated after six completed games (4/2/0).
It contributes no Elo evidence. A subsequent complete independent audit checked
all 512 generated histories: 507 passed and five were rejected (47, 50, 161, 267,
339). The trainer now requires an audit report and excludes rejected games.
The corrected training set has 22,732/3,675/3,951 training/validation/test rows.
Every strength-match move and ending still receives independent arbitration.

One fixed 40-epoch GPU run selects its checkpoint by validation WDL loss. Targets
blend 85% final game outcome and 15% baseline probability; the residual receives
a small quadratic penalty. Holdout test data are evaluated only after selection.
Better loss is diagnostic, never promotion evidence. The first strength panel
uses at least 40 games each, VSTC/STC/LTC, continuing to displayed LOS 100.0%
or 0.0% with a 64,000-game cap per control. Drain pending pairs and recheck;
if the boundary is lost, continue within the same declared cap. All three
controls must complete even if an earlier control loses. Both opponents use the same corrected
BMI2/LTO executable; the champion loads unchanged run4test and the candidate
loads the adapter file. The mandatory pinned-pawn rule fix is shared, so only
the evaluation differs. Do not proceed to another candidate before all three
controls complete.

Audited candidate: `7305715df26097f6026080db81264816b2c8741a9b6256180f5a593f9f85c148`.
RTX 3080 training took 1.86 seconds inside the training loop. Validation loss
improved 0.40455 -> 0.38736, but untouched test loss worsened 0.49982 -> 0.50762.
This is mixed diagnostic evidence, not an Elo gain. The model remains experimental.
Its 256-position integer reference check, corrupt-file rejection, two-thread
checks and native/Node protocol checks pass. The fixed-quota panel 002 was
superseded when the owner clarified the stopping rule. Its VSTC finished 40
games, 20/20/0, Elo 0 +/-111.2, LOS 50.0%; STC was interrupted and LTC did not
start. This is not a completed strength gate. Panel 003 implements the clarified
LOS contract, after Misere released the machine at the owner's explicit request.

## Features and integer format

16 features, each quantized to 1/256: existing board evaluation (without the
100-point-difference term), score difference, own/opponent P/N/B/R/Q counts,
wall count, fifty-move counter and both sides' sufficient-material flags.
See `model.py` and `src/nnue/points_adapter.h` for exact scaling, clipping and
signed truncation. A 16-unit clipped ReLU head predicts a residual, clamped to
±4,000 internal evaluation units. No rule or exact terminal outcome is learned.

The `.pnnue` file embeds the original run4test bytes unchanged, followed by:

- 8-byte ASCII magic `PTADPT01`;
- little-endian uint32 version 1, inputs 16, hidden 16;
- 16 int32 biases (scale 65,536), 256 int16 input weights (scale 256);
- 16 int16 output weights (scale 256), one int32 output bias (evaluation units);
- 64 ASCII hex characters: SHA-256 of all preceding bytes.

The loader verifies the embedded base hash, dimensions, version, checksum, exact
size and numeric bounds. It reports the full file SHA-256 before search. Legacy
files remain strict and unchanged. The head is active only in `petrifiedpoints`.
The shared network stays trivially copyable for Stockfish's NUMA replicas.

## Local commands

Use Python with PyTorch/CUDA installed and an isolated compiled binding directory:

```text
python training/points_adapter/datagen.py --engine CHAMPION --network RUN4TEST --module-dir build/debug --out DATA --games 512
python training/points_adapter/audit_data.py --data DATA --bundle CHESSCOM_CLIENT --modules BABEL_MODULES --out AUDIT --games 512
python training/points_adapter/train.py --data DATA --network RUN4TEST --out PILOT --games 512 --audit AUDIT/report.json
python training/points_adapter/validate.py --engine CANDIDATE --champion CHAMPION --network PILOT/candidate.pnnue --legacy RUN4TEST --data DATA --report VALIDATION.json
python scripts/points_panel.py --candidate CANDIDATE --opponent CHAMPION --network PILOT/candidate.pnnue --opponent-network RUN4TEST --opponent-variant petrifiedpoints --bundle CHESSCOM_CLIENT --modules BABEL_MODULES --runner SHARED_RUNNER --out PANEL --min-games 40 --max-games 64000 --concurrency 3
```

Validation compares every integer feature, residual and total evaluation against
Python, verifies unchanged run4test output, malformed-file rejection and two
threads. A zero head first reproduced the champion exactly on 256 sampled
positions. Weights, datasets and logs live in ignored `.local/points-training-v1`.
