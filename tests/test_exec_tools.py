#!/usr/bin/env python3
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
#
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
ASM=ROOT/'tools'/'cellasm.py'
MKFS=ROOT/'scripts'/'mkcellfs.py'
INSTALL=ROOT/'scripts'/'cellfs_install.py'
MAGIC=0x31584543
INODE_BYTES=128

def inode_names(raw):
    names=[]
    for i in range(64):
        off=512+i*INODE_BYTES
        inode=raw[off:off+INODE_BYTES]
        if len(inode)!=INODE_BYTES: break
        kind=inode[8]
        if kind:
            names.append(inode[28:92].split(b'\0',1)[0].decode('ascii'))
    return names

def main():
    with tempfile.TemporaryDirectory() as td:
        d=Path(td); fs=d/'fs.img'; src=d/'p.cellasm'; out=d/'tool.cellx'
        subprocess.run([sys.executable,str(MKFS),str(fs),'--sectors','64'],check=True,stdout=subprocess.DEVNULL)
        src.write_text('.gas 16\n.memory 0\n.data m "tool test\\n"\nstart:\nputs m\nhalt 0\n',encoding='utf-8')
        subprocess.run([sys.executable,str(ASM),str(src),str(out)],check=True,stdout=subprocess.DEVNULL)
        raw=out.read_bytes(); assert len(raw)>=64
        magic,ver,hbytes,ibytes,code_bytes,data_bytes,entry=struct.unpack_from('<IHHIIII',raw,0)
        assert (magic,ver,hbytes,ibytes)==(MAGIC,1,64,8)
        assert code_bytes==16 and data_bytes==10 and entry==0

        # Prove migration from the earlier proof-era .cellx user-visible name.
        subprocess.run([sys.executable,str(INSTALL),str(fs),str(out),'--name','tool.cellx'],check=True,stdout=subprocess.DEVNULL)
        legacy=fs.read_bytes(); assert 'tool.cellx' in inode_names(legacy)
        subprocess.run([sys.executable,str(INSTALL),str(fs),str(out),'--name','tool'],check=True,stdout=subprocess.DEVNULL)
        first=fs.read_bytes(); names=inode_names(first)
        assert 'tool' in names and 'tool.cellx' not in names
        gen1=struct.unpack_from('<I',first,40)[0]
        subprocess.run([sys.executable,str(INSTALL),str(fs),str(out),'--name','tool'],check=True,stdout=subprocess.DEVNULL)
        second=fs.read_bytes(); assert second==first
        gen2=struct.unpack_from('<I',second,40)[0]; assert gen2==gen1

        # An unrelated tail marker must not be touched by installer activity.
        marker=bytearray(second); marker[-32:]=bytes(range(32)); fs.write_bytes(marker)
        subprocess.run([sys.executable,str(INSTALL),str(fs),str(out),'--name','tool'],check=True,stdout=subprocess.DEVNULL)
        assert fs.read_bytes()[-32:]==bytes(range(32))

        bad=d/'bad.cellasm'; badout=d/'bad.cellx'
        bad.write_text('cap memory.status\nhalt 0\n',encoding='utf-8')
        p=subprocess.run([sys.executable,str(ASM),str(bad),str(badout)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        assert p.returncode!=0
    print('#CELLEXEC assembler ABI PASS')
    print('#CELLFS program install/idempotence PASS')
    print('#CELLFS legacy program-name migration PASS')
    print('#CELLEXEC explicit capability declaration PASS')
if __name__=='__main__': main()
