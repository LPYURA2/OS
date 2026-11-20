#!/usr/bin/env python3
"""Verify the dedicated update folder, pointer actions, and keyboard layout."""
import argparse
import json
import os
from pathlib import Path
import shutil
import time

from test_desktop_v0 import command_exists, digest, firmware_path, vm
from test_stylefs_desktop import command, file_box, row
from test_system_update import click_updates, guest_restart


def run(args):
    qemu = command_exists(os.environ.get('QEMU_X86', 'qemu-system-x86_64'))
    qimg = command_exists(os.environ.get('QEMU_IMG', 'qemu-img'))
    firmware = (firmware_path('OVMF_CODE', 'OVMF_CODE'), firmware_path('OVMF_VARS', 'OVMF_VARS'))
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    boot, data = out / 'system.vdi', out / 'data.vdi'
    shutil.copyfile(args.system, boot)
    command(qimg, 'convert', '-f', 'raw', '-O', 'vdi', args.seed, data)
    sha = digest(data)
    opts = dict(image_format='vdi', data_format='vdi', persist_boot=True)

    with vm(out / 'upgrade', boot, data, qemu, firmware, **opts) as (q, frame):
        x, y, side, file_top = box = file_box(frame, True)
        q.wait_uart('UPDATE RUN 703')
        # With 703, find the real Updates directory by browsing Disk 2.
        q.click(x + 80, y + 363)
        card = min((frame.w - x - 46 - side - 43) // 2, 220)
        q.click(x + side + 45 + card, y + 156)
        row(q, box, 6)
        q.key('ret')
        row(q, box, 1)
        q.key('ret')
        q.key('ret')
        q.wait_uart('UPDATE STAGED')
        guest_restart(q, 2)
        q.wait_uart('UPDATE RUN 705')
        q.wait_uart('UPDATE ACCEPTED')
        q.wait_uart('POINTER CLOCK PASS')

        click_updates(q, box)
        q.wait_uart('UPDATES FOLDER')
        q.screen('updates')
        px, py = x + side + 105, y + file_top + 9
        q.click(px, py)          # Select, never open immediately.
        log = q.uart.read_bytes
        assert b'DOCUMENT OPEN' not in log() and b'FILE DOUBLE CLICK' not in log()
        time.sleep(.6)
        q.click(px, py)          # A slow second press is another selection.
        assert b'DOCUMENT OPEN' not in log() and b'FILE DOUBLE CLICK' not in log()
        time.sleep(.6)
        q.click(px, py)
        q.click(px, py)
        q.wait_uart('FILE DOUBLE CLICK')
        q.wait_uart('DOCUMENT OPEN')  # This document exists only in Updates.
        q.screen('opened-from-updates')

        q.click(frame.w - 80, frame.h - 27)
        q.wait_uart('INPUT RU')
        q.key('alt-shift')
        q.wait_uart('INPUT EN')
        q.key('shift-alt')
        q.wait_uart('INPUT RU', count=2)
        q.click(frame.w - 80, frame.h - 27)
        q.wait_uart('INPUT EN', count=2)
        assert log().splitlines().count(b'STYLEOS DESKTOP INPUT RU') == 2
        assert log().splitlines().count(b'STYLEOS DESKTOP INPUT EN') == 2
        q.screen('english')
    assert sha == digest(data), 'existing data with Updates folder must not be moved or written'

    # Mount an unchanged legacy volume without the new folder. Clicking the
    # shortcut creates only the empty folder, once. No implicit data move.
    legacy_disk = out / 'legacy.vdi'
    command(qimg, 'convert', '-f', 'raw', '-O', 'vdi', args.legacy_seed, legacy_disk)
    with vm(out / 'legacy', boot, legacy_disk, qemu, firmware, **opts) as (q, frame):
        q.wait_uart('UPDATE RUN 705')
        box = file_box(frame, True)
        click_updates(q, box)
        click_updates(q, box)
        q.wait_uart('UPDATES FOLDER', count=2)
    decoded = out / 'legacy-after.raw'
    command(qimg, 'convert', '-f', 'vdi', '-O', 'raw', legacy_disk, decoded)
    assert command(args.image_tool, 'list', decoded, '/').count('Обновления') == 1
    assert command(args.image_tool, 'list', decoded, '/Обновления').strip() == ''
    assert 'Build-705.supdate' in command(args.image_tool, 'list', decoded, '/Загрузки')
    result = {'status': 'PASS', 'checks': ['703 to 705 with local signed package',
        'separate Updates directory; real file opened from it', 'single click only selects',
        'slow repeat does not open; rapid double click opens a real file',
        'Alt+Shift in both orders toggles once; panel button toggles both ways',
        'existing data disk byte-identical; legacy volume creates folder exactly once',
        'no implicit move out of Downloads']}
    (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('DESKTOP INPUT + UPDATES: PASS')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('system', 'seed', 'legacy-seed', 'image-tool', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    run(p.parse_args())
