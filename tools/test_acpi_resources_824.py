#!/usr/bin/env python3
"""S824 deterministic parser fixtures and forged-UART refusals; NOT hardware."""
import argparse
from pathlib import Path
from acpi_resources_824_verify import resources,bus_set,mcfg,verify,POLICY

def addr(tag,kind,lo,hi,length,flags=12,translation=0):
    w={0x88:2,0x87:4,0x8a:8}[tag]
    b=bytes([kind,flags,0])+b''.join(x.to_bytes(w,'little') for x in (0,lo,hi,translation,length))
    return bytes([tag])+len(b).to_bytes(2,'little')+b

def table():
    t=bytearray(60);t[:4]=b'MCFG';t[4:8]=(60).to_bytes(4,'little');t[8]=1
    t[44:52]=(0xe0000000).to_bytes(8,'little');t[55]=255;t[9]=(-sum(t))%256
    return bytes(t)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--parent-uart',required=True,type=Path);args=ap.parse_args()
    end=b'\x79\x00';bus=addr(0x88,2,0,255,256);mem=addr(0x87,0,0x80000000,0x8fffffff,0x10000000)
    crs=bus+mem+end;uefi=addr(0x8a,2,0,255,256,0)+end
    assert bus_set(resources(crs))==set(range(256));assert bus_set(resources(uefi,True))==set(range(256))
    assert mcfg(table())[1]==set(range(256))
    cases=0
    for broken in (b'',crs[:-1],crs+b'0',crs[:-1]+b'\x01',bus+bus+end,
                   addr(0x88,2,0,255,255)+end,addr(0x88,2,0,255,256,translation=1)+end,
                   addr(0x88,2,0,255,256,flags=0)+end,b'\x90\x00\x00'+end,
                   addr(0x87,0,1,0,1)+bus+end):
        try: bus_set(resources(broken))
        except ValueError: cases+=1
        else: raise AssertionError('accepted bad resource')
    base=args.parent_uart.read_bytes().replace(b'\r',b'')
    import re
    base=re.sub(rb'crs-bytes=0x[0-9a-f]{16}',f'crs-bytes=0x{len(crs):016x}'.encode(),base,count=1)
    def line(kind,b):return f'STYLEOS S824 {kind} data={b.hex()}\n'.encode()
    first=line('CRS',crs);rest=(line('MCFG',table())+line('UEFI index=0x0000000000000000 segment=0x0000000000000000',uefi)+line('BUSES',b'\x01'+bytes(31))+(POLICY+'\n').encode())
    anchor=b'STYLEOS S821 PASS\n';before=b'STYLEOS S816 PLATFORM'
    assert base.count(anchor)==base.count(before)==1
    fixture=base.replace(anchor,anchor+first).replace(before,rest+before)
    ok,d=verify(fixture,33,'disabled');assert ok and d['s824_live_bus_coverage'] and not d['s824_resource_ownership']
    bad=[base,fixture.replace(first,b''),fixture.replace(first,first+first),fixture.replace(first,line('CRS',crs[:-1])),
         fixture.replace(line('MCFG',table()),line('MCFG',table()[:-1]+b'\x01')),
         fixture.replace(line('BUSES',b'\x01'+bytes(31)),line('BUSES',b'\x02'+bytes(31))),
         fixture.replace(b'index=0x0000000000000000 segment=',b'index=0x0000000000000001 segment='),
         fixture.replace(b'segment=0x0000000000000000 data=',b'segment=0x0000000000000001 data='),
         fixture.replace(POLICY.encode(),POLICY.replace('dma-grants=no','dma-grants=yes').encode()),
         fixture.replace(first,b'')+first,fixture+b'\x00']
    for broken in bad:
        assert not verify(broken,33,'disabled')[0];cases+=1
    for rc in (None,0,32,34):assert not verify(fixture,rc,'disabled')[0];cases+=1
    print(f'S824 synthetic negative {cases}/{cases} PASS; NOT hardware')
if __name__=='__main__':main()
