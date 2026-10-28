# Local testing and the strength gate

Freeze inputs from `baseline.json`. Rebuild the unmodified Walls commit separately
with the same compiler and ISA as the candidate. In an isolated Walls checkout,
`python scripts/prepare_walls_referee.py <checkout> --patch <patch-output>` adds
read-only `rulesraw` and `evalraw` diagnostics. Build that checkout under a different
executable name. The resulting referee's source patch and binary hash are part of
every campaign manifest. The unmodified binary remains the opponent.

The native tests live in `tests/petrified`. `differential.py` compares full legal
move sets, perft and FEN against Walls; `evaluation.py` compares raw adjusted,
unadjusted and final integer evaluations; `bindings.py` compares Python/SAN/public
terminal flags; `board.mjs` consumes the same oracle fixtures for CJS and ESM;
`interfaces.py` exercises native UCI, CECP and complete Node WASM search.
`petrified-state-test <network>` checks do/undo, keys, and scalar/incremental/SIMD
evaluation on deterministic random walks. Use a debug build for its assertions.

## Campaign command

Run in the project's isolated Python environment. All paths are explicit; an
existing output directory is rejected.

```text
python scripts/local_campaign.py --candidate <candidate.exe> --opponent <walls.exe>
  --referee <walls-referee.exe> --network <run4test.nnue> --book <petrified.epd>
  --runner <variantfishtest_new1.py> --out <new-campaign-directory>
  --controls VSTC STC LTC -T 4 --manage-local-worker
```

The actual shell command must be a single line, or use the shell's continuation
syntax. VSTC is 2+0.02, STC 10+0.1, LTC 30+0.3; Threads=1 and Hash=512. A control
must finish with more than 100 games and variantfishtest's displayed LOS 100.0%.
The maximum is 64,000 games per control, with no incomplete pairs discarded.
Already assigned pairs finish when a stopping threshold is reached. The final
result is checked again; a rounded 100% is an operational rule, not certainty.
A failed control stops the campaign before the longer controls.

The shared runner and statistics source are hash-checked and copied with their
Python dependencies. Its result codes, Elo/LOS computation and display are used
unchanged. Its transport and game loop are replaced because the original trusts
engine scores for terminal adjudication. Here each move must be legal according
to the independent Walls process, and game results come from that referee or an
observed clock flag. No `cp 0`, mate-in-one or evaluation-based early adjudication
is used. Terminal boards precede optional draw claims, as in Walls' Board API.

Clock accounting deliberately preserves the existing variantfishtest protocol:
the last reported UCI `time` is charged and the increment is credited. Real elapsed
time and real-clock overruns are also recorded per move. A clock flag, illegal move,
missing time, process crash, timeout or incomplete game is never hidden in a score.
An illegal move or process/protocol failure invalidates the campaign rather than
awarding a qualifying victory. The outer 48-hour bound can be set before a run;
reaching it leaves the campaign incomplete, never passed.

The worker lease uses only the existing local worker's `openbench.exit` contract
and the same `launch_t8.py` single-instance launcher for restoration. It verifies
the old PID, waits at most ten minutes and records pause/restore events. It never
controls remote workers. The memory preflight reserves about 1.3 GiB per concurrent
pair plus 1.5 GiB for the desktop. Both engines are alive during a game, but only
the side to move searches.

Create a `STOP` file in the campaign directory to cancel cooperatively. The wrapper
stops its exact registered child processes, records interruption and restores T8
in its cleanup path. Do not forcibly kill the campaign process: no user-space
cleanup can run after an OS hard kill. Logs include separate stdin/stdout/stderr,
exact PIDs, opening FEN, every move, clocks, terminal reason, complete-pair scores
and final JSON. `--smoke --concurrency 1` runs four short games with Hash=32 to test
the mechanism; it cannot satisfy the strength gate.

OpenBench remains disabled for Petrified until all three local controls and the
initial interface acceptance pass. Linux/sanitizer and release-artifact validation
must also pass before a version 1.0 release is claimed.
