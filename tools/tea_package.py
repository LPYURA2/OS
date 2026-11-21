#!/usr/bin/env python3
"""Canonical StyleOS .tea v1 ZIP packer/verifier. Not a guest executable loader."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile

ARCH_MACHINE={'x86_64':62,'aarch64':183}
APP_ID=re.compile(r'[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*){1,7}\Z')
VERSION=re.compile(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\Z')
MAX_ELF=16*1024*1024
MAX_MANIFEST=4096

class TeaError(ValueError): pass

def validate_elf(blob,arch):
    if len(blob)<64 or blob[:4]!=b'\x7fELF' or blob[4:6]!=b'\x02\x01':
        raise TeaError('payload must be little-endian ELF64')
    typ,machine=struct.unpack_from('<HH',blob,16)
    if typ not in (2,3) or machine!=ARCH_MACHINE[arch]:
        raise TeaError('ELF architecture/type mismatch')
    entry,phoff=struct.unpack_from('<QQ',blob,24)
    phentsize,phnum=struct.unpack_from('<HH',blob,54)
    if phentsize!=56 or not (1<=phnum<=64) or phoff>len(blob) or phnum*phentsize>len(blob)-phoff:
        raise TeaError('invalid program header table')
    can_execute=False
    for i in range(phnum):
        typ,flags,off,vaddr,_,filesz,memsz,_=struct.unpack_from('<IIQQQQQQ',blob,phoff+i*56)
        if typ!=1:continue
        if flags&3==3:raise TeaError('RWX segment forbidden')
        if filesz>memsz or off>len(blob) or filesz>len(blob)-off or vaddr+memsz>2**64:
            raise TeaError('invalid load segment')
        if flags&1 and vaddr<=entry<vaddr+memsz:can_execute=True
    if not can_execute:raise TeaError('entry outside executable segment')

def validate_manifest(m):
    if not isinstance(m,dict) or set(m)!={'format','schema_version','id','name','version','architecture','entry','sha256','permissions'}:
        raise TeaError('invalid manifest fields')
    arch=m['architecture']
    if m['format']!='styleos.tea' or type(m['schema_version']) is not int or m['schema_version']!=1:
        raise TeaError('unsupported .tea version')
    if not isinstance(arch,str) or arch not in ARCH_MACHINE:raise TeaError('invalid architecture')
    if not isinstance(m['id'],str) or not APP_ID.fullmatch(m['id']):raise TeaError('invalid app id')
    if not isinstance(m['name'],str) or not 1<=len(m['name'])<=64:raise TeaError('invalid name')
    if not isinstance(m['version'],str) or not VERSION.fullmatch(m['version']):raise TeaError('invalid version')
    if m['entry']!=f'bin/{arch}/app.elf':raise TeaError('noncanonical entry path')
    if not isinstance(m['permissions'],list) or m['permissions']!=[]:raise TeaError('ambient permissions denied')
    h=m['sha256']
    if not isinstance(h,str) or len(h)!=64 or any(x not in '0123456789abcdef' for x in h):
        raise TeaError('invalid digest')
    return arch

def inspect(path):
    path = Path(path)
    if path.suffix != '.tea':
        raise TeaError('native format is .tea only')
    # A native package is a canonical, comment-free ZIP. Python's ZipFile
    # normally accepts arbitrary bytes appended after the final directory,
    # which would make archive identity ambiguous for installers/signatures.
    if path.stat().st_size < 22:
        raise TeaError('truncated ZIP')
    with path.open('rb') as raw:
        raw.seek(-22, 2)
        trailer = raw.read(22)
    if trailer[:4] != b'PK\x05\x06' or trailer[-2:] != b'\x00\x00':
        raise TeaError('noncanonical ZIP trailer or appended data')
    with zipfile.ZipFile(path) as z:
        entries=z.infolist()
        if len(entries)!=2 or len({e.filename for e in entries})!=2:
            raise TeaError('exactly two members required')
        for e in entries:
            if e.compress_type!=zipfile.ZIP_STORED or e.flag_bits&1 or e.is_dir() or \
               e.file_size>MAX_ELF or e.compress_size!=e.file_size or \
               (e.external_attr>>16)&0o170000==0o120000:
                raise TeaError('unsafe member')
        if 'manifest.json' not in z.namelist() or z.getinfo('manifest.json').file_size>MAX_MANIFEST:
            raise TeaError('manifest missing or oversized')
        try:m=json.loads(z.read('manifest.json').decode('utf-8'))
        except (ValueError,UnicodeDecodeError) as ex:raise TeaError('invalid JSON') from ex
        arch=validate_manifest(m)
        if set(z.namelist())!={'manifest.json',m['entry']}:
            raise TeaError('unexpected member')
        data=z.read(m['entry'])
        if not data or len(data)>MAX_ELF or hashlib.sha256(data).hexdigest()!=m['sha256']:
            raise TeaError('payload integrity failure')
        validate_elf(data,arch)
        return m

def pack(path,elf,app_id,name,version,arch):
    if Path(path).suffix!='.tea':raise TeaError('output must end .tea')
    data=Path(elf).read_bytes()
    if len(data)>MAX_ELF:raise TeaError('ELF too large')
    validate_elf(data,arch)
    m={'format':'styleos.tea','schema_version':1,'id':app_id,'name':name,
       'version':version,'architecture':arch,'entry':f'bin/{arch}/app.elf',
       'sha256':hashlib.sha256(data).hexdigest(),'permissions':[]}
    validate_manifest(m)
    metadata=(json.dumps(m,ensure_ascii=False,sort_keys=True,separators=(',',':'))+'\n').encode()
    if len(metadata)>MAX_MANIFEST:raise TeaError('manifest too large')
    with zipfile.ZipFile(path,'x') as z:
        for n,b in [('manifest.json',metadata),(m['entry'],data)]:
            i=zipfile.ZipInfo(n,date_time=(2000,1,1,0,0,0))
            i.compress_type=zipfile.ZIP_STORED
            i.external_attr=0o100644<<16
            z.writestr(i,b)
    return inspect(path)

if __name__=='__main__':
    p=argparse.ArgumentParser()
    sp=p.add_subparsers(dest='cmd',required=True)
    q=sp.add_parser('pack')
    for key in ('out','elf','id','name','version','arch'):
        q.add_argument('--'+key,required=True)
    s=sp.add_parser('verify');s.add_argument('package')
    a=p.parse_args()
    result=pack(a.out,a.elf,a.id,a.name,a.version,a.arch) if a.cmd=='pack' else inspect(a.package)
    print(json.dumps(result,ensure_ascii=False,indent=2))
