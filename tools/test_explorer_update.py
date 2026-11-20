#!/usr/bin/env python3
"""Apply build 703 from build 701, then exercise the new Explorer by mouse."""
import argparse
import json
import os
from pathlib import Path
import shutil

from test_desktop_v0 import command_exists, digest, firmware_path, vm
from test_stylefs_desktop import command, file_box, keys, row
from test_system_update import click_updates, guest_restart


def run(args):
    qemu = command_exists(os.environ.get('QEMU_X86', 'qemu-system-x86_64'))
    qimg = command_exists(os.environ.get('QEMU_IMG', 'qemu-img'))
    firmware = (firmware_path('OVMF_CODE', 'OVMF_CODE'), firmware_path('OVMF_VARS', 'OVMF_VARS'))
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    boot, data = out/'system.vdi', out/'data.vdi'
    shutil.copyfile(args.system701, boot)
    command(qimg, 'convert', '-f', 'raw', '-O', 'vdi', args.seed, data)
    before = digest(data)
    settings = dict(image_format='vdi', data_format='vdi', persist_boot=True)

    # The original 701 only supports mouse-open, then needs keyboard confirm.
    # Prove that the same original disk accepts the new 703 as an ordinary update.
    with vm(out/'update', boot, data, qemu, firmware, **settings) as (q, initial):
        q.wait_uart('UPDATE RUN 701')
        x, y, side, _ = box = file_box(initial, True)
        click_updates(q, box)
        row(q, box, 0)
        q.click(x + side + 292, y + 105)
        q.key('ret')
        q.wait_uart('UPDATE STAGED')
        guest_restart(q, 2)
        q.wait_uart('UPDATE RUN 703')
        q.wait_uart('UPDATE ACCEPTED')

        # Detect both real disk cards and open the data card by mouse.
        q.click(x + 80, y + 363)
        q.wait_uart('STYLEFS V2', count=2)
        frame = q.screen('computer')
        card = min((initial.w - x - 46 - side - 43) // 2, 220)
        frame.check(x + side + 14 + 5, y + 129, '1b283a')
        frame.check(x + side + 28 + card + 5, y + 129, '1b283a')
        q.click(x + side + 45 + card, y + 156)
        q.screen('root')

        # Two different input routes on an actual text document and one Cyrillic filter.
        q.click(x + 90, y + 164)
        q.screen('documents-list')
        w = initial.w - x - 46
        q.click(x + w - 31, y + 104)
        q.screen('documents-grid').check(x + w - 44, y + 105, '76572f')
        q.click(x + w - 70, y + 104)
        q.click(x + w - 75, y + 62)
        keys(q, ('f6', 'g', 'k', 'f', 'y', 'f6', 'ret'))
        q.screen('filtered-list')
        row(q, box, 0)
        q.click(x + side + 300, y + 105)
        q.wait_uart('DOCUMENT OPEN')
        q.screen('opened-by-mouse')
        q.key('f2')
        q.key('esc')  # Clear search to select a different real document.
        row(q, box, 2)
        q.key('ret')
        q.wait_uart('DOCUMENT OPEN', count=2)

        # Force a signed but old package through both mouse actions.
        # The previous package is still on the test data disk unchanged.
        q.key('f2')
        click_updates(q, box)
        row(q, box, 0)
        q.click(x + side + 300, y + 105)
        q.screen('install-confirmation')
        h = initial.h - 54 - y - 49
        buttons_y = y + h - 43 - 100 + 77
        q.click(x + side + 205, buttons_y)  # Отмена
        q.screen('cancelled')
        assert b'UPDATE REJECT VERSION' not in q.uart.read_bytes()
        q.click(x + side + 300, y + 105)
        q.click(x + side + 79, buttons_y)  # Установить
        q.wait_uart('UPDATE REJECT VERSION')
        row(q, box, 0)
        q.key('ret')  # The same confirmation is still reachable by Enter.
        q.key('ret')
        q.wait_uart('UPDATE REJECT VERSION', count=2)

        # A separate later package is installed entirely by mouse from the new Explorer.
        row(q, box, 1)
        q.click(x + side + 300, y + 105)
        q.click(x + side + 79, buttons_y)
        q.wait_uart('UPDATE STAGED', count=2)
        q.click(82, q.h - 27)
        q.click(110, q.h - 54 - 273 + 175)
        q.wait_uart('REBOOT', count=2)
        q.wait_uart('READY', count=3)
        q.wait_uart('UPDATE RUN 704')
        q.wait_uart('UPDATE ACCEPTED', count=2)

    with vm(out/'power-cycle', boot, data, qemu, firmware, **settings) as (q, _):
        q.wait_uart('UPDATE RUN 704')
    assert digest(data) == before, 'StyleFS data changed during a system update'
    (out/'results.json').write_text(json.dumps({'status': 'PASS', 'checks': [
        'original 701 to 703 signed update', 'two disk cards; open data card by mouse',
        'list and grid selector', 'Cyrillic search; open filtered text by click and by Enter',
        'dialog cancellation and explicit install by mouse; Enter still confirms',
        '703 to 704 using only mouse, followed by fresh VM boot',
        'data VDI byte-identical']}, indent=2) + '\n')
    print('EXPLORER UPDATE: PASS')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('system701', 'seed', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    run(p.parse_args())
