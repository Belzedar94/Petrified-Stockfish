"""Audit already completed queries; preserve the failed PV association check."""
import json,os,subprocess,time,traceback
from pathlib import Path
import run as r
r.check()
paths=[Path(__file__),Path(__file__).parent/'audit_005.cjs',Path(__file__).parent/'normalize.py',r.TASK/'normalization.json']
paths+=list((r.TASK/'normalized-baseline').glob('*.json'))+list((r.TASK/'normalized-controls').glob('*.json'))
manifest={'original_manifest_sha256':r.sha(r.TASK/'execution-manifest.json'),'overlay':{str(p.relative_to(r.ROOT)):r.sha(p) for p in paths},'reason':'188 raw last-unbounded-iteration PVs differ from final bestmove after a bounded interrupted iteration. Audit the actual selected PV and the older unbounded PV separately. Preserve bounds; do not rerun engines or alter original outputs.'}
r.write(r.TASK/'continuation-005-manifest.json',manifest)
try:
 with (r.TASK/'searches-audit-005.stdout.log').open('x') as log:
  p=subprocess.Popen(['node',str(Path(__file__).parent/'audit_005.cjs'),str(r.ROOT),'searches'],stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW);rc=p.wait(timeout=3600)
 assert rc==0,('Independent search audit failed',rc)
 r.check()
 for p,h in manifest['overlay'].items():assert r.sha(r.ROOT/p)==h,p
 r.write(r.TASK/'complete.json',{'result':'PASS','histories':256,'positions':2048,'control_positions':256,'queries':7680,'completed_epoch':time.time(),'new_games':0,'training':False,'extract_audit':'extract-004-audit.json','search_audit':'searches-005-audit.json','original_audit_failures_preserved':True})
 print('DIAGNOSIS_QUERIES_COMPLETE',flush=True)
except BaseException as ex:
 r.write(r.TASK/'failure-005.json',{'error':repr(ex),'traceback':traceback.format_exc()});raise
