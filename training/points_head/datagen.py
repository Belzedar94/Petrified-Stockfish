"""Bounded, independently refereed self-play for a frozen-transformer head.

Fresh opening families, complete histories, searched teacher scores and final
points results. No old match openings, static-eval labels, or censored draws.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from contextlib import ExitStack
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
sys.path[:0] = [str(ROOT/'scripts'), str(ROOT/'tests/petrified')]
from points_panel import Referee
from protocol import Uci
from local_campaign import verify_loaded_network


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(index, args, rules):
    rng = random.Random(args.seed + index)
    folder = args.out/f'game-{index:05d}'
    folder.mkdir()
    start = rules.start_fen('petrifiedpoints')
    moves, samples = [], []
    with ExitStack() as stack:
        engine = stack.enter_context(Uci(args.engine, network=args.network, timeout=90))
        verify_loaded_network(engine, args.network)
        engine.send('setoption name UCI_Variant value petrifiedpoints')
        engine.send('setoption name Hash value 64')
        engine.ready()
        ref = Referee(args, folder/'referee.jsonl')
        stack.callback(ref.close)
        (folder/'processes.json').write_text(json.dumps({'owner':os.getpid(), 'engine':engine.proc.pid,
                                                       'referee':ref.process.pid}))
        state = ref.command(op='reset', moves=[])
        # Multiple random-prefix lengths diversify material/score distributions.
        # Every move and terminal is accepted by the external client.
        for _ in range(rng.choice([8,12,16,20])):
            if state['terminal']: break
            legal = rules.legal_moves('petrifiedpoints', start, moves)
            move = rng.choice(legal)
            state = ref.command(op='move', move=move)
            moves.append(move)
        opening = list(moves)
        for ply in range(600):
            if (args.out/'STOP').exists(): raise InterruptedError('Datagen stopped')
            if state['terminal']: break
            fen = rules.get_fen('petrifiedpoints', start, moves)
            earned = list(map(int, fen.split('points=')[1].split(',')))
            if earned != state['points']: raise RuntimeError('Referee/board score mismatch')
            engine.send('position startpos moves '+' '.join(moves))
            engine.send(f'go nodes {args.nodes}')
            lines = engine.until(lambda s:s.startswith('bestmove '))
            info = [s for s in lines if s.startswith('info depth ') and ' score ' in s
                    and 'lowerbound' not in s and 'upperbound' not in s]
            if ply % 3 == index % 3 and not rules.gives_check('petrifiedpoints', start, moves) and info:
                match = re.search(r'\bscore (cp|mate) (-?\d+)\b', info[-1])
                if not match: raise RuntimeError('Missing teacher score')
                kind, score = match[1], int(match[2])
                samples.append({'fen':fen, 'ply':len(moves), 'teacher_kind':kind, 'teacher_score':score,
                                'teacher_info':info[-1]})
            move = lines[-1].split()[1]
            if move == '0000':
                action = next((s.rsplit(' ',1)[-1] for s in lines if s.startswith('info string points action ')),None)
                if action not in ('claim-win','claim-draw'): raise RuntimeError('Unexplained null move')
                side = state['side']
                state = ref.command(op='resign')
                expected = ('1-0' if side=='w' else '0-1') if action=='claim-win' else '1/2-1/2'
                if not state['terminal'] or state['result'] != expected: raise RuntimeError('Invalid points claim')
                break
            state = ref.command(op='move', move=move)
            moves.append(move)
        result = state['result'] if state['terminal'] else '*'
        record = {'schema':'points-head-data-v1', 'game':index, 'seed':args.seed+index,
                  'opening':opening, 'opening_family':' '.join(opening[:8]), 'moves':moves,
                  'result':result, 'final':state, 'samples':samples if result!='*' else [],
                  'censored_samples':len(samples) if result=='*' else 0,
                  'referee_sha256':sha(folder/'referee.jsonl')}
    (folder/'game.json').write_text(json.dumps(record,separators=(',',':'))+'\n')
    return result, len(record['samples']), len(moves)


def main():
    ap=argparse.ArgumentParser()
    for name in ('engine','network','module-dir','bundle','modules','out'):
        ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--games',type=int,default=1024)
    ap.add_argument('--nodes',type=int,default=8192)
    ap.add_argument('--concurrency',type=int,default=4)
    ap.add_argument('--seed',type=int,default=29102901)
    args=ap.parse_args()
    for name in ('engine','network','module_dir','bundle','modules','out'):
        setattr(args,name,getattr(args,name).resolve())
    args.out.mkdir(parents=True,exist_ok=False)
    dll=os.add_dll_directory(r'C:\msys64\mingw64\bin') if os.name=='nt' else None
    sys.path.insert(0,str(args.module_dir)); rules=importlib.import_module('pyffish')
    inputs=[args.engine,args.network,args.bundle,Path(rules.__file__),Path(__file__),
            ROOT/'tests/petrified/chesscom_reference.cjs', ROOT/'tests/petrified/protocol.py',
            ROOT/'scripts/points_panel.py',args.modules.parent/'package-lock.json']
    manifest={'schema':'points-head-data-manifest-v1','pid':os.getpid(),'games':args.games,'nodes':args.nodes,
              'concurrency':args.concurrency,'seed':args.seed,'sources':{str(p):sha(p) for p in inputs},
              'split':'SHA256 of first 8 opening plies modulo 10: 0 test, 1 validation, 2..9 train',
              'referee':'Pinned external Chess.com client, every move and ending', 'labels':'searched score and actual final points result'}
    (args.out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    started=time.monotonic(); games=rows=plies=0; results={}
    with ThreadPoolExecutor(max_workers=args.concurrency) as pool:
        futures=[pool.submit(generate,i,args,rules) for i in range(args.games)]
        try:
            for f in as_completed(futures):
                result,n,p=f.result(); games+=1; rows+=n; plies+=p; results[result]=results.get(result,0)+1
                progress={'games':games,'rows':rows,'audited_plies':plies,'results':results,
                          'seconds':round(time.monotonic()-started,1)}
                (args.out/'progress.json').write_text(json.dumps(progress)+'\n')
                if games%32==0 or games==args.games: print(json.dumps(progress),flush=True)
        except BaseException:
            (args.out/'STOP').touch()
            for f in futures: f.cancel()
            raise
    for p,digest in manifest['sources'].items():
        if sha(Path(p)) != digest: raise RuntimeError('Datagen input changed: '+p)
    (args.out/'complete.json').write_text(json.dumps(progress)+'\n')


if __name__=='__main__': main()
