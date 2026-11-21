#!/usr/bin/env python3
"""Contract, isolation and atomic host staging tests for all nine .tea apps."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from preinstall_tea import read_catalog, verify_packages, stage, StageError
from tea_package import pack

ROOT = Path(__file__).resolve().parents[1]


def denied(fn):
    try:
        fn()
    except (StageError, ValueError, OSError):
        return
    raise AssertionError('unsafe/incomplete package set accepted')


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', required=True, type=Path)
    args = p.parse_args()
    target = args.output.resolve()
    target.mkdir(parents=True, exist_ok=True)
    apps = read_catalog()
    assert len(apps) == 9
    # Ensure the guest's compiled immutable IDs match installer configuration.
    kernel = (ROOT / 'kernel' / 'system_apps.c').read_text(encoding='utf-8')
    for app in apps:
        row = '{"' + app['id'] + '", "' + app['name'] + '", "/System/Applications/' + app['filename'] + '"}'
        assert row in kernel, 'catalog/source mismatch: ' + app['id']
    ctest = target / 'system-apps-host'
    cc = shutil.which('clang') or 'clang'
    subprocess.run([cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra',
                    '-Werror', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', '-I', str(ROOT/'include'),
                    str(ROOT/'kernel/system_apps.c'),str(ROOT/'tests/test_system_apps.c'),
                    '-o',str(ctest)],check=True)
    proc = subprocess.run([str(ctest)], check=True,text=True,capture_output=True)
    print(proc.stdout, end='')
    with tempfile.TemporaryDirectory(prefix='styleos-nine-tea-',dir=target) as tmp:
        base = Path(tmp)
        elf_source = base / 'stub.c'
        elf_source.write_text('void _start(void){for(;;)__asm__ volatile("pause");}\n')
        elf = base / 'stub.elf'
        subprocess.run([cc, '-target', 'x86_64-unknown-none',
                        '-ffreestanding','-fno-stack-protector','-nostdlib',
                        '-fuse-ld=lld','-Wl,--entry=_start',
                        str(elf_source),'-o',str(elf)],check=True)
        packages = base / 'packages'
        packages.mkdir()
        for app in apps:
            pack(packages / app['filename'],elf,app['id'],app['name'],
                 '0.1.0','x86_64')
        assert len(verify_packages(apps, packages)) == 9
        finished = base / 'staged'
        index = stage(packages, finished)
        assert index['status'] == 'prepared-not-installed'
        assert index['required_count'] == 9
        assert len(list((finished/'System'/'Applications').glob('*.tea'))) == 9
        index2 = json.loads((finished/'System'/'Applications'/'index.json').read_text())
        assert index2 == index
        for item in index['applications']:
            pth = finished / item['package_path'].lstrip('/')
            assert pth.is_file()
        denied(lambda:stage(packages,finished))
        # Refuse missing packages without leaving partially published output.
        missing = packages / apps[0]['filename']
        removed = base / 'temporarily-missing.tea'
        missing.rename(removed)
        denied(lambda:stage(packages,base/'incomplete'))
        assert not (base/'incomplete').exists()
        removed.rename(missing)
        extra = packages / 'org.styleos.fake.tea'
        extra.write_bytes(b'not a package')
        denied(lambda:stage(packages,base/'unexpected'))
        assert not (base/'unexpected').exists()
        extra.unlink()
        # Tampered packages and identity swaps must not install.
        changed = packages/apps[1]['filename']
        content = changed.read_bytes()
        changed.write_bytes(content + b'tamper')
        denied(lambda:stage(packages,base/'tampered'))
        assert not (base/'tampered').exists()
        changed.write_bytes(content)
        changed.rename(packages/'org.styleos.other.sapp')
        denied(lambda:stage(packages,base/'legacy'))
        assert not (base/'legacy').exists()
        (packages/'org.styleos.other.sapp').rename(changed)
        bad_catalog = json.loads((ROOT/'installer/system-apps.json').read_text())
        bad_catalog['applications'].pop()
        cat = base/'bad.json'
        cat.write_text(json.dumps(bad_catalog))
        denied(lambda:read_catalog(cat))
        bad_catalog = json.loads((ROOT/'installer/system-apps.json').read_text())
        bad_catalog['applications'][0]['filename'] = '../bad.tea'
        cat.write_text(json.dumps(bad_catalog))
        denied(lambda:read_catalog(cat))
    print('SYSTEM APP PREINSTALL HOST PASS: exactly nine valid .tea, registry,')
    print('atomic staging, required IDs, no .sapp, no partial install')
    (target/'result.json').write_text(json.dumps({
        'result':'PASS','app_count':9,'scope':'host staging only; not installed or launched in StyleOS',
        'checks':['nine required native .tea','matching guest catalog',
                  'ZIP/ELF/hash identity','atomic destination staging',
                  'refusal of missing, extra, tampered, .sapp, invalid catalog']
    },ensure_ascii=False,indent=2)+'\n')


if __name__ == '__main__':
    main()
