#!/usr/bin/env python3
"""Real guest acceptance: F3 Editor + F5 Notes keep distinct drafts and PIDs across F10."""
import argparse
import json
import os
from pathlib import Path
import re

from test import firmware_path
from test_desktop_v0 import command_exists, vm, digest


def crop(frame, x, y, w, h):
    assert x >= 0 and y >= 0 and x + w <= frame.w and y + h <= frame.h
    return b''.join(frame.pixels[(yy * frame.w + x) * 3:
                                 (yy * frame.w + x + w) * 3]
                    for yy in range(y, y + h))


def exercise(iso, folder):
    qemu = command_exists(os.environ.get('QEMU_X86', 'qemu-system-x86_64'))
    firmware = (firmware_path('OVMF_CODE', 'OVMF_CODE'),
                firmware_path('OVMF_VARS', 'OVMF_VARS'))
    if not all(firmware):
        raise RuntimeError('OVMF_CODE and OVMF_VARS are required')
    folder.mkdir(parents=True, exist_ok=False)
    initial_hash = digest(iso)
    with vm(folder / 'qemu', iso, None, qemu, firmware,
            image_format='iso', data_format='raw') as (guest, first):
        h = first.h - 54
        ed_x = first.w * 405 // 1536 + 18
        ed_y = h * 175 // 804 + 96
        no_x = first.w * 435 // 1536 + 18
        no_y = h * 200 // 804 + 96
        guest.key('f3')
        guest.wait_uart('WINDOW OPEN')
        guest.key('ctrl-n')
        blank_editor = crop(guest.screen('blank-editor'), ed_x, ed_y, 105, 36)
        for key in ('a', 'b', 'c'): guest.key(key)
        editor_before = crop(guest.screen('editor-abc'), ed_x, ed_y, 105, 36)
        if editor_before == blank_editor:
            raise AssertionError('editor text not visibly changed by typing')
        guest.key('f5')
        guest.wait_uart('WINDOW OPEN', count=2)
        guest.key('ctrl-n')
        blank_note = crop(guest.screen('blank-notes'), no_x, no_y, 105, 36)
        for key in ('x', 'y', 'z'): guest.key(key)
        notes_before = crop(guest.screen('notes-xyz'), no_x, no_y, 105, 36)
        if notes_before == blank_note:
            raise AssertionError('notes text not visibly changed by typing')
        guest.key('f8')
        guest.wait_uart('WINDOW OPEN', count=3)
        guest.key('f10')
        guest.wait_uart('SHELL RESTART')
        guest.key('f3')
        editor_after = crop(guest.screen('editor-after-f10'), ed_x, ed_y, 105, 36)
        if editor_after != editor_before:
            raise AssertionError('F10 lost or changed unsaved Editor draft')
        guest.key('f5')
        notes_after = crop(guest.screen('notes-after-f10'), no_x, no_y, 105, 36)
        if notes_after != notes_before:
            raise AssertionError('F10 lost or mixed unsaved Notes draft')
        uart = guest.uart.read_bytes().decode('utf-8', 'replace')
        starts = [int(x) for x in re.findall(r'STYLEOS PROCESS START pid=(\d+)', uart)]
        restarts = [int(x) for x in re.findall(r'STYLEOS SHELL RESTART pid=(\d+)', uart)]
        if len(starts) != 4 or len(set(starts)) != 4:
            raise AssertionError(f'expected four distinct app PIDs, got {starts}')
        if len(restarts) != 1 or restarts[0] <= max(starts):
            raise AssertionError(f'wrong new shell PID: {restarts}, app PIDs: {starts}')
    if digest(iso) != initial_hash:
        raise AssertionError('test modified the ISO')
    report = {
        'status': 'PASS', 'iso_sha256': initial_hash,
        'app_pids_preserved': starts, 'new_shell_pid': restarts[0],
        'checks': [
            'booted native UEFI ISO in QEMU/OVMF',
            'F3 and F5 open distinct editable windows',
            'separate unsaved Editor abc and Notes xyz visible on screen',
            'F8 process panel shows separately started sessions',
            'F10 changes only shell PID, not app PIDs',
            'F3/F5 restore pixel-identical independent drafts after F10',
            'test does not alter the boot ISO'
        ]}
    (folder / 'results.json').write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n')
    print('STYLEOS 730 F5 NOTES / F10 QEMU PASS')

if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--iso', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    exercise(args.iso.resolve(strict=True), args.output.resolve())
