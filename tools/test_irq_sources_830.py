import struct, sys, json
from pathlib import Path
from irq_sources_830 import inspect, madt_ioapics
from test_iommu_evidence_828 import table, drhd, scope, ivhd, entry

def seal(raw):
    r=bytearray(raw);r[4:8]=len(r).to_bytes(4,'little');r[9]=0;r[9]=(-sum(r))%256;return bytes(r)
def madt(*items):
    b=bytearray(44);b[:4]=b'APIC';b[8]=1;b[36:40]=(0xfee00000).to_bytes(4,'little');b.extend(b''.join(items));return seal(b)
def io(id=0,addr=0xfec00000,gsi=0):return struct.pack('<BBBBII',1,12,id,0,addr,gsi)
def dm(sc,flags=1):
    b=bytearray(table(b'DMAR',[drhd(sc)]));b[37]=flags;return seal(b)
def amd(s):return table(b'IVRS',[ivhd(entry(2,8)+s)])
rows=[(8,1,0,0),(16,1,0,0)]
scope0=bytes.fromhex('0308000000ff0000');special0=bytes.fromhex('4800000000a00001')
count=0
for raw,b,source in [(dm(scope0),1,0xff00),(amd(special0),2,0xa0)]:
    for m in (None,madt(io())):
        r=inspect(raw,b,rows,m);s=r['special_sources'][0];assert s['source_rid']==source and s['ioapic_id']==0 and s['madt_id_matched']==(m is not None)
        assert not any(r[k] for k in ('blockers_removed','ownership','interrupt_remapping_authorized','dma_authorized','production_cr3_authorized','same_boot_madt_proven'));count+=1
assert not inspect(b'',0,rows)['special_sources'];count+=1

def reject(fn):
    global count
    try:fn()
    except ValueError:count+=1
    else:raise AssertionError('accepted bad fixture')
for raw,b,r,m in [(dm(scope0,0),1,rows,None),(dm(bytes.fromhex('0308010000ff0000')),1,rows,None),(dm(bytes.fromhex('0308000000ff2000')),1,rows,None),(dm(bytes.fromhex('0308000000ff0008')),1,rows,None),(dm(scope(3,0,1,0,2,0)),1,rows,None),(dm(scope(3,0,1,0)),1,rows,None),(dm(scope0+scope0),1,rows,None),(dm(scope0),1,rows,madt(io(1))),(dm(scope0),1,rows,madt(io(),io(1,0xfec01000,24))),(amd(bytes.fromhex('4801000000a00001')),2,rows,None),(amd(bytes.fromhex('4800000100a00001')),2,rows,None),(amd(bytes.fromhex('4800000000a00002')),2,rows,None),(amd(bytes.fromhex('4800000000080001')),2,rows,None),(amd(special0+special0),2,rows,None),(amd(special0),2,rows+[(0xa0,1,0,0)],None),(amd(special0),2,rows,madt(io(1))),(amd(special0),2,rows,madt(io(),io(1,0xfec01000,24)))]:
    reject(lambda raw=raw,b=b,r=r,m=m:inspect(raw,b,r,m))
for blob in (madt(io())[:-1],madt(io())[:-1]+b'\x01',madt(io()+io()),madt(io(0,0)),madt(io(0,0xfec00001)),madt(bytes([1,0])),madt(bytes([1,2])),madt(),madt(io()+io(1,0xfec00000,24)),madt(io()+io(1,0xfec01000,0))):reject(lambda b=blob:madt_ioapics(b))
print(f'{count}/{count} synthetic checks PASS; no same-boot MADT or hardware grants')
