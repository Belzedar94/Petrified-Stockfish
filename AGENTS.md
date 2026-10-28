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
