import pathlib
import sys
import tempfile
import unittest
import zipfile

ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import build_preinstall_bundle as b

class BundleTests(unittest.TestCase):
    def test_repro_and_validation(self):
        with tempfile.TemporaryDirectory() as d:
            d=pathlib.Path(d);app=ROOT/'build/756/Documents.tea'
            a=d/'a.zip';c=d/'c.zip'
            b.make(app,a);b.make(app,c)
            self.assertEqual(a.read_bytes(),c.read_bytes())
            self.assertEqual(b.verify(a),b.verify(c))

    def test_binary_tamper_refused(self):
        with tempfile.TemporaryDirectory() as d:
            d=pathlib.Path(d);original=d/'orig.zip';bad=d/'bad.zip'
            b.make(ROOT/'build/756/Documents.tea',original)
            data=bytearray(original.read_bytes());data[len(data)//2]^=0x80
            bad.write_bytes(data)
            with self.assertRaises((zipfile.BadZipFile,ValueError)):
                b.verify(bad)

    def test_installer_consumes_bundle_without_shell_dependency(self):
        with tempfile.TemporaryDirectory() as d:
            d=pathlib.Path(d);volume=d/'system';volume.mkdir()
            package=d/'bundle.zip';b.make(ROOT/'build/756/Documents.tea',package)
            generation=b.stage_bundle(package,volume,'development')
            self.assertEqual(generation,b.sysapps.verify(volume)[0])
            self.assertEqual(b.stage_bundle(package,volume,'development'),generation)
            with self.assertRaises(b.sysapps.ProvisionError):
                b.stage_bundle(package,volume,'production')

    def test_not_bootable_and_requires_guest_loader(self):
        with tempfile.TemporaryDirectory() as d:
            dest=pathlib.Path(d)/'bundle.zip';b.make(ROOT/'build/756/Documents.tea',dest)
            with zipfile.ZipFile(dest) as z:
                meta=b.sysapps._load_json(z.read('BUNDLE-MANIFEST.json'))
                self.assertIs(meta['bootable'],False)
                self.assertIs(meta['requires_native_guest_loader'],True)

if __name__=='__main__':unittest.main()
