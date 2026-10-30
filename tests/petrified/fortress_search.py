"""Real UCI search, legal defence, unchanged claims, TT reuse and stop/ponder."""
import argparse,json,re
from pathlib import Path
from protocol import Uci

OWNER='8/k5r1/q2bbr2/5p2/**P**1**/nPPPPPP1/4N3/7K w - - 39 118 points=10,1'

def main():
    ap=argparse.ArgumentParser()
    for name in ('engine','network','cases','out'):ap.add_argument('--'+name,type=Path,required=True)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    rows=[json.loads(s) for s in args.cases.read_text().splitlines()]
    fixtures=[]
    for row in rows:
        side=0 if row['fen'].split()[1]=='w' else 1
        if row['accepted'] and row['defender']==side and len(fixtures)<12:fixtures.append(row)
    report=[]
    with Uci(args.engine,network=args.network,log_prefix=args.out/'engine',timeout=60) as engine:
        engine.send('setoption name UCI_Variant value petrifiedpoints')
        for threads in (1,2):
            engine.send(f'setoption name Threads value {threads}');engine.ready()
            for row in fixtures:
                for repeat in range(2):
                    engine.position(row['fen']);engine.send('go depth 2')
                    lines=engine.until(lambda s:s.startswith('bestmove '));move=lines[-1].split()[1]
                    assert move==row['defence'] and move!='0000',(row,lines)
                    info=[s for s in lines if s.startswith('info depth ') and ' pv ' in s][-1]
                    assert int(re.search(r' score cp (-?\d+)',info)[1])>19000 and ' mate ' not in info,info
                    assert any('fortress certified' in s for s in lines) and not any('action claim-' in s for s in lines)
                    engine.send('points');status=engine.until(lambda s:s.startswith('info string points {'))[-1]
                    state=json.loads(status.split('info string points ',1)[1]);assert state['result']=='*' and not state['canClaimWin']
                    report.append({'fen':row['fen'],'pv':[move],'info':info,'threads':threads,'repeat':repeat})
        engine.position(OWNER);engine.send('go infinite')
        engine.until(lambda s:'fortress certified' in s)
        assert not any(s.startswith('bestmove ') for s in engine.ready()),'infinite returned before stop'
        engine.send('stop');engine.until(lambda s:s.startswith('bestmove '))
        engine.position(OWNER);engine.send('go ponder wtime 10000 btime 10000')
        engine.until(lambda s:'fortress certified' in s)
        assert not any(s.startswith('bestmove ') for s in engine.ready()),'ponder returned before ponderhit'
        engine.send('ponderhit');engine.until(lambda s:s.startswith('bestmove '))
        # Searchmoves must remain authoritative, even when it excludes the policy.
        row=fixtures[0];other=next(m for m in row['legal'] if m!=row['defence'])
        engine.position(row['fen']);engine.send('go depth 2 searchmoves '+other)
        lines=engine.until(lambda s:s.startswith('bestmove '));assert lines[-1].split()[1]==other,lines
        assert not any('fortress certified' in s for s in lines),lines
        # The same position with an equal worst-case bound is unknown, not a draw.
        engine.position(OWNER.replace('10,1','9,1'));engine.send('go depth 1')
        lines=engine.until(lambda s:s.startswith('bestmove '));assert not any('fortress certified' in s for s in lines)
        # The legacy variant keeps the champion's search and settlement contract.
        engine.send('setoption name UCI_Variant value petrified')
        engine.position(OWNER.split(' points=')[0]);engine.send('go depth 1')
        lines=engine.until(lambda s:s.startswith('bestmove '));assert not any('fortress certified' in s for s in lines)
    result={'result':'PASS','searches':len(report),'protocol_checks':['infinite-stop','ponderhit','searchmoves','unknown-bound','legacy'],'cases':report}
    (args.out/'report.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k!='cases'}))
if __name__=='__main__':main()
