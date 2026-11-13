#!/usr/bin/env python3
"""Create a reproducible GPT/FAT16 UEFI boot disk using only Python's stdlib.

This is a build artifact generator, not a disk formatter: it writes the declared
output file and never opens a host block device. The one ESP contains exactly
EFI/BOOT/BOOTX64.EFI. All identifiers and timestamps are fixed for reproducibility.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import stat
import struct
import tempfile
import uuid
import zlib

SECTOR_SIZE = 512
DISK_SECTORS = 68 * 1024 * 1024 // SECTOR_SIZE
ESP_START_LBA = 2048
ESP_SECTORS = 64 * 1024 * 1024 // SECTOR_SIZE
GPT_ENTRY_COUNT = 128
GPT_ENTRY_SIZE = 128
GPT_ARRAY_SECTORS = GPT_ENTRY_COUNT * GPT_ENTRY_SIZE // SECTOR_SIZE
SECTORS_PER_CLUSTER = 4
FAT_SECTORS = 128
RESERVED_SECTORS = 1
ROOT_ENTRY_COUNT = 512
ROOT_SECTORS = ROOT_ENTRY_COUNT * 32 // SECTOR_SIZE
DATA_START_SECTOR = RESERVED_SECTORS + 2 * FAT_SECTORS + ROOT_SECTORS
CLUSTER_SIZE = SECTOR_SIZE * SECTORS_PER_CLUSTER
DATA_CLUSTERS = (ESP_SECTORS - DATA_START_SECTOR) // SECTORS_PER_CLUSTER
MAX_EFI_SIZE = (DATA_CLUSTERS - 2) * CLUSTER_SIZE

ESP_TYPE_GUID = uuid.UUID("c12a7328-f81f-11d2-ba4b-00a0c93ec93b")
DISK_GUID = uuid.UUID("5354594c-454f-5300-8000-000000000001")
PARTITION_GUID = uuid.UUID("5354594c-454f-5300-8000-000000000002")
VOLUME_ID = 0x5354594C
FAT_DATE = 0x0021  # 1980-01-01, time 00:00:00.


def _partition_array() -> bytes:
    entries = bytearray(GPT_ENTRY_COUNT * GPT_ENTRY_SIZE)
    struct.pack_into(
        "<16s16sQQQ", entries, 0, ESP_TYPE_GUID.bytes_le,
        PARTITION_GUID.bytes_le, ESP_START_LBA,
        ESP_START_LBA + ESP_SECTORS - 1, 0,
    )
    name = "StyleOS ESP".encode("utf-16-le")
    entries[56:56 + len(name)] = name
    return bytes(entries)


def _gpt_header(current_lba: int, backup_lba: int, array_lba: int,
                array_crc: int) -> bytes:
    header = bytearray(SECTOR_SIZE)
    struct.pack_into(
        "<8sIIIIQQQQ16sQIII", header, 0,
        b"EFI PART", 0x00010000, 92, 0, 0,
        current_lba, backup_lba, 2 + GPT_ARRAY_SECTORS,
        DISK_SECTORS - GPT_ARRAY_SECTORS - 2, DISK_GUID.bytes_le,
        array_lba, GPT_ENTRY_COUNT, GPT_ENTRY_SIZE, array_crc,
    )
    struct.pack_into("<I", header, 16, zlib.crc32(header[:92]))
    return bytes(header)


def _protective_mbr() -> bytes:
    mbr = bytearray(SECTOR_SIZE)
    struct.pack_into("<I", mbr, 440, VOLUME_ID)
    struct.pack_into("<B3sB3sII", mbr, 446, 0, b"\x00\x02\x00",
                     0xEE, b"\xff\xff\xff", 1, DISK_SECTORS - 1)
    mbr[510:512] = b"\x55\xaa"
    return bytes(mbr)


def _fat_boot_sector() -> bytes:
    boot = bytearray(SECTOR_SIZE)
    boot[:3] = b"\xeb\x3c\x90"
    boot[3:11] = b"STYLEOS "
    struct.pack_into(
        "<HBHBHHBHHHII", boot, 11, SECTOR_SIZE, SECTORS_PER_CLUSTER,
        RESERVED_SECTORS, 2, ROOT_ENTRY_COUNT, 0, 0xF8, FAT_SECTORS,
        63, 255, ESP_START_LBA, ESP_SECTORS,
    )
    struct.pack_into("<BBBI11s8s", boot, 36, 0x80, 0, 0x29, VOLUME_ID,
                     b"STYLEOS ESP", b"FAT16   ")
    boot[510:512] = b"\x55\xaa"
    return bytes(boot)


def _directory_entry(name: bytes, attributes: int, cluster: int,
                     size: int = 0) -> bytes:
    if len(name) != 11:
        raise ValueError("FAT short names must contain exactly 11 bytes")
    entry = bytearray(32)
    entry[:11] = name
    entry[11] = attributes
    struct.pack_into("<H", entry, 16, FAT_DATE)  # Creation date.
    struct.pack_into("<H", entry, 18, FAT_DATE)  # Last access date.
    struct.pack_into("<H", entry, 24, FAT_DATE)  # Last write date.
    struct.pack_into("<HI", entry, 26, cluster, size)
    return bytes(entry)


def _cluster_offset(cluster: int) -> int:
    return ((ESP_START_LBA + DATA_START_SECTOR) * SECTOR_SIZE
            + (cluster - 2) * CLUSTER_SIZE)


def _write_at(output, offset: int, data: bytes) -> None:
    output.seek(offset)
    output.write(data)


def create_disk(efi_path: str | Path, output_path: str | Path) -> Path:
    """Atomically write a deterministic image, rejecting invalid input first.

    The input may have any nonzero size fitting the ESP. Boot-image format is
    checked by the compiler/build checks, not by this filesystem packager.
    A failed input/read/write leaves an existing output untouched.
    """
    source = Path(efi_path)
    destination = Path(output_path)
    if not stat.S_ISREG(source.stat().st_mode):
        raise ValueError("EFI input must be a regular file")
    if destination.exists() and not destination.is_file():
        raise ValueError("disk output must be a regular file")
    if source.resolve() == destination.resolve():
        raise ValueError("EFI input and disk output must be different files")
    if destination.exists() and os.path.samefile(source, destination):
        raise ValueError("EFI input and disk output refer to the same file")
    if destination.is_dir():
        raise ValueError("disk output must be a file, not a directory")

    with source.open("rb") as payload:
        metadata = os.fstat(payload.fileno())
        if not stat.S_ISREG(metadata.st_mode):
            raise ValueError("EFI input must be a regular file")
        payload_size = metadata.st_size
        if payload_size == 0:
            raise ValueError("EFI input is empty")
        if payload_size > MAX_EFI_SIZE:
            raise ValueError(f"EFI input exceeds ESP capacity ({MAX_EFI_SIZE} bytes)")
        clusters = (payload_size + CLUSTER_SIZE - 1) // CLUSTER_SIZE
        if not 4085 <= DATA_CLUSTERS < 65525:
            raise ValueError("internal FAT16 geometry is invalid")

        entries = _partition_array()
        entries_crc = zlib.crc32(entries)
        fat = bytearray(FAT_SECTORS * SECTOR_SIZE)
        struct.pack_into("<4H", fat, 0, 0xFFF8, 0xFFFF, 0xFFFF, 0xFFFF)
        for cluster in range(4, 4 + clusters):
            following = cluster + 1 if cluster < 3 + clusters else 0xFFFF
            struct.pack_into("<H", fat, cluster * 2, following)

        destination.parent.mkdir(parents=True, exist_ok=True)
        fd, name = tempfile.mkstemp(prefix=destination.name + ".", suffix=".tmp",
                                    dir=destination.parent)
        temporary = Path(name)
        try:
            with os.fdopen(fd, "w+b") as output:
                output.truncate(DISK_SECTORS * SECTOR_SIZE)
                _write_at(output, 0, _protective_mbr())
                _write_at(output, SECTOR_SIZE,
                          _gpt_header(1, DISK_SECTORS - 1, 2, entries_crc))
                _write_at(output, 2 * SECTOR_SIZE, entries)
                backup_array = DISK_SECTORS - GPT_ARRAY_SECTORS - 1
                _write_at(output, backup_array * SECTOR_SIZE, entries)
                _write_at(output, (DISK_SECTORS - 1) * SECTOR_SIZE,
                          _gpt_header(DISK_SECTORS - 1, 1, backup_array, entries_crc))
                partition_offset = ESP_START_LBA * SECTOR_SIZE
                _write_at(output, partition_offset, _fat_boot_sector())
                for copy in range(2):
                    _write_at(output, partition_offset
                              + (RESERVED_SECTORS + copy * FAT_SECTORS) * SECTOR_SIZE,
                              fat)
                root = partition_offset + (RESERVED_SECTORS + 2 * FAT_SECTORS) * SECTOR_SIZE
                _write_at(output, root, _directory_entry(b"EFI        ", 0x10, 2))
                efi_directory = b"".join((
                    _directory_entry(b".          ", 0x10, 2),
                    _directory_entry(b"..         ", 0x10, 0),
                    _directory_entry(b"BOOT       ", 0x10, 3),
                ))
                boot_directory = b"".join((
                    _directory_entry(b".          ", 0x10, 3),
                    _directory_entry(b"..         ", 0x10, 2),
                    _directory_entry(b"BOOTX64 EFI", 0x20, 4, payload_size),
                ))
                _write_at(output, _cluster_offset(2), efi_directory)
                _write_at(output, _cluster_offset(3), boot_directory)
                output.seek(_cluster_offset(4))
                remaining = payload_size
                while remaining:
                    block = payload.read(min(1024 * 1024, remaining))
                    if not block:
                        raise ValueError("EFI input changed size while being read")
                    output.write(block)
                    remaining -= len(block)
                if payload.read(1):
                    raise ValueError("EFI input changed size while being read")
                output.flush()
                os.fsync(output.fileno())
            os.replace(temporary, destination)
        finally:
            temporary.unlink(missing_ok=True)
    return destination


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--efi", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        create_disk(args.efi, args.output)
    except (OSError, ValueError) as error:
        parser.exit(1, f"disk image error: {error}\n")
    print(f"UEFI disk: {args.output} ({DISK_SECTORS * SECTOR_SIZE} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
