"""Bounded self-play pilot; preserve scored FEN and whole-game split identity."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import importlib
import json
import os
from pathlib import Path
import random
import re
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tests/petrified'))
from protocol import Uci


def game(index, args, rules):
    rng = random.Random(args.seed + index)
    variant = 'petrifiedpoints'
    start = rules.start_fen(variant)
    moves = []
    samples = []
    # New opening histories; never use the frozen match openings as training rows.
    for _ in range(12):
        status = json.loads(rules.points_status(variant, start, moves))
        if status['result'] != '*': break
        moves.append(rng.choice(rules.legal_moves(variant, start, moves)))
    opening = list(moves)
    with Uci(args.engine, network=args.network) as engine:
        engine.send('setoption name UCI_Variant value petrifiedpoints')
        engine.send('setoption name Hash value 32')
        engine.ready()
        result = '*'
        for ply in range(600):
            if (args.out/'STOP').exists(): raise InterruptedError('Datagen stopped')
            status = json.loads(rules.points_status(variant, start, moves))
            if status['result'] != '*':
                result = status['result']; break
            fen = rules.get_fen(variant, start, moves)
            engine.send('position startpos moves '+' '.join(moves))
            if ply % 2 == index % 2 and not rules.gives_check(variant, start, moves):
                engine.send('eval')
                lines = engine.ready()
                value = next(int(line.split(': ')[1]) for line in lines if line.startswith('Legacy final: '))
                samples.append({'fen':fen, 'baseline':value, 'ply':len(moves)})
            engine.send(f'go nodes {args.nodes}')
            lines = engine.until(lambda line:line.startswith('bestmove '))
            move = lines[-1].split()[1]
            if move == '0000':
                assert any(line.startswith('info string points action ') for line in lines), lines
                result = rules.end_result(variant, start, moves, 'resign', ' w ' in fen)
                break
            assert move in rules.legal_moves(variant, start, moves), move
            moves.append(move)
        if result == '*': samples = []  # Censored games do not become false draws.
    record = {'schema':'petrified-scored-selfplay-v1','game':index,'seed':args.seed+index,
              'opening':opening,'moves':moves,'result':result,'samples':samples}
    (args.out/f'game-{index:05d}.json').write_text(json.dumps(record,separators=(',',':'))+'\n')
    return len(samples), result


def main():
    ap=argparse.ArgumentParser()
    for name in ['engine','network','module-dir','out']: ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--games',type=int,default=2048)
    ap.add_argument('--nodes',type=int,default=1024)
    ap.add_argument('--threads',type=int,default=4)
    ap.add_argument('--seed',type=int,default=72831001)
    args=ap.parse_args()
    for name in ['engine','network','module_dir','out']: setattr(args,name,getattr(args,name).resolve())
    args.out.mkdir(parents=True,exist_ok=False)
    dll=os.add_dll_directory(r'C:\msys64\mingw64\bin') if os.name=='nt' else None
    sys.path.insert(0,str(args.module_dir)); rules=importlib.import_module('pyffish')
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    manifest={'schema':'petrified-scored-selfplay-v1','rules':'chesscom-points-v1',
              'games':args.games,'nodes':args.nodes,'threads':args.threads,'seed':args.seed,
              'sources':{str(p):sha(p) for p in [args.engine,args.network,Path(__file__),Path(rules.__file__)]},
              'split':'game modulo 8: 0 test, 1 validation, 2..7 training; no match opening rows'}
    (args.out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    started=time.monotonic(); count=0; rows=0; results={}
    with ThreadPoolExecutor(max_workers=args.threads) as pool:
        futures=[pool.submit(game,index,args,rules) for index in range(args.games)]
        try:
            for future in as_completed(futures):
                n,result=future.result(); count+=1; rows+=n; results[result]=results.get(result,0)+1
                if count % 32 == 0 or count==args.games:
                    progress={'games':count,'rows':rows,'results':results,'seconds':round(time.monotonic()-started,1)}
                    (args.out/'progress.json').write_text(json.dumps(progress)+'\n')
                    print(json.dumps(progress),flush=True)
        except BaseException:
            (args.out/'STOP').touch()
            for f in futures: f.cancel()
            raise

if __name__=='__main__': main()
