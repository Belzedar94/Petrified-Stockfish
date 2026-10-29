"""Color-paired LOS panel under the pinned Chess.com points referee.

Uses the frozen shared variantfishtest statistics, never its orthodox result
adjudication. The owner's manually paused T8 instance stays paused.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, wait, FIRST_COMPLETED
import hashlib
import json
import os
from pathlib import Path
import queue
import random
import re
import shutil
import subprocess
import sys
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tests/petrified'))
from protocol import Uci
from local_campaign import frozen_runner, TCS, sha, write_json, verify_loaded_network
from points_network import verify_points_network


class Referee:
    def __init__(self, args, log):
        self.log = log.open('w', encoding='utf-8')
        self.err = log.with_suffix('.stderr.log').open('w', encoding='utf-8')
        self.lines = queue.Queue()
        self.process = subprocess.Popen(['node', str(ROOT/'tests/petrified/chesscom_reference.cjs'),
                                        str(args.bundle), str(args.modules)], stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=self.err, text=True, encoding='utf-8', bufsize=1)
        self.reader = threading.Thread(target=self.read, daemon=True)
        self.reader.start()

    def read(self):
        for line in self.process.stdout: self.lines.put(line)
        self.lines.put(None)

    def command(self, **request):
        self.process.stdin.write(json.dumps(request)+'\n'); self.process.stdin.flush()
        line = self.lines.get(timeout=30)
        if line is None: raise RuntimeError('Independent Chess.com referee exited')
        response = json.loads(line)
        self.log.write(json.dumps({'request':request, 'response':response})+'\n'); self.log.flush()
        if 'error' in response: raise RuntimeError(response['error'])
        return response

    def close(self):
        self.process.stdin.close()
        try: self.process.wait(timeout=5)
        except subprocess.TimeoutExpired: self.process.kill(); self.process.wait(timeout=5)
        self.reader.join(timeout=5); self.process.stdout.close(); self.log.close(); self.err.close()


def pair(index, opening, tc, args, directory, cancelled):
    from contextlib import ExitStack
    folder = directory/f'pair-{index:04d}'; folder.mkdir()
    with ExitStack() as stack:
        networks = [args.network, args.opponent_network]
        engines = [stack.enter_context(Uci(exe, network=networks[i], timeout=60, log_prefix=folder/f'engine-{i}'))
                   for i, exe in enumerate((args.candidate, args.opponent))]
        referee = Referee(args, folder/'referee.jsonl'); stack.callback(referee.close)
        for i, engine in enumerate(engines):
            if sha(networks[i]) == '345d67b733991ae5beb2d3a6ea40d54ea01abb1fb699aa39e6f6406495f4d933':
                verify_loaded_network(engine, networks[i])
            else:
                verify_points_network(engine, networks[i])
            engine.send(f'setoption name Hash value {args.hash}')
            engine.send('setoption name UCI_Variant value '+('petrifiedpoints' if i == 0 else args.opponent_variant))
            engine.ready()
        write_json(folder/'processes.json', {'owner':os.getpid(), 'engines':[e.proc.pid for e in engines], 'referee':referee.process.pid})
        results = []
        for candidate_white in [True, False]:
            moves = list(opening)
            state = referee.command(op='reset', moves=moves)
            if state['terminal']: raise RuntimeError('Terminal opening')
            for e in engines: e.send('ucinewgame'); e.ready()
            clocks = [tc[0], tc[0]]; timeline = []; time_losses = [0,0]
            started = time.monotonic()
            for ply in range(2000):
                if cancelled.is_set() or (args.out/'STOP').exists(): raise InterruptedError('Panel stopped')
                if state['terminal']: break
                side = int(state['side'] == 'b')
                i = 0 if (side == 0) == candidate_white else 1
                e = engines[i]
                e.send('position startpos'+(' moves '+' '.join(moves) if moves else ''))
                e.timeout = max(15, clocks[side]/1000*3+5)
                tick = time.monotonic()
                e.send(f'go wtime {clocks[0]} btime {clocks[1]} winc {tc[1]} binc {tc[1]}')
                lines = e.until(lambda s:s.startswith('bestmove '))
                elapsed = round((time.monotonic()-tick)*1000,3)
                move = lines[-1].split()[1]
                action = next((s.rsplit(' ',1)[-1] for s in lines if s.startswith('info string points action ')), None)
                times = [int(m.group(1)) for s in lines if (m:=re.search(r'\btime (\d+)\b',s))]
                if not times and not action: raise RuntimeError('Missing engine clock')
                charged = times[-1] if times else 0
                clocks[side] += tc[1]-charged
                timeline.append({'engine':i,'move':move,'action':action,'reported_ms':charged,'wall_ms':elapsed,'remaining_ms':clocks[side]})
                if clocks[side] < 0:
                    time_losses[i] += 1
                    state = referee.command(op='timeout')
                    break
                if move == '0000' and action in ('claim-win','claim-draw'):
                    if action == 'claim-win' and not state['canClaimWin']: raise RuntimeError('Invalid win claim')
                    state = referee.command(op='resign')
                    expected = ('1-0' if side == 0 else '0-1') if action == 'claim-win' else '1/2-1/2'
                    if not state['terminal'] or state['result'] != expected: raise RuntimeError('Invalid points claim')
                    break
                # The independent official client validates the move and all endings.
                state = referee.command(op='move', move=move)
                moves.append(move)
            if not state['terminal']: raise RuntimeError('No legal ending within the fixed move limit')
            white = {'1-0':1, '0-1':0, '1/2-1/2':.5}[state['result']]
            game = {'pair':index,'candidate_white':candidate_white,'opening':opening,'moves':moves,
                    'candidate_score':white if candidate_white else 1-white,'result':state['result'],
                    'points':state['points'],'termination':state['reason'],'timeline':timeline,'time_losses':time_losses,
                    'elapsed_seconds':time.monotonic()-started}
            write_json(folder/('white.json' if candidate_white else 'black.json'), game)
            results.append(game)
        return results


def decided(scores, statistics, minimum):
    return sum(scores) >= minimum and any(
        f'LOS: {boundary}%' in statistics for boundary in ('100.0', '0.0'))


def main():
    ap = argparse.ArgumentParser()
    for name in ['candidate','opponent','network','bundle','modules','runner','out']:
        ap.add_argument('--'+name, type=Path, required=True)
    ap.add_argument('--min-games', '--games', dest='min_games', type=int, default=40)
    ap.add_argument('--max-games', type=int, default=64000)
    ap.add_argument('--opponent-network', type=Path)
    ap.add_argument('--opponent-variant', choices=['petrified','petrifiedpoints'], default='petrified')
    ap.add_argument('--concurrency', type=int, default=3)
    ap.add_argument('--hash', type=int, default=512)
    ap.add_argument('--controls', nargs='+', choices=list(TCS), default=list(TCS))
    args = ap.parse_args()
    if (args.min_games < 40 or args.min_games % 2 or args.max_games % 2
        or not args.min_games <= args.max_games <= 64000 or not 1 <= args.concurrency <= 4):
        ap.error('Require complete pairs, minimum >=40, maximum <=64000 and concurrency 1..4')
    for name in ['candidate','opponent','network','bundle','modules','runner','out']: setattr(args,name,getattr(args,name).resolve())
    args.opponent_network = (args.opponent_network or args.network).resolve()
    if os.name == 'nt':
        flag = Path(r'D:\OpenBench-workers\opportunistic-t8-20260719-launch2\Client\openbench.exit')
        if not flag.exists(): raise RuntimeError('Owner pause marker is absent; do not touch the worker')
        free = int(subprocess.check_output(['powershell','-NoProfile','-Command','(Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory'],text=True))*1024
        required = args.concurrency*(2*args.hash+400)*1024**2+1536*1024**2
        if free < required: raise MemoryError(f'Require {required} bytes; have {free}')
    args.out.mkdir(parents=True, exist_ok=False)
    runner = frozen_runner(args.runner, args.out/'dependencies')
    walks_path = ROOT/'tests/petrified/fixtures/chesscom-walks.json'
    walks = json.loads(walks_path.read_text())['walks']
    # Histories, not bare FENs: opening captures have known provenance and scores.
    openings = [w['moves'][:16] for w in walks]
    write_json(args.out/'openings.json', openings)
    write_json(args.out/'manifest.json', {'schema':'petrified-points-panel-v2','pid':os.getpid(),
       'rules':'Chess.com client 2026.10.3, FFA Deadwall EnPassant FatalCapture Sideways, 2PC',
       'clock_basis':'variantfishtest engine-reported milliseconds; real elapsed separately logged',
       'stopping':'minimum games then displayed LOS 100.0% or 0.0%; drain pending pairs and recheck; resume if boundary lost; maximum games remains binding; run every control regardless of direction',
       'minimum_games':args.min_games,'maximum_games':args.max_games,'hash':args.hash,'threads':1,'concurrency':args.concurrency,
       'opponent_variant':args.opponent_variant,
       'worker':'owner-paused; no restart', 'files':{str(p):sha(p) for p in [args.candidate,args.opponent,args.network,args.opponent_network,args.bundle,
        Path(__file__),ROOT/'scripts/points_network.py',ROOT/'scripts/local_campaign.py',ROOT/'tests/petrified/chesscom_reference.cjs',ROOT/'tests/petrified/protocol.py',walks_path,args.out/'openings.json']}})
    cancelled = threading.Event()
    summaries = []
    for name in args.controls:
        directory = args.out/name; directory.mkdir()
        scores=[0,0,0]; losses=[0,0]; pentanomial=[0]*5
        result={'control':name,'status':'RUNNING','scores':scores,'time_losses':losses,
                'minimum_games':args.min_games,'maximum_games':args.max_games}
        write_json(directory/'result.json',result)
        pool=ThreadPoolExecutor(max_workers=args.concurrency)
        pending={}; submitted=0; draining=False; stats=''; started=time.monotonic()
        try:
            rng=random.Random(20261028)
            while True:
                while not draining and len(pending)<args.concurrency and submitted*2<args.max_games:
                    future=pool.submit(pair,submitted,rng.choice(openings),TCS[name],args,directory,cancelled)
                    pending[future]=submitted; submitted+=1
                if not pending:
                    if decided(scores,stats,args.min_games) or submitted*2>=args.max_games: break
                    # A late pair can move the rounded LOS away from the boundary.
                    draining=False
                    continue
                done,_=wait(pending,timeout=1,return_when=FIRST_COMPLETED)
                if (args.out/'STOP').exists(): raise InterruptedError('Panel stopped')
                for future in sorted(done,key=lambda f:pending[f]):
                    games=future.result(); del pending[future]
                    for game in games:
                        score=game['candidate_score']; scores[0 if score==1 else 1 if score==0 else 2]+=1
                        losses=[a+b for a,b in zip(losses,game['time_losses'])]
                    pentanomial[int(2*sum(g['candidate_score'] for g in games))]+=1
                    stats=runner.print_scores(scores)+' '+runner.elo_stats(scores).strip()
                    result.update(scores=scores,time_losses=losses,pentanomial=pentanomial,statistics=stats,
                                  completed_pairs=sum(pentanomial),elapsed_seconds=round(time.monotonic()-started,3))
                    write_json(directory/'result.json',result)
                    print(name+' '+stats,flush=True)
                    if decided(scores,stats,args.min_games): draining=True
                if submitted*2>=args.max_games: draining=True
        except BaseException as error:
            cancelled.set()
            for future in pending: future.cancel()
            result.update(status='INTERRUPTED' if isinstance(error,(InterruptedError,KeyboardInterrupt)) else 'INVALID',error=str(error)); write_json(directory/'result.json',result)
            raise
        finally: pool.shutdown(wait=True,cancel_futures=True)
        result['status']=('WIN' if 'LOS: 100.0%' in stats else 'LOSS') if decided(scores,stats,args.min_games) else 'INCONCLUSIVE_LIMIT'
        write_json(directory/'result.json',result)
        summaries.append(result); write_json(args.out/'results.json',summaries)

if __name__ == '__main__': main()
