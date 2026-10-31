"""Depth-one search must see an opponent's winning quiet third repetition."""
import argparse,json,re
from pathlib import Path
from protocol import Uci
BOARD='***r**1k/******11/********/********/********/********/1*******/K**R****'
HISTORY=['a1a2','h8h7','a2a1','h7h8','a1a2','h8h7']
def rotate(board):
    rows=[]
    for rank in board.split('/'):
        row=''.join('1'*int(c) if c.isdigit() else c for c in rank)
        rows.append(row)
    return '/'.join(re.sub('1+',lambda m:str(len(m[0])),row[::-1].swapcase()) for row in rows[::-1])
def move_flip(m):
    return ''.join(chr(ord('h')-(ord(m[i])-ord('a')))+str(9-int(m[i+1])) for i in (0,2))
def main():
    ap=argparse.ArgumentParser()
    for n in ('engine','network','out'):ap.add_argument('--'+n,type=Path,required=True)
    a=ap.parse_args();a.out.mkdir(exist_ok=False);rows=[]
    with Uci(a.engine,network=a.network,timeout=90,log_prefix=a.out/'engine') as e:
        e.send('setoption name UCI_Variant value petrifiedpoints')
        for threads in (1,2):
            e.send(f'setoption name Threads value {threads}')
            for flipped in (False,True):
                for lead in (1,19):
                    board=rotate(BOARD) if flipped else BOARD
                    initial=f'{board} '+('b' if flipped else 'w')+' - - 0 1 points='+ (f'{lead},0' if flipped else f'0,{lead}')
                    history=[move_flip(m) for m in HISTORY] if flipped else HISTORY
                    expected=[move_flip(m) for m in ['a2a1','h7h8']] if flipped else ['a2a1','h7h8']
                    result='1-0' if flipped else '0-1'
                    for depth in (1,4):
                        for warm in (False,True):
                            e.send('ucinewgame');e.ready();e.position(initial,history)
                            current=e.fen()
                            if warm:
                                # Same board/clock/scores, but no repetition history.
                                e.position(current);e.send(f'go depth {depth}')
                                e.until(lambda s:s.startswith('bestmove '))
                            e.position(initial,history);e.send(f'go depth {depth}')
                            lines=e.until(lambda s:s.startswith('bestmove '))
                            info=[s for s in lines if s.startswith('info depth ') and ' pv ' in s][-1]
                            score=re.search(r' score cp (-?\d+)',info)
                            assert score and int(score[1]) < -19000,(initial,info)
                            pv=info.split(' pv ',1)[1].split()
                            assert pv==expected and lines[-1].split()[1]==expected[0],(initial,info)
                            rows.append(dict(initial=initial,history=history,pv=pv,result=result,threads=threads,depth=depth,warm_without_history=warm,info=info))
    report={'result':'PASS','searches':len(rows),'cases':rows,'meaning':'Exact immediate winning repetition opportunity, not strength evidence'}
    (a.out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='cases'}))
if __name__=='__main__':main()
