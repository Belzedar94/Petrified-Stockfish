"""Integer equality with a diagnostic-only patch of the pinned Walls oracle."""
import argparse
import hashlib
import json
import random
from pathlib import Path
from protocol import Uci

def main():
    ap=argparse.ArgumentParser()
    for name in ('candidate','oracle','network','book','report'):
        ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--positions',type=int,default=1000)
    args=ap.parse_args()
    lines=args.book.read_text().splitlines()
    random.Random(20261028).shuffle(lines)
    count=0
    with Uci(args.candidate.resolve(),network=args.network) as a, Uci(args.oracle.resolve(),network=args.network) as b:
        for fen in lines[:args.positions]:
            results=[]
            for engine,cmd in ((a,'eval'),(b,'evalraw')):
                engine.position(fen)
                engine.send(cmd)
                out=engine.ready()
                result={s.split(': ')[0]:int(s.split(': ')[1]) for s in out
                        if s.startswith(('Legacy raw adjusted:', 'Legacy raw unadjusted:', 'Legacy final:', 'Legacy reference:'))}
                results.append(result)
            for key in ('Legacy raw adjusted','Legacy raw unadjusted','Legacy final'):
                assert results[0].get(key)==results[1].get(key), (fen,key,results)
            assert results[0]['Legacy reference']==results[0]['Legacy raw adjusted'],(fen,results)
            count+=1
    report={'passed':True,'integer_equal_positions':count,
            'network_sha256':hashlib.sha256(args.network.read_bytes()).hexdigest(),
            'candidate_sha256':hashlib.sha256(args.candidate.read_bytes()).hexdigest(),
            'oracle_sha256':hashlib.sha256(args.oracle.read_bytes()).hexdigest()}
    args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))

if __name__=='__main__': main()
