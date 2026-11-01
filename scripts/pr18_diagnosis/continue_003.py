"""Resume after correcting only the independent adapter's dead-wall encoding."""
import json,os,subprocess,time,traceback
from pathlib import Path
import run as r
os.environ['PATH']=r'C:\msys64\mingw64\bin;'+os.environ['PATH']
r.check()
manifest={'original_manifest_sha256':r.sha(r.TASK/'execution-manifest.json'),'bank_unchanged':r.sha(r.TASK/'bank.json'),'overlay':{str(p.relative_to(r.ROOT)):r.sha(p) for p in [Path(__file__),Path(__file__).parent/'audit_003.cjs']},'reason':'History walls may be dead colored pieces. After settlement, the client marks the eliminated side dead and advances turn; native retains live piece annotations. Terminal placement, clock and result are checked, without excluding terminals. No engine, bank or budget changes.'}
r.write(r.TASK/'continuation-003-manifest.json',manifest)
def audit(phase):
 with (r.TASK/f'{phase}-audit-003.stdout.log').open('x') as log:
  p=subprocess.Popen(['node',str(Path(__file__).parent/'audit_003.cjs'),str(r.ROOT),phase],stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW);rc=p.wait(timeout=3600)
 assert rc==0,(phase,'Independent audit failed',rc)
 print(phase,'INDEPENDENT_PASS',flush=True)
try:
 bank=json.loads((r.TASK/'bank.json').read_text());audit('extract')
 r.phase('baseline',bank);r.phase('controls',[x for x in bank if x['controls']]);audit('searches');r.check()
 for p,h in manifest['overlay'].items():assert r.sha(r.ROOT/p)==h
 r.write(r.TASK/'complete.json',{'result':'PASS','histories':256,'positions':2048,'control_positions':256,'completed_epoch':time.time(),'new_games':0,'training':False,'original_audit_failure_preserved':True})
 print('DIAGNOSIS_QUERIES_COMPLETE',flush=True)
except BaseException as ex:
 r.write(r.TASK/'failure-003.json',{'error':repr(ex),'traceback':traceback.format_exc()});raise
