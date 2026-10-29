"""One preregistered CUDA run of wall deltas, selecting only by validation loss."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import time
import numpy as np
import torch
from torch.nn import functional as F
from model import (WallModel, V1_SHA, export, integer_forward, read_network,
                   read_wall, wall_inputs)


def main():
    ap = argparse.ArgumentParser()
    for k in ('data', 'champion', 'out'): ap.add_argument('--'+k, type=Path, required=True)
    a = ap.parse_args(); a.out.mkdir(parents=True, exist_ok=False)
    torch.manual_seed(29102904); np.random.seed(29102904); torch.set_num_threads(2)
    torch.backends.cuda.matmul.allow_tf32 = False
    if not torch.cuda.is_available(): raise RuntimeError('CUDA required')
    prepared = json.loads((a.data/'manifest.json').read_text())
    for name, digest in prepared['files'].items():
        assert hashlib.sha256((a.data/name).read_bytes()).hexdigest() == digest, name
    data = np.load(a.data/'data.npz'); splits = []
    keys = ('raw', 'ids', 'scores', 'meta', 'y', 'teacher', 'baseline')
    for split in range(3):
        mask = data['split'] == split
        splits.append(tuple(torch.as_tensor(data[k][mask], device='cuda',
            dtype=torch.int64 if k == 'ids' else torch.float32) for k in keys))
    net = WallModel(a.champion).cuda()
    frozen = {k:v.detach().cpu().clone() for k,v in net.head.state_dict().items()}
    opt = torch.optim.Adam([net.delta], lr=.04)

    def blend(pred, y, teacher):
        return F.binary_cross_entropy_with_logits(pred/600, .5*y+.5*torch.sigmoid(teacher/600))

    def predictions(i):
        arrays = splits[i]; out = []
        for start in range(0, len(arrays[0]), 512):
            out.append(net(*(v[start:start+512] for v in arrays[:4])))
        return torch.cat(out)

    def metrics(i):
        *_, y, teacher, baseline = splits[i]
        p = predictions(i)
        return {'candidate_blend_loss':blend(p,y,teacher).item(),
                'baseline_blend_loss':blend(baseline,y,teacher).item(),
                'candidate_result_loss':F.binary_cross_entropy_with_logits(p/600,y).item(),
                'baseline_result_loss':F.binary_cross_entropy_with_logits(baseline/600,y).item()}

    # Zero deltas must reproduce the stored v1 evaluation on every split before fitting.
    net.eval()
    with torch.no_grad():
        for i in range(3):
            assert torch.equal(predictions(i), splits[i][-1].double()), ('Initial identity',i)
    started = time.monotonic(); history=[]; best=None; best_loss=float('inf')
    for epoch in range(24):
        net.train(); rate=.04*(.15+.85*(1+math.cos(math.pi*epoch/24))/2)
        for group in opt.param_groups: group['lr']=rate
        arrays=splits[0]
        for indices in torch.randperm(len(arrays[0]),device='cuda').split(512):
            raw,ids,scores,meta,y,teacher,baseline=(v[indices] for v in arrays)
            pred=net(raw,ids,scores,meta)
            loss=blend(pred,y,teacher)+.001*((pred-baseline)/600).square().mean()+.0001*(net.delta/8).square().mean()
            opt.zero_grad(); loss.backward(); torch.nn.utils.clip_grad_norm_([net.delta],1.)
            opt.step(); net.constrain()
        net.eval()
        with torch.no_grad(): validation=metrics(1)
        row={'epoch':epoch+1,'lr':rate,**validation}; history.append(row)
        print(json.dumps(row),flush=True)
        if validation['candidate_blend_loss']<best_loss:
            best_loss=validation['candidate_blend_loss']; best=net.delta.detach().cpu().clone(); best_epoch=epoch+1
    with torch.no_grad(): net.delta.copy_(best)
    for key,value in net.head.state_dict().items(): assert torch.equal(value.cpu(),frozen[key]),key
    delta=np.rint(best.numpy()).astype(np.int16)
    digest=export(delta,a.champion,a.out/'candidate.pnnue')
    assert np.array_equal(delta,read_wall(a.out/'candidate.pnnue',a.champion))
    with torch.no_grad(): validation=metrics(1); test=metrics(2); qat=predictions(2).cpu().numpy()
    heads=read_network(a.champion); integer=[]; test_ids=np.flatnonzero(data['split']==2)
    for indices in np.array_split(test_ids, max(1,math.ceil(len(test_ids)/128))):
        x=wall_inputs(data['raw'][indices],data['ids'][indices],data['scores'][indices],delta)
        integer.extend(integer_forward(x,data['meta'][indices],heads)[1])
    mismatch=int(np.count_nonzero(qat!=integer))
    if mismatch: raise RuntimeError(f'QAT/integer mismatches: {mismatch}')
    torch.save({'schema':'points-wall-ft-v1','delta':best,'selected_epoch':best_epoch,'history':history},a.out/'model.pt')
    report={'schema':'points-wall-training-v1','device':torch.cuda.get_device_name(),
        'torch':torch.__version__,'cuda':torch.version.cuda,'seconds':round(time.monotonic()-started,2),
        'rows':prepared['rows'],'selected_epoch':best_epoch,'epochs':24,'seed':29102904,
        'trainable_parameters':net.delta.numel(),'head_frozen':True,'live_features_frozen':True,
        'psqt_frozen':True,'delta_bound':8,'delta_min':int(delta.min()),'delta_max':int(delta.max()),
        'nonzero_deltas':int(np.count_nonzero(delta)),'initial_all_rows_identity':'EXACT',
        'validation':validation,'test':test,'qat_integer_mismatches':mismatch,
        'sha256':digest,'champion_network_sha256':V1_SHA,'history':history,
        'meaning':'Validation loss selects one epoch; strength requires the independent three-control panel'}
    (a.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='history'}),flush=True)


if __name__=='__main__': main()
