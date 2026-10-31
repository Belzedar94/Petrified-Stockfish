# PR18 diagnosis v1 evidence scripts

Diagnostic only; no candidate, training or strength panel. The authoritative
protocol and findings are in `docs/petrified/pr18-diagnosis-v1.md` and
`docs/petrified/pr18-diagnosis-results-2026-10-31.md`.

The existing run is closed. These scripts use its fixed local directory and
exclusive output creation; do not rerun the workload there. A new execution
needs a separately authorized directory/lease and unchanged input manifests.

`prepare.py` selected and sealed the bank and exported exact PR18. `build.py`
and `build_continue.py` preserve the original failed compilation and corrected
instrumental builds. `run.py` is the original frozen runner; it retains its
original independent-auditor rejection. Continuations 002–004 preserve successive
adapter corrections. Only continuation 004 ran the bank queries, after the
corrected extraction audit passed.

`normalize.py` reads saved output only: last unbounded iteration scores and
final bestmove/PV are separate, and terminal bounds retain their meaning.
`continue_005.py` ran only `audit_005.cjs` over those saved queries, with no new
engine work. The extraction authority is `extract-004-audit.json`; the search
authority is `searches-005-audit.json`. Earlier rejected scripts/logs remain.

`analyze.py` computes descriptive metrics and history-clustered bootstrap
intervals without fitting or engine execution. `verify.py` independently
reconciles counts, saved raw/normalized queries, frozen hashes, terminal losses
and aggregate arithmetic. `exit_audit.py` performs strict Windows process
identity checks; its own exit is then observed outside that process.
`package.py` preserves the evidence and checks every archive member. The
adjacent `closure.json` proves exits after packaging and binds the final archive
hash; this avoids pretending that a packager can certify its own future exit.

Build and data artifacts, original executables, network, pinned third-party
reference and full raw transcripts remain in the local evidence bundle. They
are not published in Git. The repo contains the protocol, findings, numeric
summary and scripts, not a replacement engine.
