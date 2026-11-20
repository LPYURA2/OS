#!/usr/bin/env python3
"""Reference StyleOS system-app image provisioner (HOST-ONLY, stage 757).

Creates a content-addressed app generation under the system volume. Never edits
an EFI image, changes the 744 ISO, registers an app in the 744 shell, or
runs untrusted .tea bytes. A future native installer/loader must implement the
same contract before an installed-guest claim is possible.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / 'installer' / 'system-apps.json'
SYSTEM_SUBDIR = ('System', 'Applications')
RELEASES = 'releases'
CURRENT = 'current'
FORMAT = 'styleos.system-apps.catalog.v1'
MAX_TEA = 65536
MAX_MANIFEST = 8192
APP_ID = 'styleos.documents'
SHELL_ABI_UNSUPPORTED = 'styleos.documents-preview-v1'

class ProvisionError(Exception):
    pass

def _load_json(raw):
    def no_dupes(pairs):
        obj = {}
        for k, v in pairs:
            if k in obj:
                raise ProvisionError('duplicate JSON key: ' + k)
            obj[k] = v
        return obj
    try:
        return json.loads(raw, object_pairs_hook=no_dupes)
    except (ValueError, UnicodeDecodeError) as exc:
        raise ProvisionError('invalid JSON') from exc

def _encode(data):
    return (json.dumps(data, ensure_ascii=False, sort_keys=True,
                       separators=(',', ':')) + '\n').encode('utf-8')

def _sha(data):
    return hashlib.sha256(data).hexdigest()

def _no_links(path):
    """No symlink in an existing system root/release path."""
    anchor = path if path.is_absolute() else path.resolve()
    for item in [anchor, *anchor.parents]:
        if item.is_symlink():
            raise ProvisionError('symlink is forbidden in system path: ' + str(item))

def _base(root):
    root = Path(root).absolute()
    _no_links(root)
    if not root.is_dir():
        raise ProvisionError('system root must be an existing directory')
    base = root.joinpath(*SYSTEM_SUBDIR)
    _no_links(base)
    return base

def _safe_read(path, cap):
    _no_links(path)
    try:
        if not path.is_file() or path.stat().st_size > cap:
            raise ProvisionError('missing or oversized file: ' + str(path))
        data = path.read_bytes()
    except OSError as exc:
        raise ProvisionError('cannot read file: ' + str(path)) from exc
    if len(data) > cap:
        raise ProvisionError('oversized file: ' + str(path))
    return data

def _plan():
    info = _load_json(_safe_read(PLAN, MAX_MANIFEST))
    if info.get('format') != 'styleos.system-apps.plan.v1' or not isinstance(info.get('apps'), list):
        raise ProvisionError('invalid system-apps installation plan')
    apps = info['apps']
    if len(apps) != 1 or apps[0] != {
        'id': APP_ID, 'name': 'Документы', 'package_name': 'Documents.tea',
        'preinstall': True, 'location': 'system-volume',
        'process': 'independent', 'launcher_category': 'Office',
        'interface': SHELL_ABI_UNSUPPORTED,
        'file_access': 'granted-tokens-only', 'window_access': 'client-area-only'
    }:
        raise ProvisionError('unexpected system app plan (review changes before expanding)')
    return apps[0]

def _check_package(raw, expected_id=APP_ID):
    """Structural integrity only: NOT a publisher signature or safety proof."""
    import io
    if not 1 <= len(raw) <= MAX_TEA:
        raise ProvisionError('invalid .tea package length')
    try:
        with zipfile.ZipFile(io.BytesIO(raw)) as archive:
            parts = archive.infolist()
            if len(parts) != 2 or [p.filename for p in parts] != ['manifest.json', 'payload.elf']:
                raise ProvisionError('unexpected .tea members')
            if any(p.compress_type != zipfile.ZIP_STORED or p.flag_bits != 0 or p.is_dir()
                   or p.file_size != p.compress_size for p in parts):
                raise ProvisionError('unsupported ZIP member/flags')
            if not 1 <= parts[0].file_size <= MAX_MANIFEST:
                raise ProvisionError('bad manifest bounds')
            for p in parts:
                if (p.external_attr >> 16) & 0o170000 not in (0, 0o100000):
                    raise ProvisionError('unsafe .tea member type')
            manifest = _load_json(archive.read('manifest.json'))
            payload = archive.read('payload.elf')
            if archive.testzip() is not None:
                raise ProvisionError('damaged ZIP CRC')
    except (zipfile.BadZipFile, EOFError, OSError, RuntimeError) as exc:
        raise ProvisionError('invalid .tea zip') from exc
    if (not isinstance(manifest, dict) or manifest.get('format') != 'styleos.tea.sdk-v0' or
        manifest.get('arch') != 'x86_64' or manifest.get('interface') != SHELL_ABI_UNSUPPORTED or
        manifest.get('installable_in_744') is not False):
        raise ProvisionError('unsupported app ABI/format')
    project = manifest.get('project')
    if (not isinstance(project, dict) or project.get('id') != expected_id or
        not isinstance(project.get('name'), str) or not project['name'] or
        not isinstance(project.get('version'), str) or
        not re.fullmatch(r'[0-9]{1,4}\.[0-9]{1,4}\.[0-9]{1,4}', project['version'])):
        raise ProvisionError('invalid app identity/version')
    meta = manifest.get('payload')
    if (not isinstance(meta, dict) or meta.get('name') != 'payload.elf' or
        meta.get('kind') != 'elf64-exec' or meta.get('bytes') != len(payload) or
        type(meta.get('entry')) is not int or meta.get('sha256') != _sha(payload)):
        raise ProvisionError('invalid payload integrity')
    from tea_package import verify_elf64_x86
    try:
        if verify_elf64_x86(payload) != meta['entry']:
            raise ProvisionError('ELF entry mismatch')
    except (ValueError, KeyError, IndexError) as exc:
        raise ProvisionError('invalid ELF') from exc
    return project

def _catalog(raw, profile):
    identity = _check_package(raw)
    desc = _plan()
    return {
        'format': FORMAT,
        'build_profile': profile,
        'apps': [{
            'id': desc['id'],
            'display_name': desc['name'],
            'version': identity['version'],
            'origin': 'system-preinstall',
            'system_app': True,
            'process': 'independent',
            'launch_via': 'external-tea-loader',
            'launch_interface': desc['interface'],
            'registration': 'on-install',
            'menu_category': desc['launcher_category'],
            'file_access': desc['file_access'],
            'window_access': desc['window_access'],
            'package': 'packages/Documents.tea',
            'bytes': len(raw),
            'sha256': _sha(raw),
            'publisher_signature_verified': False,
            'compatible_with_744': False
        }]
    }

def _release_id(catalog, raw):
    return _sha(_encode(catalog) + raw)

def _sync_dir(directory):
    try:
        fd = os.open(directory, os.O_RDONLY)
        try: os.fsync(fd)
        finally: os.close(fd)
    except OSError:
        # Best-effort portability for host filesystems; user data never touched.
        pass

def _write_fsynced(path, data):
    with path.open('xb') as out:
        out.write(data)
        out.flush()
        os.fsync(out.fileno())

def _validate_id(release_id):
    if not re.fullmatch(r'[0-9a-f]{64}', release_id):
        raise ProvisionError('invalid generation ID')
    return release_id

def _verify_release(base, release_id):
    release_id = _validate_id(release_id)
    release = base / RELEASES / release_id
    _no_links(release)
    data = _safe_read(release / 'packages' / 'Documents.tea', MAX_TEA)
    raw = _safe_read(release / 'catalog.json', MAX_MANIFEST)
    cat = _load_json(raw)
    if _encode(cat) != raw:
        raise ProvisionError('catalog encoding is not canonical')
    if not isinstance(cat, dict) or cat.get('format') != FORMAT or cat.get('build_profile') != 'development':
        raise ProvisionError('unrecognized catalog/profile')
    if cat != _catalog(data, 'development'):
        raise ProvisionError('catalog/package integrity mismatch')
    if _release_id(cat, data) != release_id:
        raise ProvisionError('release ID mismatch')
    return cat

def _write_current(base, release_id):
    _validate_id(release_id)
    current = base / CURRENT
    _no_links(current)
    if current.exists() and not current.is_file():
        raise ProvisionError('invalid current pointer')
    if current.exists():
        _read_current(base)
    fd, temporary = tempfile.mkstemp(prefix='.next-', dir=base)
    try:
        with os.fdopen(fd, 'wb') as out:
            out.write((release_id + '\n').encode('ascii'))
            out.flush();os.fsync(out.fileno())
        os.replace(temporary, current)
        _sync_dir(base)
    finally:
        if os.path.exists(temporary): os.unlink(temporary)

def _read_current(base):
    current = base / CURRENT
    raw = _safe_read(current, 128)
    try: value = raw.decode('ascii')
    except UnicodeDecodeError as exc: raise ProvisionError('invalid current pointer') from exc
    if len(value) != 65 or not value.endswith('\n'):
        raise ProvisionError('invalid current pointer')
    return _validate_id(value[:-1])

def stage(root, package, profile):
    if profile != 'development':
        raise ProvisionError('production is BLOCKED: app ABI and publisher-signature policy not implemented')
    base = _base(root)
    raw = _safe_read(Path(package).absolute(), MAX_TEA)
    catalog = _catalog(raw, profile)
    gen = _release_id(catalog, raw)
    # No mutation before package, path, policy and existing current checks.
    if (base / CURRENT).exists():
        _verify_release(base, _read_current(base))
    _no_links(base / RELEASES)
    (base / RELEASES).mkdir(parents=True, exist_ok=True)
    release = base / RELEASES / gen
    if release.exists():
        _verify_release(base, gen)
    else:
        temp = Path(tempfile.mkdtemp(prefix='.incoming-', dir=base / RELEASES))
        try:
            (temp / 'packages').mkdir()
            _write_fsynced(temp / 'packages' / 'Documents.tea', raw)
            _write_fsynced(temp / 'catalog.json', _encode(catalog))
            _sync_dir(temp / 'packages');_sync_dir(temp)
            temp.rename(release)
            _sync_dir(base / RELEASES)
            _verify_release(base, gen)
        finally:
            if temp.exists(): shutil.rmtree(temp)
    _write_current(base, gen)
    return gen

def verify(root):
    base = _base(root)
    gen = _read_current(base)
    return gen, _verify_release(base, gen)

def rollback(root, target):
    base = _base(root)
    _verify_release(base, _read_current(base))
    _verify_release(base, target)
    _write_current(base, target)
    return target

def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='cmd', required=True)
    install = sub.add_parser('stage', help='stage a DEVELOPMENT system app generation (not a 744 installation)')
    install.add_argument('--root', type=Path, required=True)
    install.add_argument('--tea', type=Path, required=True)
    install.add_argument('--profile', choices=('development', 'production'), default='production')
    for command in ('verify','show'):
        cmd=sub.add_parser(command)
        cmd.add_argument('--root', type=Path, required=True)
    revert=sub.add_parser('rollback')
    revert.add_argument('--root', type=Path, required=True)
    revert.add_argument('--release', required=True)
    args=p.parse_args()
    try:
        if args.cmd == 'stage':
            gen=stage(args.root, args.tea, args.profile)
            print('STAGE PASS (HOST ONLY; NOT BOOTABLE):', gen)
        elif args.cmd in ('verify','show'):
            gen, cat=verify(args.root)
            print('VERIFY PASS (HOST ONLY):', gen)
            if args.cmd=='show':print(json.dumps(cat, ensure_ascii=False, indent=2))
        else:
            print('ROLLBACK PASS (HOST ONLY):', rollback(args.root, args.release))
        return 0
    except (ProvisionError, OSError, ValueError) as exc:
        print('STAGE REJECTED:', exc, file=sys.stderr)
        return 1

if __name__=='__main__':
    raise SystemExit(main())
