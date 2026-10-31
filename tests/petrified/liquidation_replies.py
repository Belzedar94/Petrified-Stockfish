"""Depth-one petrification replies; fixed synthetic cases, not Elo evidence."""
import argparse,json,re,time
from pathlib import Path
from protocol import Uci

FIXTURES=[
 ('7k/8/8/8/8/3q4/1*5p/K*5R w - - 0 1 points=0,0','h1h2',
  '7k/8/8/8/8/3q4/K*5*/1*6 w - - 0 1 points=1,0','a2a1','0-1'),
 ('k*5r/1*5P/3Q4/8/8/8/8/7K b - - 0 1 points=0,0','h8h7',
  '1*6/k*5*/3Q4/8/8/8/8/7K b - - 0 1 points=0,1','a7a8','1-0'),
]
def main():
    ap=argparse.ArgumentParser()
    for n in ('engine','network','out'):ap.add_argument('--'+n,type=Path,required=True)
    a=ap.parse_args();a.out.mkdir(exist_ok=False,parents=True);cases=[]
    with Uci(a.engine,network=a.network,timeout=90,log_prefix=a.out/'engine') as e:
        e.send('setoption name UCI_Variant value petrifiedpoints')
        for threads in (1,2):
            e.send(f'setoption name Threads value {threads}')
            for fen,capture,warm,quiet,result in FIXTURES:
                for depth in (1,4):
                    for mode in ('cold','ordinary-warm','event-warm'):
                        e.send('ucinewgame');e.ready()
                        if mode!='cold':
                            wf,wm=(warm,quiet) if mode=='ordinary-warm' else (fen,capture)
                            e.position(wf);e.send(f'go depth 1 searchmoves {wm}')
                            e.until(lambda s:s.startswith('bestmove '))
                        e.position(fen);e.send(f'go depth {depth} searchmoves {capture}')
                        lines=e.until(lambda s:s.startswith('bestmove '))
                        info=[s for s in lines if s.startswith('info depth ') and ' pv ' in s][-1]
                        score=re.search(r' score mate (-?\d+)',info)
                        assert score and int(score[1])==-1,(fen,mode,info)
                        pv=info.split(' pv ',1)[1].split()
                        assert len(pv)==2 and pv[0]==capture and lines[-1].split()[1]==capture,(info,lines[-1])
                        cases.append(dict(fen=fen,pv=pv,result=result,threads=threads,depth=depth,mode=mode,info=info))
        # A bounded search of a real middlegame exercises cancellation with the
        # widened path. UCI must return a legal move and accept another request.
        e.send('ucinewgame');e.ready();e.send('setoption name Threads value 1')
        stop_fen='r1bqk2r/pppp1ppp/2n2n2/2b1p3/4P3/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 4 4 points=0,0'
        e.position(stop_fen);legal,_=e.perft();e.position(stop_fen)
        e.send('go infinite');time.sleep(.15);started=time.monotonic();e.send('stop')
        lines=e.until(lambda s:s.startswith('bestmove '));elapsed=time.monotonic()-started
        assert lines[-1].split()[1] in legal,lines[-1];e.ready()
    report=dict(result='PASS',searches=len(cases),cases=cases,stop_response_seconds=elapsed,
                meaning='Real petrification then quiet terminal reply at depth1/4; no strength claim')
    (a.out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='cases'}))
if __name__=='__main__':main()
