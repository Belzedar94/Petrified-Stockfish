"""Build isolated PR18 instrumentation; no candidate or source-tree mutation."""
import difflib,hashlib,json,os,shutil,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];TASK=ROOT/'.local/pr18-diagnosis-v1';SOURCE=TASK/'source'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def patch(name,pairs):
 p=SOURCE/name;s=p.read_text();old=s
 for a,b in pairs:
  assert s.count(a)==1,(name,a,s.count(a));s=s.replace(a,b)
 p.write_text(s,encoding='utf-8')
 return ''.join(difflib.unified_diff(old.splitlines(True),s.splitlines(True),'a/'+name,'b/'+name))
def main():
 diff=''
 for name in ['diagnostic.cpp','diagnostic.h']:shutil.copy2(Path(__file__).parent/name,SOURCE/'src'/name)
 diff+=patch('CMakeLists.txt',[('src/benchmark.cpp src/evaluate.cpp','src/diagnostic.cpp src/benchmark.cpp src/evaluate.cpp')])
 diff+=patch('src/engine.h',[('void trace_eval() const;','void trace_eval() const;\n    void diagnose();')])
 diff+=patch('src/engine.cpp',[('#include "engine.h"','#include "engine.h"\n#include "diagnostic.h"'),('    options.add("UCI_Chess960", Option(false));','    options.add("UCI_Chess960", Option(false));\n    options.add("DiagnosticReduced", Option(false, [](const Option& o) { Diagnostic::reduced=bool(o); return std::nullopt; }));\n    options.add("DiagnosticScale", Option(0,0,2, [](const Option& o) { Diagnostic::scale=int(o); return std::nullopt; }));')])
 diff+=patch('src/uci.cpp',[('else if (token == "eval")\n            engine.trace_eval();','else if (token == "eval")\n            engine.trace_eval();\n        else if (token == "diagnose")\n            engine.diagnose();')])
 diff+=patch('src/evaluate.cpp',[('#include "evaluate.h"','#include "evaluate.h"\n#include "diagnostic.h"'),('const int material=Eval::NNUE::legacy_material(pos,WHITE)+Eval::NNUE::legacy_material(pos,BLACK);\n    const int scale=', 'const int material=Eval::NNUE::legacy_material(pos,WHITE)+Eval::NNUE::legacy_material(pos,BLACK)\n      + (pos.game_of_points() && Diagnostic::scale==1 ? 107*(pos.count<PAWN>(BLACK)-pos.count<PAWN>(WHITE)) : 0);\n    const int scale='),('v=v*(100-std::min(pos.rule50_count(),100))/100;', 'if (!(pos.game_of_points() && Diagnostic::scale==2))\n        v=v*(100-std::min(pos.rule50_count(),100))/100;')])
 guards=['if (priorReduction >= 3 && !opponentWorsening)','if (priorReduction >= 2 && depth >= 2 && ss->staticEval + (ss - 1)->staticEval > 166)','if (allNode && eval < alpha - 342 * depth && !seekMate)','if (!ss->ttPv && depth < (seekMate ? 6 : 19) && eval >= beta && (!ttData.move || ttCapture)','if (cutNode\n        && (!pointsNmp','if (!ss->followPV && !allNode && depth >= 6 && !ttData.move)','if (depth >= 3 && !is_decisive(beta) && !(is_valid(ttData.value) && ttData.value < probCutBeta))','if ((ttData.bound & BOUND_LOWER) && ttData.depth >= depth - 4 && ttData.value >= probCutBeta','if (!rootNode && pos.non_pawn_material(us) && !is_loss(bestValue))','if (!rootNode && move == ttData.move && !excludedMove && depth >= 6 + ss->ttPv','if (depth >= 2 && moveCount > 1)']
 pairs=[('#include "search.h"','#include "search.h"\n#include "diagnostic.h"')]+[(a,a.replace('if (','if (!Diagnostic::reduced && ',1)) for a in guards]
 pairs += [('newDepth - (r > 5234) - (r > 5487 && newDepth > 2), !cutNode);','Diagnostic::reduced ? newDepth : newDepth - (r > 5234) - (r > 5487 && newDepth > 2), !cutNode);')]
 diff+=patch('src/search.cpp',pairs)
 (TASK/'instrumentation.patch').write_text(diff,encoding='utf-8')
 env=dict(os.environ,PATH=r'C:\msys64\mingw64\bin;'+os.environ['PATH'])
 cmake=r'C:\Program Files\CMake\bin\cmake.exe'
 stages=[]
 for kind in ['Release','Debug']:
  build=TASK/('build-'+kind.lower())
  commands=[('configure',[cmake,'-S',str(SOURCE),'-B',str(build),'-G','Ninja','-DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe','-DCMAKE_MAKE_PROGRAM=C:/msys64/mingw64/bin/ninja.exe','-DCMAKE_BUILD_TYPE='+kind,'-DPETRIFIED_BMI2=ON','-DPETRIFIED_PYTHON=OFF']),('build',[cmake,'--build',str(build),'-j2','--target','petrified-stockfish'])]
  for name,args in commands:
   with (TASK/f'{kind}-{name}.log').open('x') as log:
    tick=time.monotonic();p=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW);rc=p.wait(timeout=1200)
   stages.append({'kind':kind,'name':name,'exit_code':rc,'seconds':time.monotonic()-tick,'identity':p._journal_identity})
   (TASK/'build-state.json').write_text(json.dumps(stages,indent=2))
   print(kind,name,rc,flush=True)
   if rc:raise RuntimeError('Build failed; preserve log')
  print(kind,'sha256',sha(build/'petrified-stockfish.exe'),flush=True)
 (TASK/'instrumentation-source-hashes.json').write_text(json.dumps({str(p.relative_to(SOURCE)):sha(p) for p in SOURCE.rglob('*') if p.is_file() and (p.parts[-2]=='src' or p.name=='CMakeLists.txt')},indent=2))
if __name__=='__main__':main()
