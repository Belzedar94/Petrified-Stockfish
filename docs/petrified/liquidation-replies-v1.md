# Points liquidation replies v1

The owner authorized one liquidation-horizon candidate from champion PR18 after
the 256-history diagnostic. This is a search experiment with the existing network,
not training or a new evaluation scale. The diagnostic found budget sensitivity;
it did not prove that this particular extension improves strength.

PR18's quiescence usually permits static stand-pat and searches captures. A real
non-pawn capture in Petrified removes both live pieces and adds a permanent wall.
The resulting board can require a quiet reply before its settlement prospects
are understood. This candidate therefore searches all legal replies at a points
quiescence node immediately following such a capture, unless already in check
(where PR18 already searches evasions). Stand-pat cannot cut off or raise alpha
in this reply layer. Existing useful resignation bounds and terminal precedence
remain authoritative, as does the unique-legal-move continuation.

A quiet reply consumes the obligation. Another real petrifying capture renews it.
There is no hypothetical pass, threat/SEE threshold, added depth parameter or
coefficient. Renewal consumes two live pieces, so it cannot create an unbounded
sequence of ordinary quiet layers; check/forced continuations and MAX_PLY retain
PR18 behavior. This extends a specific horizon; it does not solve arbitrary quiet
fortresses, forced cycles or every liquidation. Pawn captures and promotions do
not trigger the extra layer because they do not create a wall by petrification.

The predicate uses the existing captured-piece, petrified and plies-from-null
fields, with explicit null isolation. No persistent state or rule changes. A bare
FEN has no incoming move obligation; full histories are used for acceptance.
Mandatory-reply qnodes use a fixed XOR TT domain key. Ordinary qsearch and main
search keep the original key, so ordinary stand-pat bounds cannot bypass the
obligation. The full picker receives all six continuation-history pointers.
New widened nodes check time; points qsearch propagates cancellation after undo
without caching a partial result. Legacy Walls search retains its prior policy.

Base: fb04cbca1a7584edd7a29fd5f0a19b28d551c217. Only search.cpp and the new
search_liquidation.h change engine behavior. No PR19/20/21/22/23/24 engine changes.
NNUE bytes, rules, scoring, coefficients, ordering and main-search reductions stay
fixed. The engine is frozen before strength results; no subsequent tuning or
automatic second candidate is authorized.

Validation requires full Debug/Release state/reference/undo/null/NNUE, inherited
exchange/capture/forced-choice tests, new real board-delta event and full-picker
comparisons, null/FEN/TT-domain isolation, exact NNUE after make/undo, special moves
and repeated events. The independent pinned Chess.com client replays histories,
all exported legal children and terminal settlements without terminal exclusions
in this new test. Constructed branching quiet mates following petrification must
be seen at depth1/depth4, T1/T2, cold TT, ordinary-mode warming and event warming,
with independent PV checks. Native UCI/CECP, bounded stop, ordinary searches,
candidate bench twice, PR18 167330, legacy 179335 and five source CI jobs precede
the single strength panel. Correctness and node counts do not imply Elo gains.

Panel: 40–400 games per VSTC2+0.02, STC10+0.1, LTC30+0.3; T1/Hash512/concurrency4,
same PR18 rival/network and independent referee. Displayed LOS100.0%/0.0% requires
draining reversed-color pairs and rechecking; continue within cap if it disappears.
Cap without a boundary is NEUTRAL_SCREEN, never equivalence or statistical LOSS.
Only three audited wins plus native/WASM/interfaces/Fairyground acceptance permit
promotion. PR18's historical manual exception is not inherited.

Preserve failures, raw results, pairs/colors, referee logs, input hashes, inventory
and verified archive entries. Prove exact PID+CreationDate exits of every owned
process, including auditors/packagers, before releasing the lease. No OpenBench,
AtomicDB, remote workers, Misere or unrelated applications. Both Fairyground
profiles remain closed/preserved; T8 remains paused. Prior experiments stay closed.

Oracle browser advisory review was attempted before freezing. It failed before
verified model selection because the automation browser had no usable ChatGPT
session. No second-model answer was obtained; no API fallback was used.
