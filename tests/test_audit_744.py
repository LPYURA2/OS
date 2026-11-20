import hashlib
import importlib.util
import pathlib
import unittest

script = pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'audit_744.py'
spec = importlib.util.spec_from_file_location('audit_744', script)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class Audits(unittest.TestCase):
    def test_rejects_empty_input(self):
        with self.assertRaises(ValueError):
            mod.iso_files(b'')

    def test_rejects_invalid_fat(self):
        with self.assertRaises(ValueError):
            mod.esp_files(b'notfat')

    def test_sha256(self):
        self.assertEqual(mod.sha(b'abc'), hashlib.sha256(b'abc').hexdigest())

    def test_original_iso_when_present(self):
        p = pathlib.Path('/mnt/data/styleos-recovery-744/StyleOS-Install-x64.iso')
        if not p.exists():
            self.skipTest('original 744 ISO not provided here')
        result = mod.audit(p)
        self.assertEqual(result['iso_bytes'], 17797120)
        self.assertTrue(result['iso_and_esp_efi_identical'])
        self.assertTrue(result['documents_tea_mentioned_in_boot_executable'])
        self.assertFalse(result['standalone_documents_tea_file_in_installer'])

if __name__ == '__main__':
    unittest.main()
