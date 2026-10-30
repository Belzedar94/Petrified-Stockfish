"""Check searched PVs against the external referee; this is not a strength panel."""
import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from points_panel import Referee
from points_network import verify_points_network
from protocol import Uci


def main():
    ap = argparse.ArgumentParser()
    for name in ('engine', 'network', 'bundle', 'modules', 'out'):
        ap.add_argument('--' + name, type=Path, required=True)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    walks = json.loads((ROOT / 'tests/petrified/fixtures/chesscom-walks.json').read_text())['walks']
    cases = [walk['moves'][:ply] for walk in walks[:8] for ply in (24, 39, 58, 73)]
    reports = []
    checked_moves = 0
    with ExitStack() as stack:
        engine = stack.enter_context(Uci(args.engine, network=args.network, timeout=90,
                                        log_prefix=args.out / 'engine'))
        referee = Referee(args, args.out / 'referee.jsonl')
        stack.callback(referee.close)
        verify_points_network(engine, args.network)
        engine.send('setoption name UCI_Variant value petrifiedpoints')
        engine.send('setoption name Hash value 64')
        engine.ready()
        for number, history in enumerate(cases):
            initial = referee.command(op='reset', moves=history)
            assert not initial['terminal'], (number, initial)
            threads = 1 if number < 24 else 2
            engine.send(f'setoption name Threads value {threads}')
            engine.send('ucinewgame')
            engine.ready()
            engine.send('position startpos moves ' + ' '.join(history))
            engine.send('go nodes 32768')
            lines = engine.until(lambda s: s.startswith('bestmove '))
            best = lines[-1].split()[1]
            action = next((s.rsplit(' ', 1)[-1] for s in lines
                           if s.startswith('info string points action ')), None)
            if action:
                assert best == '0000' and action in ('claim-win', 'claim-draw')
                side = initial['side']
                result = referee.command(op='resign')
                assert result['terminal']
                expected = ('1-0' if side == 'w' else '0-1') if action == 'claim-win' else '1/2-1/2'
                assert result['result'] == expected
                reports.append({'case': number, 'action': action, 'result': result['result']})
                continue
            infos = [s for s in lines if s.startswith('info depth ') and ' pv ' in s]
            assert infos, (number, lines)
            last = infos[-1]
            pv = last.split(' pv ', 1)[1].split()
            assert pv and best == pv[0] and '0000' not in pv, (number, last)
            state = initial
            for move in pv:
                assert not state['terminal'], (number, pv, state)
                state = referee.command(op='move', move=move)
                checked_moves += 1
            mate = re.search(r' score mate (-?\d+)', last)
            if mate:
                assert state['terminal'], (number, last, state)
                winning_side = initial['side'] if int(mate[1]) > 0 else ('b' if initial['side'] == 'w' else 'w')
                assert state['result'] == ('1-0' if winning_side == 'w' else '0-1'), (number, last, state)
            reports.append({'case': number, 'history_plies': len(history), 'threads': threads,
                            'last_info': last, 'pv_plies': len(pv), 'referee_result': state['result']})
    report = {'result': 'PASS', 'positions': len(reports), 'independent_pv_plies': checked_moves,
              'engine_sha256': hashlib.sha256(args.engine.read_bytes()).hexdigest(),
              'network_sha256': hashlib.sha256(args.network.read_bytes()).hexdigest(),
              'T1_positions': 24, 'T2_positions': 8, 'cases': reports,
              'interpretation': 'Correctness check, never Elo or a strength-gate result'}
    (args.out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'cases'}))


if __name__ == '__main__':
    main()
