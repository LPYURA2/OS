"""Host-only preinstall contract tests. Never claim running StyleOS guest."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import io
import zipfile
import hashlib
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import system_apps as s

PKG = ROOT / 'build/756/Documents.tea'


def rewrite_package(source, **changes):
    with zipfile.ZipFile(source) as z:
        meta = json.loads(z.read('manifest.json'))
        elf = z.read('payload.elf')
    for path, value in changes.items():
        obj = meta
        keys=path.split('.')
        for k in keys[:-1]:obj=obj[k]
        obj[keys[-1]]=value
    raw = (json.dumps(meta,ensure_ascii=False,sort_keys=True,separators=(',',':'))+'\n').encode()
    b=io.BytesIO()
    with zipfile.ZipFile(b,'w') as z:
        for name,payload in [('manifest.json',raw),('payload.elf',elf)]:
            info=zipfile.ZipInfo(name, (1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_STORED
            info.external_attr=0o100644<<16
            z.writestr(info,payload)
    return b.getvalue()


class PreinstallTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)/'volume'
        self.root.mkdir()
        self.package=Path(self.tmp.name)/'Documents.tea'
        self.package.write_bytes(PKG.read_bytes())

    def _stage(self, profile='development'):
        return s.stage(self.root,self.package,profile)

    def _replace(self,raw):
        self.package.write_bytes(raw)

    def test_initial_preinstall_registers_separate_process(self):
        gen=self._stage();found,cat=s.verify(self.root)
        self.assertEqual(gen,found)
        app=cat['apps'][0]
        self.assertEqual(app['id'],'styleos.documents')
        self.assertEqual(app['origin'],'system-preinstall')
        self.assertTrue(app['system_app'])
        self.assertEqual(app['process'],'independent')
        self.assertEqual(app['launch_via'],'external-tea-loader')
        self.assertFalse(app['compatible_with_744'])
        self.assertFalse(app['publisher_signature_verified'])
        directory=self.root/'System/Applications'
        self.assertTrue((directory/'releases'/gen/'packages/Documents.tea').is_file())
        self.assertEqual((directory/'current').read_text(),gen+'\n')

    def test_stage_is_idempotent(self):
        gen=self._stage()
        self.assertEqual(self._stage(),gen)
        self.assertEqual(len(list((self.root/'System/Applications/releases').iterdir())),1)

    def test_reinstall_preserves_user_data(self):
        user=self.root/'Users/sample/Documents/important.txt'
        user.parent.mkdir(parents=True)
        user.write_text('не менять')
        self._stage();self._stage()
        self.assertEqual(user.read_text(),'не менять')

    def test_production_refused_without_mutation(self):
        with self.assertRaisesRegex(s.ProvisionError,'production is BLOCKED'):
            self._stage(profile='production')
        self.assertEqual(list(self.root.iterdir()),[])

    def test_bad_zip_rejected_without_mutation(self):
        self._replace(b'not-zip')
        with self.assertRaises(s.ProvisionError):self._stage()
        self.assertEqual(list(self.root.iterdir()),[])

    def test_wrong_app_id_rejected(self):
        self._replace(rewrite_package(self.package,**{'project.id':'styleos.presentation'}))
        with self.assertRaisesRegex(s.ProvisionError,'identity'):
            self._stage()

    def test_wrong_interface_rejected(self):
        self._replace(rewrite_package(self.package,**{'interface':'styleos.bad-abi-v1'}))
        with self.assertRaisesRegex(s.ProvisionError,'ABI'):
            self._stage()

    def test_not_declared_installable_in_744(self):
        self._replace(rewrite_package(self.package,**{'installable_in_744':True}))
        with self.assertRaisesRegex(s.ProvisionError,'ABI'):
            self._stage()

    def test_corrupted_payload_does_not_replace_current(self):
        good=self._stage()
        raw=bytearray(self.package.read_bytes())
        raw[len(raw)//2] ^= 1
        self._replace(raw)
        with self.assertRaises(s.ProvisionError):self._stage()
        self.assertEqual(s.verify(self.root)[0],good)

    def test_update_and_rollback_keep_prior_generation(self):
        old=self._stage()
        self._replace(rewrite_package(self.package,**{'project.version':'0.7.7'}))
        new=self._stage()
        self.assertNotEqual(old,new)
        self.assertEqual(s.verify(self.root)[0],new)
        self.assertEqual(s.verify(self.root)[1]['apps'][0]['version'],'0.7.7')
        s.rollback(self.root,old)
        self.assertEqual(s.verify(self.root)[0],old)
        self.assertEqual(s.verify(self.root)[1]['apps'][0]['version'],'0.7.6')
        self.assertEqual(len(list((self.root/'System/Applications/releases').iterdir())),2)

    def test_release_tampering_denied(self):
        gen=self._stage()
        app=self.root/'System/Applications/releases'/gen/'packages/Documents.tea'
        raw=bytearray(app.read_bytes());raw[-28]^=1;app.write_bytes(raw)
        with self.assertRaises(s.ProvisionError):s.verify(self.root)
        with self.assertRaises(s.ProvisionError):self._stage()

    def test_pointer_tampering_denied(self):
        self._stage()
        current=self.root/'System/Applications/current'
        current.write_text('../evil\n')
        with self.assertRaises(s.ProvisionError):s.verify(self.root)
        with self.assertRaises(s.ProvisionError):self._stage()

    def test_symlink_in_system_path_denied(self):
        other=Path(self.tmp.name)/'outside';other.mkdir()
        (self.root/'System').symlink_to(other,target_is_directory=True)
        with self.assertRaisesRegex(s.ProvisionError,'symlink'):
            self._stage()
        self.assertEqual(list(other.iterdir()),[])

    def test_symlink_package_denied(self):
        link=Path(self.tmp.name)/'link.tea';link.symlink_to(self.package)
        with self.assertRaisesRegex(s.ProvisionError,'symlink'):
            s.stage(self.root,link,'development')

    def test_symlink_pointer_denied(self):
        gen=self._stage()
        current=self.root/'System/Applications/current'
        current.unlink()
        current.symlink_to(Path(self.tmp.name)/'outside')
        with self.assertRaises(s.ProvisionError):s.verify(self.root)
        self.assertTrue((self.root/'System/Applications/releases'/gen).is_dir())

    def test_unknown_rollback_refused(self):
        self._stage()
        with self.assertRaises(s.ProvisionError):s.rollback(self.root,'0'*64)
        with self.assertRaises(s.ProvisionError):s.rollback(self.root,'../evil')

    def test_duplicate_json_key_rejected(self):
        with self.assertRaisesRegex(s.ProvisionError,'duplicate'):
            s._load_json(b'{"id":1,"id":2}')

    def test_package_bigger_than_limit_rejected(self):
        self._replace(b'X'*(s.MAX_TEA+1))
        with self.assertRaises(s.ProvisionError):self._stage()

    def test_future_gui_is_separate_from_installation(self):
        gen=self._stage()
        app=s.verify(self.root)[1]['apps'][0]
        self.assertNotIn('desktop_shell',json.dumps(app))
        self.assertEqual(app['file_access'],'granted-tokens-only')
        self.assertEqual(app['window_access'],'client-area-only')

if __name__=='__main__': unittest.main()
