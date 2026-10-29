"""Independent integer inputs/output, legacy identity and malformed PTWF tests."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import numpy as np
from model import (ROOT, BASE_BYTES, V1_SHA, WALL_OFFSET, Features, integer_forward,
                   read_network, read_wall, score_inputs, wall_inputs)
sys.path.insert(0,str(ROOT/'tests/petrified'))
from protocol import Uci
from training.points_head.model import RECORD_DTYPE


def evaluations(exe, network, fens, points=True, threads=1):
    with Uci(exe,network=network) as e:
        e.send('setoption name UCI_Variant value '+('petrifiedpoints' if points else 'petrified'))
        e.send(f'setoption name Threads value {threads}');e.ready();values=[]
        for fen in fens:
            e.position(fen if points else fen.split(' points=')[0]);e.send('eval');lines=e.ready()
            row={s.split(': ')[0]:int(s.split(': ')[1]) for s in lines if s.startswith(('Legacy raw adjusted:', 'Legacy reference:', 'Legacy final:'))}
            assert row['Legacy raw adjusted']==row['Legacy reference'],fen
            values.append((row['Legacy raw adjusted'],row['Legacy final']))
        e.position(fens[0] if points else fens[0].split(' points=')[0]);e.send('go depth 2')
        lines=e.until(lambda s:s.startswith('bestmove '))
        return np.array(values),lines


def main():
    ap=argparse.ArgumentParser()
    for k in ('engine','champion-engine','network','champion-network','zero','exporter','fens','out'):
        ap.add_argument('--'+k,type=Path,required=True)
    a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=False)
    all_fens=a.fens.read_text().splitlines()
    fens=[all_fens[i] for i in np.linspace(0,len(all_fens)-1,384,dtype=int)]
    probe='*4r2/2p1qpk1/2*2*P1/1p2p3/1P2P1*1/8/2P4Q/1N2K1NR w K - 3 22'
    fens.extend(probe+f' points={w},{b}' for w,b in [(0,0),(0,19),(0,20),(0,21),(19,0),(20,0),(21,0),(80,80)])
    (a.out/'fens.txt').write_text('\n'.join(fens)+'\n')
    subprocess.run([str(a.exporter.resolve()),str(a.network.resolve()),str((a.out/'fens.txt').resolve()),
                    str((a.out/'features.bin').resolve())],check=True,timeout=120,capture_output=True)
    records=np.fromfile(a.out/'features.bin',dtype=RECORD_DTYPE)
    delta=read_wall(a.network,a.champion_network);features=Features(a.champion_network)
    for i,fen in enumerate(fens):
        raw,ids=features(fen)
        expected=wall_inputs(raw[None],ids[None],score_inputs(fen)[None],delta)[0]
        assert np.array_equal(expected,records['x'][i]),('Transformer',i)
    candidate,lines=evaluations(a.engine,a.network,fens)
    raw,total=integer_forward(records['x'],records['meta'],read_network(a.champion_network))
    assert np.array_equal(candidate,np.stack((raw,total),axis=1)),'Independent integer output'
    baseline,_=evaluations(a.champion_engine,a.champion_network,fens)
    identity,_=evaluations(a.engine,a.zero,fens,threads=2)
    original,_=evaluations(a.engine,a.champion_network,fens)
    assert np.array_equal(identity,baseline) and np.array_equal(original,baseline),'Zero/v1 identity'
    legacy,_=evaluations(a.champion_engine,a.champion_network,fens[:64],points=False)
    modified,_=evaluations(a.engine,a.network,fens[:64],points=False)
    assert np.array_equal(legacy,modified),'Legacy changed'
    digest=hashlib.sha256(a.network.read_bytes()).hexdigest()
    assert any('NNUE PointsWallFT v1 loaded; SHA256='+digest in s for s in lines)
    with Uci(a.engine,network=a.network) as e:
        saved=a.out/'roundtrip.pnnue'
        e.send('export_net '+saved.resolve().relative_to(ROOT).as_posix());e.ready()
        assert saved.read_bytes()==a.network.read_bytes()
    data=a.network.read_bytes();bad={}
    def resign(buf):return buf[:-64]+hashlib.sha256(buf[:-64]).hexdigest().encode()
    bad['truncated']=data[:-1];bad['trailing']=data+b'x'
    for name,offset,value in [('version',BASE_BYTES+8,2),('dimensions',BASE_BYTES+12,31),('changed-base',1024,data[1024]^1)]:
        buf=bytearray(data);buf[offset]=value;bad[name]=resign(buf)
    buf=bytearray(data);buf[WALL_OFFSET]^=1;bad['checksum']=buf
    for name,value in [('positive-delta-range',9),('negative-delta-range',-9),('delta-int16-min',-32768)]:
        buf=bytearray(data);struct.pack_into('<h',buf,WALL_OFFSET,value);bad[name]=resign(buf)
    buf=bytearray(data);struct.pack_into('<i',buf,BASE_BYTES+28,2**30);bad['bias-overflow']=resign(buf)
    buf=bytearray(data);buf[BASE_BYTES+28+16*4+16*1024+32*4+16]=1;bad['nonzero-padding']=resign(buf)
    for name,buf in bad.items():
        path=a.out/(name+'.pnnue');path.write_bytes(buf)
        commands=f'uci\nsetoption name EvalFile value {path.resolve()}\nposition startpos\ngo depth 1\n'
        p=subprocess.run([str(a.engine.resolve())],input=commands,text=True,capture_output=True,timeout=30)
        assert p.returncode!=0 and 'search aborted' in p.stdout and 'bestmove ' not in p.stdout,(name,p.stdout)
    report={'schema':'points-wall-validation-v1','result':'PASS','positions':len(fens),
        'independent_transformer':'PASS','numpy_integer_cpp':'EXACT','zero_and_original_vs_v1':'EXACT',
        'legacy_unchanged':'EXACT','roundtrip':'EXACT','threads':'1 and 2',
        'invalid_files_rejected':list(bad),'candidate_sha256':digest,'champion_sha256':V1_SHA}
    (a.out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))


if __name__=='__main__':main()
