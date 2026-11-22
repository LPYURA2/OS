#!/usr/bin/env python3
"""792: static source anchors and sanitizer-backed, non-activation preflight."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
ANCHORS={
    'boot_info_v1_missing_framebuffer':('include/styleos/boot_info.h','uint64_t uart_base;',False),
    'gop_boot_adapter':('boot/x86_64/desktop_display.c','framebuffer_base',True),
    'framebuffer_direct_writes':('kernel/desktop_display.c','volatile uint32_t *base',True),
    'serial_port_io':('arch/x86_64/serial.c','outb',True),
    'ps2_ata_port_io':('arch/x86_64/desktop_io.c','outw',True),
    'memory_boot_handoff':('boot/x86_64/uefi.c','ExitBootServices',True),
    'inherited_root_audit':('arch/x86_64/paging.c','inherited_range',True),
    'tss_ist_experiment':('kernel/ist_hw_trial_786.c','ist',True),
    'owned_low_experiment':('kernel/owned_low_trial_790.c','owned_low',True),
}

def verify_anchors():
    records=[]
    for key,(rel,needle,exists) in ANCHORS.items():
        data=(ROOT/rel).read_text(encoding='utf-8')
        if needle not in data or (key=='boot_info_v1_missing_framebuffer' and
             any(token in data for token in ('framebuffer_base;', 'framebuffer_size;'))):
            raise RuntimeError('Source anchor changed; reassess: '+key+' in '+rel)
        records.append({'key':key,'path':rel,'anchor':needle,
                        'proof':'source-structure-only',
                        'hardware_verified':False})
    return records

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    anchors=verify_anchors()
    clang=shutil.which('clang') or 'clang'
    exe=a.output/'pc792-host'
    common=['-std=c11','-Wall','-Wextra','-Werror','-I',str(ROOT/'include')]
    src=str(ROOT/'kernel/production_closure_792.c')
    test=str(ROOT/'tests/test_production_closure_792.c')
    cmd=[clang,*common,'-O1','-g','-fsanitize=address,undefined',
         '-fno-omit-frame-pointer',src,test,'-o',str(exe)]
    subprocess.run(cmd,check=True,capture_output=True,text=True)
    r=subprocess.run([str(exe)],capture_output=True,text=True)
    (a.output/'test.log').write_text(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError('preflight host FAILED: '+r.stdout+r.stderr)
    coff=a.output/'production_closure_792.obj'
    subprocess.run([clang,'--target=x86_64-pc-win32-coff',*common,'-O2',
         '-ffreestanding','-fno-builtin','-fno-stack-protector',
         '-mno-red-zone','-mno-sse','-mno-mmx','-msoft-float',
         '-c',src,'-o',str(coff)],check=True,capture_output=True,text=True)
    result={'status':'PASS','stage':792,'source_anchors':anchors,
      'checks':['bounds','rights','cache','overlap','duplicate','assertion',
       'no-activation-even-synthetic-complete','asan','ubsan','x86-64-coff'],
      'hardware_closure_verified':False,'production_cr3_authorized':False,
      'ordinary_desktop_changed':False}
    (a.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(r.stdout,end='')
    print('STYLEOS 792 READ-ONLY SOURCE ANCHORS PASS',len(anchors))
    print('STYLEOS 792 ASAN UBSAN COFF PASS; PRODUCTION CR3 NOT AUTHORIZED')
if __name__=='__main__':main()
