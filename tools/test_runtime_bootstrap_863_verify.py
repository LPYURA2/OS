#!/usr/bin/env python3
"""Bind ordinary Desktop startup markers to a linked shared bootstrap and actual guest boots."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
from fault_site import _unpack, _range

MARKERS = (
    b'STYLEOS kernel_main entered',
    b'STYLEOS boot_info validated v1',
    b'STYLEOS HANDOFF PASS x86_64',
    b'STYLEOS DESKTOP SHARED BOOTSTRAP PASS',
    b'STYLEOS DESKTOP WINDOW IPC READY ports=0',
    b'STYLEOS DESKTOP USER SESSION SERVICE READY sessions=0',
    b'STYLEOS DESKTOP SCHEDULER READY registered=0',
    b'STYLEOS DESKTOP PROCESS SERVICE READY registered=0',
    b'STYLEOS DESKTOP WINDOW SERVICE READY registered=0',
    b'STYLEOS DESKTOP RUNTIME BOUNDARY PASS',
    b'STYLEOS DESKTOP READY',
)


def site(efi, map_text):
    pe, = _unpack('<I', efi, 60, 'PE offset')
    if efi[:2] != b'MZ' or _range(efi, pe, 4, 'PE magic') != b'PE\0\0':
        raise ValueError('not PE')
    machine, count, _, _, _, opt, _ = _unpack('<HHIIIHH', efi, pe+4, 'COFF')
    magic, = _unpack('<H', efi, pe+24, 'optional magic')
    base, = _unpack('<Q', efi, pe+24+24, 'image base')
    if machine != 0x8664 or magic != 0x20b:
        raise ValueError('not x86-64 PE32+')
    rows = re.findall(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+(\S+)\s+([0-9a-fA-F]{16})\s+(\S+\.o)\s*$', map_text, re.M)
    symbols = {}
    for name, obj in (('kernel_main','kernel_main.o'), ('dpr_arch_safe_root','arch_x86_64_desktop_process_arch.o'), ('rt863_bootstrap','kernel_runtime_entry_858.o'), ('rt863_bootstrap_ready','kernel_runtime_entry_858.o'), ('desktop_v0_run','kernel_desktop_shell.o')):
        found = [r for r in rows if r[2] == name and r[4] == obj]
        if len(found) != 1:
            raise ValueError('linked symbol '+name)
        seg, rel, _, va, _ = found[0]
        seg, rel, va = int(seg,16), int(rel,16), int(va,16)
        if not 1 <= seg <= count:
            raise ValueError('invalid section')
        _, vs, v, rs, ro, _, _, _, _, flags = _unpack('<8sIIIIIIHHI',efi,pe+24+opt+40*(seg-1),'section')
        if not 0 <= rel < min(vs,rs) or va-base != v+rel or not flags&0x20000000 or flags&0x80000000:
            raise ValueError('bad code range '+name)
        _range(efi,ro+rel,1,'code entry')
        symbols[name] = (va-base, ro+rel, seg)
    start, offset, seg = symbols['kernel_main']
    after = [int(r[3],16)-base for r in rows if int(r[0],16)==seg and int(r[3],16)-base>start]
    if not after or not 0<min(after)-start<=4096:
        raise ValueError('kernel_main bound')
    code = _range(efi,offset,min(after)-start,'kernel_main')
    targets = {symbols[n][0]:n for n in symbols if n!='kernel_main'}
    calls = []
    for i in range(len(code)-4):
        if code[i] == 0xe8:
            dst = start+i+5+struct.unpack_from('<i',code,i+1)[0]
            if dst in targets:
                calls.append((targets[dst],start+i))
    want = ['dpr_arch_safe_root','rt863_bootstrap','rt863_bootstrap_ready','dpr_arch_safe_root','desktop_v0_run']
    if [n for n,_ in calls]!=want:
        raise ValueError('ordinary main shared-bootstrap call binding '+str(calls))
    return {'symbols':{n:v[0] for n,v in symbols.items()},'call_rvas':dict((str(i),[n,r]) for i,(n,r) in enumerate(calls))}


def check_uart(raw, count):
    seen = [line for line in raw.splitlines() if line in MARKERS]
    if seen != list(MARKERS)*count or any(line.startswith((b'STYLEOS DESKTOP RUNTIME SERVICE ERROR',b'STYLEOS DESKTOP RUNTIME BOUNDARY ERROR')) for line in raw.splitlines()):
        raise ValueError('shared bootstrap markers missing, duplicated or out of order')
    return count


def main():
    p=argparse.ArgumentParser();p.add_argument('--efi',type=Path,required=True);p.add_argument('--map',type=Path,required=True);p.add_argument('--guest',type=Path);p.add_argument('--iso',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
    result={'site':site(a.efi.read_bytes(),a.map.read_text()),'efi_sha256':hashlib.sha256(a.efi.read_bytes()).hexdigest(),'map_sha256':hashlib.sha256(a.map.read_bytes()).hexdigest()}
    if a.guest:
        if not a.iso or not a.output:
            p.error('--guest requires --iso and --output')
        run=json.loads((a.guest/'results.json').read_text())
        iso_hash=hashlib.sha256(a.iso.read_bytes()).hexdigest()
        if run.get('status')!='PASS' or run.get('boot_format')!='iso' or run.get('boot_sha256')!=iso_hash:
            raise ValueError('wrong ordinary guest result or boot image')
        expected={'optical-only':1,'first':2,'second':1,'foreign':1}
        result['uart']={}
        for name,count in expected.items():
            raw=(a.guest/name/'COM1.log').read_bytes();check_uart(raw,count)
            result['uart'][name]={'boots':count,'sha256':hashlib.sha256(raw).hexdigest()}
        result.update(status='PASS',boot_sha256=iso_hash,boot_count=sum(expected.values()))
        a.output.write_text(json.dumps(result,indent=2)+'\n')
    print('S863 shared bootstrap',result)

if __name__=='__main__':main()
