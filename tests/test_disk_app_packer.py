"""Host-only integration checks for the real disk-app fixture writer."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(os.name == 'posix', 'descriptor-relative POSIX fixture writer')
class DiskAppPackerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc = os.environ.get('CC', 'cc')
        if not shutil.which(cc):
            raise unittest.SkipTest(f'host C compiler {cc!r} is unavailable')
        temp = tempfile.TemporaryDirectory(prefix='styleos-disk-app-packer-')
        cls.addClassCleanup(temp.cleanup)
        cls.packer = Path(temp.name).resolve() / 'pack-disk-app'
        subprocess.run([cc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
            '-Iinclude', 'tools/pack_disk_app.c', 'kernel/storage.c',
            '-o', cls.packer], cwd=ROOT, check=True, timeout=60)

    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='styleos-disk-app-path-')
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name).resolve()
        self.app = self.root / 'app.bin'
        self.app.write_bytes(b'fixture')

    def pack(self, path):
        return subprocess.run([self.packer, self.app, path], capture_output=True,
            text=True, timeout=10)

    @unittest.skipUnless(sys.platform.startswith('linux') or sys.platform == 'android',
        'search-only directory descriptors use Linux O_PATH')
    def test_absolute_output_inside_searchable_but_unreadable_parent(self):
        ancestor = self.root / 'search-only'
        disks = ancestor / 'disks'
        disks.mkdir(parents=True)
        ancestor.chmod(0o100)
        try:
            try:
                fd = os.open(ancestor, os.O_RDONLY | os.O_DIRECTORY)
            except PermissionError:
                pass
            else:
                os.close(fd)
                self.skipTest('host permits reading a search-only directory')
            disk = disks / 'fixture.img'
            result = self.pack(disk)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(result.stdout)['file_length'], 7)
            self.assertEqual(disk.stat().st_size, 1048576)
            self.assertEqual(disk.stat().st_nlink, 1)
        finally:
            ancestor.chmod(0o700)

    def test_existing_output_and_symbolic_parents_or_leaf_are_refused(self):
        disks = self.root / 'disks'
        disks.mkdir()
        existing = disks / 'existing.img'
        existing.write_bytes(b'untouched')
        alias = self.root / 'alias'
        alias.symlink_to(disks, target_is_directory=True)
        symlink = disks / 'leaf.img'
        symlink.symlink_to(existing)
        for path in (existing, alias / 'redirected.img', symlink):
            with self.subTest(path=path):
                self.assertNotEqual(self.pack(path).returncode, 0)
        self.assertEqual(existing.read_bytes(), b'untouched')
        self.assertTrue(symlink.is_symlink())
        self.assertFalse((disks / 'redirected.img').exists())
