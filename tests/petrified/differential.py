"""Compare independent pinned Walls with the Stockfish-derived implementation."""
import argparse
import hashlib
import json
import random
import time
from pathlib import Path
from protocol import Uci

CASES = [
    'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
    '8/8/8/8/8/8/4k3/4K3 w - - 0 1',
    '4k3/8/8/8/8/8/3***2/3*K*2 w - - 0 1',
    '4k2r/6P1/8/8/8/8/8/4K3 w - - 0 1',
    '4k3/8/8/8/8/n7/8/R3K3 w Q - 0 1',
    '4k3/8/8/8/4P3/8/8/4K3 w - - 0 1',
    'r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1',
    '4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1',
    '8/8/8/8/8/8/3*k*2/3*K*2 w - - 0 1',
    '8/8/8/8/8/8/3*k*2/3*K*2 w - - 100 1',
    '8/8/8/8/8/8/5k2/4K2R w K - 0 1',
    '8/8/8/8/8/6k1/8/4K2R w K - 0 1',
    '4r1k1/8/8/8/8/8/3*1*2/3*K*2 w - - 0 1',
]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--candidate', type=Path, required=True)
    ap.add_argument('--oracle', type=Path, required=True)
    ap.add_argument('--book', type=Path)
    ap.add_argument('--book-limit', type=int, default=0)
    ap.add_argument('--walk-plies', type=int, default=1000)
    ap.add_argument('--seed', type=int, default=20261028)
    ap.add_argument('--report', type=Path, required=True)
    args = ap.parse_args()
    rng = random.Random(args.seed)
    report = {'seed': args.seed, 'comparisons': 0, 'book_positions': 0,
              'candidate_sha256': hashlib.sha256(args.candidate.read_bytes()).hexdigest(),
              'oracle_sha256': hashlib.sha256(args.oracle.read_bytes()).hexdigest()}
    started = time.monotonic()
    with Uci(args.candidate.resolve()) as candidate, Uci(args.oracle.resolve()) as oracle:
        def compare(fen, depth=1, moves=()):
            candidate.position(fen, moves)
            oracle.position(fen, moves)
            actual, expected = candidate.fen(), oracle.fen()
            assert actual == expected, ('FEN', fen, moves, actual, expected)
            a, b = candidate.perft(depth), oracle.perft(depth)
            assert a == b, ('perft', fen, moves, depth, a, b)
            report['comparisons'] += 1
            return sorted(b[0]), expected
        for fen in CASES:
            compare(fen, 3)
        if args.book:
            lines = args.book.read_text().splitlines()
            if args.book_limit: lines = lines[:args.book_limit]
            for fen in lines:
                compare(' '.join(fen.split()[:6]))
                report['book_positions'] += 1
        fen, moves = CASES[0], []
        for i in range(args.walk_plies):
            legal, _ = compare(fen, 2 if i % 50 == 0 else 1, moves)
            if not legal or len(moves) >= 180:
                fen = rng.choice(CASES[:1] + CASES[3:])
                moves = []
            else:
                moves.append(rng.choice(legal))
    report['elapsed_seconds'] = round(time.monotonic()-started, 3)
    report['passed'] = True
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))

if __name__ == '__main__': main()
