#!/usr/bin/env python3
"""Fail-closed host staging for StyleOS' nine required native .tea system apps.

Creates an installation-ready DIRECTORY only after all nine concrete .tea
packages are present and verified. Does not modify a guest disk or ISO.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

from tea_package import inspect, TeaError

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CATALOG = ROOT / 'installer' / 'system-apps.json'
REQUIRED = (
    ('explorer', 'Проводник'),
    ('settings', 'Параметры'),
    ('notes', 'Заметки'),
    ('editor', 'Текстовый редактор'),
    ('documents', 'Документы'),
    ('spreadsheets', 'Таблицы'),
    ('presentations', 'Презентации'),
    ('browser', 'Браузер'),
    ('processes', 'Процессы'),
)
FIELDS = {'id', 'name', 'filename', 'required'}


class StageError(ValueError):
    pass


def read_catalog(path=DEFAULT_CATALOG):
    obj = json.loads(Path(path).read_text(encoding='utf-8'))
    if not isinstance(obj, dict) or set(obj) != {
        'schema_version', 'native_format', 'installation_policy',
        'system_directory', 'applications'
    }:
        raise StageError('invalid system app catalog fields')
    if type(obj['schema_version']) is not int or obj['schema_version'] != 1 or \
       obj['native_format'] != 'styleos.tea' or \
       obj['installation_policy'] != 'required-system-apps' or \
       obj['system_directory'] != '/System/Applications':
        raise StageError('unsupported system app catalog contract')
    apps = obj['applications']
    if not isinstance(apps, list) or len(apps) != len(REQUIRED):
        raise StageError('all nine system apps are mandatory')
    for item, (short, title) in zip(apps, REQUIRED):
        app_id = 'org.styleos.' + short
        if not isinstance(item, dict) or set(item) != FIELDS or \
           item.get('id') != app_id or item.get('name') != title or \
           item.get('filename') != app_id + '.tea' or \
           item.get('required') is not True:
            raise StageError('invalid, missing or reordered required app: ' + app_id)
    return apps


def verify_packages(apps, packages_dir, architecture='x86_64'):
    root = Path(packages_dir)
    if not root.is_dir() or root.is_symlink():
        raise StageError('source must be a plain directory')
    expected = {app['filename'] for app in apps}
    actual = {entry.name for entry in root.iterdir()}
    if actual != expected:
        raise StageError('incomplete/unexpected native package set: ' +
                         ', '.join(sorted(expected ^ actual)))
    verified = []
    for app in apps:
        source = root / app['filename']
        if source.is_symlink() or not source.is_file():
            raise StageError('symlink or non-file package rejected')
        try:
            manifest = inspect(source)
        except (OSError, ValueError, TeaError) as exc:
            raise StageError('invalid .tea for ' + app['id']) from exc
        if manifest['id'] != app['id'] or manifest['name'] != app['name'] or \
           manifest['architecture'] != architecture:
            raise StageError('native app identity or CPU mismatch: ' + app['id'])
        verified.append({
            'id': app['id'],
            'name': app['name'],
            'version': manifest['version'],
            'architecture': architecture,
            'package_path': '/System/Applications/' + app['filename'],
            'package_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
            'elf_sha256': manifest['sha256']
        })
    return verified


def stage(packages_dir, output, *, catalog=DEFAULT_CATALOG, architecture='x86_64'):
    apps = read_catalog(catalog)
    checked = verify_packages(apps, packages_dir, architecture)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() or output.is_symlink():
        raise StageError('never overwrite an existing staging/install directory')
    temp = Path(tempfile.mkdtemp(prefix='.styleos-app-stage-', dir=output.parent))
    try:
        dst = temp / 'System' / 'Applications'
        dst.mkdir(parents=True)
        for app in apps:
            source = Path(packages_dir) / app['filename']
            target = dst / app['filename']
            shutil.copyfile(source, target)
            copied = inspect(target)
            expected = next(e for e in checked if e['id'] == app['id'])
            if copied['id'] != app['id'] or \
               hashlib.sha256(target.read_bytes()).hexdigest() != expected['package_sha256']:
                raise StageError('package changed during staging: ' + app['id'])
        index = {
            'schema_version': 1,
            'native_format': 'styleos.tea',
            'status': 'prepared-not-installed',
            'required_count': len(checked),
            'applications': checked
        }
        (dst / 'index.json').write_text(
            json.dumps(index, ensure_ascii=False, indent=2) + '\n',
            encoding='utf-8')
        # Rename only a completely verified tree. No existing installed data touched.
        if output.exists() or output.is_symlink():
            raise StageError('destination changed during staging')
        os.rename(temp, output)
        return index
    finally:
        if temp.exists():
            shutil.rmtree(temp)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('check-catalog').add_argument('--catalog', type=Path, default=DEFAULT_CATALOG)
    st = sub.add_parser('stage')
    st.add_argument('--packages', type=Path, required=True)
    st.add_argument('--output', type=Path, required=True)
    st.add_argument('--catalog', type=Path, default=DEFAULT_CATALOG)
    st.add_argument('--architecture', choices=('x86_64', 'aarch64'), default='x86_64')
    args = parser.parse_args()
    if args.command == 'check-catalog':
        print('SYSTEM APPS CATALOG VALID: ' + str(len(read_catalog(args.catalog))))
    else:
        info = stage(args.packages, args.output, catalog=args.catalog,
                     architecture=args.architecture)
        print('SYSTEM APPS STAGED (NOT GUEST INSTALLED): ' + str(info['required_count']))


if __name__ == '__main__':
    main()
