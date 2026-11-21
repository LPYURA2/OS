#!/usr/bin/env python3
"""Real x86 QEMU hardware proof: returning CPL3 -> NW channel -> CPL3 and denial."""
import argparse,hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
from test_user_syscall_772_guest import qemu_run as guest, val as field, only as one_line
from test import firmware_path
ROOT=Path(__file__).resolve().parents[1]

def verified_stack_pte(lines):
    a=one_line(lines,b'STYLEOS USER ROOT id=A ')
    b=one_line(lines,b'STYLEOS USER ROOT id=B ')
    safe=one_line(lines,b'STYLEOS USER ROOT id=SAFE ')
    root=field(a,b'pa')
    if len({root,field(b,b'pa'),field(safe,b'pa')})!=3:
        raise AssertionError('non-isolated CR3 roots')
    stack=one_line(lines,b'STYLEOS USER OBJECT name=stack_a ')
    backing=int(stack.split()[-2],16)
    if int(stack.split()[-1],16)-backing!=4096 or backing&4095:
        raise AssertionError('private stack not admitted as full page')
    table={}
    for row in lines:
        if not row.startswith(b'STYLEOS USER TABLE id='):continue
        who=row.split()[3].split(b'=')[1]
        key=(who,field(row,b'table'),field(row,b'index'))
        if key in table:raise AssertionError('duplicate PTE')
        table[key]=field(row,b'entry')
    def walk(who,base,va):
        page=base; user=True; rw=True; noexec=True
        for shift in (39,30,21,12):
            entry=table.get((who,page,(va>>shift)&511),0)
            if not entry&1:return None
            user &= bool(entry&4);rw &= bool(entry&2)
            noexec &= bool(entry&(1<<63))
            page=entry&0x000ffffffffff000
        return page+(va&4095),user,rw,bool(entry&(1<<63))
    if walk(b'A',root,0x11000000)!=(backing,True,True,True):
        raise AssertionError('A stack user PTE is not RW/NX')
    if walk(b'A',root,0x12010000) is not None:
        raise AssertionError('foreign B page mapped to A user root')
    return root

def validate(trace,deny):
    lines=trace.splitlines()
    for line in lines:
        if line.startswith((b'STYLEOS USER ERROR:',b'STYLEOS EMERGENCY',
                            b'STYLEOS BOOT_ERROR')):
            raise AssertionError('hardware guest error: '+str(line))
    for marker in (b'STYLEOS IPC773 WINDOW PORT ARMED',
                   b'STYLEOS IPC773 RETURNING CPL3 WINDOW IPC PASS',
                   b'STYLEOS USER PASS',b'STYLEOS KERNEL HALT'):
        if lines.count(marker)!=1:
            raise AssertionError('missing / repeated terminal proof: '+str(marker))
    frame=[(m[1],m[2],l) for l in lines
           if (m:=re.match(rb'STYLEOS USER FRAME id=([AB]) kind=(YIELD|FAULT|EXIT) ',l))]
    if [(a,b) for a,b,_ in frame] != [(b'A',b'YIELD'),(b'B',b'YIELD'),
                                       (b'A',b'FAULT'),(b'B',b'EXIT')]:
        raise AssertionError('CPL3 continuation/fault/exit chain broken')
    a=frame[0][2]
    root=verified_stack_pte(lines)
    if field(a,b'vector')!=128 or field(a,b'root')!=root or field(a,b'cs')!=0x23:
        raise AssertionError('untrusted CPL3 hardware origin')
    idx=lines.index(a)
    regs={field(line,b'index'):field(line,b'value') for line in lines[idx+1:idx+32]
          if line.startswith(b'STYLEOS USER REG id=A ')}
    if regs.get(0)!=1 or regs.get(11)!=0xa11ce55a12345678 or \
       regs.get(3)!=0x11000fc0 or regs.get(2)!=0x11000fd0:
        raise AssertionError('CPL3 registers did not survive IRET and yield')
    evidence=one_line(lines,b'STYLEOS IPC773 HARDWARE IRET COPY PASS pid=')
    if field(evidence,b'root')!=root or field(evidence,b'success')!=1 or \
       field(evidence,b'denied')!=int(deny) or not field(evidence,b'pid'):
        raise AssertionError('missing actual return-to-user, denial or IPC evidence')
    if lines.index(evidence)>=lines.index(a):
        raise AssertionError('kernel proof did not follow the user dispatch correctly')
    return {'mode':'deny-and-retry' if deny else 'accepted',
            'real_cpl3_terminal_frames':len(frame),'root':hex(root),
            'real_pid':field(evidence,b'pid'),
            'denied_before_valid_reply':bool(deny),'kernel_to_user_reply':True}

def run(output):
    output.mkdir(parents=True,exist_ok=True)
    qemu=shutil.which('qemu-system-x86_64')
    code=firmware_path('OVMF_CODE','OVMF_CODE')
    var=firmware_path('OVMF_VARS','OVMF_VARS')
    if not qemu or not code or not var:
        raise RuntimeError('QEMU/OVMF required for real hardware proof')
    evidence=[]
    for deny in (False,True):
        name='denial' if deny else 'success'
        target=output/name/'compiled';target.mkdir(parents=True,exist_ok=True)
        with (output/(name+'-build.log')).open('w') as log:
            subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64',
              '--test-user-ipc-deny' if deny else '--test-user-ipc',
              '--output',str(target)],check=True,stdout=log,stderr=subprocess.STDOUT)
        trace=guest(qemu,code,var,target/'x86_64',output/name/'qemu')
        result=validate(trace,deny)
        result['serial_sha256']=hashlib.sha256(trace).hexdigest()
        result['efi_sha256']=hashlib.sha256((target/'x86_64/BOOTX64.EFI').read_bytes()).hexdigest()
        evidence.append(result)
        print('IPC773 REAL QEMU',name,'PASS',result,flush=True)
    (output/'results.json').write_text(json.dumps({'result':'PASS','stage':773,
        'hardware':True,'test_only':True,'checks':evidence},indent=2)+'\n')
    print('IPC773 REAL CPL3 RETURN + CROSS-PROCESS DENIAL + WINDOW PORT PASS',flush=True)
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True)
    run(ap.parse_args().output)
