"""Bounded diagnostic queries with cold TT, complete history and durable results."""
import concurrent.futures,hashlib,json,os,re,subprocess,sys,threading,time,traceback
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];TASK=ROOT/'.local/pr18-diagnosis-v1'
sys.path.insert(0,str(ROOT/'tests/petrified'))
from protocol import Uci
CHAMP=json.loads((ROOT/'docs/petrified/points-champion.json').read_text())
NET=ROOT/CHAMP['network'];ORIGINAL=ROOT/CHAMP['binary']
RELEASE=TASK/'build-release/petrified-stockfish.exe';DEBUG=TASK/'build-debug/petrified-stockfish.exe'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,d):
 tmp=p.with_suffix(p.suffix+'.tmp');tmp.write_text(json.dumps(d,ensure_ascii=True,separators=(',',':'))+'\n',encoding='utf-8')
 for attempt in range(20):
  try:os.replace(tmp,p);return
  except PermissionError:
   if attempt==19:raise
   time.sleep(.05)
def start(exe,prefix):
 e=Uci(exe,network=NET,timeout=90,log_prefix=TASK/prefix)
 e.send('setoption name UCI_Variant value petrifiedpoints');e.send('setoption name Hash value 128');e.ready();return e
def position(e,row):e.send('position startpos moves '+' '.join(row['history']))
def extract(e,row):
 position(e,row);e.send('diagnose');lines=e.until(lambda s:s.startswith('diagnostic '));d=json.loads(lines[-1][11:]);assert d['fen']==row['fen'],row['id'];return d
def search(e,row,budget,mode='baseline'):
 if mode!='original':
  e.send('setoption name DiagnosticReduced value '+str(mode=='reduced').lower())
  e.send('setoption name DiagnosticScale value '+str({'sym_scale':1,'no_clock':2}.get(mode,0)))
 e.send('ucinewgame');e.ready();position(e,row);tick=time.monotonic();e.send('go '+budget)
 lines=e.until(lambda s:s.startswith('bestmove '));elapsed=time.monotonic()-tick
 infos=[s for s in lines if s.startswith('info depth ') and ' score ' in s and 'lowerbound' not in s and 'upperbound' not in s]
 last=infos[-1] if infos else '';score=re.search(r'\bscore (cp|mate) (-?\d+)',last)
 action=next((s.rsplit(' ',1)[-1] for s in lines if s.startswith('info string points action ')),None)
 def num(k):
  m=re.search(r'\b'+k+r' (\d+)',last);return int(m[1]) if m else None
 rowout={'mode':mode,'budget':budget,'bestmove':lines[-1].split()[1],'action':action,'score_kind':score[1] if score else None,'score':int(score[2]) if score else None,'depth':num('depth'),'nodes':num('nodes'),'reported_ms':num('time'),'seconds':elapsed,'pv':last.split(' pv ',1)[1].split() if ' pv ' in last else [],'info':last,'output':lines}
 assert score or action,lines
 return rowout
def signature(x):return {k:x[k] for k in ['bestmove','action','score_kind','score','depth','nodes','pv']}
def freeze():
 paths=[NET,ORIGINAL,RELEASE,DEBUG,TASK/'bank.json',TASK/'histories.json',TASK/'selection-manifest.json']
 paths+=list(Path(__file__).parent.glob('*.*'))+[ROOT/'tests/petrified/protocol.py',ROOT/'tests/petrified/chesscom_reference.cjs',ROOT/'.local/points-repetition-win-v1-inputs/variants.js',ROOT/'.local/fairyground/package-lock.json']
 paths+=list((TASK/'instrumentation').glob('*.py'))
 paths+=[p for p in (TASK/'source/src').rglob('*') if p.is_file()]
 write(TASK/'execution-manifest.json',{'files':{str(p.relative_to(ROOT)):sha(p) for p in paths},'scope':'Diagnostic only; fixed bank and controls; no training or match','threads':1,'hash_mb':128,'concurrency':4})
