#!/usr/bin/env python3
"""Test the package-import wire protocol over a real emulated COM2."""
import argparse
import json
import os
from pathlib import Path
import shutil
import select
import struct
import tempfile

from test_desktop_v0 import command_exists, digest, firmware_path, vm
from test_stylefs_desktop import command, file_box, row
from test_system_update import click_updates, guest_restart

ACK, NAK = b'\x06', b'\x15'


class Pipe:
    def __init__(self, endpoint):
        self.endpoint = endpoint

    def __enter__(self):
        self.out = os.open(str(self.endpoint) + '.in', os.O_RDWR | os.O_NONBLOCK)
        self.inp = os.open(str(self.endpoint) + '.out', os.O_RDWR | os.O_NONBLOCK)
        # The firmware may have emitted terminal text before this client
        # connected. Do not treat that stale output as the header ACK.
        for _ in range(64):
            try:
                if not os.read(self.inp, 1024):
                    break
            except BlockingIOError:
                break
        return self

    def __exit__(self, *unused):
        os.close(self.out)
        os.close(self.inp)

    def sendall(self, payload):
        if os.write(self.out, payload) != len(payload):
            raise RuntimeError('incomplete test write')

    def recv(self, length):
        ready, _, _ = select.select([self.inp], [], [], 10)
        if not ready:
            raise RuntimeError('COM2 reply timeout')
        return os.read(self.inp, length)


def send(peer, payload, expected):
    peer.sendall(struct.pack('<8sI', b'STYHOST1', len(payload)))
    reply = peer.recv(1)
    assert reply == ACK, ('header was not admitted', reply)
    for offset in range(0, len(payload), 12):
        peer.sendall(payload[offset:offset+12])
        reply = peer.recv(1)
        needed = expected if offset + 12 >= len(payload) else ACK
        assert reply == needed, (offset, reply, needed)


def run(args):
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    system, data = out / 'system.vdi', out / 'data.vdi'
    qemu = command_exists(os.environ.get('QEMU_X86', 'qemu-system-x86_64'))
    qimg = command_exists(os.environ.get('QEMU_IMG', 'qemu-img'))
    firmware = (firmware_path('OVMF_CODE', 'OVMF_CODE'), firmware_path('OVMF_VARS', 'OVMF_VARS'))
    shutil.copyfile(args.system, system)
    command(qimg, 'convert', '-f', 'raw', '-O', 'vdi', args.seed, data)
    source_system = digest(system)
    payload = args.package.read_bytes()
    tampered = bytearray(payload)
    tampered[88] ^= 1
    # Real QEMU serial ports using local FIFOs, not a simulated guest parser.
    with tempfile.TemporaryDirectory(prefix='styleos-com2-') as temp:
        endpoint = Path(temp) / 'link'
        os.mkfifo(str(endpoint) + '.in', 0o600)
        os.mkfifo(str(endpoint) + '.out', 0o600)
        with vm(out/'import', system, data, qemu, firmware, image_format='vdi',
                data_format='vdi', persist_boot=True, transfer_pipe=endpoint) as (q, frame):
            x, y, side, _ = box = file_box(frame, True)
            click_updates(q, box)
            q.wait_uart('UPDATES FOLDER')
            with Pipe(endpoint) as link:
                q.click(x + side + 195, y + 105)  # С ПК
                q.wait_uart('IMPORT ARMED')
                q.screen('waiting')
                send(link, bytes(tampered), NAK)
                q.wait_uart('IMPORT INVALID')
                q.click(x + side + 195, y + 105)
                q.wait_uart('IMPORT ARMED', count=2)
                link.sendall(struct.pack('<8sI', b'STYHOST1', len(payload)))
                assert link.recv(1) == ACK
                link.sendall(payload[:12])
                assert link.recv(1) == ACK
                q.key('esc')
                assert link.recv(1) == NAK
                q.wait_uart('IMPORT CANCELLED')
            with Pipe(endpoint) as link:
                q.click(x + side + 195, y + 105)
                q.wait_uart('IMPORT ARMED', count=3)
                send(link, payload, ACK)
                q.wait_uart('IMPORT SAVED')
                q.screen('imported')
                assert b'UPDATE STAGED' not in q.uart.read_bytes()
            # Use the real file after a fresh connection; no disk is swapped.
            with Pipe(endpoint) as link:
                q.click(x + side + 195, y + 105)
                q.wait_uart('IMPORT ARMED', count=4)
                send(link, payload, NAK)
                q.wait_uart('IMPORT EXISTS')
    assert digest(system) == source_system, 'import itself modified the system VDI'
    raw, imported = out/'after.raw', out/'readback.supdate'
    command(qimg, 'convert', '-f', 'vdi', '-O', 'raw', data, raw)
    listing = command(args.image_tool, 'list', raw, '/Обновления')
    assert len(listing.splitlines()) == 1
    command(args.image_tool, 'get', raw, '/Обновления/Build-708.supdate', imported)
    assert imported.read_bytes() == payload
    original = out/'original.txt'
    command(args.image_tool, 'get', raw, '/Документы/Original.txt', original)
    assert original.read_bytes() == b'My original file.\n'
    with vm(out/'install', system, data, qemu, firmware, image_format='vdi',
            data_format='vdi', persist_boot=True) as (q, frame):
        box = file_box(frame, True)
        click_updates(q, box)
        row(q, box, 0)
        q.key('ret')
        q.key('ret')
        q.wait_uart('UPDATE STAGED')
        guest_restart(q, 2)
        q.wait_uart('UPDATE RUN 708')
        q.wait_uart('UPDATE ACCEPTED')
    evidence = {'status': 'PASS', 'checks': ['real serial channel COM2 in an active guest',
        'tampered signature and cancelled partial transfer leave no output file',
        'signed package imports into existing StyleFS; duplicate does not overwrite',
        'unrelated data and whole system VDI unchanged by import',
        'stored byte-for-byte then confirmed through existing updater; boot 708']}
    (out/'results.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print('LIVE UPDATE IMPORT: PASS')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('system', 'seed', 'package', 'image-tool', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    run(p.parse_args())
