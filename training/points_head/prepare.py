"""Freeze a deduplicated, opening-family-disjoint dataset and C++ activations."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import numpy as np
from model import BASE_SHA, RECORD_DTYPE, integer_forward, read_network, score_inputs

ROOT=Path(__file__).resolve().parents[2]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    ap=argparse.ArgumentParser()
    for name in ('data','network','exporter','out'): ap.add_argument('--'+name,type=Path,required=True)
    args=ap.parse_args(); args.out.mkdir(parents=True,exist_ok=False)
    manifest=json.loads((args.data/'manifest.json').read_text())
    complete=json.loads((args.data/'complete.json').read_text())
    assert complete['games']==manifest['games']
    assert sha(args.network)==BASE_SHA
    for p,h in manifest['sources'].items(): assert sha(Path(p))==h, p
    book=json.loads((ROOT/'tests/petrified/fixtures/chesscom-walks.json').read_text())
    excluded_families={' '.join(g['moves'][:8]) for g in book['walks']}
    candidates=[]; by_key={}; sources={}; excluded=0; censored=0
    for index in range(manifest['games']):
        p=args.data/f'game-{index:05d}'/'game.json'; g=json.loads(p.read_text()); sources[str(p)]=sha(p)
        assert sha(p.parent/'referee.jsonl')==g['referee_sha256']
        assert g['opening_family']==' '.join(g['opening'][:8])
        if g['opening_family'] in excluded_families: excluded+=1; continue
        if g['result']=='*': censored+=1; continue
        split_key=int(hashlib.sha256(g['opening_family'].encode()).hexdigest()[:8],16)%10
        split=2 if split_key==0 else 1 if split_key==1 else 0
        for r in g['samples']:
            parts=r['fen'].split(); key=' '.join(parts[:5]+parts[6:])
            y=.5 if g['result']=='1/2-1/2' else float((g['result']=='1-0')==(parts[1]=='w'))
            teacher=r['teacher_score']*2.08 if r['teacher_kind']=='cp' else (32000 if r['teacher_score']>0 else -32000)
            row={**r,'split':split,'game':index,'family':g['opening_family'],'result_target':y,'teacher':teacher,'key':key}
            by_key.setdefault(key,[]).append(len(candidates)); candidates.append(row)
    keep=[]; cross_split=duplicate=0
    for indexes in by_key.values():
        if len({candidates[i]['split'] for i in indexes})>1: cross_split+=len(indexes); continue
        keep.append(indexes[0]); duplicate+=len(indexes)-1
    rows=[candidates[i] for i in sorted(keep)]
    assert rows
    (args.out/'rows.jsonl').write_text(''.join(json.dumps(r,separators=(',',':'))+'\n' for r in rows))
    fens=args.out/'fens.txt'; fens.write_text(''.join(r['fen']+'\n' for r in rows))
    with (args.out/'export.log').open('w') as log:
        subprocess.run([str(args.exporter.resolve()),str(args.network.resolve()),str(fens.resolve()),
                        str((args.out/'features.bin').resolve())],stdout=log,stderr=log,check=True,timeout=600)
    records=np.fromfile(args.out/'features.bin',dtype=RECORD_DTYPE)
    assert len(records)==len(rows)
    for r,x in zip(rows,records['x']): assert np.array_equal(x[1024:],score_inputs(r['fen']))
    _,baseline=integer_forward(records['x'],records['meta'],read_network(args.network))
    np.savez_compressed(args.out/'data.npz',x=records['x'],meta=records['meta'],baseline=baseline,
                        y=np.array([r['result_target'] for r in rows],dtype=np.float32),
                        teacher=np.array([r['teacher'] for r in rows],dtype=np.float32),
                        split=np.array([r['split'] for r in rows],dtype=np.uint8))
    summary={'schema':'points-head-prepared-v1','game_count':manifest['games'],'rows':[sum(r['split']==i for r in rows) for i in range(3)],
             'games_per_split':[len({r['game'] for r in rows if r['split']==i}) for i in range(3)],
             'censored_games':censored,'match_opening_families_excluded':excluded,
             'cross_split_duplicates_removed':cross_split,'within_split_duplicates_removed':duplicate,
             'bucket_rows':np.bincount(records['meta'][:,0],minlength=8).tolist(),
             'score_difference_range':[int(records['meta'][:,5].min()),int(records['meta'][:,5].max())],
             'base_sha256':BASE_SHA,'exporter_sha256':sha(args.exporter),'game_sources':sources,
             'files':{p.name:sha(p) for p in args.out.iterdir() if p.is_file()},
             'referee':'Every source game move and terminal checked by pinned Chess.com client; log hashes checked'}
    (args.out/'manifest.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k not in ('game_sources','files')}))


if __name__=='__main__': main()
