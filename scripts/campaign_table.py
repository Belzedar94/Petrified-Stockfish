"""Print the owner's per-control table from a live or completed campaign."""
import argparse
import json
from pathlib import Path
import re


def table(root):
    rows = [
        '| TC | Games | W/L/D | Elo +/- 95% CI | LOS | Time losses | Status |',
        '|---|---:|---:|---:|---:|---:|---|',
    ]
    for name, clock in [('VSTC', '2+0.02'), ('STC', '10+0.1'), ('LTC', '30+0.3')]:
        path = root / name / 'result.json'
        if not path.exists():
            rows.append(f'| {name} {clock} | 0 | - | - | - | - | PENDING |')
            continue
        # The live writer can be between truncate and write; a caller can retry.
        result = json.loads(path.read_text())
        scores = result.get('scores', [0, 0, 0])
        statistics = result.get('statistics', '')
        elo = re.search(r'ELO: ([-+\d.]+) \+-([\d.]+)', statistics)
        los = re.search(r'LOS: ([\d.]+%)', statistics)
        interval = f'{float(elo[1]):+.2f} +/- {elo[2]}' if elo else '-'
        losses = '/'.join(map(str, result.get('time_losses', [0, 0])))
        wld = '/'.join(map(str, scores))
        rows.append(f'| {name} {clock} | {sum(scores)} | {wld} | {interval} | '
                    f'{los[1] if los else "-"} | {losses} | {result["status"]} |')
    return '\n'.join(rows)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('campaign', type=Path)
    print(table(parser.parse_args().campaign))
