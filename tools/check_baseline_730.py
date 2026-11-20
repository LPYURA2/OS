#!/usr/bin/env python3
"""Read-only provenance + architecture guard for approved StyleOS baselines.

This script never modifies, mounts, boots or repairs either image. It is not
an emulator and does not certify that a new StyleOS build works.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
import zipfile

from audit_744 import audit, iso_files

KNOWN = {
    '730': {
        'zip_sha256': 'ae9641da573bc8618dcde06b6effa30fb8fb058adae35a7e00df4a2cafd1f407',
        'iso_sha256': '175db7b0d8ec777a2b2da0c69b3b41b94ac5e837f49a7da25dcb49243da7ce91',
        'efi_sha256': 'e84740348fbfd32ce5808258614c072c673db806003ae26b632b115fee317e7e',
        'file_names': ['START.txt', 'StyleOS-Install-x64.iso', 'StyleOS-Data-2.vdi',
                       'Installer-Preview.png', 'SHA256SUMS.txt'],
        'old_app_marker': b'Documents.sapp', 'tea_marker': False,
    },
    '744': {
        'zip_sha256': '22e87caca314d43bd8832fe35b8838dc18179105e32c59a204e3e18d43c825af',
        'iso_sha256': '011dd3560e8d232b00201623443d9a09b0c67e03227b66504312a31f75e285ae',
        'efi_sha256': 'be89054aaae7efd10f83b10b07f3bd2c50bad113b61374b00f8d9dcc8c219f24',
        'file_names': ['StyleOS-Install-x64.iso', 'StyleOS-Data-2.vdi', 'START.txt',
                       'BUILD-NOTES.md', 'Documents-Preview.png', 'SHA256SUMS.txt'],
        'old_app_marker': b'Documents.tea', 'tea_marker': True,
    },
}

class BaselineError(ValueError):
    pass


def digest(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def check_zip(archive: Path, version: str) -> dict:
    if version not in KNOWN:
        raise BaselineError('unsupported baseline')
    expect = KNOWN[version]
    raw = archive.read_bytes()
    if digest(raw) != expect['zip_sha256']:
        raise BaselineError(f'{version} archive changed: SHA-256 mismatch')
    with zipfile.ZipFile(archive) as z:
        infos = z.infolist()
        if (len(infos) != len(expect['file_names']) or
                set(x.filename for x in infos) != set(expect['file_names']) or
                any(x.is_dir() or x.filename.startswith('/') or '..' in Path(x.filename).parts for x in infos)):
            raise BaselineError('archive contents differ from the approved control')
        if z.testzip() is not None:
            raise BaselineError('ZIP CRC mismatch')
        iso = z.read('StyleOS-Install-x64.iso')
        if digest(iso) != expect['iso_sha256']:
            raise BaselineError('ISO checksum mismatch')
        # The ISO audit parses ISO9660 + embedded FAT without executing guest code.
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as temp:
            iso_file = Path(temp)/'control.iso'
            iso_file.write_bytes(iso)
            state = audit(iso_file)
        if state['efi_sha256'] != expect['efi_sha256']:
            raise BaselineError('EFI checksum mismatch')
        img_files = iso_files(iso)
        efi = img_files['/EFI/BOOT/BOOTX64.EFI']
        if expect['old_app_marker'] not in efi:
            raise BaselineError('expected historical app marker not found')
        if state['documents_tea_mentioned_in_boot_executable'] != expect['tea_marker']:
            raise BaselineError('unexpected historical application packaging state')
        sums = z.read('SHA256SUMS.txt').decode('ascii')
        for line in sums.splitlines():
            expected_sum, filename = line.split(None, 1)
            filename = filename.strip('* ').strip()
            if filename not in z.namelist() or digest(z.read(filename)) != expected_sum:
                raise BaselineError(f'archive SHA256SUMS entry invalid: {filename}')
        return {
            'baseline': version, 'unchanged': True,
            'zip_sha256': digest(raw), 'iso_sha256': state['iso_sha256'],
            'efi_sha256': state['efi_sha256'],
            'iso_efi_matches_esp': state['iso_and_esp_efi_identical'],
            'standalone_documents_tea_in_iso': state['standalone_documents_tea_file_in_installer'],
            'native_source_code_in_archive': False,
            'verified_by': 'read-only binary+checksum audit, not a VM boot',
        }


def check_native_app_catalog(path: Path) -> dict:
    catalog = json.loads(path.read_text(encoding='utf-8'))
    if catalog.get('format') != 'styleos.system-apps.plan.v1':
        raise BaselineError('not a supported system app manifest')
    apps = catalog.get('apps')
    if not isinstance(apps, list) or not apps:
        raise BaselineError('system app catalog is empty')
    seen = set()
    for app in apps:
        if not isinstance(app, dict):
            raise BaselineError('invalid app object')
        app_id = app.get('id')
        filename = app.get('package_name')
        if not isinstance(app_id, str) or not app_id or app_id in seen:
            raise BaselineError('invalid/duplicate app ID')
        seen.add(app_id)
        if not isinstance(filename, str) or not filename.endswith('.tea') or Path(filename).name != filename:
            raise BaselineError('StyleOS native app packages MUST be .tea only')
        if app.get('process') != 'independent':
            raise BaselineError('native app must be independent of desktop shell')
        if app.get('preinstall') is not True or app.get('location') != 'system-volume':
            raise BaselineError('built-in apps must be preinstalled to system volume')
    if not any(app['id'] == 'styleos.documents' and app['package_name'] == 'Documents.tea' for app in apps):
        raise BaselineError('system Documents.tea missing from preinstall catalog')
    return {'registered_system_apps': len(apps), 'native_format': '.tea',
            'documents_preinstall': True, 'separate_process_contract': True,
            'note': 'catalog policy only; no native guest application launch was tested'}


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--730', dest='zip730', type=Path)
    p.add_argument('--744', dest='zip744', type=Path)
    p.add_argument('--catalog', type=Path)
    p.add_argument('--output', type=Path)
    args = p.parse_args(argv)
    if not (args.zip730 or args.zip744 or args.catalog):
        p.error('provide an archive or catalog')
    try:
        result = {'check_kind': 'read_only_baseline_and_contract',
                  'not_a_system_build': True, 'not_a_guest_runtime_test': True}
        for version in ('730', '744'):
            path = getattr(args, 'zip'+version)
            if path is not None:
                result['baseline_'+version] = check_zip(path, version)
        if args.catalog:
            result['catalog'] = check_native_app_catalog(args.catalog)
        serialized = json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True)+'\n'
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(serialized, encoding='utf-8')
        else:
            sys.stdout.write(serialized)
        return 0
    except (OSError, ValueError, zipfile.BadZipFile, KeyError) as exc:
        print('BASELINE GATE FAIL:', str(exc), file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
