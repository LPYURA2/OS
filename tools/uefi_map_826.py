#!/usr/bin/env python3
"""Offline candidate verifier for future S826 final UEFI map evidence.
No firmware calls, no ECAM mapping, no ownership. Synthetic selftests only
unless supplied original same-boot bytes. A missing descriptor is not a reservation.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

PAGE = 4096
MAX = 1 << 64
RUNTIME = 1 << 63
MMIO = 11
RESERVED = 0
MAX_MAP = 8 * 1024 * 1024

def require(ok, why):
    if not ok:
        raise ValueError(why)

def parse(raw, stride, version):
    require(version == 1 and 40 <= stride <= 4096, 'unsupported descriptor layout')
    require(0 < len(raw) <= MAX_MAP and len(raw) % stride == 0, 'bad map extent')
    require(len(raw)//stride <= 8192, 'descriptor count limit')
    entries = []
    for offset in range(0, len(raw), stride):
        typ, lo, virtual, pages, attr = struct.unpack_from('<I4xQQQQ', raw, offset)
        require(lo % PAGE == 0 and pages > 0 and pages <= (MAX-lo)//PAGE, 'bad physical extent')
        entries.append({'type':typ, 'start':lo, 'end_exclusive':lo+pages*PAGE, 'attributes':attr, 'index':offset//stride})
    entries.sort(key=lambda v: v['start'])
    require(all(a['end_exclusive'] <= b['start'] for a,b in zip(entries,entries[1:])), 'overlapping UEFI descriptors')
    return entries

def audit(raw, stride, version, start, end):
    require(0 <= start < end <= MAX and start % PAGE == end % PAGE == 0, 'invalid ECAM interval')
    entries = parse(raw,stride,version)
    covered, gaps, conflicts = [], [], []
    cursor = start
    for entry in entries:
        lo, hi = max(start,entry['start']), min(end,entry['end_exclusive'])
        if lo >= hi: continue
        if cursor < lo: gaps.append([cursor,lo])
        hit = {**entry,'intersection':[lo,hi]}
        if entry['type'] in (RESERVED,MMIO) and not entry['attributes'] & RUNTIME:
            covered.append(hit)
        else:
            conflicts.append(hit)
        cursor = hi
    if cursor < end: gaps.append([cursor,end])
    status = 'CONFLICT' if conflicts else 'GAP' if gaps else 'EXPLICIT_NON_RAM_COVERAGE'
    return {'status':status,'map_sha256':hashlib.sha256(raw).hexdigest(),'descriptor_version':version,'descriptor_size':stride,'descriptor_count':len(entries),'ecam':[start,end], 'covered':covered,'gaps':gaps,'conflicts':conflicts,'explicit_non_ram_coverage':status=='EXPLICIT_NON_RAM_COVERAGE','same_boot_proven':False,'ecam_reservation_verified':False,'ecam_access_authorized':False,'dma_authorized':False,'production_cr3_authorized':False}

def selftest():
    a,b=0xe0000000,0xf0000000
    def desc(t,lo,hi,attr=0,stride=48):
        return struct.pack('<I4xQQQQ',t,lo,0,(hi-lo)//PAGE,attr)+bytes(stride-40)
    good=desc(MMIO,a,b)
    cases=[('MMIO',good,48,1,'EXPLICIT_NON_RAM_COVERAGE'),('reserved',desc(0,a,b),48,1,'EXPLICIT_NON_RAM_COVERAGE'),('adjacent mixed',desc(0,a,a+PAGE)+desc(11,a+PAGE,b),48,1,'EXPLICIT_NON_RAM_COVERAGE'),('out of order',desc(11,a+PAGE,b)+desc(0,a,a+PAGE),48,1,'EXPLICIT_NON_RAM_COVERAGE'),('extra outside',desc(7,0x1000,0x2000)+good,48,1,'EXPLICIT_NON_RAM_COVERAGE'),('40 byte',desc(11,a,b,stride=40),40,1,'EXPLICIT_NON_RAM_COVERAGE'),('missing',desc(7,0x1000,0x2000),48,1,'GAP'),('head gap',desc(11,a+PAGE,b),48,1,'GAP'),('middle gap',desc(0,a,a+PAGE)+desc(11,a+2*PAGE,b),48,1,'GAP'),('tail gap',desc(11,a,b-PAGE),48,1,'GAP'),('conventional',desc(7,a,b),48,1,'CONFLICT'),('loader data',desc(2,a,b),48,1,'CONFLICT'),('runtime code',desc(5,a,b),48,1,'CONFLICT'),('MMIO runtime attribute',desc(11,a,b,RUNTIME),48,1,'CONFLICT'),('unknown',desc(0x80000000,a,b),48,1,'CONFLICT'),('I/O port type',desc(12,a,b),48,1,'CONFLICT'),('conflict with gap',desc(7,a,a+PAGE),48,1,'CONFLICT'),('overlap',good+desc(0,a,a+PAGE),48,1,'REJECT'),('zero pages',desc(11,a,a),48,1,'REJECT'),('misaligned',desc(11,a+1,b+1),48,1,'REJECT'),('overflow',struct.pack('<I4xQQQQ',11,MAX-PAGE,0,2,0)+bytes(8),48,1,'REJECT'),('truncated',good[:-1],48,1,'REJECT'),('wrong stride',good,39,1,'REJECT'),('wrong version',good,48,2,'REJECT'),('empty',b'',48,1,'REJECT')]
    for name,raw,stride,version,want in cases:
        try:
            result=audit(raw,stride,version,a,b)
            got=result['status']
            assert not any(result[k] for k in ('same_boot_proven','ecam_reservation_verified','ecam_access_authorized','dma_authorized','production_cr3_authorized'))
        except ValueError: got='REJECT'
        assert got==want,(name,got,want)
        print(name,got,'PASS')
    print(f'{len(cases)}/{len(cases)} synthetic cases PASS; no hardware proof')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--selftest',action='store_true');p.add_argument('--map',type=Path);p.add_argument('--stride',type=int);p.add_argument('--version',type=int,default=1);p.add_argument('--start',type=lambda x:int(x,0),default=0xe0000000);p.add_argument('--end',type=lambda x:int(x,0),default=0xf0000000);p.add_argument('--output',type=Path);a=p.parse_args()
    if a.selftest: selftest();return
    if not a.map or a.stride is None or not a.output: p.error('--map, --stride and --output are required')
    r=audit(a.map.read_bytes(),a.stride,a.version,a.start,a.end);a.output.write_text(json.dumps(r,indent=2)+'\n')
    print(r['status'],'(offline data only; no hardware grants)')
if __name__=='__main__': main()
