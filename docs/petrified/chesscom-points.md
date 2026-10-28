# Chess.com Game of Points

The owner's 2026-10-28 request supersedes the original no-points target. Use
`UCI_Variant=petrifiedpoints` for Chess.com scoring. `petrified` remains the
frozen Walls ruleset so previous experiments remain reproducible.

## Contract and evidence

Profile: FFA, StartFen4 `2PC`, RuleVariants `Deadwall EnPassant FatalCapture
Sideways`. No StalemateLoss or promotion override. The user's public game
[111037628](https://www.chess.com/variants/petrified/game/111037628) ends with
Black mating, but White wins 25–24. Its full 70 plies and scores are regression
fixtures. The logged-in game page confirmed that final score.

The independent rules source is Chess.com's public client
[2026.10.3](https://www.chess.com/r2/client-packages/variants/2026.10.3/variants.js),
SHA-256 `3e6c98bb6f95b124b3949778c4360603976a39c898424aa5d9ae7f441352582b`.
`tests/petrified/chesscom_reference.cjs` verifies that hash, extracts the pure
rules factory and its dependencies into a restricted VM, and exposes a bounded
JSON-lines referee. It does not access an account, browser state or network.
The external client is not included in this repository. Our fixtures record
23 boundary cases and 32 seeded walks (4,976 plies), in addition to the game.

| Event | Points |
|---|---|
| Capture | Victim value: pawn 1, knight 3, bishop 5, rook 5, queen 9 |
| Capturer petrifies | No points to the other player |
| Checkmate | +20 to the mating player, then compare totals |
| Stalemate, 50 moves, third repetition, insufficient material, agreement | +10 each, then compare totals |
| Resignation / timeout | +20 to the other player, or +10 each if that player lacks sufficient material |

Equal totals draw. A side may close the game by resigning and still win or draw
by points. The ordinary claim-win threshold is a lead greater than 20; the
insufficient-material exception can change it. King alone is insufficient;
king plus one bishop/knight is insufficient only without walls. Automatic
insufficient-material ending is deferred while the side to move can capture.
Lateral pawn moves do not reset the fifty-move counter. Repetition includes
scores and occurs on the third actual occurrence. Promotions are QRBN; a
promoted queen is worth 9 in this profile. Adjacent kings do not attack.
Chess.com also forbids lateral moves by a slider-pinned pawn, even if the pawn
would stay on the pin line. Walls permits that case. An independently refereed
NNUE pilot exposed it; five exact legal-move fixtures now cover it, including
both colors, a wall breaking the pin and a permitted forward pawn move.

## State and interfaces

Extended six-field FEN: append `points=WHITE,BLACK`, for example
`r3kb1r/3*4/4**2/6p1/p3*3/P5PP/1P*4K/2R1*1q1 w kq - 2 36 points=25,4`.
The extension stores **earned points before the final bonus**, so this terminal
position reports final 25–24 without double-counting mate. This differs from
loading a Chess.com FEN4 that already contains its final score. Scores must be
nonnegative integers no larger than 1,000,000. `startpos` supplies 0,0; a custom
points FEN without scores is rejected. Historical scores cannot be recovered
from the remaining pieces. Legacy FEN rejects a points suffix.

Scores, mode and promotion/capture state survive make/unmake, FEN copies and
thread setup. Keys and correction histories distinguish score states. Exact
terminal results and optional resignation outcomes are searched before pruning.
Points endings use decisive non-mate scores; a losing mate is never presented
as a winning `score mate`. Null-move pruning is disabled in points mode.

- UCI: `points` returns `info string points {JSON}` with `rules`, `earned`,
  `final`, `result`, `termination`, `canClaimWin`, and `resignResult`.
- A chosen resignation claim returns `info string points action claim-win`
  (or `claim-draw`) followed by `bestmove 0000`. This is an explicit extension
  that a match controller must recognize; 0000 alone is not a result.
- CECP: select `variant petrifiedpoints`; `setboard` accepts extended FEN,
  and the reported game result follows points.
- Python: `points_status(variant, fen, moves)` returns the same JSON;
  `end_result(variant, fen, moves, reason, white)` settles resignation, timeout
  or agreement without mutating the position.
- CJS/ESM Board WASM: `new api.Board('petrifiedpoints', fen)`, `pointsStatus()`
  and `endResult(reason, white)`. Native and complete Node UCI WASM share rules.

Legacy run4test still evaluates board geometry, not the score. The first points
baseline adds a provisional 100 internal evaluation units per earned-point
difference, after historical normalization. This coefficient is not a trained
network and is not an asserted optimum. Exact outcomes never depend on it.

## NNUE next steps

The owner requests a fast local GPU pilot after the initial points panel and
Fairyground work. Freeze the demonstrated points baseline first. Prefer a small
adaptation preserving run4test's useful board features; introduce explicit score
information so identical boards at different scores can evaluate differently.
Keep Legacy available and version any new weights/features instead of silently
interpreting them as run4test. Quantized inference must agree with its reference.

Training rows must preserve both scores, walls, side, rights, counters, original
game/opening identity and the rules hash. Split by games/openings, not positions.
Targets must use points outcomes, including losing mates, tied mates, resignation
thresholds and material-dependent endings. Old ordinary mate WDL labels are not
valid points labels. Include captures, promotions and blocking positions.

Run one bounded pilot, validate serialization and native inference, and then
complete VSTC 2+0.02, STC 10+0.1 and LTC 30+0.3 against the frozen current best
before promoting it or moving to another candidate. Same ISA/compiler, net
inputs recorded, paired openings, T1/Hash512, independent points referee.
The new target is playing strength; neither lower loss nor faster evaluation
alone qualifies a network. T8 remains manually paused; no remote workloads.

## Local verification

`tests/petrified/points.py` checks the real game, independent fixtures/walks,
score FEN rejection, threefold, mate losses/ties, native protocol and claims.
`tests/petrified/state.cpp` checks 9,338 states and their reversals, including
points modes; local real-net scalar/incremental/SIMD evaluations agree exactly.
Board CJS/ESM and complete Node UCI WASM are checked separately. The local panel
uses `scripts/points_panel.py`, frozen shared variantfishtest statistics and
the external Chess.com referee for every move and ending. Bare old EPD openings
are not reused without historical scores; this panel freezes opening histories.
