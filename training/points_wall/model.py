"""Bounded integer wall-transformer adaptation with the winning v1 head frozen."""
import hashlib
from pathlib import Path
import struct
import sys

import numpy as np
import torch
from torch import nn
from torch.nn import functional as F

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from training.points_head.model import (BASE_BYTES, BASE_SHA, EXT_BYTES, Head,
    integer_forward, read_heads, read_network, score_inputs, ste)

V1_SHA = 'ab8bc1e4bb57be21eab8af061259c2bbcf065b6dfcd13c848e2a62b52739941c'
WALL_SHAPE = (4096, 512)
WALL_BYTES = 4096 * 512 * 2
WALL_OFFSET = BASE_BYTES + EXT_BYTES - 64


def champion_bytes(path):
    data = Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest() != V1_SHA:
        raise ValueError('Exact qualified PR #7 network required')
    read_network(path)
    return data


def export(delta, champion, path):
    source = champion_bytes(champion)
    delta = np.rint(np.asarray(delta)).astype(np.int64)
    if delta.shape != WALL_SHAPE or np.any(np.abs(delta) > 8):
        raise ValueError('Wall delta shape/range')
    data = bytearray(source[:-64])
    data[BASE_BYTES:BASE_BYTES+8] = b'PTWF0001'
    data += delta.astype('<i2').tobytes()
    data += hashlib.sha256(data).hexdigest().encode()
    assert data[:BASE_BYTES] == source[:BASE_BYTES]
    assert data[BASE_BYTES+8:WALL_OFFSET] == source[BASE_BYTES+8:-64]
    Path(path).write_bytes(data)
    return hashlib.sha256(data).hexdigest()


def read_wall(path, champion):
    source = champion_bytes(champion)
    data = Path(path).read_bytes()
    if len(data) != BASE_BYTES + EXT_BYTES + WALL_BYTES:
        raise ValueError('Wall format length')
    if data[BASE_BYTES:BASE_BYTES+8] != b'PTWF0001':
        raise ValueError('Wall magic')
    if data[:BASE_BYTES] != source[:BASE_BYTES] or data[BASE_BYTES+8:WALL_OFFSET] != source[BASE_BYTES+8:-64]:
        raise ValueError('Frozen base/head changed')
    if hashlib.sha256(data[:-64]).hexdigest().encode() != data[-64:]:
        raise ValueError('Wall checksum')
    delta = np.frombuffer(data, '<i2', count=4096*512, offset=WALL_OFFSET).reshape(WALL_SHAPE)
    if np.any(np.abs(delta.astype(np.int32)) > 8):
        raise ValueError('Wall delta range')
    return delta.copy()


class Features:
    """Independent FEN -> unclipped transformer and wall indices (no binding)."""
    def __init__(self, champion):
        data = champion_bytes(champion)
        offset = 16 + struct.unpack_from('<I', data, 8)[0]
        self.bias = np.frombuffer(data, '<i2', 512, offset).astype(np.int32)
        self.weights = np.frombuffer(data, '<i2', 49152*512, offset+1024).reshape(49152, 512)

    def __call__(self, fen):
        parts = fen.split(); board = {}; walls = []
        for rank, row in enumerate(parts[0].split('/')):
            file = 0
            for pc in row:
                if pc.isdigit():
                    file += int(pc); continue
                sq = (7-rank)*8 + file; file += 1
                if pc == '*': walls.append(sq)
                else: board[sq] = pc
        sums = []; ids = np.full((2, 64), 4096, dtype=np.int16)
        for side in (0, 1):
            flip = side*56
            king = next(sq for sq, pc in board.items() if pc == ('k' if side else 'K'))
            indices = []
            for sq, pc in board.items():
                plane = 10 if pc.upper() == 'K' else 2*{'N':0,'B':1,'R':2,'Q':3,'P':4}[pc.upper()]+int(pc.islower() != bool(side))
                indices.append((king^flip)*768 + plane*64 + (sq^flip))
            indices.extend((king^flip)*768 + 704 + (sq^flip) for sq in walls)
            sums.append((self.bias + self.weights[indices].sum(0, dtype=np.int32)).astype(np.int16))
            ids[side, :len(walls)] = [(king^flip)*64 + (sq^flip) for sq in walls]
        order = [1, 0] if parts[1] == 'b' else [0, 1]
        return np.asarray(sums)[order], ids[order]


def wall_inputs(raw, ids, scores, delta):
    padded = np.concatenate((delta.astype(np.int32), np.zeros((1, 512), dtype=np.int32)))
    # The deployed accumulators use wraparound int16 arithmetic before clipping.
    summed = (raw.astype(np.int32) + padded[ids].sum(2, dtype=np.int32)).astype(np.int16)
    return np.concatenate((np.clip(summed, 0, 127).reshape(-1, 1024), scores), axis=1).astype(np.uint8)


class WallModel(nn.Module):
    def __init__(self, champion):
        super().__init__()
        self.delta = nn.Parameter(torch.zeros(WALL_SHAPE))
        self.head = Head(read_network(champion))
        self.head.requires_grad_(False)

    def forward(self, raw, ids, scores, meta):
        weights = torch.cat((ste(self.delta, torch.round), self.delta.new_zeros((1, 512))))
        indices = ids.flatten()
        offsets = torch.arange(0, indices.numel(), 64, device=ids.device)
        changes = F.embedding_bag(indices, weights, offsets, mode='sum', padding_idx=4096).reshape(-1, 2, 512)
        summed = raw + changes
        summed = ste(summed, lambda v: torch.remainder(v+32768, 65536)-32768)
        x = torch.cat((summed.clamp(0, 127).reshape(-1, 1024), scores), dim=1)
        return self.head(x, meta)

    def constrain(self):
        with torch.no_grad(): self.delta.clamp_(-8, 8)
