"""Frozen variantfishtest statistics with bounded UCI and independent Walls rules.

The shared runner remains untouched. Its statistics/formatting and result codes
are imported from a hash-checked snapshot; its engine-score adjudication and
unbounded transports are replaced here. A whole color-reversed pair is the unit
of scheduling, scoring and stopping. This script never submits OpenBench work.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, wait, FIRST_COMPLETED
from contextlib import ExitStack, nullcontext
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import random
import re
import shutil
import signal
import subprocess
import sys
import threading
import time

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests/petrified'))
from protocol import Uci
from worker_lease import WorkerLease

TCS={'VSTC':(2000,20),'STC':(10000,100),'LTC':(30000,300)}
BASELINE=json.loads((ROOT/'docs/petrified/baseline.json').read_text())
STAT_SHA='06af2f59cc22eb17213f67d243bafc0fb2e4bb6627026787ede9d4cf337387ea'

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def write_json(path,data): path.write_text(json.dumps(data,indent=2)+'\n')

def frozen_runner(source,destination):
    if sha(source)!=BASELINE['runner']['sha256']: raise ValueError('Shared runner hash changed')
    if sha(source.parent/'stat_util.py')!=STAT_SHA: raise ValueError('Statistics dependency hash changed')
    destination.mkdir()
    shutil.copy2(source,destination/source.name)
    shutil.copy2(source.parent/'stat_util.py',destination/'stat_util.py')
    (destination/'chess').mkdir()
    for p in (source.parent/'chess').glob('*.py'): shutil.copy2(p,destination/'chess'/p.name)
    sys.path.insert(0,str(destination))
    spec=importlib.util.spec_from_file_location('frozen_variantfishtest',destination/source.name)
    runner=importlib.util.module_from_spec(spec); spec.loader.exec_module(runner)
    return runner

def status(ref,fen,moves):
    ref.position(fen,moves); ref.send('rulesraw'); lines=ref.ready()
    values={s.split(':',1)[0]:s.split(':',1)[1].strip() for s in lines if ': ' in s}
    terminal,value=values['Terminal'].split()
    optional,_=values['Optional'].split()
    legal={s.split()[1] for s in lines if s.startswith('Move: ')}
    if terminal=='1': return legal,int(value),'terminal'
    if optional=='1': return legal,0,'draw-claim'
    return legal,None,None

def verify_loaded_network(engine,network):
    engine.position('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1')
    engine.send('go depth 1')
    lines=engine.until(lambda s:s.startswith('bestmove '))
    legacy='NNUE Legacy Walls run4test loaded; SHA256='+BASELINE['network']['sha256']
    fsf='NNUE evaluation using '+str(network)+' enabled'
    if not any(fsf in line or (legacy in line and sha(network)==BASELINE['network']['sha256']) for line in lines):
        raise RuntimeError('Engine did not positively identify the requested NNUE')

def play_pair(index,fen,args,directory,stop):
    pairdir=directory/f'pair-{index:05d}'; pairdir.mkdir()
    with ExitStack() as stack:
        networks=(args.network,args.opponent_network)
        engines=[stack.enter_context(Uci(exe,network=networks[i],timeout=90,
                   log_prefix=pairdir/f'engine-{i}')) for i,exe in enumerate((args.candidate,args.opponent))]
        ref=stack.enter_context(Uci(args.referee,log_prefix=pairdir/'referee'))
        for engine,network in zip(engines,networks):
            engine.send(f'setoption name Hash value {32 if args.smoke else 512}'); engine.ready()
            verify_loaded_network(engine,network)
        write_json(pairdir/'processes.json',{'owner':os.getpid(),'engines':[e.proc.pid for e in engines],
                                           'referee':ref.proc.pid})
        results=[]
        for candidate_white in (True,False):
            for engine in engines: engine.send('ucinewgame'); engine.ready()
            moves=[]; timeline=[]; clocks=[args.base,args.base]
            initial_side=0 if fen.split()[1]=='w' else 1
            started=time.monotonic()
            for ply in range(2000):
                if stop.is_set() or (directory/'STOP').exists(): raise InterruptedError('Campaign cancellation')
                legal,result,reason=status(ref,fen,moves)
                side=(initial_side+ply)%2
                if result is not None:
                    white_score=.5 if result==0 else float((result>0)==(side==0))
                    break
                engine_index=0 if (side==0)==candidate_white else 1
                engine=engines[engine_index]
                engine.position(fen,moves)
                engine.timeout=max(15,clocks[side]/1000*3+5)
                start=time.monotonic()
                engine.send(f'go wtime {clocks[0]} btime {clocks[1]} winc {args.inc} binc {args.inc}')
                output=engine.until(lambda s:s.startswith('bestmove '))
                elapsed_ms=round((time.monotonic()-start)*1000,3)
                move=output[-1].split()[1]
                if move not in legal: raise RuntimeError(f'Illegal move by engine {engine_index}, pair {index}, ply {ply}: {move}')
                times=[int(m.group(1)) for s in output if (m:=re.search(r'\btime (\d+)\b',s))]
                if not times: raise RuntimeError('Engine did not report search time')
                charged=times[-1]
                timeline.append({'move':move,'engine':engine_index,'reported_ms':charged,
                                 'wall_ms':elapsed_ms,'remaining_before_ms':clocks[side],
                                 'wall_clock_overrun':elapsed_ms>clocks[side]+args.inc})
                moves.append(move)
                clocks[side]+=args.inc-charged
                if clocks[side]<0:
                    reason='time-loss'; white_score=float(side==1)
                    # Validate the last move's resulting state even on a flag.
                    status(ref,fen,moves)
                    break
            else: raise RuntimeError('Game exceeded 2000 plies; no artificial draw was awarded')
            score=white_score if candidate_white else 1-white_score
            result={'pair':index,'candidate_white':candidate_white,'fen':fen,'moves':moves,
                    'candidate_score':score,'termination':reason,'timeline':timeline,
                    'elapsed_seconds':time.monotonic()-started}
            write_json(pairdir/('white.json' if candidate_white else 'black.json'),result)
            results.append(result)
        return results

def run_control(name,args,runner,book,root):
    directory=root/name; directory.mkdir()
    args.base,args.inc=(100,10) if args.smoke else TCS[name]
    limit=4 if args.smoke else args.max_games
    rng=random.Random(args.seed)
    scores=[0,0,0]; pentanomial=[0]*5; time_losses=[0,0]; wall_overruns=[0,0]
    pending={}; submitted=0; stop=threading.Event(); drain=False
    result={'control':name,'base_ms':args.base,'increment_ms':args.inc,'hash_mb':32 if args.smoke else 512,
            'threads':1,'concurrency':args.concurrency,'status':'RUNNING','clock_basis':'engine-reported-ms',
            'adjudication':'Walls terminal before optional draw claim; no evaluation adjudication',
            'seed':args.seed,'minimum_games':args.min_games,'maximum_games':limit}
    write_json(directory/'result.json',result)
    started=time.monotonic()
    with ThreadPoolExecutor(max_workers=args.concurrency) as pool:
        try:
            while pending or not drain:
                while not drain and len(pending)<args.concurrency and submitted*2<limit:
                    future=pool.submit(play_pair,submitted,rng.choice(book),args,directory,stop)
                    pending[future]=submitted; submitted+=1
                if not pending: break
                done,_=wait(pending,timeout=1,return_when=FIRST_COMPLETED)
                if (directory/'STOP').exists() or (root/'STOP').exists(): raise InterruptedError('STOP requested')
                if time.monotonic()-started>args.max_hours*3600: raise TimeoutError('Campaign wall-time budget reached')
                for future in done:
                    games=future.result(); del pending[future]
                    for game in games:
                        score=game['candidate_score']
                        scores[0 if score==1 else 1 if score==0 else 2]+=1
                        if game['termination']=='time-loss': time_losses[0 if score==0 else 1]+=1
                        for turn in game['timeline']:
                            wall_overruns[turn['engine']]+=turn['wall_clock_overrun']
                    pentanomial[int(2*sum(g['candidate_score'] for g in games))]+=1
                    statistics=runner.print_scores(scores)+' '+runner.elo_stats(scores).strip()
                    print(f'{name} {statistics}',flush=True)
                    with (directory/'scores.log').open('a') as f: f.write(statistics+'\n')
                    result.update(scores=scores,pentanomial=pentanomial,time_losses=time_losses,
                                  wall_clock_overruns=wall_overruns,statistics=statistics,
                                  elapsed_seconds=round(time.monotonic()-started,3))
                    write_json(directory/'result.json',result)
                    # Predeclared historical stopping rule. Drain already assigned
                    # pairs and assess the final result; never discard a late pair.
                    if sum(scores)>=args.min_games and ('LOS: 100.0%' in statistics or 'LOS: 0.0%' in statistics): drain=True
                if submitted*2>=limit: drain=True
        except BaseException as error:
            stop.set()
            result.update(status='INTERRUPTED' if isinstance(error,(KeyboardInterrupt,InterruptedError)) else 'INVALID',error=str(error))
            write_json(directory/'result.json',result)
            raise
    statistics=runner.print_scores(scores)+' '+runner.elo_stats(scores).strip()
    passed=sum(scores)>=args.min_games and 'LOS: 100.0%' in statistics
    result.update(status='SMOKE' if args.smoke else 'PASS' if passed else 'FAIL_OR_INCONCLUSIVE',
                  statistics=statistics,completed_pairs=sum(pentanomial),passed=passed and not args.smoke)
    write_json(directory/'result.json',result)
    return result

def main():
    ap=argparse.ArgumentParser()
    for name in ('candidate','opponent','referee','network','book','runner','out'):
        ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--opponent-network',type=Path)
    ap.add_argument('--controls',nargs='+',choices=list(TCS),default=list(TCS))
    ap.add_argument('--concurrency','-T',type=int,default=4)
    ap.add_argument('--min-games',type=int,default=40)
    ap.add_argument('--max-games',type=int,default=64000)
    ap.add_argument('--max-hours',type=float,default=48)
    ap.add_argument('--seed',type=int,default=20261028)
    ap.add_argument('--smoke',action='store_true')
    ap.add_argument('--manage-local-worker',action='store_true')
    args=ap.parse_args()
    if not 1<=args.concurrency<=4: ap.error('Concurrency must be 1..4')
    if args.min_games%2 or not 40<=args.min_games<=64000: ap.error('min-games must be even and between 40 and 64000')
    if args.max_games%2 or not args.min_games<=args.max_games<=64000: ap.error('max-games must be even and between min-games and 64000')
    if not args.smoke and not args.manage_local_worker: ap.error('Local campaigns require the T8 pause/restore lease')
    for name in ('candidate','opponent','referee','network','book','runner','out'): setattr(args,name,getattr(args,name).resolve())
    args.opponent_network=(args.opponent_network or args.network).resolve()
    for name in ('network','book'):
        if sha(getattr(args,name))!=BASELINE[name]['sha256']: raise ValueError(name+' hash changed')
    if sha(args.opponent_network) not in (BASELINE['network']['sha256'],BASELINE['secondary_reference']['network']['sha256']):
        raise ValueError('Opponent network is not a frozen reference')
    args.out.mkdir(parents=True,exist_ok=False)
    # Exclusive directory and lease file prevent duplicate campaign processes.
    write_json(args.out/'owner.json',{'pid':os.getpid(),'started_utc':time.time()})
    runner=frozen_runner(args.runner,args.out/'dependencies')
    book=args.book.read_text().splitlines()
    if len(book)!=7939: raise ValueError('Book position count changed')
    manifest={'schema':'petrified-local-campaign-v2','baseline':BASELINE,
              'files':{name:{'path':str(getattr(args,name)),'sha256':sha(getattr(args,name))}
                       for name in ('candidate','opponent','referee','network','opponent_network','book','runner')},
              'dependencies':{str(p.relative_to(args.out)):sha(p) for p in (args.out/'dependencies').rglob('*.py')},
              'wrapper_sha256':sha(Path(__file__)),'transport_sha256':sha(ROOT/'tests/petrified/protocol.py'),
              'python':sys.version,'arguments':{k:str(v) if isinstance(v,Path) else v for k,v in vars(args).items()}}
    write_json(args.out/'manifest.json',manifest)
    source_snapshot=args.out/'source-snapshot'; source_snapshot.mkdir()
    for source in (Path(__file__),ROOT/'scripts/worker_lease.py',ROOT/'tests/petrified/protocol.py'):
        shutil.copy2(source,source_snapshot/source.name)
    results=[]
    lease=WorkerLease(args.out/'worker-lease.json') if args.manage_local_worker else nullcontext()
    try:
        with lease:
            if not args.smoke:
                free=int(subprocess.check_output(['powershell','-NoProfile','-Command',
                         '(Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory'],text=True))*1024
                required=args.concurrency*1300*1024**2+1536*1024**2
                write_json(args.out/'memory.json',{'free_bytes':free,'required_bytes':required})
                if free<required: raise MemoryError(f'Need {required/1024**3:.2f} GiB free, have {free/1024**3:.2f}')
            for control in args.controls:
                result=run_control(control,args,runner,book,args.out); results.append(result)
                write_json(args.out/'results.json',results)
                if not args.smoke and not result['passed']: break
    except BaseException as error:
        write_json(args.out/'failure.json',{'error':type(error).__name__,'message':str(error),
                                          'completed_controls':results})
        raise
    print(json.dumps({'campaign':str(args.out),'results':results}),flush=True)

if __name__=='__main__': main()
