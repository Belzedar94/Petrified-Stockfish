"""One bounded GPU pilot: frozen NNUE plus a small score-conditioned residual."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import time
import numpy as np
import torch
from torch import nn
from model import export, features, integer_forward, read_adapter


def main():
    ap=argparse.ArgumentParser()
    for name in ['data','network','out']: ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--games',type=int,default=512)
    ap.add_argument('--epochs',type=int,default=40)
    ap.add_argument('--audit',type=Path,required=True)
    args=ap.parse_args(); args.out.mkdir(parents=True,exist_ok=False)
    torch.manual_seed(20261028); torch.set_num_threads(2)
    if not torch.cuda.is_available(): raise RuntimeError('CUDA is required for this GPU pilot')
    audit=json.loads(args.audit.read_text())
    valid=set(audit['valid'])
    sets=[[],[],[]]; openings=[set(),set(),set()]; sources={}
    for game in range(args.games):
        file=args.data/f'game-{game:05d}.json'
        data=json.loads(file.read_text()); sources[file.name]=hashlib.sha256(file.read_bytes()).hexdigest()
        if game not in valid: continue
        if data['result']=='*': continue
        split=game%8; split=2 if split==0 else 1 if split==1 else 0
        openings[split].add(' '.join(data['opening']))
        for row in data['samples']:
            y=.5 if data['result']=='1/2-1/2' else float((data['result']=='1-0')==(' w ' in row['fen']))
            sets[split].append((features(row['fen'],row['baseline']),row['baseline'],y))
    assert not (openings[0]&openings[1] or openings[0]&openings[2] or openings[1]&openings[2])
    tensors=[]
    for rows in sets:
        x=np.stack([r[0] for r in rows]).astype(np.float32)/256
        b=np.array([r[1] for r in rows],dtype=np.float32)
        y=np.array([r[2] for r in rows],dtype=np.float32)
        tensors.append(tuple(torch.tensor(v,device='cuda') for v in [x,b,y]))
    net=nn.Sequential(nn.Linear(16,16),nn.Hardtanh(0,4),nn.Linear(16,1)).cuda()
    with torch.no_grad():
        net[0].weight.mul_(.5);net[0].bias.fill_(.1)
        net[2].weight.zero_();net[2].bias.zero_()
    export(net,args.network,args.out/'zero.pnnue')
    opt=torch.optim.AdamW(net.parameters(),lr=.003,weight_decay=.0001)
    def loss_on(split,model=True):
        x,b,y=tensors[split]
        delta=(256*net(x).squeeze(1)).clamp(-4000,4000) if model else 0
        return nn.functional.binary_cross_entropy_with_logits((b+delta)/600,y).item()
    best=None;best_loss=float('inf');history=[];started=time.monotonic()
    x,b,y=tensors[0]
    for epoch in range(args.epochs):
        order=torch.randperm(len(x),device='cuda');net.train()
        for indices in order.split(1024):
            baseline=b[indices]
            delta=(256*net(x[indices]).squeeze(1)).clamp(-4000,4000)
            target=.85*y[indices]+.15*torch.sigmoid(baseline/600)
            loss=nn.functional.binary_cross_entropy_with_logits((baseline+delta)/600,target)
            loss+=.002*(delta/600).square().mean()
            opt.zero_grad();loss.backward();opt.step()
            with torch.no_grad():
                for param in net.parameters(): param.clamp_(-4,4)
        net.eval()
        with torch.no_grad(): validation=loss_on(1)
        history.append({'epoch':epoch+1,'validation_wdl_loss':validation})
        if validation<best_loss: best_loss=validation;best=copy.deepcopy(net.state_dict())
        print(json.dumps(history[-1]),flush=True)
    net.load_state_dict(best);net.eval()
    torch.save({'state_dict':best,'schema':'points-adapter-v1','history':history},args.out/'model.pt')
    digest=export(net,args.network,args.out/'candidate.pnnue')
    with torch.no_grad():
        result={'schema':'points-adapter-pilot-v1','device':torch.cuda.get_device_name(),
          'torch':torch.__version__,'games_requested':args.games,'games_used':len(valid),'rows':[len(s) for s in sets],
          'audit_sha256':hashlib.sha256(args.audit.read_bytes()).hexdigest(),
          'seconds':round(time.monotonic()-started,2),'sha256':digest,
          'validation_baseline':loss_on(1,False),'validation_candidate':loss_on(1),
          'test_baseline':loss_on(2,False),'test_candidate':loss_on(2),
          'history':history,'sources':sources,'promotion':'requires all three direct match controls'}
        raw=tensors[2][0].cpu().numpy()*256
        integer=integer_forward(raw.astype(np.int16),read_adapter(args.out/'candidate.pnnue'))
        floating=(256*net(tensors[2][0]).squeeze(1)).clamp(-4000,4000).cpu().numpy()
        result['integer_float_mae']=float(np.mean(np.abs(integer-floating)))
        result['integer_float_max_error']=float(np.max(np.abs(integer-floating)))
    (args.out/'report.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ['history','sources']}),flush=True)

if __name__=='__main__': main()
