"""Chess.com scoring regressions; expected transitions come from the pinned public client.

Use --module-dir with an isolated locally built pyffish; never install globally.
"""
import argparse
import importlib
import json
import os
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
from protocol import Uci


def run(module, engine=None, network=None):
    variant = 'petrifiedpoints'
    start = module.start_fen(variant)
    fixture = json.loads((HERE/'fixtures/chesscom-111037628.json').read_text())
    moves = []
    for row in fixture['records']:
        moves.append(row['uci'])
        status = json.loads(module.points_status(variant, start, moves))
        assert status['final'] == row['points'], (len(moves), status, row)
        assert (status['result'] != '*') == row['terminal']
    assert status['earned'] == [25, 4] and status['final'] == [25, 24]
    assert status['result'] == '1-0' and status['termination'] == 'checkmate'
    # Regression from adapter panel 001: f7e7 keeps blocking Qc7--Kh7,
    # but Chess.com forbids every sideways move of this pinned pawn.
    # Also cover the color mirror, a wall breaking the pin and a forward move.
    pins=json.loads((HERE/'fixtures/chesscom-pins.json').read_text())
    for case in pins['cases']:
        assert sorted(module.legal_moves(variant,case['fen'],[]))==case['legal'],case
    final = module.get_fen(variant, start, moves)
    before = module.get_fen(variant, start, moves[:-1])
    assert module.game_result(variant, start, moves) > 0  # White is mated and wins.
    for score, result in [('24,4', '1/2-1/2'), ('23,4', '0-1')]:
        fen = final.replace('points=25,4', 'points='+score)
        assert json.loads(module.points_status(variant, fen, []))['result'] == result
    cases = json.loads((HERE/'fixtures/chesscom-points-cases.json').read_text())['cases']
    for case in cases:
        action = case['action']
        played = [action['move']] if 'move' in action else []
        if action.get('kind') in ('resign', 'timeout', 'agreement'):
            result = module.end_result(variant, case['fen'], [], action['kind'], ' w ' in case['fen'])
        else:
            actual = json.loads(module.points_status(variant, case['fen'], played))
            result = actual['result']
            # Loading a terminal Chess.com FEN does not re-award its end bonus.
            # Our FEN stores earned points; transition fixtures compare full totals.
            if action.get('kind') != 'position':
                assert actual['final'] == case['points'], (case['name'], actual, case)
        expected = '*' if not case['terminal'] else ('1-0' if case['winner'] == 0 else '0-1' if case['winner'] == 2 else '1/2-1/2')
        assert result == expected, (case['name'], result, expected)
    # Automatic threefold: two occurrences are insufficient, and unequal scores win.
    cycle_fen = '7k/8/8/8/8/8/8/KR6 w - - 0 1 points=7,3'
    cycle = ['b1c1', 'h8h7', 'c1b1', 'h7h8']
    assert json.loads(module.points_status(variant, cycle_fen, cycle))['result'] == '*'
    repeated = json.loads(module.points_status(variant, cycle_fen, cycle*2))
    assert repeated['result'] == '1-0' and repeated['final'] == [17, 13]
    assert repeated['termination'] == 'threefold-repetition'
    walks = json.loads((HERE/'fixtures/chesscom-walks.json').read_text())
    for number, walk in enumerate(walks['walks']):
        for check in walk['checks']:
            actual = json.loads(module.points_status(variant, walks['start'], walk['moves'][:check['ply']]))
            assert actual['result'] == check['result'] and actual['final'] == check['points'], (number, check, actual)
    for suffix in ['points=-1,0', 'points=0,x', 'points=1,2,3', 'points=1000001,0', 'points=0,0 junk']:
        assert module.validate_fen(start.split(' points=')[0]+' '+suffix, variant) < 0, suffix
    assert json.loads(module.points_status('petrified', module.start_fen('petrified'), moves))['result'] == '0-1'
    if engine:
        with Uci(engine, network=network) as uci:
            uci.send('setoption name UCI_Variant value petrifiedpoints')
            for case in pins['cases']:
                uci.position(case['fen'])
                legal, count = uci.perft(1)
                assert sorted(legal) == case['legal'], (case, legal)
                assert count == len(case['legal']), (case, count)
            for fen, history in [(start, moves), (before, []), (cycle_fen, cycle*2)]:
                uci.position(fen, history)
                uci.send('points')
                got = json.loads(uci.until(lambda s: s.startswith('info string points {'))[-1].split('points ',1)[1])
                assert got == json.loads(module.points_status(variant, fen, history))
            if network:
                uci.position(before)
                uci.send('go depth 2 searchmoves f1g1')
                lines = uci.until(lambda s: s.startswith('bestmove '))
                assert any('score cp -' in s for s in lines), lines
                assert not any('score mate ' in s for s in lines), lines
                assert lines[-1] == 'bestmove f1g1', lines
                uci.position(final)
                uci.send('go depth 2')
                lines = uci.until(lambda s: s.startswith('bestmove '))
                assert lines[-1] == 'bestmove 0000' and any('score cp 20000' in s for s in lines), lines
                claim = start.replace('points=0,0', 'points=25,4')
                uci.position(claim)
                uci.send('go depth 2')
                lines = uci.until(lambda s: s.startswith('bestmove '))
                assert 'info string points action claim-win' in lines and lines[-1] == 'bestmove 0000', lines
    return {'game_plies': len(moves), 'reference_cases': len(cases), 'pin_regressions': len(pins['cases']), 'oracle_walk_plies': sum(len(w['moves']) for w in walks['walks']), 'result': 'PASS', 'final_points': [25,24]}


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--module-dir', type=Path, required=True)
    ap.add_argument('--engine', type=Path)
    ap.add_argument('--network', type=Path)
    args = ap.parse_args()
    dll = os.add_dll_directory(r'C:\msys64\mingw64\bin') if os.name == 'nt' and Path(r'C:\msys64\mingw64\bin').is_dir() else None
    sys.path.insert(0, str(args.module_dir.resolve()))
    print(json.dumps(run(importlib.import_module('pyffish'), args.engine, args.network)))
