import importlib.util
import json
from pathlib import Path
import struct
import sys
import unittest
import zlib

MOD = Path(__file__).resolve().parents[1] / 'tools' / 'embedded_tea_catalog.py'
spec = importlib.util.spec_from_file_location('embedded_tea_catalog', MOD)
m = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = m
spec.loader.exec_module(m)


def local(name, raw):
    filename = name.encode('ascii')
    return struct.pack('<IHHHHHIIIHH', 0x04034B50, 20, 0, 0, 0, 0,
                       zlib.crc32(raw), len(raw), len(raw), len(filename), 0) + filename + raw


def fixture():
    elf = b'\x7fELF\x02\x01\x01' + b'\0' * 70
    import hashlib
    digest = hashlib.sha256(elf).hexdigest()
    raw = json.dumps({'format': 'styleos.tea.sdk-v0',
                      'interface': 'styleos.system-module-v1',
                      'project': {'id': 'styleos.documents', 'name': 'Документы'},
                      'payload': {'sha256': digest}}).encode()
    return b'MZ' + b'\0' * 120 + local('manifest.json', raw) + local('payload.elf', elf), raw, elf


class TeaAuditTests(unittest.TestCase):
    def test_valid_fixture(self):
        b, _, _ = fixture()
        records = m.scan(b)
        self.assertEqual(len(records), 1)
        self.assertTrue(records[0]['manifest_crc_valid'])
        self.assertTrue(records[0]['elf_crc_valid'])
        self.assertTrue(records[0]['elf_sha256_valid'])
        self.assertFalse(records[0]['extractable_as_verified_package'])

    def test_modification_detected(self):
        b, _, _ = fixture()
        b = bytearray(b)
        b[-1] ^= 1
        records = m.scan(bytes(b))
        self.assertEqual(len(records), 1)
        self.assertFalse(records[0]['elf_crc_valid'])
        self.assertFalse(records[0]['elf_sha256_valid'])

    def test_truncated_and_invalid_manifest(self):
        b, _, _ = fixture()
        self.assertEqual(m.scan(b[:140]), [])
        self.assertEqual(m.scan(b.replace(b'styleos.tea.sdk-v0', b'unknown.format.......')), [])

    def test_limit(self):
        with self.assertRaises(ValueError):
            m.scan(b'x' * (m.MAX_IMAGE + 1))

    def test_embedded_744_when_available(self):
        p = Path('/mnt/data/styleos-recovery-744/extracted/EFI/BOOT/BOOTX64.EFI')
        if not p.exists():
            self.skipTest('744 is not mounted; synthetic tests still run')
        records = m.scan(p.read_bytes())
        self.assertEqual(len(records), 6)
        self.assertIn('styleos.documents', {x['project_id'] for x in records})
        self.assertFalse(next(x for x in records if x['project_id'] == 'styleos.documents')['elf_sha256_valid'])


if __name__ == '__main__':
    unittest.main()
