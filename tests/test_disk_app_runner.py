"""Strict external-byte/EL0 evidence and fresh-file/process runner tests."""
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from tools import test_disk_app as runner
from tools.disk_app_site import describe_disk_app, validate_app_bytes, build_app_metadata
from test_disk_app_site import fixture, app_file, app_elf, tables, FLAGS


def valid_site():
    with tempfile.TemporaryDirectory() as temp:
        folder = Path(temp)
        elf, raw, _ = fixture()
        (folder/'kernel.elf').write_bytes(elf)
        (folder/'styleos.bin').write_bytes(raw)
        return describe_disk_app('aarch64', folder/'kernel.elf',
            raw_path=folder/'styleos.bin', compiler_flags=FLAGS)


def artifact(marker=0x1818a001, exit_code=41, invalid=False):
    data = app_file(marker, exit_code)
    value = validate_app_bytes(data)
    if invalid:
        data[0] ^= 1
        value.update(file_hex=data.hex(), file_sha256=hashlib.sha256(data).hexdigest(),
                     reject_reason='MAGIC')
    return value


def valid_trace(phase='run', site=None, app=None):
    site = site or valid_site()
    app = app or artifact(invalid=phase == 'reject')
    obj = site['objects']
    image = site['image_start'], site['image_end']
    stack = obj['stack']['start'], obj['stack']['end']
    source = (0x44000000, 0x44002000)
    handoff = (stack[1]-128, stack[1]-32)
    reserved = [(0x9000000, 0x9001000), (image[0], image[1]), source]
    candidates = [(0x40000000, image[0]), (image[1], source[0]), (source[1], 0x48000000)]
    lines = [b'STYLEOS AARCH64: initial EL1', b'STYLEOS AARCH64: current EL1',
        b'STYLEOS AARCH64: DTB header OK', b'STYLEOS kernel_main entered',
        b'STYLEOS boot_info validated v1', b'STYLEOS HANDOFF PASS aarch64',
        b'STYLEOS MEMMAP DIAGNOSTIC ONLY']

    def exact(value):
        lines.append(value.encode() if isinstance(value, str) else value)

    def fields(label, **values):
        exact('STYLEOS DISKAPP ' + label + ''.join(' %s=0x%016x' % item for item in values.items()))

    for name, rows in [('IMAGE', [image]), ('STACK', [stack]), ('HANDOFF', [handoff]),
        ('SOURCE', [source]), ('RAM', [(0x40000000, 0x48000000)]),
        ('RESERVED', reserved), ('CANDIDATE', candidates)]:
        for a, b in rows:
            exact('STYLEOS MEMMAP %s 0x%016x 0x%016x' % (name, a, b))
    exact('STYLEOS MEMMAP PASS')
    exact('STYLEOS DISKAPP PHASE name=' + phase)
    order = [('tables','tables'), ('control','control'), ('entry_stack','entry_stack'),
        ('code','code'), ('user_stack','user_stack'), ('workspace','workspace'),
        ('queue','queue'), ('request','request'), ('vblk_control','vblk_control'),
        ('vectors','disk_app_vectors'), ('old_vectors','vectors'), ('stack','stack'),
        ('handoff',None), ('source',None), ('diagnostic','diagnostic'),
        ('pool','pool'), ('pool_metadata','pool_metadata')]
    for label, key in order:
        bounds = handoff if label == 'handoff' else source if label == 'source' else (obj[key]['start'],obj[key]['end'])
        exact('STYLEOS DISKAPP OBJECT name=%s 0x%016x 0x%016x' % (label, *bounds))
    root_safe = obj['tables']['start']
    root_app = root_safe + 32*4096
    machine = dict(root=0, mode=0x30d00800, control0=0, control1=0,
        control2=4, control3=0x3c0, vectors=obj['vectors']['start'],
        upper_root=0, active=(1<<64)-1, dispatcher_sp=0, context_live=0, sp=stack[1]-512)
    fields('MACHINE phase=IO', **machine)
    fields('FDT', source=source[0], bytes=source[1]-source[0], candidates=32)
    fields('DEVICE', base=0xa003e00, bytes=0x200, version=2, id=2,
        features=0x100000200, negotiated=0x100000200, sectors=2048, queue=8)
    exact('STYLEOS DISKAPP READ generation=0x%016x name=%s length=0x%016x data=%s' %
        (1,b'app.bin'.hex(),app['file_bytes'],app['file_hex']))
    fields('DRAIN', reads=14, writes=0, flushes=0, requests=14, avail=14, used=14)
    if phase == 'reject':
        exact('STYLEOS DISKAPP REJECT reason=MAGIC')
        exact('STYLEOS KERNEL HALT')
        return b'\n'.join(lines)+b'\n'
    fields('FORMAT', file=app['file_bytes'],code=app['code_bytes'],entry=0,fault=8,exit=24,
        marker=app['marker'],exit_code=app['exit_code'],checksum=app['checksum'])
    exact('STYLEOS DISKAPP COPY source=0x%016x destination=0x%016x bytes=0x%016x crc=0x%016x va=0x%016x data=%s' %
        (obj['control']['start']+1024+32,obj['code']['start'],28,
         zlib.crc32(bytes.fromhex(app['code_hex']).ljust(4096,b'\0')),
         0x10000000,bytes.fromhex(app['code_hex']).ljust(4096,b'\0').hex()))
    machine.update(root=root_safe,mode=0x30d00801,control0=0x580903010,control1=0x44,
        vectors=obj['disk_app_vectors']['start'])
    fields('MACHINE phase=SAFE', **machine)
    for name, user in [('SAFE',False),('APP',True)]:
        base, addresses, pages, _insert = tables(site,user)
        used = sum(any(page) for page in pages.values())
        fields('ROOT id='+name,pa=base,pages=used)
        count=0
        for pa in addresses:
            for index,value in enumerate(pages[pa]):
                if value:
                    fields('TABLE id='+name,table=pa,index=index,entry=value)
                    count+=1
        fields('TABLES END id='+name,entries=count)
    for arg in (0,1):
        fields('RUN',arg=arg,entry=0x10000000,sp=0x11001000,root=root_app)
        fields('EVENT arg=0x%016x kind=%s' % (arg,'fault' if arg==0 else 'exit'),
            pc=0x10000008 if arg==0 else 0x1000001c,sp=0x11001000,status=0x3c0,vector=8,
            syndrome=0x92000006 if arg==0 else 0x56000002,
            fault_va=0x12010000 if arg==0 else 0,root=root_app,
            kernel_sp=obj['entry_stack']['end']-512,
            x0=0 if arg==0 else app['exit_code'],x1=0x12010000 if arg==0 else 0,
            x19=0 if arg==0 else app['marker'])
        fields('SAFE',arg=arg,root=root_safe,sp=stack[1]-768)
        fields('MACHINE phase=SAFE',**machine)
        exact('STYLEOS DISKAPP RELEASE arg=0x%016x state=FREE' % arg)
    exact('STYLEOS DISKAPP PASS')
    exact('STYLEOS KERNEL HALT')
    return b'\n'.join(lines)+b'\n'


class DiskAppTraceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.site=valid_site()
        cls.app=artifact()
        cls.output=valid_trace(site=cls.site,app=cls.app)

    def test_complete_byte_bound_read_copy_tables_fault_and_later_exit(self):
        parsed=runner.parse_disk_app_trace(self.output,'run',self.site,self.app)
        self.assertIsNotNone(parsed)
        self.assertEqual(parsed['file_sha256'],self.app['file_sha256'])
        self.assertEqual(parsed['io']['writes'],0)
        self.assertEqual(parsed['io']['flushes'],0)
        self.assertEqual([row['event']['pc'] for row in parsed['launches']], [0x10000008,0x1000001c])
        self.assertFalse(parsed['table_closures']['APP']['fault_va_mapped'])
        self.assertEqual(parsed['table_closures']['APP']['user_leaf_count'],2)
        app=artifact(0x1818b002,73)
        self.assertIsNotNone(runner.parse_disk_app_trace(valid_trace(site=self.site,app=app),'run',self.site,app))
        self.assertIsNone(runner.parse_disk_app_trace(self.output,'run',self.site,app))

    def test_valid_store_bad_container_is_rejected_without_el0(self):
        app=artifact(invalid=True)
        output=valid_trace('reject',self.site,app)
        value=runner.parse_disk_app_trace(output,'reject',self.site,app)
        self.assertTrue(value['rejected'])
        self.assertEqual(value['launches'],[])
        self.assertIsNone(runner.parse_disk_app_trace(output,'run',self.site,app))
        self.assertIsNone(runner.parse_disk_app_trace(output.replace(b'STYLEOS KERNEL HALT',
            b'STYLEOS DISKAPP RUN arg=0x0000000000000000\nSTYLEOS KERNEL HALT'),'reject',self.site,app))
        self.assertIsNone(runner.parse_disk_app_trace(valid_trace(site=self.site),'reject',self.site,self.app))

    def test_missing_read_wrong_file_data_copy_crc_and_pass_only_fail(self):
        bad=[b'STYLEOS DISKAPP PASS\nSTYLEOS KERNEL HALT\n',
             b'\n'.join(line for line in self.output.split(b'\n') if not line.startswith(b'STYLEOS DISKAPP READ')),
             self.output.replace(self.app['file_hex'].encode(),b'00'+self.app['file_hex'][2:].encode(),1),
             self.output.replace(b'name=6170702e62696e',b'name=6f746865722e62696e'),
             self.output.replace(b' crc=0x',b' crc=1x',1),
             self.output.replace((self.app['code_hex']+'0000').encode(),
                 (self.app['code_hex']+'0100').encode(),1),
             self.output.replace(b'STYLEOS KERNEL HALT\n',b'STYLEOS EMERGENCY HALT\n')]
        for output in bad:
            with self.subTest(last=output[-120:]):
                self.assertIsNone(runner.parse_disk_app_trace(output,'run',self.site,self.app))

    def test_fault_origin_pc_va_status_and_safe_context_are_exact(self):
        changes=[(b'syndrome=0x0000000092000006',b'syndrome=0x0000000096000006'),
            (b'pc=0x0000000010000008',b'pc=0x0000000040080008'),
            (b'fault_va=0x0000000012010000',b'fault_va=0x0000000012011000'),
            (b'status=0x00000000000003c0',b'status=0x00000000000003c5'),
            (b'vector=0x0000000000000008',b'vector=0x0000000000000004'),
            (b'kind=fault',b'kind=kernel'),
            (b'active=0xffffffffffffffff',b'active=0x0000000000000000'),
            (b'context_live=0x0000000000000000',b'context_live=0x0000000000000001'),
            (b'flushes=0x0000000000000000',b'flushes=0x0000000000000001'),
            (b'used=0x000000000000000e',b'used=0x000000000000000d')]
        for old,new in changes:
            with self.subTest(field=old):
                self.assertIn(old,self.output)
                self.assertIsNone(runner.parse_disk_app_trace(self.output.replace(old,new,1),'run',self.site,self.app))

    def test_after_fault_real_second_entry_and_exit_are_mandatory(self):
        lines=self.output.splitlines(keepends=True)
        second=next(i for i,line in enumerate(lines) if line.startswith(b'STYLEOS DISKAPP RUN arg=0x0000000000000001'))
        pass_index=next(i for i,line in enumerate(lines) if line==b'STYLEOS DISKAPP PASS\n')
        outputs=[b''.join(lines[:second]+lines[pass_index:]),
            self.output.replace(b'RUN arg=0x0000000000000001',b'RUN arg=0x0000000000000000'),
            self.output.replace(b'pc=0x000000001000001c',b'pc=0x0000000010000018'),
            self.output.replace(b'x19=0x000000001818a001',b'x19=0x000000001818b002'),
            self.output+b'STYLEOS AARCH64: initial EL1\n',
            self.output.replace(b'STYLEOS DISKAPP PASS\n',b'STYLEOS DISKAPP REJECT reason=MAGIC\n')]
        for output in outputs:
            self.assertIsNone(runner.parse_disk_app_trace(output,'run',self.site,self.app))

    def test_sparse_dump_requires_complete_exact_descriptor_closure(self):
        lines=self.output.splitlines(keepends=True)
        table=next(i for i,line in enumerate(lines) if b'TABLE id=APP' in line)
        bad=[b''.join(lines[:table]+lines[table+1:]),
             b''.join(lines[:table]+[lines[table]]+lines[table:]),
             self.output.replace(b'TABLES END id=APP entries=0x',b'TABLES END id=APP entries=1x')]
        for output in bad:
            self.assertIsNone(runner.parse_disk_app_trace(output,'run',self.site,self.app))
        changed=deepcopy(self.site)
        changed['objects']['code']['start']+=4096
        self.assertIsNone(runner.parse_disk_app_trace(self.output,'run',changed,self.app))


