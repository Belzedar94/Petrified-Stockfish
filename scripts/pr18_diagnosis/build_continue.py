"""Preserved first failure; compile corrected diagnostic extraction only."""
import json,os,shutil,subprocess,time
from pathlib import Path
from build import ROOT,TASK,SOURCE,sha
shutil.copy2(Path(__file__).parent/'diagnostic.cpp',SOURCE/'src/diagnostic.cpp')
env=dict(os.environ,PATH=r'C:\msys64\mingw64\bin;'+os.environ['PATH'])
cmake=r'C:\Program Files\CMake\bin\cmake.exe';stages=[]
for kind in ['Release','Debug']:
 build=TASK/('build-'+kind.lower())
 cmds=[]
 if kind=='Debug':cmds.append(('configure',[cmake,'-S',str(SOURCE),'-B',str(build),'-G','Ninja','-DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe','-DCMAKE_MAKE_PROGRAM=C:/msys64/mingw64/bin/ninja.exe','-DCMAKE_BUILD_TYPE='+kind,'-DPETRIFIED_BMI2=ON','-DPETRIFIED_PYTHON=OFF']))
 cmds.append(('build',[cmake,'--build',str(build),'-j2','--target','petrified-stockfish']))
 for name,args in cmds:
  with (TASK/f'{kind}-{name}-002.log').open('x') as log:
   tick=time.monotonic();p=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW);rc=p.wait(timeout=1200)
  stages.append({'kind':kind,'name':name,'exit_code':rc,'seconds':time.monotonic()-tick,'identity':p._journal_identity})
  (TASK/'build-state-002.json').write_text(json.dumps(stages,indent=2))
  print(kind,name,rc,flush=True)
  if rc:raise RuntimeError('Build failed; log preserved')
 print(kind,'sha256',sha(build/'petrified-stockfish.exe'),flush=True)
(TASK/'instrumentation-source-hashes.json').write_text(json.dumps({str(p.relative_to(SOURCE)):sha(p) for p in SOURCE.rglob('*') if p.is_file() and (p.parts[-2]=='src' or p.name=='CMakeLists.txt')},indent=2))
