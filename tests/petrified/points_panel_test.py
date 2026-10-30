"""Scheduler contract only: synthetic outcomes, no engines or Elo evidence."""
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
import points_panel as panel


def fake_pair(index, opening, tc, args, directory, cancelled):
    # VSTC first hits the boundary at 40, loses it during draining, then regains
    # it at 48. STC loses decisively; LTC must still run to the maximum.
    score = {'VSTC': 1, 'STC': 0, 'LTC': .5}[directory.name]
    return [{'candidate_score': score, 'time_losses': [0, 0]} for _ in range(2)]


def statistics(scores):
    n = sum(scores)
    if scores[0] == n:
        los = '100.0' if n in (40, 48) else '99.9'
    elif scores[1] == n:
        los = '0.0'
    else:
        los = '50.0'
    return f'LOS: {los}%'


with tempfile.TemporaryDirectory() as temp:
    base = Path(temp)
    dummy = base / 'dummy'
    dummy.write_bytes(b'synthetic scheduler input')
    output = base / 'panel'
    argv = ['points_panel.py']
    for name in ('candidate', 'opponent', 'network', 'bundle', 'modules', 'runner'):
        argv += ['--' + name, str(dummy)]
    argv += ['--out', str(output), '--min-games', '40', '--max-games', '48', '--concurrency', '3']
    runner = SimpleNamespace(print_scores=lambda scores: f'Total: {sum(scores)}', elo_stats=statistics)
    with patch.object(sys, 'argv', argv), \
         patch.object(panel, 'pair', fake_pair), \
         patch.object(panel, 'frozen_runner', lambda *args: runner), \
         patch.object(panel, 'os', SimpleNamespace(name='posix', getpid=os.getpid)), \
         contextlib.redirect_stdout(io.StringIO()):
        panel.main()
    results = json.loads((output / 'results.json').read_text())
    assert [r['control'] for r in results] == ['VSTC', 'STC', 'LTC']
    assert results[0]['status'] == 'WIN' and sum(results[0]['scores']) == 48
    assert results[1]['status'] == 'LOSS' and 40 <= sum(results[1]['scores']) <= 44
    assert results[2]['status'] == 'INCONCLUSIVE_LIMIT' and sum(results[2]['scores']) == 48
    assert all(r['completed_pairs'] * 2 == sum(r['scores']) for r in results)
print('PASS: minimum, drain/recheck/resume, negative LOS, all controls and maximum')
