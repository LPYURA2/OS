#!/usr/bin/env python3
"""QEMU/OVMF test: secondary IDE master + slave, unchanged desktop and data."""
import argparse
import json
import os
from pathlib import Path
import subprocess

from test_desktop_v0 import vm, command_exists, digest
from test import firmware_path, ROOT


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--iso', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    exe = folder / 'stylefs-maker'
    subprocess.run([command_exists('clang'), '-std=c11', '-O2', '-DSTYLEFS_HOST_FORMAT=1',
                    '-I', str(ROOT / 'include'), str(ROOT / 'tests/make_guest_stylefs_v2.c'),
                    str(ROOT / 'kernel/stylefs.c'), '-o', str(exe)], check=True)
    disks = [folder / 'data1.img', folder / 'data2.img']
    for i, path in enumerate(disks):
        subprocess.run([str(exe), str(path), f'data{i+1}'], check=True)
    before = [digest(d) for d in disks]
    firmware = (firmware_path('OVMF_CODE', 'OVMF_CODE'), firmware_path('OVMF_VARS', 'OVMF_VARS'))
    if not all(firmware):
        parser.error('OVMF firmware required')
    qemu = command_exists(os.environ.get('QEMU_X86', 'qemu-system-x86_64'))
    with vm(folder/'dual', args.iso.resolve(strict=True), disks[0], qemu, firmware,
            image_format='iso', data_format='raw', extra_data=disks[1]) as (q, frame):
        q.wait_uart('DATA2 STYLEFS V2 MOUNT')
        q.wait_uart('DATA2 ROOT FILE FOUND')
        q.wait_uart('DATA MOUNT')
        q.screen('desktop-unchanged')
    if [digest(d) for d in disks] != before:
        raise RuntimeError('guest unexpectedly modified data volumes')
    foreign = folder / 'foreign-data2.img'
    marker = b'UNKNOWN-VOLUME-DO-NOT-MODIFY'
    foreign.write_bytes(marker + bytes(65536*512 - len(marker)))
    frozen = digest(foreign)
    with vm(folder/'foreign', args.iso.resolve(strict=True), disks[0], qemu, firmware,
            image_format='iso', data_format='raw', extra_data=foreign) as (q, frame):
        q.wait_uart('DATA2 UNRECOGNIZED - UNMODIFIED')
        q.wait_uart('DATA MOUNT')
    if digest(foreign) != frozen or digest(disks[0]) != before[0]:
        raise RuntimeError('unknown disk or existing data modified')
    (folder/'results.json').write_text(json.dumps({
        'status':'PASS', 'data_sha256':before, 'iso_sha256':digest(args.iso),
        'checks':['real IDE secondary master and slave', 'two mounted StyleFS v2 volumes',
                  'Data2 root file lookup in guest','foreign disk rejected without writes',
                  'unchanged desktop and data']
    },indent=2)+'\n')
    print('STYLEOS DUAL IDE QEMU: PASS', flush=True)


if __name__ == '__main__':
    main()
