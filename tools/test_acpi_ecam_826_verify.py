#!/usr/bin/env python3
"""Synthetic S826 records inserted into authentic S824 UART; not hardware proof."""
import sys,struct
from pathlib import Path
from acpi_ecam_826_verify import verify,POLICY
from drac_scope_826 import scope

def main(folder):
    raw=(folder/'serial.log').read_bytes().replace(b'\r',b'')
    crs=bytes.fromhex(scope((folder/'dsdt.aml').read_bytes())['resource_hex'])
    start,end=0xe0000000,0xf0000000
    def desc(t,a,b,attr=0):return struct.pack('<I4xQQQQ',t,a,0,(b-a)//4096,attr)+bytes(8)
    def records(data, crs=crs):
        return (f'STYLEOS S826 FINAL-MAP size=0x{len(data):016x} stride=0x0000000000000030 version=0x0000000000000001 data={data.hex()}\n'
                f'STYLEOS S826 DRAC path=\\_SB.DRAC hid=PNP0C01 crs={crs.hex()}\n{POLICY}\n').encode()
    anchor=b'STYLEOS S823 PASS\n';assert raw.count(anchor)==1
    def insert(rec):return raw.replace(anchor,rec+anchor)
    rec=records(desc(11,start,end));good=insert(rec)
    n=0
    for sample,status in [(good,'EXPLICIT_NON_RAM_COVERAGE'),(insert(records(desc(7,0x1000,0x2000))),'GAP'),(insert(records(desc(0,start,start+4096)+desc(11,start+4096,end))),'EXPLICIT_NON_RAM_COVERAGE')]:
        ok,d=verify(sample,33,'disabled');assert ok and d['s826_map_audit']['status']==status and not d['s826_ecam_reservation_verified'];n+=1
    malformed=[raw,insert(rec+rec),insert(records(desc(7,start,end))),insert(records(desc(11,start,end,1<<63))),insert(records(desc(11,start,end)+desc(0,start,start+4096))),insert(records(desc(11,start,end)[:-1])),insert(records(desc(11,start,end),crs[:-1]+b'\x01')),good.replace(b'path=\\_SB.DRAC',b'path=\\_SB.PCI0.DRAC'),good.replace(b'hid=PNP0C01',b'hid=PNP0C02'),good.replace(b'stride=0x0000000000000030',b'stride=0x0000000000000031'),good.replace(b'version=0x0000000000000001',b'version=0x0000000000000002'),good.replace(b'S826 POLICY observation-only=yes',b'S826 POLICY observation-only=no'),raw+rec,good+b'\x00']
    for sample in malformed: assert not verify(sample,33,'disabled')[0];n+=1
    for rc in (None,0,32,34):assert not verify(good,rc,'disabled')[0];n+=1
    print(f'S826 verifier synthetic {n}/{n} PASS; no new hardware execution')
if __name__=='__main__':main(Path(sys.argv[1]))
