"""Build Board CJS/ESM and the complete Node UCI/Legacy NNUE engine.

Run inside an Emscripten environment. Each target records its exact command,
compiler, source hashes and output hashes. No network weights are embedded.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
RULES = 'attacks bitboard misc movegen position memory uci_move petrified_init'.split()
RULES += ['api/petrified_'+name for name in ('board','fen','notation','outcome')]
ENGINE = 'benchmark evaluate movepick search thread timeman tt uci ucioption tune syzygy/tbprobe nnue/network engine score main'.split()

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def build(target, out, compiler):
    out.mkdir(parents=True, exist_ok=True)
    flags = ['-std=c++17','-fexceptions','-DNO_PREFETCH','-DNO_TABLEBASES',
             '-DNNUE_EMBEDDING_OFF','-sDISABLE_EXCEPTION_CATCHING=0',
             '-sWASM_BIGINT=1','-sDETERMINISTIC=1','-O3','-flto','-DNDEBUG',
             '-fconstexpr-steps=500000000', f'-ffile-prefix-map={ROOT.as_posix()}=.']
    sources = list(RULES)
    if target == 'engine':
        sources += ENGINE
        flags += ['-pthread','-msimd128','-msse2','-msse3','-mssse3','-msse4.1',
                  '-DUSE_POPCNT','-DUSE_PTHREADS','-DUSE_SSE2','-DUSE_SSSE3',
                  '-DUSE_SSE41','-DUSE_SLOPPY_ATOMICS',
                  '-include',str(ROOT/'scripts/wasm/uci_only.h'),
                  '-sENVIRONMENT=node','-sNODERAWFS=1','-sFORCE_FILESYSTEM=1',
                  '-sINITIAL_MEMORY=536870912','-sSTACK_SIZE=8388608',
                  '-sPTHREAD_POOL_SIZE=4','-sEXIT_RUNTIME=1','-sNO_EXIT_RUNTIME=0']
        filename = 'petrified-stockfish-nnue.js'
    else:
        sources += ['ffishjs']
        flags += ['--bind','--no-entry','-sMODULARIZE=1',
                  '-sEXPORT_NAME=createPetrifiedStockfish','-sENVIRONMENT=web,worker,node',
                  '-sALLOW_MEMORY_GROWTH=1','-sINITIAL_MEMORY=33554432',
                  '-sMAXIMUM_MEMORY=1073741824','-sFILESYSTEM=0',
                  '-sNO_EXIT_RUNTIME=1','-sMALLOC=emmalloc',
                  '-sEXPORT_ES6='+str(int(target=='esm'))]
        filename = 'ffish.mjs' if target=='esm' else 'ffish.js'
    files = [ROOT/'src'/(name+'.cpp') for name in sources]
    command = [compiler,*flags,*(str(f) for f in files),'-o',str(out/filename)]
    env = dict(os.environ, SOURCE_DATE_EPOCH=os.environ.get('SOURCE_DATE_EPOCH','0'))
    subprocess.run(command,check=True,env=env)
    if target=='engine':
        shutil.copyfile(ROOT/'scripts/wasm/node-uci-wrapper.mjs',out/'petrified-stockfish-nnue-node.mjs')
    manifest = {'schema':'petrified-wasm-build-v1','target':target,
                'compiler':subprocess.check_output([compiler,'--version'],text=True).splitlines()[0],
                'command':command,'source_date_epoch':env['SOURCE_DATE_EPOCH'],
                'sources':{str(f.relative_to(ROOT)):digest(f) for f in files},
                'headers':{str(f.relative_to(ROOT)):digest(f) for f in (ROOT/'src').rglob('*.h')},
                'artifacts':{f.name:{'sha256':digest(f),'bytes':f.stat().st_size}
                             for f in out.iterdir() if f.is_file() and f.name!='manifest.json'}}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'{target}: built {out/filename}',flush=True)

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('--target',choices=['all','cjs','esm','engine'],default='all')
    ap.add_argument('--out',type=Path,default=ROOT/'build/wasm')
    ap.add_argument('--compiler',default=shutil.which('em++.bat' if os.name=='nt' else 'em++') or 'em++')
    args=ap.parse_args()
    for target in ['cjs','esm','engine'] if args.target=='all' else [args.target]:
        build(target,args.out.resolve()/target,args.compiler)
