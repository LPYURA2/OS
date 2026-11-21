#!/usr/bin/env python3
"""Real x86 QEMU CPL3 INT80 user copy and foreign-pointer denial, not Desktop IPC."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from test import firmware_path

ROOT=Path(__file__).resolve().parents[1]
PAGE=4096

def val(line,key):
    m=re.search(rb'(?:^| )'+key+rb'=0x([0-9a-f]{16})(?: |$)',line)
    if not m:raise AssertionError('missing '+str(key)+' in '+str(line))
    return int(m.group(1),16)

def only(lines,prefix):
    a=[line for line in lines if line.startswith(prefix)]
    if len(a)!=1:raise AssertionError(f'expected exactly one {prefix!r}, got {len(a)}')
    return a[0]

def qemu_run(qemu,code,var,boot,out):
    out.mkdir(parents=True,exist_ok=True)
    v=out/'VARS.fd';shutil.copyfile(var,v)
    serial=out/'UART.log';log=out/'QEMU.log'
    cmd=[qemu,'-accel','tcg,thread=single','-m','128M','-machine','q35',
         '-display','none','-serial',f'file:{serial}','-monitor','none',
         '-net','none','-no-reboot','-vga','none',
         '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
         '-drive',f'if=pflash,format=raw,file={v}',
         '-drive',f'if=none,id=boot,format=raw,readonly=on,file={boot/"styleos.img"}',
         '-device','virtio-blk-pci,drive=boot,bootindex=1']
    text=b'';start=time.monotonic()
    with log.open('wb') as error:
        p=subprocess.Popen(cmd,stdout=subprocess.DEVNULL,stderr=error,stdin=subprocess.DEVNULL)
        try:
            while time.monotonic()-start<75:
                if serial.exists():text=serial.read_bytes()
                lines=text.splitlines()
                if b'STYLEOS KERNEL HALT' in lines:break
                if any(x.startswith(b'STYLEOS USER ERROR:') for x in lines):break
                if p.poll() is not None:break
                time.sleep(.1)
        finally:
            if p.poll() is None:
                p.terminate()
                try:p.wait(timeout=5)
                except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
    if not text:
        raise RuntimeError('No guest UART: '+log.read_text(errors='replace')[-1800:])
    return text

def inspect(raw,deny):
    lines=raw.splitlines()
    if any(x.startswith((b'STYLEOS USER ERROR:',b'STYLEOS EMERGENCY',
                         b'STYLEOS BOOT_ERROR')) for x in lines):
        raise AssertionError('kernel error: inspect UART.log')
    for m in (b'STYLEOS UC772 KERNEL-TRAP COPY PROOF PASS',
              b'STYLEOS USER PASS',b'STYLEOS KERNEL HALT'):
        if lines.count(m)!=1:raise AssertionError('missing/duplicate terminal marker '+repr(m))
    frames=[]
    for x in lines:
        m=re.match(rb'STYLEOS USER FRAME id=([AB]) kind=(YIELD|FAULT|EXIT) ',x)
        if m:frames.append((m[1],m[2],x))
    if [(a,b) for a,b,_ in frames]!=[(b'A',b'YIELD'),(b'B',b'YIELD'),
                                    (b'A',b'FAULT'),(b'B',b'EXIT')]:
        raise AssertionError('incorrect observed CPL3 fault/exit sequence')
    first=frames[0][2]
    if val(first,b'vector')!=128 or val(first,b'syndrome')!=0:
        raise AssertionError('not a real CPL3 INT80 frame')
    roots={n:val(only(lines,b'STYLEOS USER ROOT id='+n+b' '),b'pa')
           for n in (b'SAFE',b'A',b'B')}
    if len(set(roots.values()))!=3 or val(first,b'root')!=roots[b'A']:
        raise AssertionError('no distinct observed user root')
    obj=only(lines,b'STYLEOS USER OBJECT name=stack_a ')
    physical=int(obj.split()[-2],16)
    if int(obj.split()[-1],16)-physical!=PAGE or physical&(PAGE-1):
        raise AssertionError('not an admitted full image stack')
    table={}
    for line in lines:
        if line.startswith(b'STYLEOS USER TABLE id='):
            name=line.split()[3].split(b'=')[1]
            k=(name,val(line,b'table'),val(line,b'index'))
            if k in table:raise AssertionError('duplicate PTE')
            table[k]=val(line,b'entry')
    def walk(name,va):
        pa=roots[name];user=True;write=True;exec_allowed=True
        for shift in (39,30,21,12):
            entry=table.get((name,pa,(va>>shift)&511),0)
            if not entry&1:return None
            user=user and bool(entry&4);write=write and bool(entry&2)
            exec_allowed=exec_allowed and not bool(entry&(1<<63))
            pa=entry&0x000ffffffffff000
        return (pa+(va&4095),user,write,not exec_allowed)
    if walk(b'A',0x11000000)!=(physical,True,True,True):
        raise AssertionError('live A user stack leaf lacks private RW/NX mapping')
    if walk(b'A',0x12010000) is not None:
        raise AssertionError('foreign B page present in A root')
    idx=lines.index(first)
    regs={val(x,b'index'):val(x,b'value') for x in lines[idx+1:idx+32]
          if x.startswith(b'STYLEOS USER REG id=A ')}
    source=0x12010000 if deny else 0x11000ff0
    if regs.get(0)!=3 or regs.get(2)!=8 or regs.get(3)!=source or \
       regs.get(11)!=0xa11ce55a12345678:
        raise AssertionError('observed saved CPL3 arguments do not match user code')
    prefix=(b'STYLEOS UC772 HARDWARE DENIED foreign_user_page root='
            if deny else b'STYLEOS UC772 HARDWARE COPIED pid=')
    marker=only(lines,prefix)
    if val(marker,b'root')!=roots[b'A'] or val(marker,b'source')!=source or \
       val(marker,b'len')!=8 or lines.index(marker)>idx:
        raise AssertionError('trap-copy marker not hardware-bound')
    if not deny and not val(marker,b'pid'):raise AssertionError('no kernel registry PID')
    if deny and any(x.startswith(b'STYLEOS UC772 HARDWARE COPIED') for x in lines):
        raise AssertionError('foreign user memory leaked')
    return dict(denied=deny,frame_count=len(frames),root=hex(roots[b'A']),
                source=hex(source),copied=not deny)

def run(output):
    output.mkdir(parents=True,exist_ok=True)
    qemu=shutil.which('qemu-system-x86_64')
    code,var=firmware_path('OVMF_CODE','OVMF_CODE'),firmware_path('OVMF_VARS','OVMF_VARS')
    if not qemu or not code or not var:
        raise RuntimeError('Real QEMU-OVMF is mandatory: no synthetic PASS')
    checks=[]
    for deny in (False,True):
        name='deny' if deny else 'copy'
        build=output/name/'compiled'
        with (output/(name+'-build.log')).open('w') as log:
            subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64',
                            '--test-user-syscall-deny' if deny else '--test-user-syscall',
                            '--output',str(build)],check=True,stdout=log,stderr=subprocess.STDOUT)
        target=build/'x86_64'
        trace=qemu_run(qemu,code,var,target,output/name/'qemu')
        check=inspect(trace,deny)
        check['efi_sha256']=hashlib.sha256((target/'BOOTX64.EFI').read_bytes()).hexdigest()
        check['uart_sha256']=hashlib.sha256(trace).hexdigest()
        checks.append(check)
        print('UC772 REAL QEMU',name.upper(),'PASS',check,flush=True)
    (output/'results.json').write_text(json.dumps({'result':'PASS','stage':772,
        'scope':'dedicated x86-64 yielding INT80 user-copy only',
        'not_production_desktop_syscall':True,'checks':checks},indent=2)+'\n')
    print('UC772 REAL CPL3 COPY AND FOREIGN-PAGE DENIAL PASS')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    run(p.parse_args().output)
