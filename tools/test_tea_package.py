#!/usr/bin/env python3
"""Host-only native .tea strict validator regression, including malformed archives."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import zipfile
from tea_package import pack,inspect,TeaError

def denied(fn):
    try:fn()
    except (TeaError,zipfile.BadZipFile,ValueError):return
    raise AssertionError('invalid .tea was accepted')

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    out=a.output
    out.mkdir(parents=True,exist_ok=True)
    source=out/'hello.c';source.write_text('void _start(void) { for (;;) __asm__ volatile ("pause"); }\n')
    elf=out/'hello.elf'
    subprocess.run([shutil.which('clang') or 'clang','-target','x86_64-unknown-none',
                    '-ffreestanding','-fno-stack-protector','-nostdlib','-fuse-ld=lld',
                    '-Wl,--entry=_start',str(source),'-o',str(elf)],check=True)
    native=out/'test.tea'
    manifest=pack(native,elf,'org.styleos.hello','Hello','0.1.0','x86_64')
    assert inspect(native)==manifest and manifest['permissions']==[]
    old=out/'test.sapp';shutil.copyfile(native,old)
    denied(lambda:inspect(old))
    with zipfile.ZipFile(native) as z:
        exe=z.read(manifest['entry']);metadata=z.read('manifest.json')
    def forged(label,m=None,data=None,name=None):
        path=out/(label+'.tea')
        with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_STORED) as z:
            z.writestr('manifest.json',metadata if m is None else m)
            z.writestr(manifest['entry'] if name is None else name,
                       exe if data is None else data)
        denied(lambda:inspect(path))
    forged('traversal',name='../app.elf')
    forged('altered',data=exe+b'X')
    m=json.loads(metadata);m['permissions']=['disk.all']
    forged('rights',m=json.dumps(m))
    m=json.loads(metadata);m['architecture']='aarch64'
    forged('cpu',m=json.dumps(m))
    m=json.loads(metadata);m['id']='../../evil'
    forged('name',m=json.dumps(m))
    forged('not-elf',data=b'NO')
    forged('machine',data=exe[:18]+b'\xb7\x00'+exe[20:])
    print('TEA PACKAGE CONTRACT PASS: ELF64 ZIP SHA256 CPU no permissions, invalid .sapp rejected')

if __name__=='__main__':main()
