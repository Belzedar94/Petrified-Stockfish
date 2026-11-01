# PR25 loss analysis

PR25 lost all three audited controls against PR18: 7/38/1 in 46 VSTC games,
5/39/2 in 46 STC games, and 10/35/3 in 48 LTC games. Every displayed LOS was
0.0%; neither engine lost on time. The 140 games, 70 reversed-color pairs and
28,506 independent plies passed the original audits with no exclusions.
This is a strength rejection, not a protocol or clock failure. PR25 stays closed.

The source change was much broader than a terminal-horizon correction. After
every eligible non-pawn petrification it removed stand-pat and recursively searched
all legal replies. Another petrification rearmed that layer. It also separated
those qsearch TT entries from ordinary entries. Consequently it spent work on
nonterminal quiet replies and lost some TT reuse even when no liquidation was
available. A higher tactical selective depth can coexist with a shallower principal
search. The panel's reported median principal depths were 13/14, 16/17 and 18/21
for PR25/PR18 at VSTC/STC/LTC; these are observations on different game trajectories,
not controlled causal estimates or Elo measures.

A new paired diagnostic used the unchanged, hash-checked PR18 and PR25 binaries
and common network on 64 distinct histories from the already closed development
bank. Selection, seed, budgets and ordering were recorded before any query. No
panel position was used. Both binaries received the same real histories, cold TT,
T1/Hash128 and sequential queries at depth6 capped at131072 nodes and at32768 nodes.
All256 queries and2169 independent PV/bestmove plies completed. Forty-four final
outputs were bounded and are retained separately from the last complete output.

| Budget | Paired positions | Changed best move | Median PR25/PR18 nodes | Median principal depth PR18/PR25 | Median selective depth PR18/PR25 |
|---|---:|---:|---:|---:|---:|
| depth6, cap131072 | 64, all reached6 below cap | 19 | 1.402 | 6/6 | 8/10 |
| nodes32768 | 64 | 17 | 1.00008 | 14/13 | 17/19 |

The diagnostic confirms increased work at a common principal depth and a shift
toward costly tails: 24/64 positions used over twice the nodes, 7/64 over five
times, and the maximum ratio was14.893. The linearly interpolated90th and95th
percentiles were5.284 and6.543. These are descriptive node counts, not timing.
It also confirms a shift
toward longer selective lines at the expense of principal depth under a node
budget. It does not isolate the contributions of stand-pat removal, extra branches
and TT separation, establish minimax truth for the changed choices, or explain an
exact fraction of the Elo loss. There is no evidence here that another network is
needed. The original completed validation and common network identity rule out
many implementation errors, but passing correctness tests does not establish
search quality.

The original summary failed after all queries on a recorded zero wall time. Its
script, log, raw times and exit1 are preserved. A separate summary reads only the
saved queries; no search was repeated. Wall times are too quantized for a useful
cost conclusion (62/64 depth pairs contain zero wall time); no speed claim is
based on their ratios. Raw evidence: `.local/points-settlement-probe-v1/postmortem-001`,
selection bank SHA256 `907263b0ef4da4c3affe728a2ed08d316922a7cc018d37de2b7e8665d0c88012`.

The single authorized alternative is `points-settlement-probe-v1`: recognize an
immediately winning terminal quiet reply after a real petrification, then retain
ordinary PR18 quiescence when there is no proof. This bounds the extra work to
one layer of rule checks without recursive quiet search, evaluation changes or a
new TT domain. It may still be too costly or too rare to improve strength. The
unchanged validation and frozen three-control panel decide that question.
