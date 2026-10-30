"""Native integer/reference equivalence and format rejection for adapter v1."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import sys
import tempfile
import numpy as np

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/petrified'))
from protocol import Uci
from model import features, integer_forward, read_adapter, LEGACY_SIZE


def main():
    ap=argparse.ArgumentParser()
    for name in ['engine','champion','network','legacy','data','report']: ap.add_argument('--'+name,type=Path,required=True)
    args=ap.parse_args();params=read_adapter(args.network)
    samples=[]
    for path in sorted(args.data.glob('game-*.json'))[:512]:
        samples.extend(json.loads(path.read_text())['samples'])
    random.Random(20261028).shuffle(samples);samples=samples[:256]
    with Uci(args.engine,network=args.network) as student, Uci(args.champion,network=args.legacy) as champion:
        for engine in [student,champion]:
            engine.send('setoption name UCI_Variant value petrifiedpoints');engine.ready()
        for row in samples:
            results=[]
            for engine in [student,champion]:
                engine.position(row['fen']);engine.send('eval');results.append(engine.ready())
            fields=[dict(line.split(': ',1) for line in lines if ': ' in line) for lines in results]
            baseline=int(fields[1]['Legacy final'])
            x=features(row['fen'],baseline)
            assert list(x)==list(map(int,fields[0]['Points features'].split())),(row,fields,x)
            expected=int(integer_forward(x,params))
            assert int(fields[0]['Points adapter'])==expected
            assert int(fields[0]['Legacy final'])==baseline+expected,(baseline,fields)
            assert fields[0]['Legacy raw adjusted']==fields[1]['Legacy raw adjusted']
        student.send('setoption name Threads value 2');student.ready()
        student.position(samples[0]['fen']);student.send('go nodes 2048')
        assert student.until(lambda s:s.startswith('bestmove '))[-1].split()[1]!='(none)'
    data=args.network.read_bytes()
    with tempfile.TemporaryDirectory(prefix='points-adapter-',dir=ROOT/'.local') as folder:
        for name,changed in [('truncated',data[:-1]),('corrupt',data[:-65]+bytes([data[-65]^1])+data[-64:])]:
            path=Path(folder)/(name+'.pnnue');path.write_bytes(changed)
            with Uci(args.engine,network=path) as engine:
                engine.send('go depth 1')
                lines=engine.until(lambda line:'ERROR:' in line)
                assert not any(s.startswith('bestmove ') for s in lines)
        changed=bytearray(data);struct.pack_into('<I',changed,LEGACY_SIZE+8,2)
        changed[-64:]=hashlib.sha256(changed[:-64]).hexdigest().encode()
        path=Path(folder)/'wrong-version.pnnue';path.write_bytes(changed)
        with Uci(args.engine,network=path) as engine:
            engine.send('go depth 1');engine.until(lambda line:'ERROR:' in line)
    report={'result':'PASS','integer_positions':len(samples),'base_nnue_unchanged':True,
            'invalid_files_rejected':3,'threads':2,'network_sha256':hashlib.sha256(data).hexdigest()}
    args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

if __name__=='__main__': main()
