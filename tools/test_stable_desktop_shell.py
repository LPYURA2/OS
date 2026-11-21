#!/usr/bin/env python3
"""Compare real desktop pixels across storage states, with 707 as a control."""
import argparse
import json
import os
from pathlib import Path

from make_desktop_data import provision
from test_desktop_v0 import command_exists, digest, firmware_path, vm
from test_stylefs_desktop import file_box, keys


def region(frame, x, y, w, h):
    return tuple(frame.pixels[(frame.w * row + x) * 3:(frame.w * row + x + w) * 3]
                 for row in range(y, y + h))


def run(args):
    qemu = command_exists(os.environ.get('QEMU_X86', 'qemu-system-x86_64'))
    firmware = (firmware_path('OVMF_CODE', 'OVMF_CODE'), firmware_path('OVMF_VARS', 'OVMF_VARS'))
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    old_notes = out/'old-notes.raw'
    provision(old_notes)
    foreign = out/'foreign.raw'
    with foreign.open('xb') as f:
        f.write(b'not StyleFS')
        f.truncate(args.data.stat().st_size)
    initial = digest(foreign), digest(args.data)
    opts = dict(image_format='vdi', data_format='raw')

    with vm(out/'707-without-data', args.before, None, qemu, firmware, **opts) as (q, before):
        q.wait_uart('UPDATE RUN 707')
        q.wait_uart('DATA UNAVAILABLE')
    with vm(out/'707-with-data', args.before, args.data, qemu, firmware, **opts) as (q, correct):
        q.wait_uart('UPDATE RUN 707')
        q.wait_uart('STYLEFS V2')
    assert (before.w, before.h) == (correct.w, correct.h)
    assert region(before, 18, 38, 100, 280) != region(correct, 18, 38, 100, 280), '707 control did not reproduce'
    x, y, _, _ = file_box(correct, True)

    for name, data, status in (('709-with-data', args.data, 'DATA STYLEFS'),
                               ('709-without-data', None, 'DATA ABSENT'),
                               ('709-foreign', foreign, 'DATA UNRECOGNIZED'),
                               ('709-old-notes', old_notes, 'DATA LEGACY')):
        with vm(out/name, args.after, data, qemu, firmware, **opts) as (q, frame):
            q.wait_uart('UPDATE RUN 709')
            q.wait_uart(status)
            assert (frame.w, frame.h) == (correct.w, correct.h)
            assert region(correct, 18, 38, 100, 280) == region(frame, 18, 38, 100, 280), name
            assert region(correct, x + 13, y + 6, 425, 33) == region(frame, x + 13, y + 6, 425, 33), name
            if data == old_notes:
                q.click(64, 270)   # Projects on the old notes backend.
                q.key('f3')
                keys(q, ('ctrl-n', 's', 't', 'y', 'l', 'e', 'ctrl-s'))
                q.wait_uart('DOCUMENT SAVE')

    with vm(out/'709-old-notes-restart', args.after, old_notes, qemu, firmware, **opts) as (q, frame):
        q.wait_uart('DATA LEGACY')
        q.click(64, 270)
        x, y, _, _ = file_box(frame, True)
        q.click(x + 108, y + 111)
        q.wait_uart('DOCUMENT OPEN')
        q.screen('legacy-text')
    assert (digest(foreign), digest(args.data)) == initial
    (out/'results.json').write_text(json.dumps({'status': 'PASS', 'checks': [
        'same 707 EFI has different desktop icons with and without the data disk',
        '709 shortcuts and Explorer title pixels match the accepted modern 707 UI on all four storage states',
        'no data, legacy notes, unsupported volume, and healthy StyleFS are distinct',
        'old notes backend writes and reopens in the same modern appearance',
        'untouched StyleFS and rejected disk are byte-identical']}, indent=2) + '\n')
    print('STABLE DESKTOP SHELL: PASS')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('before', 'after', 'data', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    run(p.parse_args())
