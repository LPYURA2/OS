#!/usr/bin/env python3
"""Positive, tampered package and failed trial against persistent QEMU IDE disks."""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import zlib

from system_update import REGION, SECTOR, SLOT_A, SLOT_B, SLOT_SECTORS
from test_desktop_v0 import command_exists, digest, firmware_path, vm
from test_stylefs_desktop import activate, command, file_box


def click_updates(q, box):
    q.click(box[0] + 70, box[1] + 323)


def stage(q, box, index, result, count=1):
    click_updates(q, box)
    activate(q, box, index)
    q.key('ret')
    q.wait_uart(result, count=count, timeout=30)


def system_reset(q, count):
    q.call('system_reset')
    q.wait_uart('READY', count=count, timeout=45)
    q.x, q.y = q.w // 2, q.h * 3 // 4


def guest_restart(q, count):
    q.key('f4')
    q.click(110, q.h - 54 - 273 + 175)
    q.wait_uart('REBOOT')
    q.wait_uart('READY', count=count, timeout=45)
    q.x, q.y = q.w // 2, q.h * 3 // 4


def run(folder, system, seed, good, failed, image_tool, qemu, firmware, converter):
    raw = folder / 'data.raw'
    data = folder / 'data.vdi'
    boot = folder / 'system.vdi'
    original = folder / 'pre-test.raw'
    shutil.copyfile(system, boot)
    command(converter, 'convert', '-f', 'vdi', '-O', 'raw', boot, original)
    initial = original.read_bytes()
    good_package = good.read_bytes()
    bad_signature = bytearray(good_package)
    bad_signature[88] ^= 1
    (folder / 'tampered.supdate').write_bytes(bad_signature)
    roundtrip = folder / 'owner.txt'
    roundtrip.write_text('Пользовательские файлы живут отдельно.\n')
    command(converter, 'convert', '-f', 'raw', '-O', 'raw', seed, raw)
    for name, pkg in (('Tampered.supdate', folder / 'tampered.supdate'),
                      ('Build-701.supdate', good), ('Broken-702.supdate', failed)):
        command(image_tool, 'put', raw, '/Загрузки/' + name, pkg)
    command(image_tool, 'put', raw, '/Документы/owner.txt', roundtrip)
    command(converter, 'convert', '-f', 'raw', '-O', 'vdi', raw, data)
    data_sha = digest(data)

    def options():
        return dict(image_format='vdi', data_format='vdi', persist_boot=True)

    with vm(folder / 'reject', boot, data, qemu, firmware, **options()) as (q, frame):
        q.wait_uart('UPDATE RUN 700')
        q.wait_uart('STYLEFS V2')
        box = file_box(frame, True)
        click_updates(q, box)
        activate(q, box, 1)
        q.key('esc')
        stage(q, box, 0, 'UPDATE REJECT SIGNATURE')
        q.screen('tampered-refused')
    rejected = folder / 'after-reject.raw'
    command(converter, 'convert', '-f', 'vdi', '-O', 'raw', boot, rejected)
    assert rejected.read_bytes() == initial, 'cancel/rejected signature rewrote the system disk'

    with vm(folder / 'update', boot, data, qemu, firmware, **options()) as (q, frame):
        q.wait_uart('UPDATE RUN 700')
        box = file_box(frame, True)
        stage(q, box, 1, 'UPDATE STAGED')
        q.screen('staged')
        guest_restart(q, 2)
        q.wait_uart('UPDATE TRIAL')
        q.wait_uart('UPDATE RUN 701')
        q.wait_uart('UPDATE ACCEPTED')
        system_reset(q, 3)
        q.wait_uart('UPDATE RUN 701', count=2)
        stage(q, box, 1, 'UPDATE REJECT VERSION')
        stage(q, box, 2, 'UPDATE STAGED', count=2)
        q.call('system_reset')
        q.wait_uart('UPDATE TEST STUCK', timeout=45)
        system_reset(q, 4)
        q.wait_uart('UPDATE ROLLBACK')
        q.wait_uart('UPDATE RUN 701', count=3)
        q.wait_uart('STYLEFS V2', count=4)

    with vm(folder / 'power-cycle', boot, data, qemu, firmware, **options()) as (q, frame):
        q.wait_uart('UPDATE RUN 701')
        q.wait_uart('STYLEFS V2')
        box = file_box(frame, True)
        q.click(box[0] + 75, box[1] + 166)
        activate(q, box, 0)
        q.screen('owner-document')
    final_raw = folder / 'after-update.raw'
    command(converter, 'convert', '-f', 'vdi', '-O', 'raw', boot, final_raw)
    final = final_raw.read_bytes()
    assert final[:REGION * SECTOR + SECTOR] == initial[:REGION * SECTOR + SECTOR]
    assert final[(SLOT_B + SLOT_SECTORS) * SECTOR:] == initial[(SLOT_B + SLOT_SECTORS) * SECTOR:]
    assert final[SLOT_A * SECTOR:SLOT_A * SECTOR + len(good_package)] == good_package
    journal = []
    for n in (REGION + 1, REGION + 2):
        first = final[n * SECTOR:n * SECTOR + 36]
        magic, gen, slot, build, pending, pending_build, tried, crc = struct.unpack('<8s7I', first)
        assert magic == b'STYUJR01' and crc == zlib.crc32(first[:32])
        journal.append((gen, slot, build, pending, pending_build, tried))
    assert max(journal) == (7, 1, 701, 0, 0, 0), journal
    assert data_sha == digest(data), 'separate user data VDI changed'
    return {'status': 'PASS', 'base': 700, 'installed': 701, 'rolled_back_trial': 702,
            'system_vdi_sha256': digest(system), 'active_journal': max(journal),
            'checks': ['boot from persistent system VDI, no ISO',
                       'cancel and modified Ed25519 signature leave system disk byte-identical',
                       'stage from Explorer; reboot via menu; boot and confirm 701',
                       'refuse already installed build',
                       'trial 702 stalls; reset rolls back to 701',
                       'fresh QEMU session still starts 701',
                       'EFI partition and separate user data disk unchanged']}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('system', 'data-raw', 'good', 'failed', 'image-tool', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    qemu = command_exists(os.environ.get('QEMU_X86', 'qemu-system-x86_64'))
    qimg = command_exists(os.environ.get('QEMU_IMG', 'qemu-img'))
    firmware = (firmware_path('OVMF_CODE', 'OVMF_CODE'), firmware_path('OVMF_VARS', 'OVMF_VARS'))
    if not all(firmware):
        p.error('OVMF firmware required')
    folder = a.output.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    result = run(folder, a.system.resolve(strict=True), a.data_raw.resolve(strict=True),
                 a.good.resolve(strict=True), a.failed.resolve(strict=True),
                 a.image_tool.resolve(strict=True), qemu, firmware, qimg)
    (folder / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('SYSTEM UPDATE: PASS')


if __name__ == '__main__':
    main()
