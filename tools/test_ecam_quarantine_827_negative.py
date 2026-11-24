#!/usr/bin/env python3
"""Synthetic records in genuine S826 UART; not guest proof."""
import sys
from pathlib import Path
from acpi_ecam_826_verify import verify as parent,POLICY as PARENT
from ecam_quarantine_827_verify import verify,expected,records,POLICY

def main(folder):
    raw=(folder/'serial.log').read_bytes().replace(b'\r',b'')
    ok,d=parent(raw,33,'disabled');assert ok
    rows=records(expected(d));rec=('\n'.join(rows)+'\n').encode();anchor=(PARENT+'\n').encode()
    assert raw.count(anchor)==1
    good=raw.replace(anchor,anchor+rec);assert verify(good,33,'disabled')[0]
    row=lambda r:(r+'\n').encode()
    candidate=next(r for r in rows if r.startswith('STYLEOS S827 CANDIDATE '))
    reserved=next(r for r in rows if r.startswith('STYLEOS S827 RESERVED '))
    bad=[raw,good.replace(rec,rec+rec),good.replace(row(candidate),b''),good.replace(row(reserved),b''),good.replace(row(candidate),b'STYLEOS S827 CANDIDATE 0x00000000e0000000 0x00000000f0000000\n'),good.replace(row(POLICY),row(POLICY.replace('ram-grants=no','ram-grants=yes'))),good.replace(row(POLICY),row(POLICY.replace('dma-grants=no','dma-grants=yes'))),good.replace(row(POLICY),row(POLICY.replace('ecam=not-read','ecam=read'))),raw+rec,good.replace(row(rows[0]),row(rows[0].replace('gap-pages=0x0000000000000000','gap-pages=0x0000000000000001'))),good.replace(row(candidate),row(candidate)+row(candidate)),good+b'\x00']
    for b in bad:assert not verify(b,33,'disabled')[0]
    for rc in (None,0,32,34):assert not verify(good,rc,'disabled')[0]
    print(f'S827 synthetic negative {len(bad)+4}/{len(bad)+4} PASS; NOT hardware')
if __name__=='__main__':main(Path(sys.argv[1]))
