"""Tests of immutable 730/744 baselines and a single native .tea format."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from check_baseline_730 import BaselineError, check_native_app_catalog, check_zip, main

class ContractTests(unittest.TestCase):
    def setUp(self):
        self.catalog = {'format':'styleos.system-apps.plan.v1','apps':[{
            'id':'styleos.documents','package_name':'Documents.tea','process':'independent',
            'preinstall':True,'location':'system-volume'}]}
    def run_catalog(self, catalog):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'c.json'
            p.write_text(json.dumps(catalog),encoding='utf-8')
            return check_native_app_catalog(p)
    def test_accept_native(self):
        self.assertTrue(self.run_catalog(self.catalog)['documents_preinstall'])
    def test_reject_sapp(self):
        c=copy.deepcopy(self.catalog);c['apps'][0]['package_name']='Documents.sapp'
        with self.assertRaises(BaselineError):self.run_catalog(c)
    def test_reject_exe(self):
        c=copy.deepcopy(self.catalog);c['apps'][0]['package_name']='Documents.exe'
        with self.assertRaises(BaselineError):self.run_catalog(c)
    def test_reject_dll(self):
        c=copy.deepcopy(self.catalog);c['apps'][0]['package_name']='Documents.dll'
        with self.assertRaises(BaselineError):self.run_catalog(c)
    def test_reject_arbitrary_path(self):
        c=copy.deepcopy(self.catalog);c['apps'][0]['package_name']='../Documents.tea'
        with self.assertRaises(BaselineError):self.run_catalog(c)
    def test_reject_shell_embedded_app(self):
        c=copy.deepcopy(self.catalog);c['apps'][0]['process']='inside-shell'
        with self.assertRaises(BaselineError):self.run_catalog(c)
    def test_reject_no_preinstall(self):
        c=copy.deepcopy(self.catalog);c['apps'][0]['preinstall']=False
        with self.assertRaises(BaselineError):self.run_catalog(c)
    def test_reject_user_volume(self):
        c=copy.deepcopy(self.catalog);c['apps'][0]['location']='user-volume'
        with self.assertRaises(BaselineError):self.run_catalog(c)
    def test_reject_duplicate_id(self):
        c=copy.deepcopy(self.catalog);c['apps'].append(copy.deepcopy(c['apps'][0]))
        with self.assertRaises(BaselineError):self.run_catalog(c)
    def test_reject_absent_documents(self):
        c=copy.deepcopy(self.catalog);c['apps'][0]['id']='styleos.notes'
        with self.assertRaises(BaselineError):self.run_catalog(c)
    def test_reject_empty_apps(self):
        with self.assertRaises(BaselineError):self.run_catalog({'format':'styleos.system-apps.plan.v1','apps':[]})
    def test_reject_missing_args(self):
        with self.assertRaises(SystemExit):main([])
    def test_reject_modified_binary(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'tampered.zip';p.write_bytes(b'not the approved archive')
            with self.assertRaises(BaselineError):check_zip(p,'730')
    def test_reject_unsupported_baseline(self):
        with self.assertRaises(BaselineError):check_zip(Path('not-a-real-file'),'729')

if __name__=='__main__': unittest.main()
