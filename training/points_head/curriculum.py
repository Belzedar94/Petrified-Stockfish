"""Holdout protection and deterministic replay selection; stdlib only."""
import hashlib
import json

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()


def position_key(fen):
    parts=fen.split()
    return ' '.join(parts[:5]+parts[6:])


def select_rows(candidates, previous=(), replay_fraction=0., seed=29102901):
    """New holdouts stay fresh; historical holdouts never become training data."""
    if not 0 <= replay_fraction < 1: raise ValueError('Replay fraction must be in [0,1)')
    old_keys={r['key'] for r in previous}
    old_families={r['family'] for r in previous}
    by_key={}; historical_overlap=0
    for row in candidates:
        if row['key'] in old_keys or row['family'] in old_families:
            historical_overlap+=1
            continue
        by_key.setdefault(row['key'],[]).append(row)
    rows=[]; cross_split=duplicate=0
    for group in by_key.values():
        if len({r['split'] for r in group})>1:
            cross_split+=len(group)
            continue
        rows.append(group[0]); duplicate+=len(group)-1
    fresh_train=sum(r['split']==0 for r in rows)
    count=int(fresh_train*replay_fraction/(1-replay_fraction))
    eligible=[r for r in previous if r['split']==0]
    # Hash ordering makes the subset independent of input iteration order.
    eligible.sort(key=lambda r:hashlib.sha256(f"{seed}:{r['key']}".encode()).digest())
    if count>len(eligible): raise ValueError('Insufficient historical training rows for fixed replay fraction')
    rows.extend({**r,'source':'replay'} for r in eligible[:count])
    assert len({r['key'] for r in rows})==len(rows)
    families=[{r['family'] for r in rows if r['split']==i} for i in range(3)]
    assert all(not families[i]&families[j] for i in range(3) for j in range(i))
    return rows,{'historical_overlap_rows_excluded':historical_overlap,
                 'cross_split_duplicates_removed':cross_split,'within_split_duplicates_removed':duplicate,
                 'fresh_train_rows':fresh_train,'replay_train_rows':count,
                 'replay_fraction_requested':replay_fraction,
                 'replay_fraction_actual':count/(fresh_train+count) if fresh_train+count else 0}


def load_previous(directory, expected_sha):
    manifest_path=directory/'manifest.json'
    if not expected_sha or sha(manifest_path)!=expected_sha:
        raise ValueError('Historical prepared manifest hash mismatch')
    manifest=json.loads(manifest_path.read_text())
    for name,h in manifest['files'].items():
        assert sha(directory/name)==h,name
    rows=[json.loads(line) for line in (directory/'rows.jsonl').read_text().splitlines()]
    assert [sum(r['split']==i for r in rows) for i in range(3)]==manifest['rows']
    assert len({r['key'] for r in rows})==len(rows)
    for r in rows:
        assert r['key']==position_key(r['fen'])
    return rows


