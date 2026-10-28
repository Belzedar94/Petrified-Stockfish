# Petrified-Stockfish

The approved implementation plan and immutable inputs are in
`docs/petrified/STATUS.md`, `baseline.json`, and `rules.md`. This repository is
dedicated to Petrified and starts from official SF's pinned SFNNv19 commit.
Use branches and pull requests against main. Preserve upstream history and GPL
attributions. Do not modify sibling repositories, shared runners or trainers.

Rules are the pinned Walls rules, including its pseudo-royal reporting and SAN
quirks. The unmodified Walls binary is the opponent; a separately built diagnostic
binary is the referee. Never use this engine or its Python binding as its own
independent referee. run4test must pass its full hash before evaluation/search.

Strength experiments remain local until the approved three-control gate and
initial interfaces pass. Use `scripts/local_campaign.py`, with all inputs frozen,
complete color-reversed pairs, no score-based adjudication, and `-T 4` after the
memory check. Do not extend a finished run because the result is disappointing.
The owner revised the minimum on 2026-10-28: at least 40 games per control,
with displayed LOS 100.0%, then finish pending pairs and check the final result.
Use BMI2 and LTO for both opponents, with the same compiler and no PGO.
On 2026-10-28 the owner accepted campaign-003-walls-lto as a win and stopped
LTC at 16 games. Preserve this as a manual acceptance, never as 40+ games or
an automatic three-control pass. The next requested panel is FSF dev with the
official fa7e5810f172 NNUE versus this engine with run4test.
That secondary panel completed with PASS at 46/44/44 games (VSTC/STC/LTC),
including all pending pairs; see docs/petrified/experiments/2026-10-28-fsf-dev.json.
The wrapper preserves variantfishtest's reported-time clock accounting and logs
real elapsed time separately. A smoke run is never a strength-gate result.

The user authorized temporarily draining the single existing local OpenBench T8
worker for these campaigns, then restoring the same instance via its launcher.
Use the bounded worker lease. Do not start another instance or touch remote
workers. Never print worker command lines or secrets. No Petrified OpenBench
registration, test or datagen workload before the local transition gate.

Use ignored `.local/` for external inputs, isolated Python environments, reference
checkouts, logs and campaigns; use ignored `build/` for generated distributions.
Do not replace global pyffish installations. Record source/binary/net/book hashes
and checks. Keep known failures and pending acceptance gates visible in STATUS.

For every local variantfishtest campaign, report partial and final results in a
table with one row per TC: games, W/L/D, Elo with its 95% interval, LOS, time
losses for candidate/opponent, and status. Include pending controls explicitly.
This is the owner's requested reporting format, including during a running test.

The dedicated engine must advertise its single variant using the full UCI combo
syntax: default petrified var petrified. Combo parsing must tolerate the repeated
default. Fairyground needs that list to select the native engine. The local GUI
profile and lifecycle are documented in docs/petrified/fairyground.md; keep it
loopback-only and never replace the frozen campaign executable while testing.