def check():
 for p,h in json.loads((TASK/'execution-manifest.json').read_text())['files'].items():assert sha(ROOT/p)==h,p
def pilot(bank):
 rows=sorted(bank,key=lambda r:hashlib.sha256(r['id'].encode()).hexdigest())[:8];evidence=[]
 with start(ORIGINAL,'pilot-original') as original,start(RELEASE,'pilot-release') as release,start(DEBUG,'pilot-debug') as debug:
  for row in rows:
   a=extract(release,row);b=extract(debug,row);assert a==b,('debug extraction',row['id'])
   position(original,row);original.send('eval');lines=original.ready();value=int(next(s.split(': ')[1] for s in lines if s.startswith('Legacy final:')))
   assert value==a['root']['value'],('static parity',row['id'],value,a['root'])
   x=search(original,row,'nodes 32768','original');y=search(release,row,'nodes 32768');assert signature(x)==signature(y),('search parity',row['id'],signature(x),signature(y))
   evidence.append({'id':row['id'],'native_original':x,'instrumented':y,'debug_release_extraction_equal':True,'original_static':value})
 write(TASK/'pilot.json',{'result':'PASS','positions':len(rows),'queries':evidence})
 print('PILOT_PASS',len(rows),flush=True)
def phase(name,bank):
 out=TASK/name;out.mkdir(exist_ok=False);stop=threading.Event();done=0;lock=threading.Lock()
 def worker(index,rows):
  nonlocal done
  exe=ORIGINAL if name=='baseline' else RELEASE
  with start(exe,f'{name}-worker-{index}') as e:
   for row in rows:
    if stop.is_set() or (TASK/'STOP').exists():raise InterruptedError('Diagnosis STOP')
    if name=='extract':result={'id':row['id'],'data':extract(e,row)}
    else:
     work=[('original','nodes '+str(n)) for n in [4096,32768,262144]] if name=='baseline' else [('baseline','depth 4'),('reduced','depth 4'),('reduced','nodes 32768'),('reduced','nodes 262144'),('sym_scale','nodes 32768'),('no_clock','nodes 32768')]
     result={'id':row['id'],'searches':[search(e,row,budget,mode) for mode,budget in work]}
    write(out/(row['id']+'.json'),result)
    with lock:
     done+=1
     if done%64==0 or done==len(bank):
      write(TASK/'progress.json',{'phase':name,'completed':done,'total':len(bank),'updated_epoch':time.time()});print(name,done,'/',len(bank),flush=True)
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
  jobs=[pool.submit(worker,i,bank[i::4]) for i in range(4)]
  try:
   for j in concurrent.futures.as_completed(jobs):j.result()
  except BaseException:stop.set();raise
 check()
def audit(name):
 args=['node',str(Path(__file__).parent/'audit.cjs'),str(ROOT),name]
 with (TASK/f'{name}-audit.stdout.log').open('x') as log:
  p=subprocess.Popen(args,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW);rc=p.wait(timeout=3600)
 assert rc==0,(name,'Independent audit failed',rc)
 print(name,'INDEPENDENT_PASS',flush=True)
def main():
 os.environ['PATH']=r'C:\msys64\mingw64\bin;'+os.environ['PATH']
 bank=json.loads((TASK/'bank.json').read_text());freeze();check()
 try:
  pilot(bank);phase('extract',bank);audit('extract')
  phase('baseline',bank);phase('controls',[r for r in bank if r['controls']]);audit('searches');check()
  write(TASK/'complete.json',{'result':'PASS','histories':256,'positions':2048,'control_positions':256,'completed_epoch':time.time(),'new_games':0,'training':False})
  print('DIAGNOSIS_QUERIES_COMPLETE',flush=True)
 except BaseException as ex:
  write(TASK/'failure.json',{'error':repr(ex),'traceback':traceback.format_exc()});raise
if __name__=='__main__':main()
