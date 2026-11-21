#!/usr/bin/env python3
"""QEMU/OVMF guest acceptance: F10 refresh retains PID and unsaved editor UI."""
import argparse
import json
import os
from pathlib import Path
import re

from test import firmware_path
from test_desktop_v0 import command_exists, vm, digest


def crop(frame, x, y, w, h):
    return b''.join(frame.pixels[(yy * frame.w + x) * 3:
                                 (yy * frame.w + x + w) * 3]
                    for yy in range(y, y + h))


def run(iso, out):
    qemu = command_exists(os.environ.get('QEMU_X86', 'qemu-system-x86_64'))
    firmwares = (firmware_path('OVMF_CODE', 'OVMF_CODE'),
                 firmware_path('OVMF_VARS', 'OVMF_VARS'))
    if not all(firmwares):
        raise RuntimeError('OVMF firmware required')
    out.mkdir(parents=True, exist_ok=False)
    original_iso = digest(iso)
    with vm(out / 'qemu', iso, None, qemu, firmwares,
            image_format='iso', data_format='raw') as (guest, first):
        guest.wait_uart('DATA UNAVAILABLE')
        guest.key('f3')
        guest.wait_uart('WINDOW OPEN')
        guest.key('ctrl-n')
        blank = guest.screen('blank-editor')
        for name in ('a', 'b', 'c'):
            guest.key(name)
        typed = guest.screen('typed-unsaved')
        # Legacy 709 geometry, used without a StyleFS data disk.
        x = first.w * 405 // 1536 + 18
        y = (first.h - 54) * 175 // 804 + 97
        before = crop(typed, x, y, 100, 40)
        if before == crop(blank, x, y, 100, 40):
            raise RuntimeError('no visible unsaved editor text before F10')
        guest.key('f8')
        guest.key('f10')
        guest.wait_uart('SHELL RESTART')
        guest.key('f3')
        guest.screen('after-f10')
        after = guest.screen('restored-draft')
        if crop(after, x, y, 100, 40) != before:
            raise RuntimeError('unsaved document pixels changed across F10')
        uart = guest.uart.read_bytes().decode('utf-8', 'replace')
        started = [int(pid) for pid in re.findall(r'STYLEOS PROCESS START pid=(\d+)', uart)]
        restarts = [int(pid) for pid in re.findall(r'STYLEOS SHELL RESTART pid=(\d+)', uart)]
        if len(started) != 3 or len(set(started)) != 3:
            raise RuntimeError(f'unexpected app PID startups: {started}')
        if len(restarts) != 1 or restarts[0] <= max(started):
            raise RuntimeError(f'invalid shell PID renewal: {restarts} after {started}')
        result = {
            'status': 'PASS',
            'boot_sha256': original_iso,
            'app_pids_before_and_after_f10': started,
            'new_shell_pid': restarts[0],
            'checks': [
                'real QEMU UEFI ISO boot and PS/2 keyboard',
                'unsaved text physically visible in editor before F10',
                'F8 opens process panel with no app restart',
                'F10 refreshes compositor and allocates fresh shell PID',
                'F10 does not restart app PIDs',
                'unsaved editor pixels unchanged after F10',
                'original ISO unchanged on disk'
            ]
        }
    if digest(iso) != original_iso:
        raise RuntimeError('QEMU test changed original ISO')
    (out / 'results.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print('RESTORED 730 SHELL F10 QEMU: PASS')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--iso', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    run(args.iso.resolve(strict=True), args.output.resolve())
