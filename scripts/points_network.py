"""Positive network identification for points matches (full file + base hashes)."""
import hashlib
from pathlib import Path

BASE_SHA='345d67b733991ae5beb2d3a6ea40d54ea01abb1fb699aa39e6f6406495f4d933'
BASE_BYTES=52046752


def verify_points_network(engine,network):
    network=Path(network)
    with network.open('rb') as stream:
        stream.seek(BASE_BYTES); magic=stream.read(8)
    descriptions={b'PTADPT01':('PointsAdapter v1','16/16/1'),
                  b'PTHD0001':('PointsHead v1','1056/16/32/1, 8 buckets'),
                  b'PTWF0001':('PointsWallFT v1','1056/16/32/1, 8 buckets, 4096x512 wall deltas')}
    if magic not in descriptions: raise RuntimeError('Unknown points network extension')
    name,dimensions=descriptions[magic]
    digest=hashlib.sha256(network.read_bytes()).hexdigest()
    expected=f'NNUE {name} loaded; SHA256={digest}; base={BASE_SHA}; {dimensions}'
    engine.send('position startpos');engine.send('go depth 1')
    lines=engine.until(lambda s:s.startswith('bestmove '))
    if not any(expected in line for line in lines):
        raise RuntimeError('Engine did not positively identify the points network and frozen base')
