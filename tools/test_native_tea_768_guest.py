#!/usr/bin/env python3
"""Real QEMU/OVMF proof of a verified .tea ELF64 payload executed in CPL3.

A dedicated firmware test profile. No desktop .tea launcher is claimed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from tea_package import inspect
from test import firmware_path

ROOT=Path(__file__).resolve().parents[1]
UP_CODE_VA=0x10000000
ENTRY_MASK=0x000ffffffffff000


def line_value(raw, prefix, field):
    for line in raw.splitlines():
        if line.startswith(prefix):
            m=re.search(rb'\b'+field+rb'=0x([0-9a-f]{16})\b',line)
            if m:return int(m.group(1),16)
    raise AssertionError(f'{prefix!r} {field!r} missing')


def check_uart(text, site):
    if b'STYLEOS USER ERROR' in text or b'STYLEOS EMERGENCY' in text or \
       b'STYLEOS BOOT_INFO ERROR' in text or b'STYLEOS EARLY' in text:
        raise AssertionError('guest error in UART')
    lines=text.splitlines()
    end=b'STYLEOS TEA768 CPL3 EXIT PASS'
    if lines.count(end)!=1 or lines.count(b'STYLEOS USER PASS')!=1 or \
       lines.count(b'STYLEOS KERNEL HALT')!=1:
        raise AssertionError('no clean native-user termination')
    if lines.count(('STYLEOS TEA768 PKG sha256='+site['package_sha256']).encode())!=1 or \
       lines.count(('STYLEOS TEA768 ELF sha256='+site['elf_sha256']).encode())!=1:
        raise AssertionError('package/ELF sha marker differs')
    if not any(re.fullmatch(rb'STYLEOS TEA768 ADMITTED pid=[1-9][0-9]* entry=0x10000000',
                            line) for line in lines):
        raise AssertionError('real PID from registry missing')
    events=[]
    for line in lines:
        m=re.match(rb'STYLEOS USER FRAME id=([AB]) kind=(YIELD|EXIT|FAULT) ',line)
        if m:events.append((m.group(1),m.group(2)))
    if events!=[(b'A',b'YIELD'),(b'B',b'YIELD'),(b'A',b'FAULT'),(b'B',b'EXIT')]:
        raise AssertionError('unexpected actual CPL3 trap sequence '+repr(events))
    if line_value(text,b'STYLEOS USER FRAME id=B kind=YIELD',b'pc') != \
            UP_CODE_VA+site['symbols']['tea_768_resume']:
        raise AssertionError('actual CPL3 yield PC differs from ELF symbol')
    if line_value(text,b'STYLEOS USER FRAME id=B kind=EXIT',b'pc') != \
            UP_CODE_VA+site['symbols']['tea_768_exit']+2:
        raise AssertionError('actual CPL3 exit PC differs from ELF symbol')
    if b'STYLEOS USER DATA id=B value=0x123456789abcdef0' not in lines:
        raise AssertionError('private user DATA has wrong intermediate result')
    if not any(b'STYLEOS USER DATA id=B value=0xfedcba9876543210'==x for x in lines):
        raise AssertionError('private user DATA missing final value')
    if line_value(text,b'STYLEOS USER ROOT id=B',b'pa') == \
       line_value(text,b'STYLEOS USER ROOT id=SAFE',b'pa'):
        raise AssertionError('B root aliases SAFE')
    obj=next((x for x in lines if x.startswith(b'STYLEOS USER OBJECT name=tea_code_b ')),None)
    if not obj or len(obj.split())!=6:
        # OBJECT line has: STYLEOS USER OBJECT name=... 0xSTART 0xEND
        if not obj or len(obj.split())!=5:
            raise AssertionError('staged code page object missing')
    start=int(obj.split()[-2],16);stop=int(obj.split()[-1],16)
    if stop-start!=4096 or start&4095:
        raise AssertionError('bad staged physical code page')
    roots={}
    table={}
    for line in lines:
        m=re.fullmatch(rb'STYLEOS USER ROOT id=(SAFE|A|B) pa=0x([0-9a-f]{16}) pages=0x([0-9a-f]{16})',line)
        if m:roots[m.group(1)]=int(m.group(2),16)
        m=re.fullmatch(rb'STYLEOS USER TABLE id=(SAFE|A|B) table=0x([0-9a-f]{16}) index=0x([0-9a-f]{16}) entry=0x([0-9a-f]{16})',line)
        if m:
            key=(m.group(1),int(m.group(2),16),int(m.group(3),16))
            if key in table:raise AssertionError('duplicate PTE')
            table[key]=int(m.group(4),16)
    if set(roots)!={b'SAFE',b'A',b'B'}:raise AssertionError('missing root evidence')
    def walk(name):
        pa=roots[name];user=True;write=True;execute=True
        for shift in (39,30,21,12):
            entry=table.get((name,pa,(UP_CODE_VA>>shift)&511),0)
            if not (entry&1):return None
            user &=bool(entry&4);write &=bool(entry&2);execute &=not bool(entry&(1<<63))
            pa=entry&ENTRY_MASK
        return pa,user,write,execute
    if walk(b'B')!=(start,True,False,True):
        raise AssertionError(f'ELF code not mapped with user RX from admitted physical BSS page: {walk(b"B")}')
    if walk(b'SAFE') is not None:
        raise AssertionError('SAFE root incorrectly maps userspace entry')
    if walk(b'A') is None or walk(b'A')[0]==start:
        raise AssertionError('A/B physical code isolation is missing')
    return {'real_traps':[f'{a.decode()}:{b.decode()}' for a,b in events],
            'code_page_pa':hex(start),'root_b':hex(roots[b'B']),
            'pid':next(x.decode() for x in lines if x.startswith(b'STYLEOS TEA768 ADMITTED'))}


def run(output):
    output.mkdir(parents=True,exist_ok=True)
    compiled=output/'compiled'
    buildlog=output/'build.log'
    with buildlog.open('w') as log:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64',
                        '--test-native-tea','--output',str(compiled)],
                       check=True,stdout=log,stderr=subprocess.STDOUT)
    target=compiled/'x86_64'
    site=json.loads((target/'tea-768-site.json').read_text())
    manifest=inspect(target/'minimal.tea')
    if manifest != site['package_manifest'] or manifest['sha256']!=site['elf_sha256']:
        raise AssertionError('canonical .tea signature or digest site differs')
    if hashlib.sha256((target/'minimal.tea').read_bytes()).hexdigest()!=site['package_sha256']:
        raise AssertionError('full .tea hash changed')
    qemu=shutil.which(os.environ.get('QEMU_X86','qemu-system-x86_64'))
    code,var=firmware_path('OVMF_CODE','OVMF_CODE'),firmware_path('OVMF_VARS','OVMF_VARS')
    if not qemu or not code or not var:
        raise RuntimeError('REAL QEMU + OVMF required: no synthetic PASS')
    v=output/'OVMF_VARS.fd';shutil.copyfile(var,v)
    serial=output/'UART.log';err=output/'QEMU.log'
    cmd=[qemu,'-accel','tcg,thread=single','-m','128M','-machine','q35',
         '-display','none','-serial',f'file:{serial}','-monitor','none',
         '-net','none','-no-reboot','-vga','none',
         '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
         '-drive',f'if=pflash,format=raw,file={v}',
         '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={target/"styleos.img"}',
         '-device','virtio-blk-pci,drive=styleos']
    text=b'';t=time.monotonic()
    with err.open('wb') as errors:
        proc=subprocess.Popen(cmd,stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=errors)
        try:
            while time.monotonic()-t<75:
                if serial.exists():text=serial.read_bytes()
                # UEFI serial writes CRLF on QEMU/OVMF; match a *whole* line
                # rather than an LF-only byte sequence, and do not accept a
                # partial error message containing the same words.
                if b'STYLEOS KERNEL HALT' in text.splitlines():break
                if proc.poll() is not None:break
                time.sleep(.1)
            if not text or b'STYLEOS KERNEL HALT' not in text.splitlines():
                raise RuntimeError('hardware guest did not reach terminal halt; inspect UART/QEMU logs')
            verified=check_uart(text,site)
        finally:
            proc.terminate()
            try:proc.wait(timeout=5)
            except subprocess.TimeoutExpired:proc.kill();proc.wait(timeout=5)
    result={'result':'PASS','profile':'boot-embedded, build-verified .tea ELF in x86 CPL3',
            'architecture':'x86_64','package_sha256':site['package_sha256'],
            'elf_sha256':site['elf_sha256'],'disk_sha256':hashlib.sha256((target/'styleos.img').read_bytes()).hexdigest(),
            'uart_sha256':hashlib.sha256(text).hexdigest(),
            'proof':verified,'limitations':[
                'package extracted during build, not at runtime from guest storage',
                'only a signed-off embedded test app, no signature-based trust or general ELF ABI',
                'uses existing experimental CPL3 root and manual trap dispatcher; no desktop app process',
                'does not prove native_launch_stage session/window integration']}
    (output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print('NATIVE TEA 768 REAL QEMU CPL3 PASS:',verified)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    run(p.parse_args().output)
