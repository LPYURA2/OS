"""Compile an independent C SDK client and prove the actual pinned guest IPC."""
import argparse
from pathlib import Path
import re
import subprocess
import test_native_expanded_76_guest as old

ROOT=old.ROOT

def prepare(out):
    src=ROOT/'tests/guest'
    sdk=ROOT/'sdk/native'
    common=['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-ffreestanding',
            '-fno-stack-protector','-fno-pie','-fno-pic','-fno-asynchronous-unwind-tables',
            '-mno-red-zone','-mgeneral-regs-only','-I'+str(sdk/'include')]
    for source,name in ((src/'native_sdk_77_client.c','client'),(sdk/'x86_64/syscall.c','syscall')):
        subprocess.run([*common,'-c',str(source),'-o',str(out/(name+'.o'))],check=True)
    subprocess.run(['gcc','-c',str(src/'native_sdk_77_entry.S'),'-o',str(out/'entry.o')],check=True)
    linked=out/'sdk77-linked.elf'
    subprocess.run(['ld.lld','-T',str(src/'native_sdk_77_fixture.ld'),'-o',str(linked),
                    str(out/'entry.o'),str(out/'client.o'),str(out/'syscall.o')],check=True)
    dump=subprocess.check_output(['objdump','-d',str(linked)],text=True)
    (out/'sdk77-disassembly.log').write_text(dump)
    calls=re.findall(r'^\s*([0-9a-f]+):\s+cd 80\s+int\s+\$0x80',dump,re.M)
    assert len(calls)==1,'expected one SDK trap instruction'
    ret=int(calls[0],16)+2
    symbols={}
    for line in subprocess.check_output(['nm','-n',str(linked)],text=True).splitlines():
        parts=line.split()
        if len(parts)==3:symbols[parts[2]]=int(parts[0],16)
    assert symbols['_start']==old.CODE+old.P
    wait=tuple(symbols[s] for s in ('sdk77_pause','sdk77_compare','sdk77_branch'))
    assert old.CODE+old.P<=ret<old.CODE+2*old.P
    assert all(old.CODE+old.P<=pc<old.CODE+2*old.P for pc in wait)
    raw=out/'sdk77-code.bin'
    subprocess.run(['objcopy','-O','binary','--only-section=.text',str(linked),str(raw)],check=True)
    code=raw.read_bytes()
    assert 0<len(code)<=old.P
    return old.prepare(out,(old.elf(code,True),old.Sites(wait,ret,ret)))

def verify(trace,lines,apps,pcs):
    return old.verify(trace,lines,apps,pcs,sdk_stack=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',required=True,type=Path)
    old.run(p.parse_args().output,prepare_case=prepare,verify_case=verify)
