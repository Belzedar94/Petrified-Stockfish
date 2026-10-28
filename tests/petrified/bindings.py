"""Check the native Python interface against the independent Walls diagnostics."""
import argparse
import json
import os
import random
import sys
from pathlib import Path
from protocol import Uci
from differential import CASES

ap=argparse.ArgumentParser()
ap.add_argument('--module-dir',type=Path,required=True)
ap.add_argument('--oracle',type=Path,required=True)
ap.add_argument('--book',type=Path,required=True)
ap.add_argument('--dll-dir',type=Path)
ap.add_argument('--report',type=Path,required=True)
ap.add_argument('--fixtures',type=Path)
args=ap.parse_args()
if args.dll_dir: dll=os.add_dll_directory(str(args.dll_dir))
sys.path.insert(0,str(args.module_dir.resolve()))
import pyffish
assert Path(pyffish.__file__).parent==args.module_dir.resolve(),pyffish.__file__
assert pyffish.variants()==['petrified']
lines=args.book.read_text().splitlines()
random.Random(20261028).shuffle(lines)
count=0
fixtures=[]
with Uci(args.oracle.resolve()) as oracle:
    for fen in CASES+lines[:500]:
        oracle.position(fen)
        oracle.send('rulesraw')
        result=oracle.ready()
        san={s.split(' ',2)[1]:s.split(' ',2)[2] for s in result if s.startswith('Move: ')}
        legal=pyffish.legal_moves('petrified',fen,[])
        assert sorted(legal)==sorted(san),(fen,legal,san)
        for move in legal:
            actual=pyffish.get_san('petrified',fen,move)
            assert actual==san[move],(fen,move,actual,san[move])
        check=next(s for s in result if s.startswith('Check: ')).split()[1]=='1'
        assert pyffish.gives_check('petrified',fen,[])==check,(fen,pyffish.gives_check('petrified',fen,[]),check,result[:5])
        insufficient=tuple(x=='1' for x in next(s for s in result if s.startswith('Insufficient: ')).split()[1:])
        assert pyffish.has_insufficient_material('petrified',fen,[])==insufficient
        for prefix,fn in [('Immediate:',pyffish.is_immediate_game_end),('Optional:',pyffish.is_optional_game_end)]:
            expected=next(s for s in result if s.startswith(prefix)).split()[1:]
            actual=fn('petrified',fen,[])
            assert actual==(expected[0]=='1',int(expected[1])),(fen,prefix,actual,expected)
        assert pyffish.get_fen('petrified',fen,[])==oracle.fen()
        terminal=next(s for s in result if s.startswith('Terminal: ')).split()[1:]
        if terminal[0]=='1':
            assert pyffish.game_result('petrified',fen,[])==int(terminal[1]),(fen,terminal)
        fixtures.append({'fen':fen,'san':san,'check':check,'terminal':terminal[0]=='1',
                         'value':int(terminal[1]),'normalized':oracle.fen()})
        count+=1
args.report.write_text(json.dumps({'passed':True,'positions':count,'module':Path(pyffish.__file__).name},indent=2)+'\n')
if args.fixtures: args.fixtures.write_text(json.dumps(fixtures,indent=2)+'\n')
print(f'Python/Walls API parity: {count} positions, every legal SAN, FEN and terminal flags')
