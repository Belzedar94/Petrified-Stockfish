"""C++ / independent NumPy NNUE checks, identity and malformed-file regression."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import numpy as np
from model import BASE_BYTES, BASE_SHA, Head, RECORD_DTYPE, export, integer_forward, read_network, score_inputs

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/petrified'))
from protocol import Uci


def numpy_inputs(fen,base):
    """Recompute frozen features from FEN; no C++ accumulator or binding used."""
    offset=16+struct.unpack_from('<I',base,8)[0]
    biases=np.frombuffer(base,dtype='<i2',count=512,offset=offset); offset+=1024
    weights=np.frombuffer(base,dtype='<i2',count=49152*512,offset=offset).reshape(49152,512); offset+=49152*512*2
    psqt=np.frombuffer(base,dtype='<i4',count=49152*8,offset=offset).reshape(49152,8)
    board={}; walls=[]; parts=fen.split()
    for rank,line in enumerate(parts[0].split('/')):
        file=0
        for pc in line:
            if pc.isdigit(): file+=int(pc); continue
            sq=(7-rank)*8+file; file+=1
            if pc=='*': walls.append(sq)
            else: board[sq]=pc
    a=[]; q=[]; material=[]
    bucket=min((len(board)-1)//4,7)
    for color in (0,1):
        flip=56*color; king=next(sq for sq,pc in board.items() if pc==('K' if color==0 else 'k'))
        indices=[]
        for sq,pc in board.items():
            plane=10 if pc.upper()=='K' else 2*{'N':0,'B':1,'R':2,'Q':3,'P':4}[pc.upper()]+int(pc.islower()!=bool(color))
            indices.append((king^flip)*768+plane*64+(sq^flip))
        indices.extend((king^flip)*768+11*64+(sq^flip) for sq in walls)
        a.append(np.clip((biases.astype(np.int64)+weights[indices].astype(np.int64).sum(0)).astype(np.int16),0,127))
        q.append(psqt[indices].astype(np.int64).sum(0).astype(np.int32))
        values={'P':214 if color==0 else 0,'N':781,'B':825,'R':1276,'Q':2538,'K':700}
        material.append(sum(values[pc.upper()] for pc in board.values() if pc.islower()==bool(color)))
    us=int(parts[1]=='b'); w,b=map(int,parts[6][7:].split(',')); diff=(w-b)*(1 if us==0 else -1)
    x=np.concatenate((a[us],a[1-us],score_inputs(fen))).astype(np.uint8)
    meta=np.array([bucket,int((int(q[us][bucket])-int(q[1-us][bucket]))/2),
                   7 if abs(material[0]-material[1])<=44 else 0,
                   903+32*sum(material)//1024,min(100,int(parts[4])),max(-200,min(200,diff))],dtype=np.int32)
    return x,meta


def evaluations(executable,network,fens,threads=1):
    with Uci(executable,network=network) as e:
        e.send('setoption name UCI_Variant value petrifiedpoints')
        e.send(f'setoption name Threads value {threads}'); e.ready()
        out=[]
        for fen in fens:
            e.position(fen);e.send('eval');lines=e.ready()
            vals={s.split(': ')[0]:int(s.split(': ')[1]) for s in lines if s.startswith(('Legacy raw adjusted:','Legacy reference:','Legacy final:'))}
            assert vals['Legacy raw adjusted']==vals['Legacy reference']
            out.append([vals['Legacy raw adjusted'],vals['Legacy final']])
        e.position(fens[0]);e.send('go depth 2');lines=e.until(lambda s:s.startswith('bestmove '))
        return np.array(out),lines


def main():
    ap=argparse.ArgumentParser()
    for name in ('engine','champion','network','legacy','exporter','fens','out'):ap.add_argument('--'+name,type=Path,required=True)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    fens=args.fens.read_text().splitlines()[:192]
    for fixture in ('chesscom-winning-mate.json','chesscom-111037628.json'):
        # The normal rules regression exercises these whole games separately.
        assert (ROOT/'tests/petrified/fixtures'/fixture).exists()
    probe='*4r2/2p1qpk1/2*2*P1/1p2p3/1P2P1*1/8/2P4Q/1N2K1NR w K - 3 22'
    for w,b in ((0,0),(0,19),(0,20),(0,21),(25,4),(4,25),(19,0),(20,0),(21,0),(80,80)):
        fens.append(probe+f' points={w},{b}')
    fenfile=args.out/'fens.txt';fenfile.write_text('\n'.join(fens)+'\n')
    subprocess.run([str(args.exporter.resolve()),str(args.legacy.resolve()),str(fenfile.resolve()),
                    str((args.out/'features.bin').resolve())],check=True,timeout=120,capture_output=True)
    records=np.fromfile(args.out/'features.bin',dtype=RECORD_DTYPE); base=args.legacy.read_bytes()
    for fen,r in zip(fens,records):
        x,m=numpy_inputs(fen,base)
        assert np.array_equal(x,r['x']),('Transformer',fen)
        assert np.array_equal(m,r['meta']),('Metadata',fen,m,r['meta'])
    zero=args.out/'zero.pnnue';export(Head(read_network(args.legacy)),args.legacy,zero)
    baseline,_=evaluations(args.champion,args.legacy,fens)
    with ThreadPoolExecutor(max_workers=3) as pool:
        f1=pool.submit(evaluations,args.engine,args.network,fens)
        f2=pool.submit(evaluations,args.engine,zero,fens,2)
        f3=pool.submit(evaluations,args.engine,args.legacy,fens)
        candidate,lines=f1.result();identity,_=f2.result();plain,_=f3.result()
    assert np.array_equal(identity,baseline) and np.array_equal(plain,baseline),'Zero/base identity'
    raw,total=integer_forward(records['x'],records['meta'],read_network(args.network))
    assert np.array_equal(candidate,np.stack((raw,total),axis=1)), 'Python/C++ integer mismatch'
    digest=hashlib.sha256(args.network.read_bytes()).hexdigest()
    assert any('NNUE PointsHead v1 loaded; SHA256='+digest in line for line in lines)
    with Uci(args.engine,network=args.network) as e:
        e.send('setoption name UCI_Variant value petrified')
        for fen in fens[:32]:
            e.position(fen.split(' points=')[0]);e.send('eval');candidate_legacy=e.ready()
            e.send('setoption name EvalFile value '+str(args.legacy.resolve()));e.ready()
            e.send('eval');base_legacy=e.ready()
            assert [s for s in candidate_legacy if s.startswith('Legacy ')]==[s for s in base_legacy if s.startswith('Legacy ')]
            e.send('setoption name EvalFile value '+str(args.network.resolve()));e.ready()
        saved=args.out/'roundtrip.pnnue'
        # Upstream export_net consumes one path token. An absolute --out can
        # include spaces in the repository's parent directory, even when the
        # output directory itself is safe. Normalize against the engine's cwd.
        export_path=os.path.relpath(saved.resolve(),Path.cwd())
        if any(c.isspace() for c in export_path):
            raise ValueError('export_net test needs an output path without whitespace relative to cwd')
        e.send('export_net '+export_path);e.ready()
        assert saved.read_bytes()==args.network.read_bytes()
    data=args.network.read_bytes(); corrupt={}
    def resigned(buf):return buf[:-64]+hashlib.sha256(buf[:-64]).hexdigest().encode()
    corrupt['truncated']=data[:-1];corrupt['trailing']=data+b'x'
    buf=bytearray(data);buf[BASE_BYTES+8]=2;corrupt['version']=resigned(buf)
    buf=bytearray(data);buf[BASE_BYTES+12]=31;corrupt['dimensions']=resigned(buf)
    buf=bytearray(data);buf[1024]^=1;corrupt['changed-base']=resigned(buf)
    buf=bytearray(data);buf[BASE_BYTES+40]^=1;corrupt['checksum']=buf
    buf=bytearray(data);struct.pack_into('<i',buf,BASE_BYTES+28,2**30);corrupt['bias-overflow']=resigned(buf)
    buf=bytearray(data);buf[BASE_BYTES+28+16*4+16*1024+32*4+16]=1
    corrupt['nonzero-padding']=resigned(buf)
    for name,buf in corrupt.items():
        path=args.out/(name+'.pnnue');path.write_bytes(buf)
        commands=f'uci\nsetoption name EvalFile value {path.resolve()}\nposition startpos\ngo depth 1\n'
        p=subprocess.run([str(args.engine.resolve())],input=commands,text=True,capture_output=True,timeout=30)
        assert p.returncode!=0 and 'search aborted' in p.stdout and 'bestmove ' not in p.stdout,(name,p.stdout)
    report={'schema':'points-head-validation-v1','positions':len(fens),'independent_numpy_transformer':'PASS',
            'numpy_integer_vs_cpp':'PASS','zero_head_vs_champion':'EXACT','base_vs_champion':'EXACT',
            'legacy_unchanged':'PASS','two_threads_and_multiple_instances':'PASS','save_roundtrip':'PASS',
            'invalid_files_rejected':list(corrupt),'candidate_sha256':digest,'base_sha256':BASE_SHA}
    (args.out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))


if __name__=='__main__': main()
