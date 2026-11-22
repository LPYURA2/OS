#!/usr/bin/env python3
"""784 pure preflight fuzz/negative suite and isolated COFF opcode check."""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    p.add_argument('--desktop-efi',type=Path)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    clang=shutil.which('clang') or 'clang'
    sources=[ROOT/p for p in ('kernel/tss_ltr_trial_784.c',
        'kernel/descriptor_state_781.c','kernel/tss_descriptor_783.c',
        'kernel/tss_ist_stage_782.c','kernel/frame_grant_proof.c',
        'kernel/boot_pool.c','tests/test_tss_ltr_784.c')]
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I',str(ROOT/'include')]
    exe=a.output/'tss-784-host'
    subprocess.run([clang,*flags,'-O1','-g','-fsanitize=address,undefined',
        '-fno-omit-frame-pointer',*[str(s) for s in sources],'-o',str(exe)],check=True)
    r=subprocess.run([str(exe)],capture_output=True,text=True)
    (a.output/'test.log').write_text(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    print(r.stdout,end='',flush=True)
    obj=a.output/'tss_ltr_trial_784.obj'
    subprocess.run([clang,'--target=x86_64-pc-win32-coff',*flags,'-O2',
        '-ffreestanding','-fno-builtin','-fno-stack-protector',
        '-mno-red-zone','-mno-sse','-mno-mmx','-msoft-float',
        '-DSTYLEOS_TEST_TSS_LTR_784=1','-DSTYLEOS_TEST_FRAME_GRANTS=1','-c',str(sources[0]),'-o',str(obj)],check=True)
    disassembler=shutil.which('llvm-objdump')
    if not disassembler:raise RuntimeError('llvm-objdump required for hardware opcode audit')
    machine=subprocess.check_output([disassembler,'-d',str(obj)],text=True).lower()
    for mnemonic in ('lgdt','ltr','str','sgdt','sidt','out'):
        if mnemonic not in machine:raise AssertionError('missing hardware instruction '+mnemonic)
    (a.output/'disassembly.txt').write_text(machine)
    if a.desktop_efi:
        desktop=a.desktop_efi.read_bytes()
        if b'STYLEOS TLT 784' in desktop or b'STYLEOS TLT 784 READY' in desktop:
            raise AssertionError('terminal trial leaked into normal Desktop')
    print('STYLEOS 784 COFF LGDT LTR STR ISOLATION PASS',flush=True)
if __name__=='__main__':main()
