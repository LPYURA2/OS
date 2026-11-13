"""Independently parse the GPT/FAT image, without relying on builder offsets."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import uuid
import zlib


ROOT = Path(__file__).resolve().parents[1]


def u16(data, offset):
    return struct.unpack_from('<H', data, offset)[0]


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def u64(data, offset):
    return struct.unpack_from('<Q', data, offset)[0]


def digest(path):
    checksum = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            checksum.update(block)
    return checksum.hexdigest()


class DiskImageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location(
            'styleos_make_disk_test', ROOT / 'tools' / 'make_disk.py')
        cls.builder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.builder)
        cls.temporary = tempfile.TemporaryDirectory(prefix='styleos-disk-test-')
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.folder = Path(cls.temporary.name)
        cls.payload = bytes(range(256)) * 41 + b'STYLEOS\x00END'
        cls.efi = cls.folder / 'BOOTX64.EFI'
        cls.efi.write_bytes(cls.payload)
        cls.image = cls.folder / 'styleos.img'
        cls.builder.create_disk(cls.efi, cls.image)

    def read_at(self, offset, size):
        with self.image.open('rb') as disk:
            disk.seek(offset)
            result = disk.read(size)
        self.assertEqual(len(result), size)
        return result

    def header(self, lba):
        data = self.read_at(lba * 512, 512)
        self.assertEqual(data[:8], b'EFI PART')
        self.assertEqual(u32(data, 8), 0x00010000)
        length = u32(data, 12)
        self.assertGreaterEqual(length, 92)
        self.assertLessEqual(length, 512)
        covered = bytearray(data[:length])
        stored = u32(covered, 16)
        struct.pack_into('<I', covered, 16, 0)
        self.assertEqual(zlib.crc32(covered) & 0xffffffff, stored)
        self.assertEqual(u32(data, 20), 0)
        self.assertEqual(u64(data, 24), lba)
        entries = self.read_at(u64(data, 72) * 512,
                               u32(data, 80) * u32(data, 84))
        self.assertEqual(zlib.crc32(entries) & 0xffffffff, u32(data, 88))
        return data, entries

    def esp(self):
        header, entries = self.header(1)
        entry_size = u32(header, 84)
        self.assertGreaterEqual(entry_size, 128)
        partitions = [entries[n:n + entry_size]
                      for n in range(0, len(entries), entry_size)
                      if entries[n:n + 16] != bytes(16)]
        self.assertEqual(len(partitions), 1)
        partition = partitions[0]
        self.assertEqual(uuid.UUID(bytes_le=partition[:16]),
                         uuid.UUID('c12a7328-f81f-11d2-ba4b-00a0c93ec93b'))
        first, last = u64(partition, 32), u64(partition, 40)
        self.assertGreaterEqual(first, u64(header, 40))
        self.assertLessEqual(last, u64(header, 48))
        self.assertLessEqual(first, last)
        return first, last

    def test_gpt_headers_arrays_and_protective_mbr(self):
        sectors = self.image.stat().st_size // 512
        self.assertEqual(self.image.stat().st_size % 512, 0)
        self.assertEqual(self.image.stat().st_size, 68 * 1024 * 1024)
        mbr = self.read_at(0, 512)
        self.assertEqual(mbr[510:], b'\x55\xaa')
        self.assertEqual(mbr[446 + 4], 0xee)
        self.assertEqual(u32(mbr, 446 + 8), 1)
        self.assertEqual(u32(mbr, 446 + 12), sectors - 1)
        self.assertEqual(mbr[462:510], bytes(48))
        primary, table = self.header(1)
        backup, backup_table = self.header(sectors - 1)
        self.assertEqual(u64(primary, 32), sectors - 1)
        self.assertEqual(u64(backup, 32), 1)
        self.assertEqual(primary[40:72], backup[40:72])
        self.assertEqual(primary[80:92], backup[80:92])
        self.assertEqual(table, backup_table)
        first, last = self.esp()
        self.assertEqual(first, 2048)
        self.assertEqual(last - first + 1, 64 * 1024 * 1024 // 512)
        self.assertLess(u64(primary, 72), first)
        self.assertGreater(u64(backup, 72), last)

    def test_fat16_directories_and_multicluster_file(self):
        first, last = self.esp()
        bpb = self.read_at(first * 512, 512)
        self.assertEqual(bpb[510:], b'\x55\xaa')
        sector_bytes, cluster_sectors = u16(bpb, 11), bpb[13]
        self.assertEqual(sector_bytes, 512)
        self.assertIn(cluster_sectors, (1, 2, 4, 8, 16, 32, 64))
        reserved, copies, root_entries = u16(bpb, 14), bpb[16], u16(bpb, 17)
        self.assertEqual(copies, 2)
        fat_sectors = u16(bpb, 22)
        total_sectors = u16(bpb, 19) or u32(bpb, 32)
        self.assertEqual(total_sectors, last - first + 1)
        self.assertEqual(u32(bpb, 28), first)
        root_sectors = (root_entries * 32 + sector_bytes - 1) // sector_bytes
        data_start = reserved + copies * fat_sectors + root_sectors
        clusters = (total_sectors - data_start) // cluster_sectors
        self.assertGreaterEqual(clusters, 4085)
        self.assertLess(clusters, 65525)
        fat = self.read_at((first + reserved) * sector_bytes,
                           fat_sectors * sector_bytes)
        self.assertGreaterEqual(len(fat), (clusters + 2) * 2)
        duplicate = self.read_at((first + reserved + fat_sectors) * sector_bytes,
                                 len(fat))
        self.assertEqual(fat, duplicate)
        self.assertEqual(u16(fat, 0) & 0xff, bpb[21])
        self.assertGreaterEqual(u16(fat, 2), 0xfff8)
        cluster_bytes = sector_bytes * cluster_sectors

        def chain(start):
            seen, result = set(), bytearray()
            current = start
            while current < 0xfff8:
                self.assertGreaterEqual(current, 2)
                self.assertLess(current, clusters + 2)
                self.assertNotIn(current, seen)
                seen.add(current)
                offset = (first + data_start) * sector_bytes
                offset += (current - 2) * cluster_bytes
                result.extend(self.read_at(offset, cluster_bytes))
                current = u16(fat, current * 2)
                self.assertNotEqual(current, 0xfff7)
            return bytes(result), len(seen)

        def entry(directory, name):
            for offset in range(0, len(directory), 32):
                row = directory[offset:offset + 32]
                if row[0] == 0:
                    break
                if row[0] == 0xe5 or row[11] == 0x0f:
                    continue
                if row[:11] == name:
                    return row
            self.fail('Missing FAT entry ' + repr(name))

        root = self.read_at((first + reserved + copies * fat_sectors) * sector_bytes,
                            root_sectors * sector_bytes)
        efi = entry(root, b'EFI        ')
        self.assertEqual(efi[11] & 0x10, 0x10)
        efi_directory, _ = chain(u16(efi, 26))
        boot = entry(efi_directory, b'BOOT       ')
        self.assertEqual(boot[11] & 0x10, 0x10)
        boot_directory, _ = chain(u16(boot, 26))
        file_entry = entry(boot_directory, b'BOOTX64 EFI')
        self.assertEqual(file_entry[11] & 0x10, 0)
        self.assertEqual(u32(file_entry, 28), len(self.payload))
        recovered, count = chain(u16(file_entry, 26))
        self.assertGreater(count, 1)
        self.assertEqual(recovered[:len(self.payload)], self.payload)
        self.assertEqual(recovered[len(self.payload):],
                         bytes(len(recovered) - len(self.payload)))

    def test_reproducible_image_and_unchanged_input(self):
        second = self.folder / 'second.img'
        self.builder.create_disk(self.efi, second)
        self.assertEqual(digest(second), digest(self.image))
        self.assertEqual(self.efi.read_bytes(), self.payload)

    def test_empty_input_preserves_existing_output(self):
        empty = self.folder / 'empty.efi'
        empty.write_bytes(b'')
        output = self.folder / 'existing-empty-check.img'
        output.write_bytes(b'keep this image')
        with self.assertRaises(ValueError):
            self.builder.create_disk(empty, output)
        self.assertEqual(output.read_bytes(), b'keep this image')

    def test_oversized_input_preserves_existing_output(self):
        huge = self.folder / 'huge.efi'
        with huge.open('wb') as source:
            source.truncate(64 * 1024 * 1024)
        output = self.folder / 'existing-large-check.img'
        output.write_bytes(b'keep this image')
        with self.assertRaises(ValueError):
            self.builder.create_disk(huge, output)
        self.assertEqual(output.read_bytes(), b'keep this image')

    def test_source_and_output_cannot_alias(self):
        with self.assertRaises(ValueError):
            self.builder.create_disk(self.efi, self.efi)
        self.assertEqual(self.efi.read_bytes(), self.payload)


if __name__ == '__main__':
    unittest.main()
