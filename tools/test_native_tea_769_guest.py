#!/usr/bin/env python3
"""Real QEMU OVMF x86 PCI/IDE read of canonical .tea followed by actual CPL3.

Neither guest ELF nor package is embedded in BOOTX64.EFI; disk is independent.
A corrupt package must fail closed before any user-mode trap or PID admission.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from test import firmware_path
from test_native_tea_768_guest import check_uart

ROOT=Path(__file__).resolve().parents[1]

def guest(qemu,firmware_code,firmware_var,efi_disk,package_disk,out,seconds):
    out.mkdir(parents=True,exist_ok=True)
    vars=out/'OVMF_VARS.fd';shutil.copyfile(firmware_var,vars)
    uart=out/'UART.log';stderr=out/'QEMU.log'
    # i440fx/PIIX3 gives the separate ATA PIO test channel at 0x1f0.
    # StyleOS boot EFI lives on an independent VirtIO system disk.
    cmd=[qemu,'-accel','tcg,thread=single','-m','128M','-machine','pc',
         '-display','none','-serial',f'file:{uart}','-monitor','none',
         '-net','none','-no-reboot','-vga','none',
         '-drive',f'if=pflash,format=raw,readonly=on,file={firmware_code}',
         '-drive',f'if=pflash,format=raw,file={vars}',
         '-drive',f'if=none,id=styleos,format=raw,readonly=on,file={efi_disk}',
         '-device','virtio-blk-pci,drive=styleos,bootindex=1',
         '-drive',f'if=ide,index=0,media=disk,format=raw,snapshot=on,file={package_disk}']
    trace=b'';start=time.monotonic()
    with stderr.open('wb') as err:
        proc=subprocess.Popen(cmd,stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=err)
        try:
            while time.monotonic()-start<seconds:
                if uart.exists():trace=uart.read_bytes()
                lines=trace.splitlines()
                if b'STYLEOS KERNEL HALT' in lines:break
                if any(line.startswith(b'STYLEOS USER ERROR:') for line in lines):break
                if proc.poll() is not None:break
                time.sleep(.12)
        finally:
            proc.terminate()
            try:proc.wait(timeout=5)
            except subprocess.TimeoutExpired:proc.kill();proc.wait(timeout=5)
    if not trace:
        diagnostics=stderr.read_text(errors='replace') if stderr.exists() else '(no stderr)'
        raise RuntimeError('no guest serial; QEMU return '+str(proc.returncode)+'; QEMU.log: '+diagnostics[-2500:])
    return trace

def run(output):
    output.mkdir(parents=True,exist_ok=True)
    compiled=output/'compiled'
    with (output/'build.log').open('w') as log:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'), '--arch','x86_64',
                        '--test-native-tea-disk','--output',str(compiled)],
                       check=True,stdout=log,stderr=subprocess.STDOUT)
    target=compiled/'x86_64';site=json.loads((target/'tea-768-site.json').read_text())
    proof=json.loads((target/'tea-769-site.json').read_text())
    package=(target/'minimal.tea').read_bytes()
    disk=(target/'native-tea-769-disk.img').read_bytes()
    efi=(target/'BOOTX64.EFI').read_bytes()
    if disk[512:512+len(package)]!=package or len(disk)!=128*512 or \
       hashlib.sha256(disk).hexdigest()!=proof['image_sha256']:
        raise AssertionError('host disk assembly differs from package bytes')
    if package in efi or (target/'app.elf').read_bytes() in efi:
        raise AssertionError('ELF/package is embedded in EFI: disk-read proof invalid')
    qemu=shutil.which(os.environ.get('QEMU_X86','qemu-system-x86_64'))
    code,var=firmware_path('OVMF_CODE','OVMF_CODE'),firmware_path('OVMF_VARS','OVMF_VARS')
    if not qemu or not code or not var:
        raise RuntimeError('real QEMU and OVMF required; no synthetic PASS')
    text=guest(qemu,code,var,target/'styleos.img',target/'native-tea-769-disk.img',output/'positive',75)
    lines=text.splitlines()
    marker=f'STYLEOS TEA769 DISK LOAD PASS bytes={len(package)} sectors=128'.encode()
    if lines.count(marker)!=1 or b'STYLEOS TEA769 DISK CPL3 EXIT PASS' not in lines:
        raise AssertionError('guest did not load canonical package from separate IDE: see positive/UART.log')
    # Prove the *entire* inherited CPL3 trace with the independently reviewed
    # 768 validator (PC, PTE, X86 frame, data, PID and event sequence).
    compatible=text.replace(b'STYLEOS TEA769',b'STYLEOS TEA768')
    compatible=compatible.replace(b'STYLEOS TEA768 DISK CPL3 EXIT PASS',
                                  b'STYLEOS TEA768 CPL3 EXIT PASS')
    checked=check_uart(compatible,site)
    if hashlib.sha256((target/'native-tea-769-disk.img').read_bytes()).hexdigest()!=proof['image_sha256']:
        raise AssertionError('original external disk image modified by QEMU')
    bad=bytearray(disk);bad[512+len(package)//2]^=0x01
    badfile=output/'tampered-disk.img';badfile.write_bytes(bad)
    negative=guest(qemu,code,var,target/'styleos.img',badfile,output/'negative',40)
    if hashlib.sha256(badfile.read_bytes()).hexdigest()!=hashlib.sha256(bad).hexdigest():
        raise AssertionError('tampered test input unexpectedly written by QEMU')
    nl=negative.splitlines()
    if not any(line.startswith(b'STYLEOS USER ERROR:') for line in nl) or \
       any(b'STYLEOS USER FRAME ' in line or b'STYLEOS TEA769 ADMITTED' in line for line in nl):
        raise AssertionError('tampered disk was not fail-closed: see negative/UART.log')
    result={'result':'PASS','stage':769,'commit_source':'GitHub Actions checked-out tree',
            'positive_real_cpl3':checked,'external_disk_sha256':hashlib.sha256(disk).hexdigest(),
            'normal_efi_sha256':hashlib.sha256(efi).hexdigest(),
            'uart_sha256':hashlib.sha256(text).hexdigest(),
            'negative_rejected_before_cpl3':True,
            'limitations':['dedicated i440fx IDE test, not production VFS or Desktop',
            'pinned canonical ZIP manifest; no signing, filesystem ACL or arbitrary apps',
            'same bounded user contexts from stage 768; not desktop session/windows']}
    (output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print('TEA769 REAL IDE GUEST CPL3 + TAMPER REFUSAL PASS',checked,flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path)
    run(p.parse_args().output)
