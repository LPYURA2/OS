#!/usr/bin/env python3
"""Build a reproducible *experimental* Documents.tea. No 744 installation claim.

The package uses ZIP_STORED and SHA256, but does NOT claim an approved signed
publisher identity, or compatibility with undocumented 744 system-module ABI.
"""
import argparse
import hashlib
import json
import pathlib
import shutil
import struct
import subprocess
import tempfile
import zipfile

ROOT=pathlib.Path(__file__).resolve().parents[1]
SOURCES=["apps/documents/main.c","modules/runtime/freestanding.c",
 "modules/documents/model.c","modules/documents/review.c","modules/documents/docx.c",
 "modules/documents/import.c","modules/documents/gateway.c","modules/documents/ui.c",
 "modules/documents/runtime.c"]
MAX_TEA=65536

def elf_check(data):
    if data[:7]!=b"\x7fELF\x02\x01\x01" or len(data)<120:raise ValueError("bad ELF64")
    typ,machine,version=struct.unpack_from('<HHI',data,16)
    if (typ,machine,version)!=(2,62,1):raise ValueError('bad machine')
    entry,phoff,_,_,ehsz,phentsz,phnum=struct.unpack_from('<QQQIHHH',data,24)
    if ehsz!=64 or phentsz!=56 or not 1<=phnum<=8 or phoff+phentsz*phnum>len(data):
        raise ValueError('bad program headers')
    ok=False
    for i in range(phnum):
        ptype,flags,offset,vaddr,_,filesz,memsz,_=struct.unpack_from('<IIQQQQQQ',data,phoff+i*56)
        if ptype!=1:continue
        if offset+filesz>len(data) or filesz>memsz or flags&3==3:
            raise ValueError('bad load segment')
        if flags&1 and vaddr<=entry<vaddr+filesz:ok=True
    if not ok:raise ValueError('missing executable entry')
    return entry

def build(output):
    clang=shutil.which('clang'); lld=shutil.which('ld.lld')
    if not clang or not lld:raise RuntimeError('clang and ld.lld needed')
    with tempfile.TemporaryDirectory() as temp:
        temp=pathlib.Path(temp)
        objects=[]
        for i,name in enumerate(SOURCES):
            obj=temp/f'{i}.o'
            args=[clang,'--target=x86_64-unknown-linux-gnu','-std=c11','-Oz','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-ffunction-sections','-fdata-sections','-fno-pic','-fno-pie','-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),'-c',str(ROOT/name),'-o',str(obj)]
            subprocess.run(args,check=True);objects.append(str(obj))
        elf=temp/'payload.elf'
        subprocess.run([lld,'-m','elf_x86_64','--build-id=none','-z','noexecstack','--gc-sections',
          '-u','styleos_documents_app_event_v1','-u','styleos_documents_app_draw_v1','-u','styleos_documents_app_grammar_v1','-e','styleos_documents_app_entry_v1','-Ttext=0x10010000','-o',str(elf),*objects],check=True)
        binary=elf.read_bytes();entry=elf_check(binary)
        manifest={"format":"styleos.tea.sdk-v0","arch":"x86_64",
          "interface":"styleos.documents-preview-v1","installable_in_744":False,
          "project":json.loads((ROOT/'apps/documents/app.json').read_text(encoding='utf-8')),
          "payload":{"name":"payload.elf","kind":"elf64-exec","entry":entry,
                     "bytes":len(binary),"sha256":hashlib.sha256(binary).hexdigest()}}
        meta=(json.dumps(manifest,ensure_ascii=False,sort_keys=True,separators=(',',':'))+'\n').encode()
        output.parent.mkdir(parents=True,exist_ok=True)
        with zipfile.ZipFile(output,'w',zipfile.ZIP_STORED) as z:
            for name,data in [("manifest.json",meta),("payload.elf",binary)]:
                info=zipfile.ZipInfo(name,(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_STORED
                info.external_attr=0o100644<<16;z.writestr(info,data)
    verify(output)
    print('TEA PREVIEW BUILD PASS:',output,'bytes',output.stat().st_size)
    return manifest

def verify(path):
    data=path.read_bytes()
    if len(data)>MAX_TEA:raise ValueError(f'not StyleFS-compatible: {len(data)} bytes')
    with zipfile.ZipFile(path) as z:
        if z.namelist()!=['manifest.json','payload.elf']:raise ValueError('unexpected members')
        for info in z.infolist():
            if info.compress_type!=zipfile.ZIP_STORED or info.flag_bits:raise ValueError('unsafe ZIP flags')
        manifest=json.loads(z.read('manifest.json'))
        elf=z.read('payload.elf')
    if manifest.get('interface')!='styleos.documents-preview-v1' or manifest.get('installable_in_744') is not False:
        raise ValueError('interface mismatch')
    if (manifest['payload']['sha256']!=hashlib.sha256(elf).hexdigest() or
        manifest['payload']['bytes']!=len(elf) or manifest['payload']['entry']!=elf_check(elf)):
        raise ValueError('payload corruption')
    return manifest

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,default=ROOT/'build/755/Documents.tea')
    a=p.parse_args();build(a.output)
