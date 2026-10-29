"""Exact version-1 feature/quantization contract for the frozen run4test adapter."""
import hashlib
import struct
import numpy as np

LEGACY_SIZE=52046752
LEGACY_SHA='345d67b733991ae5beb2d3a6ea40d54ea01abb1fb699aa39e6f6406495f4d933'
MAGIC=b'PTADPT01'


def features(fen, baseline):
    board,side,_,_,half,_,points=fen.split()
    scores=list(map(int,points.removeprefix('points=').split(',')))
    own,other=(scores if side=='w' else scores[::-1])
    delta=own-other
    counts=[[],[]]
    for i,white in enumerate([side=='w',side!='w']):
        counts[i]=[board.count(pc.upper() if white else pc) for pc in 'pnbrq']
    walls=board.count('*')
    x=[int(np.clip(baseline-100*int(np.clip(delta,-200,200)),-8192,8192)/4),int(np.clip(delta,-40,40)*256/20)]
    for pt,scale in enumerate([32,64,64,64,128]):
        x.extend(min(counts[c][pt]*scale,2048) for c in [0,1])
    x.extend([walls*16,min(int(half),100)*256//100])
    for pieces in counts:
        insufficient=sum(pieces)==0 or (sum(pieces)==1 and walls==0 and pieces[1]+pieces[2]==1)
        x.append(0 if insufficient else 256)
    return np.array(x,dtype=np.int16)


def export(net, legacy, destination):
    base=legacy.read_bytes()
    assert len(base)==LEGACY_SIZE and hashlib.sha256(base).hexdigest()==LEGACY_SHA
    p={k:v.detach().cpu().numpy() for k,v in net.state_dict().items()}
    b1=np.rint(p['0.bias']*65536).astype('<i4')
    w1=np.rint(p['0.weight']*256).astype('<i2')
    w2=np.rint(p['2.weight']*256).astype('<i2')
    b2=np.rint(p['2.bias']*256).astype('<i4')
    payload=base+MAGIC+struct.pack('<III',1,16,16)+b1.tobytes()+w1.tobytes()+w2.tobytes()+b2.tobytes()
    payload+=hashlib.sha256(payload).hexdigest().encode('ascii')
    destination.write_bytes(payload)
    return hashlib.sha256(payload).hexdigest()


def read_adapter(path):
    data=path.read_bytes()
    assert len(data)==LEGACY_SIZE+696
    assert hashlib.sha256(data[:-64]).hexdigest().encode()==data[-64:]
    assert hashlib.sha256(data[:LEGACY_SIZE]).hexdigest()==LEGACY_SHA
    assert data[LEGACY_SIZE:LEGACY_SIZE+8]==MAGIC
    assert struct.unpack_from('<III',data,LEGACY_SIZE+8)==(1,16,16)
    offset=LEGACY_SIZE+20
    b=np.frombuffer(data,dtype='<i4',count=16,offset=offset).astype(np.int64);offset+=64
    w=np.frombuffer(data,dtype='<i2',count=256,offset=offset).reshape(16,16).astype(np.int64);offset+=512
    o=np.frombuffer(data,dtype='<i2',count=16,offset=offset).astype(np.int64);offset+=32
    bias=struct.unpack_from('<i',data,offset)[0]
    return b,w,o,bias


def integer_forward(x, params):
    b,w,o,bias=params
    hidden=np.clip(np.trunc((np.asarray(x,dtype=np.int64)@w.T+b)/256).astype(np.int64),0,1024)
    return np.clip(np.trunc((hidden@o)/256).astype(np.int64)+bias,-4000,4000)
