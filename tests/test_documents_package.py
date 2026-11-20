"""Preview package structural and hostile corruption tests, not 744 installation."""
import importlib.util
import pathlib
import tempfile
import unittest
import zipfile

ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('tea',ROOT/'tools'/'build_documents_tea.py')
tea=importlib.util.module_from_spec(spec);spec.loader.exec_module(tea)

class PackageTests(unittest.TestCase):
    def setUp(self):
        self.package=ROOT/'build/755/Documents.tea'
        if not self.package.is_file():self.skipTest('build via tools/build_documents_tea.py first')
    def test_valid(self):
        meta=tea.verify(self.package)
        self.assertEqual(meta['project']['id'],'styleos.documents')
        self.assertFalse(meta['installable_in_744'])
        self.assertLess(self.package.stat().st_size,65536)
    def test_modified_bytes_rejected(self):
        raw=bytearray(self.package.read_bytes())
        with zipfile.ZipFile(self.package) as z:
            f=z.infolist()[1]
            offset=f.header_offset+30+len(f.filename.encode())+len(f.extra)
        raw[offset+128]^=0x20
        with tempfile.TemporaryDirectory() as temp:
            path=pathlib.Path(temp)/'changed.tea';path.write_bytes(raw)
            with self.assertRaises((zipfile.BadZipFile,ValueError)):tea.verify(path)
    def test_truncation_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path=pathlib.Path(temp)/'short.tea';path.write_bytes(self.package.read_bytes()[:100])
            with self.assertRaises((zipfile.BadZipFile,ValueError)):tea.verify(path)
    def test_rebuild_identical(self):
        with tempfile.TemporaryDirectory() as temp:
            path=pathlib.Path(temp)/'repeat.tea'
            tea.build(path)
            self.assertEqual(path.read_bytes(),self.package.read_bytes())

if __name__=='__main__':unittest.main()
