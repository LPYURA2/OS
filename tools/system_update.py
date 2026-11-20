#!/usr/bin/env python3
"""Build a fresh system disk or sign an offline StyleOS x86-64 update."""
import argparse
import hashlib
import os
from pathlib import Path
import stat
import struct
import subprocess
import tempfile
import uuid
import zlib

import make_disk


SECTOR = 512
REGION = 133120
SLOT_SECTORS = 2048
SLOT_A = REGION + 8
SLOT_B = SLOT_A + SLOT_SECTORS
MAX_PAYLOAD = SECTOR * (SLOT_SECTORS - 1)
DER_PREFIX = bytes.fromhex('302a300506032b6570032100')


def build_number(n):
    n = int(n)
    if not 0 < n < 2**32:
        raise ValueError('build must be a positive uint32')
    return n


def create_update_disk(efi, output, build):
    build = build_number(build)
    output = Path(output)
    assert make_disk.DISK_SECTORS == 139264
    assert make_disk.ESP_START_LBA + make_disk.ESP_SECTORS == REGION
    with tempfile.TemporaryDirectory(prefix='.system-disk-', dir=output.parent) as folder:
        image = make_disk.create_disk(efi, Path(folder) / 'system.img')
        entries = bytearray(make_disk._partition_array())
        offset = make_disk.GPT_ENTRY_SIZE
        struct.pack_into('<16s16sQQQ', entries, offset,
                         uuid.UUID('9380efcc-3828-45d7-98e4-7674733c984b').bytes_le,
                         uuid.UUID('12858542-2dc0-4e19-bb89-4509f09f599e').bytes_le,
                         REGION, SLOT_B + SLOT_SECTORS - 1, 0)
        name = 'StyleOS Updates'.encode('utf-16-le')
        entries[offset + 56:offset + 56 + len(name)] = name
        crc = zlib.crc32(entries)
        end = make_disk.DISK_SECTORS - 1
        backup = end - make_disk.GPT_ARRAY_SECTORS
        superblock = struct.pack('<8s5I', b'STYLEUP1', 1, end + 1, SLOT_A, SLOT_B, SLOT_SECTORS)
        journal = struct.pack('<8s6I', b'STYUJR01', 1, 0, build, 0, 0, 0)
        with image.open('r+b') as f:
            for lba, data in ((1, make_disk._gpt_header(1, end, 2, crc)), (2, entries),
                              (backup, entries), (end, make_disk._gpt_header(end, 1, backup, crc)),
                              (REGION, checked_sector(superblock)), (REGION + 1, checked_sector(journal))):
                make_disk._write_at(f, lba * SECTOR, data)
            f.flush()
            os.fsync(f.fileno())
        # Create a new disk. Never overwrite a previously used system disk.
        os.link(image, output)
    return output


def checked_sector(prefix):
    return (prefix + struct.pack('<I', zlib.crc32(prefix))).ljust(SECTOR, b'\0')


def raw_public(key):
    der = subprocess.check_output(['openssl', 'pkey', '-in', str(key), '-pubout', '-outform', 'DER'])
    if len(der) != len(DER_PREFIX) + 32 or not der.startswith(DER_PREFIX):
        raise ValueError('signing key must be Ed25519')
    return der[len(DER_PREFIX):]


def sign(efi, output, build, key):
    build = build_number(build)
    raw_public(key)
    file_stat = Path(efi).stat()
    if not stat.S_ISREG(file_stat.st_mode) or not 128 <= file_stat.st_size <= MAX_PAYLOAD:
        raise ValueError('invalid EFI payload length')
    payload = Path(efi).read_bytes()
    pe = struct.unpack_from('<I', payload, 60)[0]
    if payload[:2] != b'MZ' or not 64 <= pe <= len(payload) - 26 or (
            payload[pe:pe+6] != b'PE\0\0\x64\x86' or payload[pe+24:pe+26] != b'\x0b\x02'):
        raise ValueError('expected x86-64 PE32+ EFI payload')
    header = struct.pack('<8sIIII64s', b'STYUPD01', 1, 0x8664, build, len(payload),
                         hashlib.sha512(payload).digest())
    with tempfile.TemporaryDirectory(prefix='.sign-update-', dir=Path(output).parent) as folder:
        src, dst = Path(folder) / 'manifest', Path(folder) / 'signature'
        src.write_bytes(header)
        subprocess.run(['openssl', 'pkeyutl', '-sign', '-rawin', '-inkey', str(key),
                        '-in', str(src), '-out', str(dst)], check=True)
        signature = dst.read_bytes()
        if len(signature) != 64:
            raise ValueError('invalid signature size')
        src.write_bytes((header + signature).ljust(SECTOR, b'\0') + payload)
        os.link(src, output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    for name in ('disk', 'sign', 'public'):
        cmd = sub.add_parser(name)
        cmd.add_argument('--output', type=Path, required=True)
        if name != 'disk':
            cmd.add_argument('--key', type=Path, required=True)
        if name != 'public':
            cmd.add_argument('--efi', type=Path, required=True)
            cmd.add_argument('--build', type=build_number, required=True)
    a = parser.parse_args()
    try:
        if a.action == 'public':
            with a.output.open('xb') as f:
                f.write(raw_public(a.key))
        elif a.action == 'disk':
            create_update_disk(a.efi, a.output, a.build)
        else:
            sign(a.efi, a.output, a.build, a.key)
    except (OSError, ValueError, subprocess.CalledProcessError) as e:
        parser.exit(1, f'{e}\n')


if __name__ == '__main__':
    main()
