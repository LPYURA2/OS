#!/usr/bin/env python3
"""Synthetic tables and forged inventory records; not new QEMU execution."""
import sys,struct
from pathlib import Path
from iommu_evidence_828_verify import decode,topology,verify,POLICY

def table(sig,blocks):
    b=bytearray(48);b[:4]=sig;b[8]=1
    if sig==b'DMAR':b[36]=47
    b.extend(b''.join(blocks));struct.pack_into('<I',b,4,len(b));b[9]=(-sum(b))%256
    return bytes(b)

def drhd(scopes,base=0xfed90000,flags=1):
    return struct.pack('<HHBBHQ',0,16+len(scopes),flags,0,0,base)+scopes

def scope(k,bus,*pairs):return bytes([k,6+len(pairs),0,0,0,bus,*pairs])

def ivhd(entries,base=0xfed80000):
    return struct.pack('<BBHHHQHHI',0x10,0,24+len(entries),0x10,0,base,0,0,0)+entries

def entry(k,rid,target=None):
    return struct.pack('<BHB',k,rid,0)+(struct.pack('<BHB',0,target,0) if target is not None else b'')

def main(folder):
    rows=[(8,1,0,0),(16,2,1,2),(0x108,2,2,2),(0x210,1,0,0)]
    f,b=topology(rows);assert len(b)==3
    dm=table(b'DMAR',[drhd(scope(1,0,1,0)+scope(1,0,2,0,1,0,2,0)+scope(2,0,2,0))])
    d=decode(dm,1,f,b);assert d['exact']==[(8,0,1),(0x210,0,1)] and d['bridge_scopes']==1
    iv=table(b'IVRS',[ivhd(entry(2,8)+entry(0x42,16,8)+entry(3,0x210)+entry(4,0x211))])
    d=decode(iv,2,f,b);assert d['exact']==[(8,0,2),(0x210,0,2),(0x211,0,2)] and d['aliases']==[(16,8,0)]
    assert decode(b'',0,f,b)['units']==[]
    n=4
    def bad(fn):
        nonlocal n
        try:fn()
        except ValueError:n+=1
        else:raise AssertionError('unexpected acceptance')
    for badrows in (rows[::-1],rows+[rows[0]],[(8,3,0,0)],[(8,1,1,1)],[(0x108,1,0,0)],[(8,2,1,1),(16,2,1,1)],[(8,2,1,1),(0x108,2,2,2)],[(0x108,2,2,2),(0x208,2,1,2)]):bad(lambda r=badrows:topology(r))
    for blob,backend in [(dm,0),(dm,2),(dm[:-1],1),(dm[:-1]+bytes([dm[-1]^1]),1),(table(b'DMAR',[drhd(b'',flags=0)]),1),(table(b'DMAR',[drhd(scope(1,0,1,0)),drhd(scope(1,0,1,0),base=0xfeda0000)]),1),(table(b'DMAR',[drhd(b''),drhd(b'')]),1),(table(b'IVRS',[ivhd(entry(3,8))]),2),(table(b'IVRS',[ivhd(entry(4,8))]),2),(table(b'IVRS',[ivhd(entry(2,8)+entry(0x42,8,16))]),2),(table(b'IVRS',[ivhd(entry(0x42,16,8)+entry(0x42,16,24))]),2),(table(b'IVRS',[ivhd(entry(2,8)),ivhd(entry(2,8),base=0xfed90000)]),2)]:bad(lambda blob=blob,backend=backend:decode(blob,backend,f,b))
    raw=(folder/'serial.log').read_bytes().replace(b'\r',b'');anchor=b'STYLEOS S816 PLATFORM ';assert raw.count(anchor)==1
    # The disabled parent supplies counts only. These RIDs are synthetic.
    import re
    count=int(re.search(rb'STYLEOS S816 TOPOLOGY buses=0x[0-9a-f]{16} functions=0x([0-9a-f]{16})',raw)[1],16)
    hx=lambda x:f'0x{x:016x}'
    records=['STYLEOS S828 TABLE backend='+hx(0)+' data=none']+[f'STYLEOS S828 FUNCTION rid={hx(i)} kind={hx(1)} secondary={hx(0)} subordinate={hx(0)}' for i in range(count)]+[POLICY]
    rec=('\n'.join(records)+'\n').encode();good=raw.replace(anchor,rec+anchor);assert verify(good,33,'disabled')[0];n+=1
    for forged in (raw,good.replace(rec,rec+rec),good.replace((records[1]+'\n').encode(),b''),good.replace(records[1].encode(),records[2].encode()),good.replace(b'data=none',b'data=00'),good.replace(POLICY.encode(),POLICY.replace('dma-grants=no','dma-grants=yes').encode()),good.replace(rec,b'')+rec,good+b'\x00'):
        assert not verify(forged,33,'disabled')[0];n+=1
    for rc in (None,0,32,34):assert not verify(good,rc,'disabled')[0];n+=1
    print(f'S828 {n}/{n} synthetic decoder and UART checks PASS; not hardware proof')
if __name__=='__main__':main(Path(sys.argv[1]))
