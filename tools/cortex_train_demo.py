#!/usr/bin/env python3
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.
import argparse, struct, zlib
from pathlib import Path
import torch
import torch.nn as nn
import torch.nn.functional as F

MAGIC=0x314D5743
VERSION=1
QUANT_Q8=1

CORPUS = ("""
Cell OS is ready.\n
cell> hello\nHello. I am Cell Cortex.\n
cell> who are you\nI am the personal intelligence of Cell OS.\n
cell> status\nCPU ready. Memory ready. Storage ready.\n
cell> help\nI can inspect capabilities, reason, and control Cell OS.\n
cell> memory\nMemory capability is available.\n
cell> storage\nStorage capability is available.\n
cell> cpu\nCPU capability is available.\n
cell> hello\nHello. I am Cell Cortex.\n
cell> status\nCPU ready. Memory ready. Storage ready.\n
""" * 80).encode('utf-8')

class RMSNorm(nn.Module):
    def __init__(self,d):
        super().__init__(); self.weight=nn.Parameter(torch.ones(d))
    def forward(self,x):
        return x * torch.rsqrt(x.pow(2).mean(-1,keepdim=True)+1e-5) * self.weight

class Block(nn.Module):
    def __init__(self,d,h,ff):
        super().__init__(); self.d=d; self.h=h; self.hd=d//h
        self.n1=RMSNorm(d); self.q=nn.Linear(d,d,bias=False); self.k=nn.Linear(d,d,bias=False); self.v=nn.Linear(d,d,bias=False); self.o=nn.Linear(d,d,bias=False)
        self.n2=RMSNorm(d); self.w1=nn.Linear(d,ff,bias=False); self.w2=nn.Linear(ff,d,bias=False)
    def forward(self,x):
        b,t,d=x.shape
        z=self.n1(x)
        q=self.q(z).view(b,t,self.h,self.hd).transpose(1,2)
        k=self.k(z).view(b,t,self.h,self.hd).transpose(1,2)
        v=self.v(z).view(b,t,self.h,self.hd).transpose(1,2)
        a=F.scaled_dot_product_attention(q,k,v,is_causal=True)
        a=a.transpose(1,2).contiguous().view(b,t,d)
        x=x+self.o(a)
        z=self.n2(x)
        x=x+self.w2(F.relu(self.w1(z)))
        return x

class TinyCellLM(nn.Module):
    def __init__(self,vocab=256,ctx=64,d=48,layers=2,heads=4,ff=96):
        super().__init__(); self.vocab=vocab; self.ctx=ctx; self.d=d; self.layers_n=layers; self.heads=heads; self.ff=ff
        self.tok=nn.Embedding(vocab,d); self.pos=nn.Embedding(ctx,d)
        self.blocks=nn.ModuleList([Block(d,heads,ff) for _ in range(layers)])
        self.final=RMSNorm(d)
    def forward(self,idx):
        t=idx.shape[1]
        x=self.tok(idx)+self.pos(torch.arange(t,device=idx.device))[None,:,:]
        for b in self.blocks: x=b(x)
        x=self.final(x)
        return x @ self.tok.weight.t()

def q8_blob(t):
    a=t.detach().cpu().float().contiguous()
    mx=float(a.abs().max())
    scale=mx/127.0 if mx else 1.0
    q=torch.clamp(torch.round(a/scale),-127,127).to(torch.int8).numpy().tobytes()
    return struct.pack('<f',scale)+q

def export(model,path):
    payload=bytearray()
    payload += q8_blob(model.tok.weight)
    payload += q8_blob(model.pos.weight)
    for b in model.blocks:
        payload += b.n1.weight.detach().cpu().float().numpy().tobytes()
        payload += q8_blob(b.q.weight)
        payload += q8_blob(b.k.weight)
        payload += q8_blob(b.v.weight)
        payload += q8_blob(b.o.weight)
        payload += b.n2.weight.detach().cpu().float().numpy().tobytes()
        payload += q8_blob(b.w1.weight)
        payload += q8_blob(b.w2.weight)
    payload += model.final.weight.detach().cpu().float().numpy().tobytes()
    header_bytes=64
    total=header_bytes+len(payload)
    crc=zlib.crc32(payload)&0xffffffff
    hdr=struct.pack('<IHHIIIIIIIQQIII',MAGIC,VERSION,header_bytes,QUANT_Q8,model.vocab,model.ctx,model.d,model.layers_n,model.heads,model.ff,len(payload),total,crc,0,0)
    assert len(hdr)==64
    Path(path).write_bytes(hdr+payload)
    print(f'# cortex-model: {path} bytes={total} crc={crc:08x} d={model.d} layers={model.layers_n} heads={model.heads}')

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('output'); ap.add_argument('--steps',type=int,default=700); ap.add_argument('--seed',type=int,default=7)
    a=ap.parse_args(); torch.manual_seed(a.seed); torch.set_num_threads(4)
    ctx=64; m=TinyCellLM(ctx=ctx)
    opt=torch.optim.AdamW(m.parameters(),lr=3e-3,weight_decay=0.01)
    data=torch.tensor(list(CORPUS),dtype=torch.long)
    g=torch.Generator().manual_seed(a.seed)
    for step in range(a.steps):
        starts=torch.randint(0,len(data)-ctx-1,(24,),generator=g)
        x=torch.stack([data[s:s+ctx] for s in starts])
        y=torch.stack([data[s+1:s+ctx+1] for s in starts])
        logits=m(x); loss=F.cross_entropy(logits.reshape(-1,256),y.reshape(-1))
        opt.zero_grad(); loss.backward(); torch.nn.utils.clip_grad_norm_(m.parameters(),1.0); opt.step()
        if step%100==0 or step==a.steps-1: print(f'step={step} loss={loss.item():.4f}')
    export(m,a.output)

if __name__=='__main__': main()
