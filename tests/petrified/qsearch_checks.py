"""Quiet checks at depth one: genuine points mates, both colors and repeated TT.

The black rook is forced in its isolated corridor. After it moves, White has
multiple quiet choices, including Nf7#, beyond the nominal one-ply horizon.
Kings cannot capture in Petrified; walls, rather than king opposition, seal h7.
"""
import argparse,json,re
from pathlib import Path
from protocol import Uci

BOARD='6*k/6**/6K1/4N3/8/********/r*******/1*******'

def flip(board):
    return '/'.join(''.join(c.swapcase() for c in rank) for rank in board.split('/')[::-1])

def main():
    ap=argparse.ArgumentParser()
    for name in ('engine','network','out'):ap.add_argument('--'+name,type=Path,required=True)
    args=ap.parse_args();args.out.mkdir(exist_ok=False,parents=True);rows=[]
    with Uci(args.engine,network=args.network,timeout=90,log_prefix=args.out/'engine') as e:
        e.send('setoption name UCI_Variant value petrifiedpoints')
        for threads in (1,2):
            e.send(f'setoption name Threads value {threads}')
            for board,side,first,check,result in [(BOARD,'b','a2a1','e5f7','1-0'),(flip(BOARD),'w','a7a8','e4f2','0-1')]:
                fen=f'{board} {side} - - 0 1 points=0,0'
                e.send('ucinewgame');e.ready()
                for repeat in range(2):
                    e.position(fen);e.send('go depth 1')
                    lines=e.until(lambda s:s.startswith('bestmove '))
                    info=[s for s in lines if s.startswith('info depth ') and ' pv ' in s][-1]
                    assert re.search(r' score mate -1(?: |$)',info),(fen,info)
                    pv=info.split(' pv ',1)[1].split()
                    assert pv==[first,check],(fen,info)
                    rows.append({'fen':fen,'pv':pv,'result':result,'threads':threads,'repeat':repeat,'info':info})
    report={'result':'PASS','searches':len(rows),'cases':rows,'meaning':'Depth-one quiet-check mate correctness, not strength'}
    (args.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='cases'}))
if __name__=='__main__':main()
