"""The run4test dense heads, with integer-exact export and frozen base bytes."""
import hashlib
from pathlib import Path
import struct
import numpy as np
import torch
from torch import nn

BASE_SHA='345d67b733991ae5beb2d3a6ea40d54ea01abb1fb699aa39e6f6406495f4d933'
BASE_BYTES=52046752
HEAD_BYTES=16*4+16*1024+32*4+32*32+4+32
EXT_BYTES=8+5*4+8*(HEAD_BYTES+16*32)+64
THRESHOLDS=(-40,-32,-24,-22,-21,-20,-19,-16,-12,-8,-4,-2,0,2,4,8,12,16,19,20,21,22,24,32,40,64)
RECORD_DTYPE=np.dtype([('x','u1',(1056,)),('meta','<i4',(6,))])
assert RECORD_DTYPE.itemsize==1080


def base_bytes(path):
    data=Path(path).read_bytes()
    if len(data)!=BASE_BYTES or hashlib.sha256(data).hexdigest()!=BASE_SHA:
        raise ValueError('Exact original run4test required')
    return data


def read_heads(data, extended=False):
    offset=BASE_BYTES+28 if extended else BASE_BYTES-8*(HEAD_BYTES+4)
    heads={k:[] for k in ('b1','w1','b2','w2','b3','w3')}
    def take(dtype,shape):
        nonlocal offset
        size=int(np.prod(shape))*np.dtype(dtype).itemsize
        a=np.frombuffer(data,dtype=dtype,count=int(np.prod(shape)),offset=offset).reshape(shape).copy()
        offset+=size
        return a
    for _ in range(8):
        if not extended:
            if struct.unpack_from('<I',data,offset)[0]!=1664317130: raise ValueError('Head hash')
            offset+=4
        b1=take('<i4',(16,)); board=take('i1',(16,1024))
        b2=take('<i4',(32,)); w2=take('i1',(32,32))[:,:16]
        b3=take('<i4',(1,)); w3=take('i1',(32,))
        score=take('i1',(16,32)) if extended else np.zeros((16,32),dtype=np.int8)
        for k,v in zip(heads,(b1,np.concatenate((board,score),axis=1),b2,w2,b3,w3)):
            heads[k].append(v)
    return {k:np.stack(v) for k,v in heads.items()}


def read_network(path):
    data=Path(path).read_bytes()
    if hashlib.sha256(data[:BASE_BYTES]).hexdigest()!=BASE_SHA: raise ValueError('Base hash')
    if len(data)==BASE_BYTES: return read_heads(data)
    if len(data)!=BASE_BYTES+EXT_BYTES or data[BASE_BYTES:BASE_BYTES+28]!=b'PTHD0001'+struct.pack('<5I',1,32,16,32,8):
        raise ValueError('Head format')
    if hashlib.sha256(data[:-64]).hexdigest().encode()!=data[-64:]: raise ValueError('Checksum')
    return read_heads(data,True)


def score_inputs(fen):
    parts=fen.split(); white,black=map(int,parts[6][7:].split(','))
    us,them=(white,black) if parts[1]=='w' else (black,white)
    d=max(-200,min(200,us-them))
    x=[2*max(0,min(63,d)),2*max(0,min(63,-d)),2*max(0,min(63,us)),2*max(0,min(63,them)),
       min(100,max(0,int(parts[4]))),2*parts[0].count('*')]
    return np.array(x+[max(0,min(127,64+16*(d-t))) for t in THRESHOLDS],dtype=np.uint8)


def integer_forward(x,meta,heads):
    """Independent NumPy int64 path; C++ divisions truncate toward zero."""
    raw=np.zeros(len(x),dtype=np.int64)
    for bucket in range(8):
        m=meta[:,0]==bucket
        if not np.any(m): continue
        a=x[m].astype(np.int64)
        h1=np.clip((a@heads['w1'][bucket].astype(np.int64).T+heads['b1'][bucket])//64,0,127)
        h2=np.clip((h1@heads['w2'][bucket].astype(np.int64).T+heads['b2'][bucket])//64,0,127)
        out=h2@heads['w3'][bucket].astype(np.int64)+heads['b3'][bucket,0]
        psqt=meta[m,1].astype(np.int64); e=meta[m,2]
        raw[m]=np.trunc(np.trunc(((128-e)*psqt+(128+e)*out)/128)/16).astype(np.int64)
    board=np.trunc(raw*meta[:,3]/1024).astype(np.int64)
    board=np.trunc(board*(100-meta[:,4])/100).astype(np.int64)
    return raw, np.clip(board+100*meta[:,5],-31506,31506)


def ste(x,fn):
    return x+(fn(x)-x).detach()


class Head(nn.Module):
    def __init__(self,parameters):
        super().__init__()
        self.p=nn.ParameterDict({k:nn.Parameter(torch.tensor(v,dtype=torch.float32)) for k,v in parameters.items()})

    def forward(self,x,meta):
        out=torch.zeros(len(x),device=x.device)
        p={k:ste(v,torch.round) for k,v in self.p.items()}
        for bucket in range(8):
            m=meta[:,0]==bucket
            h1=ste((x[m]@p['w1'][bucket].T+p['b1'][bucket])/64,torch.floor).clamp(0,127)
            h2=ste((h1@p['w2'][bucket].T+p['b2'][bucket])/64,torch.floor).clamp(0,127)
            out[m]=h2@p['w3'][bucket]+p['b3'][bucket,0]
        # Exact integer arithmetic in the final scaling can exceed float32's
        # mantissa. Float64 here preserves C++ truncation; dense products fit 24 bits.
        out=out.to(torch.float64); meta=meta.to(torch.float64)
        e=meta[:,2]
        raw=ste(ste(((128-e)*meta[:,1]+(128+e)*out)/128,torch.trunc)/16,torch.trunc)
        board=ste(raw*meta[:,3]/1024,torch.trunc)
        board=ste(board*(100-meta[:,4])/100,torch.trunc)
        return (board+100*meta[:,5]).clamp(-31506,31506)

    def constrain(self):
        with torch.no_grad():
            for k,p in self.p.items():
                p.clamp_(-127 if k.startswith('w') else -(1<<20),127 if k.startswith('w') else (1<<20))


def export(model,base,path):
    data=bytearray(base_bytes(base))
    data+=b'PTHD0001'+struct.pack('<5I',1,32,16,32,8)
    p={k:np.rint(v.detach().cpu().numpy()) for k,v in model.p.items()}
    for i in range(8):
        for k,dtype,arr in [('b1','<i4',p['b1'][i]),('w1','i1',p['w1'][i,:,:1024]),
                            ('b2','<i4',p['b2'][i]),('w2','i1',np.pad(p['w2'][i],((0,0),(0,16)))),
                            ('b3','<i4',p['b3'][i]),('w3','i1',p['w3'][i]),('w1','i1',p['w1'][i,:,1024:])]:
            if np.any(arr < (-127 if k.startswith('w') else -(1<<20))) or np.any(arr > (127 if k.startswith('w') else (1<<20))):
                raise ValueError('Unrepresentable parameter '+k)
            data+=arr.astype(dtype).tobytes()
    data+=hashlib.sha256(data).hexdigest().encode()
    assert len(data)==BASE_BYTES+EXT_BYTES
    Path(path).write_bytes(data)
    return hashlib.sha256(data).hexdigest()
