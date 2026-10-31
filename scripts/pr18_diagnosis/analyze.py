"""Descriptive, paired diagnosis. No fitting, model training or engine execution."""
import collections,csv,hashlib,json,math,random,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];TASK=ROOT/'.local/pr18-diagnosis-v1'
def load(p):return json.loads(Path(p).read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def mean(x):return statistics.mean(x) if x else None
def quantiles(x):
 if not x:return None
 x=sorted(x);return {'n':len(x),'mean':mean(x),'median':statistics.median(x),'p90':x[round(.9*(len(x)-1))],'p95':x[round(.95*(len(x)-1))],'max':max(x)}
def prob(value):return 1/(1+math.exp(-max(-60,min(60,value/600))))
def qp(q):
 if q['action']:return 1 if q['action']=='claim-win' else .5
 if q['score_kind']=='mate':return 1 if q['score']>0 else 0
 return prob(q['score']*2.08)
def ci(pairs):
 groups=collections.defaultdict(list)
 for hid,v in pairs:groups[hid].append(v)
 values=[mean(v) for v in groups.values()]
 if not values:return None
 rng=random.Random(20261031);n=len(values)
 draws=sorted(mean(rng.choices(values,k=n)) for _ in range(2000))
 return {'histories':n,'rows':len(pairs),'history_weighted_mean':mean(values),'bootstrap95':[draws[49],draws[1949]],'resamples':2000}
def brier(p,y):return (p-y)**2

def final_interval(q,y):
 if q['action']:p=qp(q)
 elif q['final_score_kind']=='mate':p=float(q['final_score']>0)
 else:p=prob(q['final_score']*2.08)
 lo=0. if q['final_bound']=='upper' else p
 hi=1. if q['final_bound']=='lower' else p
 return (0. if lo<=y<=hi else min((lo-y)**2,(hi-y)**2),max((lo-y)**2,(hi-y)**2))

def interval_summary(rows,key):
 losses=[final_interval(key(r),r['label']) for r in rows]
 return {'positions':len(rows),'mean_brier_lower':mean([x[0] for x in losses]),'mean_brier_upper':mean([x[1] for x in losses]),'bounded_queries':sum(key(r)['final_bound'] is not None for r in rows)}

def signed_result(row):return .5 if row['result']=='1/2-1/2' else float((row['result']=='1-0')==(row['fen'].split()[1]=='w'))
def main():
 assert load(TASK/'complete.json')['result']=='PASS'
 bank=load(TASK/'bank.json');records=[];exact=[]
 for row in bank:
  d=load(TASK/'extract'/(row['id']+'.json'))['data'];qs=load(TASK/'normalized-baseline'/(row['id']+'.json'))['searches']
  by={q['budget']:q for q in qs};lo,mid,hi=[by['nodes '+str(n)] for n in [4096,32768,262144]]
  y=signed_result(row);static=prob(d['root']['value']);children={c['move']:c for c in d['children']}
  def child_value(c):
   if c['terminal']:
    if c['result']=='1/2-1/2':return 0
    return 32000 if (c['result']=='1-0')==(row['fen'].split()[1]=='w') else -32000
   return -max(c['evaluation']['value'],c['claim'])
  eligible=[c for c in d['children'] if c['terminal'] or not c['evaluation']['in_check']]
  stat_best=max((child_value(c) for c in eligible),default=None)
  stat_moves=[c['move'] for c in eligible if child_value(c)==stat_best]
  deep_child=children.get(hi['bestmove']);deep_eligible=deep_child in eligible if deep_child else False
  winmoves=[c['move'] for c in d['children'] if c['terminal'] and child_value(c)>0]
  loses_immediately=bool(deep_child and deep_child['terminal'] and child_value(deep_child)<0)
  r={'id':row['id'],'history_id':row['history_id'],'corpus':row['corpus'],'stratum':row['stratum'],'slot':row['slot'],'features':row['features'],'label':y,'legal':len(children),'claim':d['claim'],'forced':len(children)==1,'root':d['root'],'mirror':d['mirror'],'static_p':static,'low':lo,'mid':mid,'high':hi,'low_p':qp(lo),'mid_p':qp(mid),'high_p':qp(hi),'static_best_moves':stat_moves,'static_deep_eligible':deep_eligible,'static_agrees_deep':hi['bestmove'] in stat_moves if deep_eligible else None,'static_gap_internal':stat_best-child_value(deep_child) if deep_eligible else None,'immediate_wins':winmoves,'deep_immediate_loss':loses_immediately}
  if row['controls']:
   controls=load(TASK/'normalized-controls'/(row['id']+'.json'))['searches'];r['controls']={q['mode']+' '+q['budget']:q for q in controls}
  if winmoves or loses_immediately:exact.append({k:r[k] for k in ['id','immediate_wins','deep_immediate_loss']}|{'high_bestmove':hi['bestmove'],'high_score':hi['info']})
  records.append(r)
 def summary(rows):
  choice=[r for r in rows if r['legal']>1 and r['claim']<0]
  return {'positions':len(rows),'choice_positions':len(choice),'forced':sum(r['forced'] for r in rows),'claim_positions':sum(r['claim']>=0 for r in rows),'static_brier':mean([brier(r['static_p'],r['label']) for r in rows]),'low_brier':mean([brier(r['low_p'],r['label']) for r in rows]),'mid_brier':mean([brier(r['mid_p'],r['label']) for r in rows]),'high_brier':mean([brier(r['high_p'],r['label']) for r in rows]),'low_mid_move_change':sum(r['low']['bestmove']!=r['mid']['bestmove'] for r in choice),'mid_high_move_change':sum(r['mid']['bestmove']!=r['high']['bestmove'] for r in choice),'all_three_agree':sum(r['low']['bestmove']==r['mid']['bestmove']==r['high']['bestmove'] for r in choice),'mid_high_probability_shift':quantiles([abs(r['mid_p']-r['high_p']) for r in choice]),'static_vs_deep_eligible':sum(r['static_deep_eligible'] for r in choice),'static_vs_deep_agree':sum(r['static_agrees_deep'] is True for r in choice),'static_gap_internal':quantiles([r['static_gap_internal'] for r in choice if r['static_gap_internal'] is not None]),'mean_depth':[mean([r[k]['depth'] for r in rows if r[k]['depth'] is not None]) for k in ['low','mid','high']]}
 controls=[r for r in records if 'controls' in r];control_summary={}
 for mode in ['reduced nodes 32768','reduced nodes 262144','sym_scale nodes 32768','no_clock nodes 32768']:
  pairs=[];moves=0;probshifts=[];stable=[]
  for r in controls:
   q=r['controls'][mode];base=r['high'] if '262144' in mode else r['mid'];p=qp(q);moves+=q['bestmove']!=base['bestmove'];probshifts.append(abs(p-qp(base)))
   pairs.append((r['history_id'],brier(qp(base),r['label'])-brier(p,r['label'])))
   high=r['high'];stable.append(q['bestmove']==high['bestmove'])
  control_summary[mode]={'positions':len(controls),'move_changes_vs_same_budget_baseline':moves,'probability_shifts':quantiles(probshifts),'source_policy_brier_improvement_vs_baseline':ci(pairs),'agreements_with_baseline_high':sum(stable),'mean_depth':mean([r['controls'][mode]['depth'] for r in controls if r['controls'][mode]['depth'] is not None]),'per_stratum':{s:{'n':len(rr:=[r for r in controls if r['stratum']==s]),'changes':sum(r['controls'][mode]['bestmove']!=(r['high'] if '262144' in mode else r['mid'])['bestmove'] for r in rr)} for s in sorted({r['stratum'] for r in controls})}}
 depth=[]
 for r in controls:
  a=r['controls']['baseline depth 4'];b=r['controls']['reduced depth 4'];depth.append({'id':r['id'],'changed':a['bestmove']!=b['bestmove'],'nodes_ratio':b['nodes']/max(1,a['nodes']) if a['nodes'] is not None and b['nodes'] is not None else None,'score_shift':abs(qp(a)-qp(b)),'baseline_nodes':a['nodes'],'reduced_nodes':b['nodes']})
 converged=[r for r in controls if r['high']['bestmove']==r['controls']['reduced nodes 262144']['bestmove'] and abs(r['high_p']-qp(r['controls']['reduced nodes 262144']))<=.1 and r['legal']>1 and r['claim']<0]
 out={'schema':'pr18-diagnosis-results-v1','source_policy_labels_are_not_minimax':True,'probability_proxy':'sigmoid(internal value / 600); existing training temperature, not recalibrated','sampling_is_stratified_not_population_prevalence':True,'search_value_semantics':'Last completed unbounded iteration is the position estimate; final bestmove may follow an incomplete bounded iteration. No bounds are treated as exact. Sensitivity below keeps only pairs with unbounded final iterations.','overall':summary(records),'by_stratum':{s:summary([r for r in records if r['stratum']==s]) for s in sorted({r['stratum'] for r in records})},'by_corpus':{s:summary([r for r in records if r['corpus']==s]) for s in sorted({r['corpus'] for r in records})},'symmetry':{'raw_unadjusted_differences':sum(r['root']['raw_unadjusted']!=r['mirror']['raw_unadjusted'] for r in records),'raw_adjusted_differences':sum(r['root']['raw']!=r['mirror']['raw'] for r in records),'final_differences':sum(r['root']['value']!=r['mirror']['value'] for r in records),'absolute_final_internal':quantiles([abs(r['root']['value']-r['mirror']['value']) for r in records]),'absolute_final_cp':quantiles([abs(r['root']['value']-r['mirror']['value'])/2.08 for r in records])},'paired_source_policy_brier_improvements':{name:ci([(r['history_id'],brier(r[a],r['label'])-brier(r[b],r['label'])) for r in records]) for name,a,b in [('static_to_high','static_p','high_p'),('low_to_high','low_p','high_p'),('mid_to_high','mid_p','high_p')]},'controls':control_summary,'fixed_depth4':{'positions':len(depth),'changed':sum(x['changed'] for x in depth),'node_ratio':quantiles([x['nodes_ratio'] for x in depth if x['nodes_ratio'] is not None]),'probability_shift':quantiles([x['score_shift'] for x in depth])},'deep_agreement_subset':summary(converged),'exact_immediate':{'positions_with_win':len([r for r in records if r['immediate_wins']]),'high_not_choosing_available_immediate_win':len([r for r in records if r['immediate_wins'] and r['high']['bestmove'] not in r['immediate_wins'] and r['high']['action']!='claim-win']),'high_chooses_immediate_loss':sum(r['deep_immediate_loss'] for r in records),'cases':exact},'runtime':{'queries':sum(3+len(r.get('controls',{})) for r in records),'engine_seconds_sum':sum(sum(q['seconds'] for q in [r['low'],r['mid'],r['high'],*r.get('controls',{}).values()]) for r in records),'nodes_sum':sum(sum(q['nodes'] or 0 for q in [r['low'],r['mid'],r['high'],*r.get('controls',{}).values()]) for r in records)}}
 cases=sorted(records,key=lambda r:abs(r['high_p']-r['static_p']),reverse=True)[:20]
 out['largest_static_deep_disagreements']=[{'id':r['id'],'history_id':r['history_id'],'stratum':r['stratum'],'features':r['features'],'static_internal':r['root']['value'],'source_result':r['label'],'low':r['low']['info'],'mid':r['mid']['info'],'high':r['high']['info'],'static_best_moves':r['static_best_moves']} for r in cases]
 out['in_check_positions']=sum(r['root']['in_check'] for r in records)
 out['static_noncheck_subset']=summary([r for r in records if not r['root']['in_check']])
 out['by_state_regime']={name:summary([r for r in records if pred(r)]) for name,pred in [('clock80',lambda r:r['features']['clock']>=80),('walls8',lambda r:r['features']['walls']>=8),('live6',lambda r:r['features']['live']<=6),('lead15',lambda r:abs(r['features']['lead'])>=15)]}
 out['final_unbounded_sensitivity']={name:ci([(r['history_id'],brier(qp(a),r['label'])-brier(qp(b),r['label'])) for r in controls if not (a:=r['high'] if '262144' in name else r['mid'])['final_bound'] and not (b:=r['controls'][name])['final_bound']]) for name in control_summary}
 out['final_unbounded_sensitivity']['mid_to_high']=ci([(r['history_id'],brier(r['mid_p'],r['label'])-brier(r['high_p'],r['label'])) for r in records if not r['mid']['final_bound'] and not r['high']['final_bound']])
 out['static_component_counterfactual']={name:{'absolute_internal_change':quantiles([abs(r['root'][field]-r['root']['value']) for r in records]),'paired_source_policy_brier_improvement':ci([(r['history_id'],brier(r['static_p'],r['label'])-brier(prob(r['root'][field]),r['label'])) for r in records])} for name,field in [('symmetric_outer_scale','symmetric_value'),('no_outer_clock','unattenuated_value')]}
 out['final_bound_loss_intervals']={k:interval_summary(records,lambda r:r[k]) for k in ['low','mid','high']}
 out['final_bound_loss_intervals']['controls']={k:interval_summary(controls,lambda r:r['controls'][k]) for k in control_summary}
 out['deep_control_baseline']={k:summary(controls)[k] for k in ['positions','mid_brier','high_brier','mean_depth']}
 out['depth_distribution']={k:quantiles([r[k]['depth'] for r in records]) for k in ['low','mid','high']}
 out['raw_normalization_counts']=load(TASK/'normalization.json')['counts']
 files=[TASK/'bank.json',TASK/'histories.json',TASK/'selection-manifest.json',TASK/'execution-manifest.json',TASK/'pilot.json',TASK/'extract-004-audit.json',TASK/'searches-005-audit.json',Path(__file__)]
 out['inputs']={str(p.relative_to(ROOT)):sha(p) for p in files}
 (TASK/'analysis.json').write_text(json.dumps(out,indent=2,ensure_ascii=True)+'\n',encoding='utf-8')
 with (TASK/'decisions.csv').open('w',newline='',encoding='utf-8') as f:
  cols=['id','history_id','corpus','stratum','slot','legal','claim','label','static_p','low_p','mid_p','high_p','static_agrees_deep','static_gap_internal'];w=csv.DictWriter(f,fieldnames=cols);w.writeheader();w.writerows({k:r[k] for k in cols} for r in records)
 print(json.dumps({k:v for k,v in out.items() if k not in ['by_stratum','by_corpus','largest_static_deep_disagreements','exact_immediate','inputs']},indent=2))
if __name__=='__main__':main()
