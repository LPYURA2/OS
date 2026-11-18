#!/usr/bin/env python3
"""Create a fresh StyleOS desktop v0 data disk. Refuse to overwrite any file."""
import argparse
from pathlib import Path
import struct


def provision(path):
    superblock = b'STYLEOS-DESK-V01'.ljust(512, b'\0')
    # A real empty «Заметки.txt» in slot 0, root folder, bank A.
    header = struct.pack('<8sIIBB2x', b'STYLED01', 1, 0, 0, 0)
    checksum = 2166136261
    for byte in header:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    with path.open('xb') as out:
        out.write(superblock)
        out.write((header + struct.pack('<I', checksum)).ljust(512, b'\0'))
        out.truncate(4096 * 512)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        provision(args.output)
    except OSError as exc:
        parser.error(str(exc))


if __name__ == '__main__':
    main()
