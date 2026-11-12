"""Regression tests for T1A native guest application loading (stdlib unittest)."""
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from matryoshka import (APP_BASE, APP_ORDER, APP_FIRST_SECTOR, BLOCK_SIZE,
                        CATALOG_SECTOR, SLOT_SIZE, U32, VirtualMachine, assemble)


class GuestAppLoadingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='matryoshka-t1a-')
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)

    def vm(self, image=None):
        return VirtualMachine(ROOT, self.folder / 'user_disk.img', system_image=image)

    def test_guest_load_without_python_app_preload(self):
        v = self.vm()
        v.boot(['hello'])
        self.assertEqual(v.app_images, [])
        self.assertEqual(bytes(v.mem[APP_BASE:APP_BASE+SLOT_SIZE]), bytes(SLOT_SIZE))
        v.run(100000)
        self.assertEqual([p.name for p in v.processes], ['hello'])
        self.assertTrue(v.halted)
        self.assertIn((CATALOG_SECTOR, 0x7000), v.boot_reads)
        self.assertIn((APP_FIRST_SECTOR, APP_BASE), v.boot_reads)
        self.assertIn('Hello from USER MODE', ''.join(v.console))

    def test_guest_exec_while_shell_alive(self):
        v = self.vm()
        v.boot(['shell'])
        v.feed_keys('run 0\nrun 9\nexit\n')
        v.run(150000)
        self.assertTrue(v.halted)
        self.assertEqual([p.name for p in v.processes], ['shell', 'hello'])
        console = ''.join(v.console)
        self.assertIn('Application launched from system disk.', console)
        self.assertIn('Unknown app ID', console)
        self.assertIn('Hello from USER MODE', console)

    def test_reject_busy_slot_before_dma(self):
        # Replace the counter binary with an infinite loop, then launch twice.
        # The second launch MUST fail without modifying the first program's RAM.
        original = bytearray((ROOT/'images'/'system.img').read_bytes())
        catalog_entry = CATALOG_SECTOR*BLOCK_SIZE + 16 + 40*1
        sector = struct.unpack_from('<I', original, catalog_entry+20)[0]
        num_sectors = struct.unpack_from('<I', original, catalog_entry+24)[0]
        self.assertEqual(num_sectors, 1)
        spinner, _ = assemble('again:\n    JMP again\n', APP_BASE+SLOT_SIZE)
        block = spinner.ljust(BLOCK_SIZE, b'\x00')
        original[sector*BLOCK_SIZE:(sector+1)*BLOCK_SIZE] = block
        word_sum = sum(word[0] for word in struct.iter_unpack('<I', block)) & U32
        struct.pack_into('<I', original, catalog_entry+32, word_sum)
        path = self.folder/'spinner.img'
        path.write_bytes(original)
        v = self.vm(image=path)
        v.boot(['shell'])
        v.feed_keys('run 1\nrun 1\n')
        v.run_until_keyboard_wait(120000)
        console = ''.join(v.console)
        self.assertIn('Application launched', console)
        self.assertIn('Unable to launch: app busy', console)
        self.assertEqual(len([proc for proc in v.processes if proc.name=='counter']), 1)
        self.assertEqual(v.mem[APP_BASE+SLOT_SIZE:APP_BASE+SLOT_SIZE+len(spinner)], spinner)
        self.assertTrue(v.processes[1].alive)
        self.assertFalse(v.halted)

    def test_corrupt_application_does_not_spawn(self):
        corrupt = bytearray((ROOT/'images'/'system.img').read_bytes())
        corrupt[APP_FIRST_SECTOR*BLOCK_SIZE+12] ^= 0x20
        path = self.folder/'corrupt.img'
        path.write_bytes(corrupt)
        v = self.vm(image=path)
        v.boot(['hello'])
        v.run(80000)
        self.assertTrue(v.halted)
        self.assertFalse(v.processes)
        self.assertIn('APP LOAD ERROR', ''.join(v.console))

    def test_user_disk_survives_reboot(self):
        v1 = self.vm()
        v1.boot(['shell'])
        v1.feed_keys('save MATRYOSHKA T1A\nexit\n')
        v1.run(100000)
        v2 = self.vm()
        v2.boot(['shell'])
        v2.feed_keys('load\nexit\n')
        v2.run(100000)
        self.assertIn('MATRYOSHKA T1A', ''.join(v2.console))


if __name__ == '__main__':
    unittest.main()
