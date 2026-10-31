"""Audit every completed training game against the external Chess.com referee."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
from pathlib import Path
import sys
import types

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
from points_panel import Referee


def audit_chunk(worker, args):
    ref=Referee(args,args.out/f'referee-{worker}.jsonl')
    valid=[];invalid=[];plies=0
    try:
        for index in range(worker,args.games,4):
            game=json.loads((args.data/f'game-{index:05d}.json').read_text())
            try:
                samples={s['ply']:s for s in game['samples']}
                state=ref.command(op='reset',moves=[])
                for ply,move in enumerate(game['moves']):
                    if ply in samples:
                        expected=list(map(int,samples[ply]['fen'].split('points=')[1].split(',')))
                        assert state['points']==expected,('score',ply,state,expected)
                    assert not state['terminal'],('early end',ply,state)
                    state=ref.command(op='move',move=move);plies+=1
                if game['result']=='*': continue
                if not state['terminal']: state=ref.command(op='resign')
                assert state['result']==game['result'],('result',state,game['result'])
                valid.append(index)
            except (RuntimeError,AssertionError) as error:
                invalid.append({'game':index,'error':str(error)})
    finally:ref.close()
    return valid,invalid,plies


if __name__=='__main__':
    ap=argparse.ArgumentParser()
    for name in ['data','bundle','modules','out']:ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--games',type=int,default=512)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    valid=[];invalid=[];plies=0
    with ThreadPoolExecutor(max_workers=4) as pool:
        for f in as_completed([pool.submit(audit_chunk,i,args) for i in range(4)]):
            good,bad,n=f.result();valid+=good;invalid+=bad;plies+=n
            print(json.dumps({'audited':len(valid)+len(invalid),'invalid':invalid}),flush=True)
    result={'schema':'scored-data-audit-v1','valid':sorted(valid),'invalid':invalid,'plies':plies,
            'referee_sha256':'3e6c98bb6f95b124b3949778c4360603976a39c898424aa5d9ae7f441352582b'}
    (args.out/'report.json').write_text(json.dumps(result,indent=2)+'\n')
