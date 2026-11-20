#!/usr/bin/env python3
"""Deterministic host-only SYSTEM APP PREINSTALL development bundle, NOT ISO."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import tempfile
import zipfile
import system_apps as sysapps

ROOT=Path(__file__).resolve().parents[1]
BUNDLE_FORMAT='styleos.system-app-preinstall.dev.v1'
MAX_BUNDLE=150000
NOTICE=(
    'STYLEOS DEVELOPMENT PREINSTALL BUNDLE (NOT AN INSTALLER OR ISO)\n'
    'This bundle is not recognized by StyleOS 744.\n'
    'The Documents.tea package requires an external ELF loader, process/window/file broker,\n'
    'stable approved guest ABI and publisher verification before production use.\n'
    'Never copy into a live 744 system or claim it boots.\n'
).encode('utf-8')

def encoded(obj):return (json.dumps(obj,sort_keys=True,ensure_ascii=False,separators=(',',':'))+'\n').encode('utf-8')
def sha(b):return hashlib.sha256(b).hexdigest()

def bundle_data(package_bytes):
    catalog=sysapps._catalog(package_bytes,'development')
    gen=sysapps._release_id(catalog,package_bytes)
    prefix=f'System/Applications/releases/{gen}/'
    files={
        'INSTALL-NOTICE.txt': NOTICE,
        'installer/system-apps.json':sysapps.PLAN.read_bytes(),
        'System/Applications/current':(gen+'\n').encode('ascii'),
        prefix+'catalog.json':sysapps._encode(catalog),
        prefix+'packages/Documents.tea':package_bytes
    }
    manifest={
        'format':BUNDLE_FORMAT,
        'profile':'development',
        'requires_native_guest_loader':True,
        'bootable':False,
        'guest_acceptance':'NOT RUN',
        'files':{name:{'sha256':sha(data),'bytes':len(data)} for name,data in sorted(files.items())}
    }
    return {'BUNDLE-MANIFEST.json':encoded(manifest),**files}

def make(package,output):
    raw=sysapps._safe_read(Path(package).absolute(),sysapps.MAX_TEA)
    files=bundle_data(raw)
    output=Path(output)
    output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_STORED) as z:
        for name,body in sorted(files.items()):
            info=zipfile.ZipInfo(name,(1980,1,1,0,0,0));info.create_system=3
            info.external_attr=0o100644<<16
            z.writestr(info,body)
    verify(output)
    return output

def verify(path):
    path=Path(path)
    if path.stat().st_size > MAX_BUNDLE:raise sysapps.ProvisionError('bundle too large')
    with zipfile.ZipFile(path) as z:
        rows=z.infolist()
        if (any(r.flag_bits or r.compress_type!=zipfile.ZIP_STORED or
                r.is_dir() or r.filename.startswith('/') or '..' in Path(r.filename).parts
                or r.file_size>sysapps.MAX_TEA+sysapps.MAX_MANIFEST for r in rows)
                or len({r.filename for r in rows})!=len(rows)):
            raise sysapps.ProvisionError('unsafe bundle paths/members')
        if len(rows)!=6 or rows[0].filename!='BUNDLE-MANIFEST.json':
            raise sysapps.ProvisionError('unexpected bundle members')
        files={r.filename:z.read(r.filename) for r in rows}
        if z.testzip() is not None:raise sysapps.ProvisionError('bundle CRC failure')
    raw=files.pop('BUNDLE-MANIFEST.json')
    manifest=sysapps._load_json(raw)
    if manifest.get('format')!=BUNDLE_FORMAT or manifest.get('bootable') is not False:
        raise sysapps.ProvisionError('unrecognized bundle format')
    entries=manifest.get('files')
    if not isinstance(entries,dict) or set(entries)!=set(files):
        raise sysapps.ProvisionError('bundle contents do not match manifest')
    for name,body in files.items():
        if entries[name] != {'sha256':sha(body),'bytes':len(body)}:
            raise sysapps.ProvisionError('bundle checksum mismatch')
    current=files.get('System/Applications/current',b'').decode('ascii')
    if len(current)!=65 or not current.endswith('\n'):
        raise sysapps.ProvisionError('invalid current pointer')
    gen=current[:-1]
    path=f'System/Applications/releases/{gen}/packages/Documents.tea'
    catalog_file=f'System/Applications/releases/{gen}/catalog.json'
    if set(files)!={'INSTALL-NOTICE.txt','installer/system-apps.json',
                   'System/Applications/current',catalog_file,path}:
        raise sysapps.ProvisionError('unexpected release path')
    if files['INSTALL-NOTICE.txt']!=NOTICE or files['installer/system-apps.json']!=sysapps.PLAN.read_bytes():
        raise sysapps.ProvisionError('bundle plan/notice mismatch')
    payload=files[path]
    if files[catalog_file]!=sysapps._encode(sysapps._catalog(payload,'development')):
        raise sysapps.ProvisionError('catalog inconsistency')
    if gen!=sysapps._release_id(sysapps._catalog(payload,'development'),payload):
        raise sysapps.ProvisionError('invalid generation ID')
    return gen

def stage_bundle(bundle,root,profile):
    """Installer-side reference implementation: verify full bundle first."""
    if profile != 'development':
        raise sysapps.ProvisionError('production bundle activation blocked')
    gen=verify(bundle)
    pkg_path=f'System/Applications/releases/{gen}/packages/Documents.tea'
    with zipfile.ZipFile(bundle) as z:
        package=z.read(pkg_path)
    with tempfile.TemporaryDirectory(prefix='styleos-preinstall-') as temp:
        path=Path(temp)/'Documents.tea'
        path.write_bytes(package)
        actual=sysapps.stage(root,path,'development')
        if actual!=gen:
            raise sysapps.ProvisionError('staging release mismatch')
        return actual


def main():
    p=argparse.ArgumentParser(description=__doc__)
    sub=p.add_subparsers(dest='action',required=True)
    build=sub.add_parser('build');build.add_argument('--tea',type=Path,required=True);build.add_argument('--output',type=Path,required=True)
    check=sub.add_parser('verify');check.add_argument('bundle',type=Path)
    stage=sub.add_parser('stage-bundle');stage.add_argument('--bundle',type=Path,required=True)
    stage.add_argument('--root',type=Path,required=True)
    stage.add_argument('--profile',choices=('development','production'),default='production')
    a=p.parse_args()
    try:
        if a.action=='build':
            print('DEVELOPMENT PREINSTALL BUNDLE PASS, NOT BOOTABLE:',make(a.tea,a.output))
        elif a.action=='verify':print('PREINSTALL BUNDLE VERIFY PASS:',verify(a.bundle))
        else:print('HOST INSTALLER STAGE PASS, NOT 744:', stage_bundle(a.bundle,a.root,a.profile))
    except (sysapps.ProvisionError, OSError, ValueError,zipfile.BadZipFile) as exc:
        p.exit(1,f'BUNDLE FAIL: {exc}\n')

if __name__=='__main__':main()