class DiskAppFileTests(unittest.TestCase):
    def test_unique_directories_exclusive_evidence_and_no_os_link_dependency(self):
        # Android Python can omit os.link entirely. Exercise that same state
        # on hosts that do expose it and restore their attribute after the test.
        if hasattr(os, 'link'):
            self.addCleanup(setattr, os, 'link', os.link)
            del os.link
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)
            old=runner.new_run_directory(root/'output')
            runner.write_new(old/'test-results/keep.log',b'evidence')
            run=runner.new_run_directory(root/'output')
            self.assertNotEqual(old,run)
            self.assertEqual((old/'test-results/keep.log').read_bytes(),b'evidence')
            with self.assertRaises(FileExistsError): runner.write_new(old/'test-results/keep.log',b'changed')
            app=run/'app.bin';app.write_bytes(b'fixture')
            def pack(command,**kwargs):
                self.assertEqual(command,['/fixture-packer',str(app),str(run/'disks/variant-a.img')])
                fd=os.open(command[-1],os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
                with os.fdopen(fd,'wb') as stream: stream.truncate(runner.DISK_BYTES)
                return subprocess.CompletedProcess(command,0,'created')
            with patch.object(runner.subprocess,'run',side_effect=pack):
                disk,state=runner.fixture_disk(run,'variant-a',app,'/fixture-packer')
                with self.assertRaises(FileExistsError): runner.fixture_disk(run,'variant-a',app,'/fixture-packer')
            self.assertEqual(runner.disk_state(disk,state),state)
            with self.assertRaises(ValueError): runner.fixture_disk(run,'outside',app,'/fixture-packer')

    def test_identity_size_symlink_and_external_output_refusals(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)
            run=runner.new_run_directory(root/'output')
            disk=run/'disks/a.img';disk.write_bytes(bytes(runner.DISK_BYTES))
            state=runner.disk_state(disk)
            with self.assertRaises(ValueError): runner.disk_state(disk,dict(state,inode=state['inode']+1))
            alias=root/'alias';alias.symlink_to(disk)
            with self.assertRaises(OSError): runner.disk_state(alias)
            target=root/'output-link';target.symlink_to(root/'output',target_is_directory=True)
            with self.assertRaises(ValueError): runner.new_run_directory(target)
            with disk.open('r+b') as stream: stream.truncate(runner.DISK_BYTES-1)
            with self.assertRaises(ValueError): runner.disk_state(disk)

    def test_json_qemu_paths_keep_modern_rw_driver_contract_without_host_option_injection(self):
        with tempfile.TemporaryDirectory(prefix='styleos,path,') as temporary:
            folder=Path(temporary)
            boot=folder/'kernel,read-only=on.bin';boot.write_bytes(b'boot')
            disk=folder/'disk,read-only=on.img';disk.touch()
            command=runner.qemu_command('/qemu',boot,disk,'run')
            nodes=[json.loads(command[i+1]) for i,arg in enumerate(command) if arg=='-blockdev']
            self.assertEqual(nodes[0]['filename'],str(disk.resolve()))
            self.assertFalse(nodes[0]['cache']['no-flush'])
            self.assertFalse(nodes[1]['read-only'])
            self.assertIn('virtio-mmio.force-legacy=false',command)
            self.assertNotIn('-snapshot',command)
            self.assertEqual(command[command.index('-append')+1],'styleos.diskapp=run')
            with self.assertRaises(ValueError): runner.qemu_command('/qemu',boot,disk,'init')


def binding_fixture(folder):
    target=folder/'aarch64';target.mkdir()
    elf,raw,_=fixture()
    (target/'kernel.elf').write_bytes(elf)
    (target/'styleos.bin').write_bytes(raw)
    site=describe_disk_app('aarch64',target/'kernel.elf',raw_path=target/'styleos.bin',compiler_flags=FLAGS)
    sources=runner.disk_app_sources()
    manifest=dict(test_disk_app=True,compiler_flags={'aarch64':FLAGS.copy()},
        disk_app_sources={'aarch64':sources},artifacts=[],**{key:False for key in runner.MODES})
    for name,key in [('kernel.elf','binary_sha256'),('styleos.bin','raw_sha256')]:
        manifest['artifacts'].append(dict(path='aarch64/'+name,size=(target/name).stat().st_size,sha256=site[key]))
    (folder/'manifest.json').write_text(json.dumps(manifest))
    (target/'disk-app-site.json').write_text(json.dumps(site))
    (target/'disk-app-compiler.json').write_text(json.dumps(dict(flags=FLAGS,sources=sources)))
    return site,manifest


class DiskAppBindingTests(unittest.TestCase):
    def test_rederive_real_elf_raw_image_sidecars_and_manifest(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder=Path(temporary);site,_manifest=binding_fixture(folder)
            self.assertEqual(runner.verified_disk_app_site(folder),site)
            (folder/'aarch64/styleos.bin').write_bytes(b'changed')
            with self.assertRaises(ValueError): runner.verified_disk_app_site(folder)

    def test_exact_boolean_profile_compiler_source_and_artifact_bindings(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder=Path(temporary);site,baseline=binding_fixture(folder)
            changes=[]
            for key in ('test_disk_app',*runner.MODES):
                for value in (not baseline[key],int(baseline[key]),None):
                    item=deepcopy(baseline);item[key]=value;changes.append(item)
            for key in ('compiler_flags','disk_app_sources'):
                item=deepcopy(baseline);item[key]['x86_64']=[];changes.append(item)
            for flags in ([],FLAGS[:-1],FLAGS+['-DSTYLEOS_TEST_STORAGE=1'],
                    FLAGS+['-DSTYLEOS_TEST_DISK_APP=1'],FLAGS+['-USTYLEOS_TEST_DISK_APP'],
                    FLAGS+['--target=x86_64-none-elf'],FLAGS+['-DSTYLEOS_EXPECT_ARCH=1'],
                    FLAGS+['-D','STYLEOS_TEST_TASKS=1']):
                item=deepcopy(baseline);item['compiler_flags']['aarch64']=flags;changes.append(item)
            for sources in ([],runner.disk_app_sources()[::-1],runner.disk_app_sources()[:-1]):
                item=deepcopy(baseline);item['disk_app_sources']['aarch64']=sources;changes.append(item)
            for key,value in [('size',True),('sha256','wrong'),('path','other')]:
                item=deepcopy(baseline);item['artifacts'][0][key]=value;changes.append(item)
            for changed in changes:
                (folder/'manifest.json').write_text(json.dumps(changed))
                with self.subTest(changed=changed),self.assertRaises(ValueError):
                    runner.verified_disk_app_site(folder)
            (folder/'manifest.json').write_text(json.dumps(baseline))
            changed=deepcopy(site);changed['objects']['code']['end']+=4096
            (folder/'aarch64/disk-app-site.json').write_text(json.dumps(changed))
            with self.assertRaises(ValueError): runner.verified_disk_app_site(folder)

    def test_external_elf_raw_container_metadata_rederived_independently(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder=Path(temporary)
            (folder/'disk_demo.elf').write_bytes(app_elf())
            (folder/'disk_demo.raw').write_bytes(app_file()[32:])
            actual=build_app_metadata(folder/'disk_demo.elf',ROOT/'apps/disk_demo.S',folder/'disk_demo.raw')
            (folder/'app.json').write_text(json.dumps(dict(actual,commands=['fixture command'])))
            (folder/'app.bin').write_bytes(bytes.fromhex(actual['file_hex']))
            self.assertEqual(runner.verified_app_artifact(folder),actual)
            changed=dict(actual,marker=actual['marker']+1)
            (folder/'app.json').write_text(json.dumps(changed))
            with self.assertRaises(ValueError): runner.verified_app_artifact(folder)
            (folder/'app.json').write_text(json.dumps(actual))
            (folder/'app.bin').write_bytes(app_file(0x1818b002,73))
            with self.assertRaises(ValueError): runner.verified_app_artifact(folder)


@unittest.skipUnless(os.name=='posix','POSIX signal and pipe lifecycle')
class DiskAppProcessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.site=valid_site()
        cls.app=artifact()
        cls.output=valid_trace(site=cls.site,app=cls.app)

    def run_fake(self,output,after='while True:\n    time.sleep(1)',timeout=1.5):
        code='import os,signal,time\nsignal.alarm(5)\n'+f'os.write(1,{output!r})\n'+after+'\n'
        return runner.collect_vm([sys.executable,'-c',code],'run',self.site,self.app,timeout)

    def test_valid_halt_requires_observation_then_intentional_stop(self):
        result=self.run_fake(self.output)
        self.assertEqual(result['status'],'PASS',result)
        self.assertTrue(result['settled'])
        self.assertTrue(result['requested_stop'])
        self.assertFalse(result['forced_stop'])
        self.assertFalse(result['natural_exit'])
        self.assertEqual(result['returncode'],-signal.SIGTERM)
        self.assertEqual(result['uart_sha256'],hashlib.sha256(self.output).hexdigest())

    def test_timeout_reset_pass_only_kernel_fault_and_late_errors_fail(self):
        cases=[(b'','while True:\n    time.sleep(1)',.1),
            (self.output,'os._exit(0)',1.5),
            (b'STYLEOS DISKAPP PASS\nSTYLEOS KERNEL HALT\n','while True:\n    time.sleep(1)',1.5),
            (b'STYLEOS EXCEPTION arch=aarch64\n','while True:\n    time.sleep(1)',1.5),
            (self.output,'time.sleep(.05)\nos.write(1,b"STYLEOS DISKAPP ERROR reason=late\\n")\nwhile True:\n    time.sleep(1)',1.5)]
        for output,after,timeout in cases:
            with self.subTest(last=output[-80:],after=after):
                result=self.run_fake(output,after,timeout)
                self.assertEqual(result['status'],'FAIL',result)
                self.assertIsNone(result['trace'])

    def test_late_signal_handler_output_invalidates_complete_trace(self):
        after=('def stop(signum,frame):\n'
            '    os.write(1,b"STYLEOS DISKAPP ERROR reason=late\\n")\n'
            '    os._exit(0)\n'
            'signal.signal(signal.SIGTERM,stop)\n'
            'while True:\n    time.sleep(1)')
        result=self.run_fake(self.output,after)
        self.assertEqual(result['status'],'FAIL',result)
        self.assertIsNone(result['trace'])


class DiskAppInventoryTests(unittest.TestCase):
    def test_focused_host_models_and_python_patterns(self):
        with tempfile.TemporaryDirectory() as temporary:
            checks=runner.DiskAppChecks(runner.new_run_directory(Path(temporary)/'output'))
            with patch.object(checks,'command',return_value=True) as commands:
                self.assertTrue(checks.host())
            self.assertEqual([call.args[0] for call in commands.call_args_list],
                ['host-storage-format','host-storage-fdt','host-storage-transport',
                 'host-disk-app-format','host-disk-app-fdt','host-storage-python','host-disk-app-python'])
            self.assertIn('test_storage*.py',commands.call_args_list[-2].args[1][0])
            self.assertIn('test_disk_app*.py',commands.call_args_list[-1].args[1][0])

    def test_three_vms_same_boot_and_all_fixture_writes_precede_first_vm_python314_probes(self):
        with tempfile.TemporaryDirectory() as temporary:
            run=runner.new_run_directory(Path(temporary)/'output')
            site=valid_site()
            app_a,app_b=artifact(),artifact(0x1818b002,73)
            events=[]
            real_check_output=subprocess.check_output
            real_run=subprocess.run
            real_which=runner.shutil.which
            real_platform=runner.platform.platform
            def check_output(command,*args,**kwargs):
                if command==['git','rev-parse','HEAD']: return 'fixture-head'
                if command==['git','status','--porcelain']: return b''
                return real_check_output(command,*args,**kwargs)
            def run_command(command,*args,**kwargs):
                if command==['/fake/qemu','--version']:
                    return subprocess.CompletedProcess(command,0,'QEMU fixture')
                return real_run(command,*args,**kwargs)
            def which(command,*args,**kwargs):
                return '/fake/qemu' if command=='styleos-inventory-qemu' else real_which(command,*args,**kwargs)
            def platform_description():
                probe=getattr(runner.platform,'_syscmd_file',None)
                if probe is not None: self.assertIsInstance(probe(sys.executable),str)
                return real_platform()
            def command(checks,name,commands):
                if name=='build-disk-app-boot':
                    (run/'boot').mkdir();(run/'boot/manifest.json').write_text('{}')
                if name.startswith('build-app-'):
                    variant=name[-1];folder=run/'apps'/variant;folder.mkdir()
                    (folder/'app.bin').write_bytes(bytes.fromhex((app_a if variant=='a' else app_b)['file_hex']))
                return True
            def fixture_create(run_dir,name,app_file,packer):
                events.append(('fixture',name))
                path=run/'disks'/(name+'.img');path.write_bytes(bytes(runner.DISK_BYTES))
                return path,runner.disk_state(path)
            def vm(checks,name,disk,identity,phase,qemu,boot,current_site,app):
                events.append(('vm',name))
                return dict(status='PASS',stage=name,phase=phase,pid=100+len(events),command=['qemu',phase])
            def app_artifact(folder): return app_a if Path(folder).name=='a' else app_b
            with (patch.object(runner.DiskAppChecks,'host',return_value=True),
                patch.object(runner.DiskAppChecks,'command',autospec=True,side_effect=command),
                patch.object(runner.DiskAppChecks,'vm',autospec=True,side_effect=vm),
                patch.object(runner,'verified_disk_app_site',return_value=site),
                patch.object(runner,'verified_app_artifact',side_effect=app_artifact),
                patch.object(runner,'metadata_snapshot',return_value={'fixture':'a'*64}),
                patch.object(runner,'source_snapshot',return_value={'fixture.py':'b'*64}),
                patch.object(runner,'tool_versions',return_value={'fixture':'version'}),
                patch.object(runner,'fixture_disk',side_effect=fixture_create),
                patch.dict(os.environ,{'QEMU_AARCH64':'styleos-inventory-qemu'}),
                patch.object(runner.platform,'platform',side_effect=platform_description),
                patch.object(runner.subprocess,'check_output',side_effect=check_output),
                patch.object(runner.subprocess,'run',side_effect=run_command),
                patch.object(runner.shutil,'which',side_effect=which),patch('builtins.print')):
                checks,provenance=runner.execute(run,1.5)
            self.assertEqual(events,[('fixture','variant-a'),('fixture','variant-b'),('fixture','reject'),
                ('vm','variant-a'),('vm','variant-b'),('vm','reject')])
            self.assertEqual(provenance['git_head'],'fixture-head')
            self.assertFalse(provenance['git_dirty'])
            self.assertEqual(provenance['qemu_version'],'QEMU fixture')
            self.assertEqual(len(provenance['qemu_processes']),3)
            self.assertEqual(len({row['pid'] for row in provenance['qemu_processes']}),3)
            self.assertTrue(all(row['status']=='PASS' for row in checks.results))
            self.assertEqual(checks.results[-1]['name'],'disk-app-replacement')

    def test_vm_unchanged_disk_hash_and_complete_bound_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            run=runner.new_run_directory(Path(temporary)/'output')
            checks=runner.DiskAppChecks(run);checks.timeout=1.5
            boot=run/'boot';(boot/'aarch64').mkdir(parents=True)
            (boot/'aarch64/styleos.bin').write_bytes(b'boot')
            (boot/'manifest.json').write_bytes(b'{}')
            disk=run/'disks/variant-a.img';disk.write_bytes(bytes(runner.DISK_BYTES))
            state=runner.disk_state(disk)
            site=valid_site();app=artifact();output=valid_trace(site=site,app=app)
            for changed in (False,True):
                result=dict(status='PASS',detail='fixture halt',pid=100,
                    command=['qemu'],phase='run',uart=output,
                    uart_sha256=hashlib.sha256(output).hexdigest(),requested_stop=True,
                    forced_stop=False,natural_exit=False,settled=True,
                    stop_signal=signal.SIGTERM,returncode=-signal.SIGTERM,
                    trace=runner.parse_disk_app_trace(output,'run',site,app))
                after=dict(state,sha256='c'*64) if changed else state.copy()
                with (patch.object(runner,'collect_vm',return_value=result),
                    patch.object(runner,'disk_state',side_effect=[state,after]),patch('builtins.print')):
                    saved=checks.vm('vm-'+str(changed),disk,state,'run','/qemu',boot,site,app)
                self.assertEqual(saved['status'],'FAIL' if changed else 'PASS')
                self.assertEqual(saved['app']['file_sha256'],app['file_sha256'])
                self.assertEqual(saved['binary_sha256'],site['binary_sha256'])
                self.assertEqual(saved['manifest_sha256'],hashlib.sha256(b'{}').hexdigest())
                self.assertEqual((checks.logs/('vm-'+str(changed)+'.uart')).read_bytes(),output)


if __name__=='__main__':
    unittest.main()
