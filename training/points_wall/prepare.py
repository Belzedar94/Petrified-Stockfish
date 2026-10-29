"""Reuse the audited v2 DATA split, never the rejected v2 model weights."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from model import Features, V1_SHA, export, score_inputs, WALL_SHAPE

EXPECTED = {
    'data.npz': 'b2084df0386d562042d84bda8305abe0df28a9a1231d350d273713c2169004aa',
    'fens.txt': '26b7255ba89c4b3275923d7ad9f8a7dfbea92fd78ab9ba4b537aa41208f01484',
    'rows.jsonl': 'be9d066a1dea1bc4ed7296f015cb55ff429c9ae908234482fbdd6c9176ca5190',
}


def main():
    ap = argparse.ArgumentParser()
    for k in ('data', 'champion', 'out'): ap.add_argument('--'+k, type=Path, required=True)
    a = ap.parse_args(); a.out.mkdir(parents=True, exist_ok=False)
    for name, digest in EXPECTED.items():
        if hashlib.sha256((a.data/name).read_bytes()).hexdigest() != digest: raise ValueError(name)
    # Materialize once: NpzFile indexing re-reads/decompresses the whole array.
    with np.load(a.data/'data.npz') as archive:
        source = {key:archive[key] for key in archive.files}
    fens = (a.data/'fens.txt').read_text().splitlines()
    assert len(fens) == len(source['x']) == 73400
    raw = np.empty((len(fens), 2, 512), dtype=np.int16)
    ids = np.empty((len(fens), 2, 64), dtype=np.int16)
    features = Features(a.champion)
    for i, fen in enumerate(fens):
        raw[i], ids[i] = features(fen)
        assert np.array_equal(np.clip(raw[i], 0, 127).reshape(1024), source['x'][i, :1024]), i
        assert np.array_equal(score_inputs(fen), source['x'][i, 1024:]), i
        if (i+1) % 5000 == 0: print(f'Independent raw reconstruction {i+1}/{len(fens)}', flush=True)
    counts = np.bincount(source['split']).tolist()
    assert counts == [62241, 5665, 5494], counts
    np.savez(a.out/'data.npz', raw=raw, ids=ids, scores=source['x'][:, 1024:],
             **{k: source[k] for k in ('meta', 'y', 'teacher', 'baseline', 'split')})
    zero = export(np.zeros(WALL_SHAPE), a.champion, a.out/'zero.pnnue')
    report = {'schema':'points-wall-prepared-v1', 'rows':counts, 'source_data':str(a.data),
        'source_hashes':EXPECTED, 'all_transformer_inputs_reconstructed':len(fens),
        'champion_network_sha256':V1_SHA, 'zero_network_sha256':zero,
        'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in a.out.iterdir()}}
    (a.out/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report), flush=True)


if __name__ == '__main__': main()
