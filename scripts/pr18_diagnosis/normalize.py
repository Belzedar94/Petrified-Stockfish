"""Parse saved UCI output without executing searches or replacing raw evidence."""
import collections,json,re
from pathlib import Path
import run as r

def main():
 r.check();stats=collections.Counter();sources={}
 for folder in ['baseline','controls']:
  target=r.TASK/('normalized-'+folder);target.mkdir(exist_ok=False)
  for p in sorted((r.TASK/folder).glob('*.json')):
   raw=json.loads(p.read_text());sources[str(p.relative_to(r.ROOT))]=r.sha(p)
   for q in raw['searches']:
    infos=[s for s in q['output'] if s.startswith('info depth ') and ' score ' in s]
    final=infos[-1] if infos else ''
    bound='upper' if 'upperbound' in final else 'lower' if 'lowerbound' in final else None
    q['last_exact_pv']=q['pv'];q['last_exact_nodes']=q['nodes']
    q['final_info']=final;q['final_bound']=bound
    m=re.search(r'\bscore (cp|mate) (-?\d+)',final)
    q['final_score_kind']=m[1] if m else None;q['final_score']=int(m[2]) if m else None
    q['nodes']=max([int(m[1]) for s in infos if (m:=re.search(r'\bnodes (\d+)',s))],default=None)
    q['reported_ms']=max([int(m[1]) for s in infos if (m:=re.search(r'\btime (\d+)',s))],default=None)
    pvs=[s.split(' pv ',1)[1].split() for s in infos if ' pv ' in s]
    if q['bestmove']=='0000':assert q['action'];q['pv']=[]
    else:
     matching=[pv for pv in pvs if pv and pv[0]==q['bestmove']]
     assert matching,(p,q['budget'],q['bestmove']);q['pv']=matching[-1]
    q['last_exact_pv_matches_bestmove']=not q['last_exact_pv'] or q['last_exact_pv'][0]==q['bestmove']
    stats['queries']+=1;stats['final_bound_'+str(bound)]+=1
    stats['last_exact_pv_differs_from_bestmove']+=not q['last_exact_pv_matches_bestmove']
   r.write(target/p.name,raw)
 assert stats['queries']==7680
 r.write(r.TASK/'normalization.json',{'result':'PASS','counts':dict(stats),'source_sha256':sources,'script_sha256':r.sha(Path(__file__)),'semantics':'Raw files unchanged. score/info/depth refer to last completed unbounded iteration, not necessarily the selected move after node-limit interruption. pv is the last reported legal variation starting with actual bestmove; last_exact_pv is separately retained. nodes/time use final reported totals. Final bounds remain bounds and are never converted to exact values.'})
 print(json.dumps(dict(stats)))
if __name__=='__main__':main()
