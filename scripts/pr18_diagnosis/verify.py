"""Reconcile saved queries, independent audits, manifests and analysis totals."""
import collections,json,re
from pathlib import Path
import run as r

def load(p):return json.loads(p.read_text())
def main():
 r.check();bank=load(r.TASK/'bank.json');histories=load(r.TASK/'histories.json')
 assert len(bank)==2048 and len(histories)==256
 assert set(collections.Counter(x['history_id'] for x in bank).values())=={8}
 assert len({x['family'] for x in histories})==256
 assert sum(x['controls'] for x in bank)==256
 for name in ['continuation-004-manifest.json','continuation-005-manifest.json']:
  for p,h in load(r.TASK/name)['overlay'].items():assert r.sha(r.ROOT/p)==h,p
 normalization=load(r.TASK/'normalization.json')
 for p,h in normalization['source_sha256'].items():assert r.sha(r.ROOT/p)==h,p
 expected=[('original','nodes '+str(n)) for n in [4096,32768,262144]]
 control_expected=[('baseline','depth 4'),('reduced','depth 4'),('reduced','nodes 32768'),('reduced','nodes 262144'),('sym_scale','nodes 32768'),('no_clock','nodes 32768')]
 count=nodes=children=terminals=0;unavoidable=[]
 for row in bank:
  d=load(r.TASK/'extract'/(row['id']+'.json'))['data'];assert d['fen']==row['fen'] and not d['terminal']
  children+=len(d['children']);terminals+=sum(c['terminal'] for c in d['children'])
  for folder,modes in [('baseline',expected)]+([('controls',control_expected)] if row['controls'] else []):
   raw=load(r.TASK/folder/(row['id']+'.json'))['searches'];new=load(r.TASK/('normalized-'+folder)/(row['id']+'.json'))['searches']
   assert [(q['mode'],q['budget']) for q in raw]==modes
   assert len(new)==len(raw)
   for a,b in zip(raw,new):
    assert a['output']==b['output'] and a['bestmove']==b['bestmove']==a['output'][-1].split()[1]
    for key in ['score','score_kind','info','depth','action','seconds']:assert a[key]==b[key]
    assert b['last_exact_pv']==a['pv']
    if b['bestmove']!='0000':assert b['pv'][0]==b['bestmove']
    count+=1;nodes+=b['nodes'] or 0
  q=load(r.TASK/'normalized-baseline'/(row['id']+'.json'))['searches'][-1]
  chosen=next((c for c in d['children'] if c['move']==q['bestmove']),None)
  winner='1-0' if row['fen'].split()[1]=='w' else '0-1'
  if chosen and chosen['terminal'] and chosen['result'] not in [winner,'1/2-1/2']:
   assert all(c['terminal'] and c['result']==chosen['result'] for c in d['children'])
   unavoidable.append({'id':row['id'],'legal_children':len(d['children']),'result':chosen['result'],'reason':chosen['reason']})
 a=load(r.TASK/'analysis.json');ea=load(r.TASK/'extract-004-audit.json');sa=load(r.TASK/'searches-005-audit.json')
 assert count==a['runtime']['queries']==sa['searches']==7680
 assert nodes==a['runtime']['nodes_sum']
 assert children==ea['children']==51417 and terminals==ea['terminalChildren']==170
 assert ea['result']==sa['result']=='PASS' and ea['terminal_exclusions']==sa['terminal_exclusions']==0
 for phase,version,audit in [('extract','004',ea),('searches','005',sa)]:assert r.sha(r.TASK/f'{phase}-{version}-independent.jsonl')==audit['log_sha256']
 # All rows have equal family weight; reconcile the pooled and paired delta independently.
 for key in ['static','low','mid']:
  delta=a['overall'][key+'_brier']-a['overall']['high_brier']
  assert abs(delta-a['paired_source_policy_brier_improvements'][key+'_to_high']['history_weighted_mean'])<1e-12
 result={'result':'PASS','histories':256,'positions':2048,'queries':count,'nodes':nodes,'legal_children':children,'terminal_children':terminals,'independent_pv_plies':sa['pvPlies'],'unavoidable_immediate_losses':unavoidable,'analysis_sha256':r.sha(r.TASK/'analysis.json'),'verify_script_sha256':r.sha(Path(__file__)),'frozen_input_mutations':0,'new_engine_queries':0}
 r.write(r.TASK/'verification.json',result);print(json.dumps(result))
if __name__=='__main__':main()
