"""Bounded black-box UCI and CECP lifecycle checks using the real Legacy net."""
import argparse
import json
from pathlib import Path
import tempfile
from protocol import Uci
from differential import CASES

def search(p, command):
    p.send(command)
    lines=p.until(lambda s:s.startswith('bestmove '))
    return lines[-1].split()[1],lines

def uci_checks(command,net):
    with Uci(command,network=net,timeout=60) as p:
        p.position(CASES[0])
        legal=p.perft()[0]
        p.send('setoption name MultiPV value 3')
        best,lines=search(p,'go depth 4')
        assert best in legal
        assert any('multipv 3 ' in s for s in lines),lines[-12:]
        assert any('NNUE Legacy Walls run4test loaded' in s for s in lines)
        p.send('setoption name Threads value 2'); p.ready()
        p.send('go infinite'); p.ready(); p.send('stop')
        assert p.until(lambda s:s.startswith('bestmove '))[-1].split()[1] in legal
        p.send('setoption name Threads value 1'); p.ready()
        p.send('setoption name MultiPV value 1')
        p.send('go ponder wtime 1000 btime 1000 winc 20 binc 20')
        p.ready(); p.send('ponderhit')
        assert p.until(lambda s:s.startswith('bestmove '))[-1].split()[1] in legal
        for fen in (CASES[2],CASES[8]):
            p.position(fen)
            best,lines=search(p,'go depth 4')
            assert best=='(none)',lines
        p.position(CASES[3]); p.send('eval')
        evaluation=[s for s in p.ready() if s.startswith('Legacy ')]
    with tempfile.TemporaryDirectory(prefix='petrified-invalid-') as directory:
        bad=Path(directory)/'wrong.nnue'; bad.write_bytes(b'wrong-network')
        for path in (bad,Path(directory)/'absent.nnue'):
            with Uci(command,network=path,timeout=60) as p:
                p.position(CASES[0]); assert p.perft()[1]==20
                p.send('go depth 1')
                lines=p.until(lambda s:'search aborted' in s)
                p.proc.wait(timeout=15)
                assert p.proc.returncode!=0 and not any(s.startswith('bestmove') for s in lines)
    return evaluation

def xboard_checks(executable,net):
    with Uci(executable,initialize=False,timeout=60) as p:
        p.send('xboard'); p.send('protover 2')
        features=p.until(lambda s:'done=1' in s)
        assert any('variants="petrified"' in s for s in features),features
        def barrier():
            p.send('ping 987'); return p.until(lambda s:s=='pong 987')
        def fen():
            p.send('d'); return next(s[5:] for s in barrier() if s.startswith('Fen: '))
        p.send('option EvalFile='+str(net)); p.send('new'); p.send('force'); barrier()
        p.send('usermove e2e4'); assert fen().split()[1]=='b'
        p.send('undo'); assert fen()==CASES[0]
        p.send('edit'); p.send('#'); p.send('Ke1'); p.send('*e4'); p.send('c'); p.send('Ke8'); p.send('.')
        assert fen()=='4k3/8/8/8/4*3/8/8/4K3 w - - 0 1'
        p.send('setboard '+CASES[0]); p.send('level 0 0:02 0.02')
        assert not any(s.startswith('Error') for s in barrier())
        p.send('st 0.05'); p.send('sd 4'); p.send('go')
        move=p.until(lambda s:s.startswith('move '))[-1].split()[1]
        assert len(move) in (4,5)
        p.send('force'); barrier()
        p.send('post'); p.send('analyze')
        p.until(lambda s:len(s.split())>=4 and s.split()[0].isdigit())
        p.send('exit'); barrier()
        p.send('setboard '+CASES[8]); p.send('go')
        assert p.until(lambda s:s.startswith('0-1 '))[-1]=='0-1 {Black mates}'
        p.send('force'); p.send('setboard '+CASES[2]); p.send('go')
        assert p.until(lambda s:s.startswith('1/2-1/2 '))[-1]=='1/2-1/2 {Stalemate}'

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('--engine',type=Path,required=True)
    ap.add_argument('--network',type=Path,required=True)
    ap.add_argument('--node-wrapper',type=Path)
    ap.add_argument('--report',type=Path,required=True)
    args=ap.parse_args()
    net=args.network.resolve()
    native=uci_checks(args.engine.resolve(),net)
    xboard_checks(args.engine.resolve(),net)
    if args.node_wrapper:
        wasm=uci_checks(['node',args.node_wrapper.resolve()],net)
        assert native==wasm,(native,wasm)
    report={'passed':True,'uci':True,'xboard':True,'node_wasm':bool(args.node_wrapper),
            'checks':['MultiPV','real NNUE','Threads 1/2','stop','ponderhit','terminal roots',
                      'invalid/missing NNUE','rules without NNUE','CECP edit/walls/fractional clocks/analysis']}
    args.report.write_text(json.dumps(report,indent=2)+'\n'); print(json.dumps(report))
