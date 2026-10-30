"""Depth-one forced continuations must reach the real points settlement.

These constructed corridors test quiescence behavior, never playing strength.
The PVs are subsequently checked with the hash-pinned independent client.
"""
import argparse
import json
from pathlib import Path
import re
from protocol import Uci

BOARD = '***r***k/*******1/********/********/********/********/1*******/K**R****'

def main():
    ap = argparse.ArgumentParser()
    for name in ('engine', 'out'):
        ap.add_argument('--' + name, type=Path, required=True)
    ap.add_argument('--network', type=Path, required=True)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    cases = []
    with Uci(args.engine, network=args.network, timeout=90,
             log_prefix=args.out/'engine') as engine:
        engine.send('setoption name UCI_Variant value petrifiedpoints')
        for threads in (1, 2):
            engine.send(f'setoption name Threads value {threads}')
            for side in ('w', 'b'):
                for halfmove in (0, 96):
                    for scores in ('0,0', '1,0', '0,1'):
                        fen = f'{BOARD} {side} - - {halfmove} 1 points={scores}'
                        points = list(map(int, scores.split(',')))
                        result = '1/2-1/2' if points[0] == points[1] else '1-0' if points[0] > points[1] else '0-1'
                        direction = (points[0] > points[1]) - (points[0] < points[1])
                        if side == 'b': direction = -direction
                        engine.send('ucinewgame'); engine.ready()
                        # A second search exercises the existing transposition entries.
                        for repeat in range(2):
                            engine.position(fen)
                            engine.send('go depth 1')
                            lines = engine.until(lambda s: s.startswith('bestmove '))
                            info = [s for s in lines if s.startswith('info depth ') and ' pv ' in s][-1]
                            match = re.search(r' score cp (-?\d+)', info)
                            assert match, info
                            score = int(match[1])
                            assert (score == 0 if not direction else score * direction > 19000), (fen, info)
                            pv = info.split(' pv ', 1)[1].split()
                            assert pv and lines[-1].split()[1] == pv[0], lines
                            assert int(re.search(r' seldepth (\d+)', info)[1]) > 1, info
                            cases.append({'fen': fen, 'pv': pv, 'result': result,
                                          'threads': threads, 'repeat': repeat, 'info': info})
    report = {'result': 'PASS', 'searches': len(cases), 'cases': cases,
              'meaning': 'Forced quiet paths at depth one; correctness, not Elo'}
    (args.out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k != 'cases'}))

if __name__ == '__main__': main()
