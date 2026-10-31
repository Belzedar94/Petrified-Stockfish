"""One CUDA/QAT run of eight dense heads; the transformer is never optimized."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import time
import numpy as np
import torch
from torch import nn
from model import Head, export, integer_forward, read_network, BASE_SHA, BASE_BYTES


def main():
    ap=argparse.ArgumentParser()
    for name in ('data','network','out'): ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--epochs',type=int,default=24)
    args=ap.parse_args(); args.out.mkdir(parents=True,exist_ok=False)
    torch.manual_seed(29102901); np.random.seed(29102901); torch.set_num_threads(2)
    torch.backends.cuda.matmul.allow_tf32=False
    if not torch.cuda.is_available(): raise RuntimeError('CUDA is required')
    prepared=json.loads((args.data/'manifest.json').read_text())
    for name,h in prepared['files'].items():
        assert hashlib.sha256((args.data/name).read_bytes()).hexdigest()==h, name
    data=np.load(args.data/'data.npz'); splits=[]
    for i in range(3):
        m=data['split']==i
        splits.append(tuple(torch.tensor(data[k][m],dtype=torch.float32,device='cuda') for k in ('x','meta','y','teacher','baseline')))
    net=Head(read_network(args.network)).cuda()
    export(net,args.network,args.out/'zero.pnnue')
    # Smaller steps for inherited geometry; score channels start at zero but train
    # with the same optimizer and exact integer forward / straight-through gradients.
    opt=torch.optim.Adam(net.parameters(),lr=.04)
    def probability_loss(pred,y,teacher):
        target=.5*y+.5*torch.sigmoid(teacher/600)
        return nn.functional.binary_cross_entropy_with_logits(pred/600,target)
    def metrics(i):
        x,m,y,t,b=splits[i]; predictions=[]
        for a in range(0,len(x),2048): predictions.append(net(x[a:a+2048],m[a:a+2048]))
        p=torch.cat(predictions)
        return {'candidate_blend_loss':probability_loss(p,y,t).item(),
                'baseline_blend_loss':probability_loss(b,y,t).item(),
                'candidate_result_loss':nn.functional.binary_cross_entropy_with_logits(p/600,y).item(),
                'baseline_result_loss':nn.functional.binary_cross_entropy_with_logits(b/600,y).item()}
    started=time.monotonic(); history=[]; best=None; best_loss=float('inf'); best_epoch=0
    x,m,y,t,b=splits[0]
    for epoch in range(args.epochs):
        net.train()
        rate=.04*(.15+.85*(1+math.cos(math.pi*epoch/args.epochs))/2)
        for group in opt.param_groups: group['lr']=rate
        order=torch.randperm(len(x),device='cuda')
        for ids in order.split(512):
            pred=net(x[ids],m[ids])
            loss=probability_loss(pred,y[ids],t[ids])+.001*((pred-b[ids])/600).square().mean()
            opt.zero_grad(); loss.backward(); torch.nn.utils.clip_grad_norm_(net.parameters(),1.0); opt.step(); net.constrain()
        net.eval()
        with torch.no_grad(): validation=metrics(1)
        record={'epoch':epoch+1,'lr':rate,**validation}
        history.append(record); print(json.dumps(record),flush=True)
        if validation['candidate_blend_loss']<best_loss:
            best_loss=validation['candidate_blend_loss']; best=copy.deepcopy(net.state_dict()); best_epoch=epoch+1
    net.load_state_dict(best); net.eval()
    torch.save({'schema':'points-head-v1','state_dict':best,'selected_epoch':best_epoch,'history':history},args.out/'model.pt')
    digest=export(net,args.network,args.out/'candidate.pnnue')
    with torch.no_grad(): validation=metrics(1); test=metrics(2)
    # Untouched test rows, integer inference is the deployed network, not a float surrogate.
    test_mask=data['split']==2
    _,integer=integer_forward(data['x'][test_mask],data['meta'][test_mask],read_network(args.out/'candidate.pnnue'))
    with torch.no_grad():
        tx,tm,*_=splits[2]; floating=torch.cat([net(tx[a:a+2048],tm[a:a+2048]) for a in range(0,len(tx),2048)]).cpu().numpy()
    mismatch=int(np.count_nonzero(integer!=floating))
    if mismatch: raise RuntimeError(f'QAT/export mismatch on {mismatch} test rows')
    candidate=(args.out/'candidate.pnnue').read_bytes()
    assert hashlib.sha256(candidate[:BASE_BYTES]).hexdigest()==BASE_SHA
    report={'schema':'points-head-training-v1','device':torch.cuda.get_device_name(),'torch':torch.__version__,
            'cuda':torch.version.cuda,'seconds':round(time.monotonic()-started,2),'rows':prepared['rows'],
            'selected_epoch':best_epoch,'epochs':args.epochs,'validation':validation,'test':test,
            'sha256':digest,'frozen_base_sha256':BASE_SHA,'qat_integer_mismatches':mismatch,
            'trainable_parameters':sum(p.numel() for p in net.parameters()),'history':history,
            'dataset_manifest_sha256':hashlib.sha256((args.data/'manifest.json').read_bytes()).hexdigest(),
            'promotion':'Requires three valid direct match wins; loss is not Elo'}
    (args.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='history'}),flush=True)


if __name__=='__main__': main()
