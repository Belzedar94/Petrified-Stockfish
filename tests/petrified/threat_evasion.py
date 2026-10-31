"""A depth-one leaf must search quiet replies to a profitable legal capture.

No expected evaluation or preferred response is fitted to the network.
Both colors, score leads, cold/warm TT and one/two threads are exercised.
"""
import argparse
import json
from pathlib import Path
from protocol import Uci


def flip(fen):
    fields = fen.split()
    fields[0] = '/'.join(rank.swapcase() for rank in reversed(fields[0].split('/')))
    fields[1] = 'b' if fields[1] == 'w' else 'w'
    fields[6] = 'points=' + ','.join(reversed(fields[6][7:].split(',')))
    return ' '.join(fields)


def main():
    ap = argparse.ArgumentParser()
    for name in ('engine', 'network', 'out'):
        ap.add_argument('--' + name, type=Path, required=True)
    args = ap.parse_args()
    args.out.mkdir(exist_ok=False, parents=True)
    cases = []
    with Uci(args.engine, network=args.network, timeout=90,
             log_prefix=args.out/'engine') as engine:
        engine.send('setoption name UCI_Variant value petrifiedpoints')
        for threads in (1, 2):
            engine.send(f'setoption name Threads value {threads}')
            for scores in ('0,0', '7,4', '4,7'):
                # After h6f5, the black queen is threatened by a knight.
                # Black has several legal moves and no capture-stage move.
                fen = f'8/7k/7N/8/3q4/P7/K7/8 w - - 0 1 points={scores}'
                for mirrored in (False, True):
                    start = flip(fen) if mirrored else fen
                    move = 'h3f4' if mirrored else 'h6f5'
                    engine.send('ucinewgame'); engine.ready()
                    for repeat in range(2):
                        engine.position(start)
                        engine.send('go depth 1 searchmoves ' + move)
                        lines = engine.until(lambda s: s.startswith('bestmove '))
                        info = [s for s in lines if s.startswith('info depth ') and ' pv ' in s][-1]
                        pv = info.split(' pv ', 1)[1].split()
                        assert lines[-1].split()[1] == move and pv[0] == move, lines
                        assert len(pv) >= 2, ('quiet threat response missing', start, info)
                        cases.append(dict(fen=start, pv=pv, threads=threads, repeat=repeat, info=info))
    report = dict(result='PASS', searches=len(cases), cases=cases,
                  meaning='Depth-one quiet response acceptance, not Elo or a fixed best move')
    (args.out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k != 'cases'}))


if __name__ == '__main__': main()
