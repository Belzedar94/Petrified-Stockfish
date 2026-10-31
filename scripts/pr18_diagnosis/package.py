"""Preserve the completed diagnostic and verify every archive member."""
import hashlib,json,os,zipfile
from pathlib import Path
import run as r

def main():
 r.check();assert json.loads((r.TASK/'verification.json').read_text())['result']=='PASS'
 archive=r.TASK/'pr18-diagnosis-v1-evidence.zip';assert not archive.exists()
 files={str(p.relative_to(r.ROOT)).replace('\\','/'):p for p in r.TASK.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.tmp'}
 for p in Path(__file__).parent.glob('*.*'):
  if p.is_file():files[str(p.relative_to(r.ROOT)).replace('\\','/')]=p
 for name in ['docs/petrified/pr18-diagnosis-v1.md','docs/petrified/pr18-diagnosis-results-2026-10-31.md','docs/petrified/pr18-diagnosis-results-2026-10-31.json','docs/petrified/points-champion.json','AGENTS.md','docs/petrified/STATUS.md']:
  files[name]=r.ROOT/name
 # Include the actual frozen executables/network/reference inputs, not only hashes.
 for name in json.loads((r.TASK/'execution-manifest.json').read_text())['files']:
  p=r.ROOT/name;files[str(p.relative_to(r.ROOT)).replace('\\','/')]=p
 entries=[{'path':name,'bytes':p.stat().st_size,'sha256':r.sha(p)} for name,p in sorted(files.items())]
 inventory=r.TASK/'inventory.json';r.write(inventory,{'files':entries,'count':len(entries),'bytes':sum(x['bytes'] for x in entries),'exclusions':['Python bytecode caches and temporary files; none are scientific evidence'],'closure':'Final packaging process exit proof is in adjacent closure.json, created after this packager exits.'})
 files[str(inventory.relative_to(r.ROOT)).replace('\\','/')]=inventory
 expected={e['path']:e['sha256'] for e in entries};expected[str(inventory.relative_to(r.ROOT)).replace('\\','/')]=r.sha(inventory)
 with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for name,p in sorted(files.items()):z.write(p,name)
 with zipfile.ZipFile(archive) as z:
  assert set(z.namelist())==set(expected) and len(z.namelist())==len(expected)
  for name,h in expected.items():assert hashlib.sha256(z.read(name)).hexdigest()==h,name
  assert z.testzip() is None
 report={'result':'PASS','archive':str(archive.relative_to(r.ROOT)),'sha256':r.sha(archive),'bytes':archive.stat().st_size,'entries':len(expected),'all_entries_hash_verified':True,'inventory_sha256':r.sha(inventory),'registered_packager_pid':os.getpid(),'exit_proof':'Pending caller observation and final strict identity audit in closure.json'}
 r.write(r.TASK/'archive-verification.json',report);print(json.dumps(report),flush=True)
if __name__=='__main__':main()
