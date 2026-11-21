#!/usr/bin/env python3
"""Archive *all tracked source bytes* of the checked-out git revision.

Must run from a complete checkout. Never archive a handpicked subset of files.
The archive is deterministic and includes a content manifest and source SHA-256.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ZIP_TIME = (2000, 1, 1, 0, 0, 0)


def tracked_files():
    data = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT)
    result = sorted(x.decode('utf-8', 'surrogateescape') for x in data.split(b'\0') if x)
    if not result or 'kernel/desktop_shell.c' not in result or 'tools/build.py' not in result:
        raise RuntimeError('refusing to archive incomplete StyleOS source tree')
    return result


def package(archive):
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    if len(commit) != 40:
        raise RuntimeError('git HEAD must be a complete SHA-1')
    dirty = subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=ROOT)
    if dirty:
        raise RuntimeError('tracked source has uncommitted changes; refusing an unidentifiable archive')
    source_files = tracked_files()
    archive.parent.mkdir(parents=True, exist_ok=True)
    if archive.is_relative_to(ROOT) and str(archive.relative_to(ROOT)) in source_files:
        raise RuntimeError('archive path collides with tracked source')
    manifest = {'source_commit': commit, 'files': []}
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as out:
        for name in source_files:
            file = ROOT / name
            if not file.is_file() or file.is_symlink():
                raise RuntimeError(f'unsupported source file: {name}')
            raw = file.read_bytes()
            meta = zipfile.ZipInfo(name, date_time=ZIP_TIME)
            meta.compress_type = zipfile.ZIP_DEFLATED
            meta.external_attr = 0o100644 << 16
            out.writestr(meta, raw, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
            manifest['files'].append({'name': name, 'size': len(raw),
                                      'sha256': hashlib.sha256(raw).hexdigest()})
        doc = (json.dumps(manifest, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
        meta = zipfile.ZipInfo('RECOVERY_SOURCE_MANIFEST.json', date_time=ZIP_TIME)
        meta.compress_type = zipfile.ZIP_DEFLATED
        meta.external_attr = 0o100644 << 16
        out.writestr(meta, doc, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
    (archive.parent / (archive.name + '.sha256')).write_text(f'{checksum}  {archive.name}\n')
    print('SOURCE ARCHIVE:', commit, len(source_files), 'files', archive, 'sha256', checksum)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    opts = parser.parse_args()
    package(opts.output.resolve())
