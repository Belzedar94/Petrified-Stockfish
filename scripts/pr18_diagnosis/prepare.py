"""Freeze a diagnostic bank from existing development histories, not engine scores."""
import hashlib,io,json,os,shutil,subprocess,tarfile
from collections import Counter
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
TASK=ROOT/'.local/pr18-diagnosis-v1'
SEED='pr18-diagnosis-v1-20261031'
REV='fb04cbca1a7584edd7a29fd5f0a19b28d551c217'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rank(x):return hashlib.sha256((SEED+'|'+str(x)).encode()).hexdigest()
def write(p,d):p.write_text(json.dumps(d,ensure_ascii=True,indent=2)+'\n',encoding='utf-8')
def features(r):
    f=r['fen'].split();a,b=map(int,f[6][7:].split(','))
    return {'clock':int(f[4]),'walls':f[0].count('*'),'live':sum(c.isalpha() for c in f[0]),'lead':abs(a-b)}
def main():
    TASK.mkdir(exist_ok=False)
    prior=json.loads((ROOT/'.local/petrified-machine-lease.json').read_text())
    assert prior['released'] is True
    write(TASK/'previous-lease.json',prior)
    write(ROOT/'.local/petrified-machine-lease.json',{'schema':'petrified-machine-lease-v1','experiment':'pr18-diagnosis-v1','state':'PREPARING','released':False,'authorization':'Owner authorized 256-history, up-to-2048-decision PR18 diagnosis; no training or strength panel.','T8':'manually paused','Fairyground':'Both closed; profiles preserved','active_experiment_processes':[]})
    instr=TASK/'instrumentation';instr.mkdir()
    shutil.copy2(ROOT/'.local/points-repetition-win-v1/instrumentation/sitecustomize.py',instr/'sitecustomize.py')
    (TASK/'journal').mkdir()
    source=TASK/'source';source.mkdir()
    archive=subprocess.check_output(['git','archive','--format=tar',REV],cwd=ROOT)
    with tarfile.open(fileobj=io.BytesIO(archive)) as tf:tf.extractall(source,filter='data')
    book=json.loads((ROOT/'tests/petrified/fixtures/chesscom-walks.json').read_text())
    forbidden={' '.join(g['moves'][:8]) for g in book['walks']}
    histories=[];bank=[];used=set();counts={}
    for corpus in ['points-head-v1','points-head-v2']:
        pool=[]
        for path in sorted((ROOT/'.local'/corpus/'data').glob('game-*/game.json')):
            g=json.loads(path.read_text());fam=g['opening_family']
            if int(hashlib.sha256(fam.encode()).hexdigest()[:8],16)%10<2 or fam in forbidden:continue
            rows=list({r['ply']:r for r in g['samples']}.values())
            if g['result']=='*' or len(rows)<8:continue
            fs=[features(r) for r in rows]
            pool.append({'path':path,'game':g,'rows':rows,'features':fs,'family':fam,'key':rank(corpus+'/'+path.parent.name)})
        categories=[('clock',lambda p:any(f['clock']>=80 for f in p['features']),16),('walls',lambda p:any(f['walls']>=8 for f in p['features']),16),('low_material',lambda p:any(f['live']<=6 for f in p['features']),16),('points',lambda p:any(f['lead']>=15 for f in p['features']),16),('ordinary',lambda p:True,64)]
        for category,predicate,quota in categories:
            eligible=sorted((p for p in pool if p['family'] not in used and predicate(p)),key=lambda p:p['key'])
            assert len(eligible)>=quota,(corpus,category,len(eligible))
            for p in eligible[:quota]:
                used.add(p['family']);g=p['game'];hid=f'h{len(histories):03d}'
                assert sha(p['path'].parent/'referee.jsonl')==g['referee_sha256']
                histories.append({'id':hid,'corpus':corpus,'stratum':category,'path':p['path'].relative_to(ROOT).as_posix(),'sha256':sha(p['path']),'referee_sha256':g['referee_sha256'],'family':p['family'],'result':g['result'],'moves':g['moves']})
                chosen=[];rows=p['rows'];maxply=max(r['ply'] for r in rows);minply=min(r['ply'] for r in rows)
                criteria=[('clock',lambda r:-features(r)['clock']),('walls',lambda r:-features(r)['walls']),('low_material',lambda r:features(r)['live']),('points20',lambda r:abs(features(r)['lead']-20))]
                criteria += [(f'q{int(q*100)}',lambda r,q=q:abs(r['ply']-(minply+q*(maxply-minply)))) for q in [.1,.35,.6,.85]]
                for slot,metric in criteria:
                    r=min((r for r in rows if r['ply'] not in chosen),key=lambda r:(metric(r),rank(hid+str(r['ply']))));chosen.append(r['ply'])
                    bank.append({'id':f'{hid}-p{r["ply"]:04d}','history_id':hid,'corpus':corpus,'stratum':category,'slot':slot,'ply':r['ply'],'fen':r['fen'],'history':g['moves'][:r['ply']],'result':g['result'],'features':features(r)})
        counts[corpus]=Counter(h['stratum'] for h in histories if h['corpus']==corpus)
    assert len(histories)==256 and len(bank)==2048 and len(used)==256
    selected={min((r for r in bank if r['history_id']==h['id']),key=lambda r:rank(r['id']))['id'] for h in histories}
    for r in bank:r['controls']=r['id'] in selected
    write(TASK/'histories.json',histories);write(TASK/'bank.json',bank)
    champ=json.loads((ROOT/'docs/petrified/points-champion.json').read_text())
    for key in ['binary','network']:assert sha(ROOT/champ[key])==champ[key+'_sha256']
    write(TASK/'selection-manifest.json',{'schema':'pr18-diagnosis-selection-v1','seed':SEED,'baseline_source':REV,'histories':256,'positions':2048,'control_positions':len(selected),'strata':counts,'files':{str(p.relative_to(ROOT)):sha(p) for p in [TASK/'histories.json',TASK/'bank.json',ROOT/'docs/petrified/pr18-diagnosis-v1.md',Path(__file__)]},'champion':{k:champ[k] for k in ['source_commit','binary','binary_sha256','network','network_sha256']},'no_scores_used_for_selection':True,'holdout_families_excluded':True})
    print(json.dumps({'histories':256,'positions':2048,'controls':256,'strata':counts}),flush=True)
if __name__=='__main__':main()
