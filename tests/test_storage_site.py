"""Independently constructed ELF/raw Image storage interval proofs."""
from pathlib import Path
import hashlib
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.storage_site import describe_storage, COMMON

BASE = 0x40080000
FLAGS = ['--target=aarch64-none-elf', '-DSTYLEOS_EXPECT_ARCH=2',
         '-DSTYLEOS_TEST_STORAGE=1', '-mgeneral-regs-only', '-mstrict-align']
PH = 64
SYM = 0x200
SH = 0x900
TEXT = 0x1000
BSS_PH = PH + 56


def fixture(extra_symbols=()):
    """No compiler or project fixture constructor supplies these bytes."""
    image_size = 0x24000
    raw = bytearray(0x1000)
    struct.pack_into('<I', raw, 0, 0x14000010)  # B image_start+64.
    struct.pack_into('<QQQ', raw, 8, 0x80000, image_size, 0)
    struct.pack_into('<I', raw, 56, 0x644d5241)
    struct.pack_into('<III', raw, 64, 0xd503201f, 0xd503205f, 0x17ffffff)
    data = bytearray(0x2000)
    data[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', data, 16, 2, 183, 1, BASE,
                     PH, SH, 0, 64, 56, 2, 64, 6, 5)
    struct.pack_into('<IIQQQQQQ', data, PH, 1, 5, TEXT,
                     BASE, BASE, len(raw), len(raw), 0x1000)
    struct.pack_into('<IIQQQQQQ', data, BSS_PH, 1, 6, 0x2000,
                     BASE+0x2000, BASE+0x2000, 0, image_size-0x2000, 0x1000)
    data[TEXT:TEXT+len(raw)] = raw
    symbols = [
        ('diagnostic', 0x01, 2, BASE+0xb000, 12400),
        ('styleos_bootpool_storage', 0x11, 2, BASE+0x2000, 0x8000),
        ('styleos_bootpool_metadata', 0x11, 2, BASE+0xa000, 0x1000),
        ('image_start', 0x10, 1, BASE, 0),
        ('_start', 0x10, 1, BASE+64, 0),
        ('__image_end', 0x10, 2, BASE+image_size, 0),
        ('__bss_start', 0x10, 2, BASE+0x2000, 0),
        ('__bss_end', 0x10, 2, BASE+image_size, 0),
        ('__stack_bottom', 0x10, 2, BASE+0xe080, 0),
        ('__stack_top', 0x10, 2, BASE+0x1e080, 0),
        ('aarch64_vectors', 0x11, 1, BASE+0x800, 2048),
        ('styleos_vblk_control', 0x11, 2, BASE+0x1f000, 4096),
        ('styleos_vblk_queue', 0x11, 2, BASE+0x20000, 4096),
        ('styleos_vblk_request', 0x11, 2, BASE+0x21000, 4096),
        ('styleos_storage_control', 0x11, 2, BASE+0x22000, 4096),
        ('styleos_storage_workspace', 0x11, 2, BASE+0x23000, 4096),
        *extra_symbols,
    ]
    strings = b'\0'+b'\0'.join(name.encode() for name, *_ in symbols)+b'\0'
    names = b'\0.text\0.bss\0.symtab\0.strtab\0.shstrtab\0'
    assert len(strings) < 0x280
    data[0x500:0x500+len(strings)] = strings
    data[0x780:0x780+len(names)] = names
    locations = {}
    for n, (name, info, index, address, size) in enumerate(symbols, 1):
        off = SYM+24*n
        struct.pack_into('<IBBHQQ', data, off, strings.index(name.encode()),
                         info, 0, index, address, size)
        locations.setdefault(name, []).append(off)
    sections = [
        (0,0,0,0,0,0,0,0,0,0),
        (names.index(b'.text'),1,6,BASE,TEXT,len(raw),0,0,0x800,0),
        (names.index(b'.bss'),8,3,BASE+0x2000,0x2000,image_size-0x2000,0,0,0x1000,0),
        (names.index(b'.symtab'),2,0,0,SYM,(len(symbols)+1)*24,4,2,8,24),
        (names.index(b'.strtab'),3,0,0,0x500,len(strings),0,0,1,0),
        (names.index(b'.shstrtab'),3,0,0,0x780,len(names),0,0,1,0),
    ]
    for n, section in enumerate(sections):
        struct.pack_into('<IIQQQQIIQQ', data, SH+64*n, *section)
    return data, raw, locations


class StorageSiteTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='styleos-storage-site-')
        self.addCleanup(temp.cleanup)
        self.folder = Path(temp.name)

    def prove(self, data=None, raw=None, flags=None, arch='aarch64', map_path=None):
        original = fixture()
        data = original[0] if data is None else data
        raw = original[1] if raw is None else raw
        binary, image = self.folder/'kernel.elf', self.folder/'styleos.bin'
        binary.write_bytes(data); image.write_bytes(raw)
        return describe_storage(arch, binary, map_path, image,
                                FLAGS if flags is None else flags)

    def test_full_resources_bss_and_raw_image_are_bound_without_external_tools(self):
        site = self.prove()
        self.assertEqual(site['image_start'], BASE)
        self.assertEqual(site['image_size'], 0x24000)
        self.assertEqual(site['image_entry_target'], BASE+64)
        self.assertEqual(site['binary_sha256'], hashlib.sha256(fixture()[0]).hexdigest())
        self.assertEqual(site['raw_sha256'], hashlib.sha256(fixture()[1]).hexdigest())
        self.assertEqual(site['compiler_flags'], FLAGS)
        self.assertEqual(set(site['objects']), set(COMMON)|{'stack','vectors','pool','pool_metadata'})
        for name in ('vblk_control','queue','request','control','workspace'):
            self.assertEqual(site['objects'][name]['bytes'], 4096)
            self.assertEqual(site['objects'][name]['start'] % 4096, 0)
        self.assertEqual(site['objects']['stack']['bytes'], 65536)
        self.assertGreater(site['bss_end']-site['bss_start'], len(fixture()[1]))
        self.assertNotIn('integer_only', site)  # No instruction decoder dependency/claim.

    def test_new_profile_requires_arm_target_and_only_storage_define(self):
        for flags in ([], ['--target=x86_64-pc-win32-coff']+FLAGS[1:], FLAGS[1:],
                      FLAGS+['-DSTYLEOS_TEST_TASKS=1'], FLAGS+['-DSTYLEOS_TEST_STORAGE=1'],
                      FLAGS+['--target=x86_64-pc-win32-coff'],
                      FLAGS+['--target', 'x86_64-pc-win32-coff'],
                      FLAGS+['-target', 'x86_64-pc-win32-coff'],
                      FLAGS+['--target=aarch64-none-elf'],
                      FLAGS+['-DSTYLEOS_EXPECT_ARCH=1'],
                      FLAGS+['-DSTYLEOS_EXPECT_ARCH=2'],
                      FLAGS+['-DSTYLEOS_TEST_STORAGE=0'],
                      FLAGS+['-USTYLEOS_TEST_STORAGE'],
                      FLAGS+['-USTYLEOS_EXPECT_ARCH'],
                      FLAGS+['-D', 'STYLEOS_TEST_TASKS=1'],
                      FLAGS+['-U', 'STYLEOS_TEST_STORAGE'],
                      FLAGS+[''], FLAGS+[None], FLAGS+['bad\0flag']):
            with self.subTest(flags=flags), self.assertRaises(ValueError):
                self.prove(flags=flags)
        with self.assertRaises(ValueError): self.prove(arch='x86_64')
        with self.assertRaises(ValueError): self.prove(map_path=self.folder/'kernel.map')

    def test_each_dma_and_control_symbol_has_full_exact_extent(self):
        for key,(name,_,_) in COMMON.items():
            for delta in (-1,1):
                data,raw,loc=fixture();off=loc[name][0]
                struct.pack_into('<Q',data,off+16,struct.unpack_from('<Q',data,off+16)[0]+delta)
                with self.subTest(name=name,delta=delta),self.assertRaises(ValueError):self.prove(data,raw)

    def test_alignment_and_cross_object_overlaps_are_rejected(self):
        for name in ('styleos_vblk_control','styleos_vblk_queue','styleos_vblk_request',
                     'styleos_storage_control','styleos_storage_workspace'):
            data,raw,loc=fixture();off=loc[name][0]
            struct.pack_into('<Q',data,off+8,struct.unpack_from('<Q',data,off+8)[0]+8)
            with self.subTest(name=name),self.assertRaises(ValueError):self.prove(data,raw)
        data,raw,loc=fixture();struct.pack_into('<Q',data,loc['styleos_storage_workspace'][0]+8,BASE+0x22000)
        with self.assertRaises(ValueError):self.prove(data,raw)

    def test_entire_interval_must_remain_inside_bss_and_reserved_image(self):
        data,raw,loc=fixture();struct.pack_into('<Q',data,loc['styleos_storage_workspace'][0]+8,BASE+0x24000)
        with self.assertRaises(ValueError):self.prove(data,raw)
        data,raw,loc=fixture();struct.pack_into('<Q',data,loc['__bss_end'][0]+8,BASE+0x23000)
        with self.assertRaises(ValueError):self.prove(data,raw)
        data,raw,_=fixture();struct.pack_into('<Q',data,BSS_PH+40,0x21000)
        with self.assertRaises(ValueError):self.prove(data,raw)

    def test_identity_and_writable_complete_load_are_required(self):
        for offset,value in ((BSS_PH+24,BASE+0x3000),(BSS_PH+4,4)):
            data,raw,_=fixture();struct.pack_into('<Q' if offset==BSS_PH+24 else '<I',data,offset,value)
            with self.subTest(offset=offset),self.assertRaises(ValueError):self.prove(data,raw)
        data,raw,_=fixture();data.extend(b'\0');struct.pack_into('<Q',data,BSS_PH+32,1)
        with self.assertRaises(ValueError):self.prove(data,raw)

    def test_all_loads_are_inside_identity_disjoint_and_fully_claimed(self):
        # The named resources still fit; the complete LOAD itself must fit too.
        data,raw,_=fixture();struct.pack_into('<Q',data,BSS_PH+40,0x23000)
        with self.assertRaises(ValueError):self.prove(data,raw)
        # Extending a LOAD into an otherwise harmless image gap is unclaimed.
        data,raw,_=fixture();struct.pack_into('<Q',data,PH+40,0x2000)
        with self.assertRaises(ValueError):self.prove(data,raw)
        for address,size,physical in ((BASE+0x30000,0x1000,BASE+0x30000),
                                      (BASE+0x1000,0x1000,BASE+0x1000),
                                      (BASE+0x800,0x100,BASE+0x800),
                                      (BASE+0x1000,0x1000,BASE+0x3000)):
            data,raw,_=fixture()
            struct.pack_into('<H',data,56,3)
            struct.pack_into('<IIQQQQQQ',data,PH+112,1,6,0x2000,
                             address,physical,0,size,0x1000)
            with self.subTest(address=address,size=size,physical=physical),self.assertRaises(ValueError):
                self.prove(data,raw)
        for alignment in (3,0x2000):
            data,raw,_=fixture();struct.pack_into('<Q',data,PH+48,alignment)
            with self.subTest(alignment=alignment),self.assertRaises(ValueError):self.prove(data,raw)

    def test_missing_duplicate_local_or_unrelated_profile_symbols_fail(self):
        data,raw,loc=fixture();data[loc['styleos_vblk_queue'][0]+4]=1
        with self.assertRaises(ValueError):self.prove(data,raw)
        for extra in [('styleos_vblk_queue',0x11,2,BASE+0x20000,4096),
                      ('styleos_user_control',0x11,2,BASE+0x20000,4096),
                      ('styleos_tasks_control',0x11,2,BASE+0x20000,4096),
                      ('styleos_ownership_workspace',0x11,2,BASE+0x20000,4096)]:
            data,raw,_=fixture([extra])
            with self.subTest(extra=extra[0]),self.assertRaises(ValueError):self.prove(data,raw)

    def test_complete_original_stack_endpoints_and_vector_bytes_are_checked(self):
        data,raw,loc=fixture();struct.pack_into('<Q',data,loc['__stack_top'][0]+8,BASE+0x1e070)
        with self.assertRaises(ValueError):self.prove(data,raw)
        data,raw,loc=fixture();struct.pack_into('<Q',data,loc['aarch64_vectors'][0]+16,1024)
        with self.assertRaises(ValueError):self.prove(data,raw)
        data,raw,_=fixture();raw[0x900]=1
        with self.assertRaises(ValueError):self.prove(data,raw)

    def test_raw_header_size_magic_and_full_allocated_bytes_are_bound(self):
        for offset in (16,56,100):
            data,raw,_=fixture();raw[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):self.prove(data,raw)
        data,raw,_=fixture();raw=raw[:0x900]
        with self.assertRaises(ValueError):self.prove(data,raw)

    def test_image_entry_branch_must_reach_executable_file_bytes(self):
        for word in (0xd503201f,0x14000000,0x14000800,0x14000200):
            data,raw,_=fixture();struct.pack_into('<I',raw,0,word);struct.pack_into('<I',data,TEXT,word)
            with self.subTest(word=word),self.assertRaises(ValueError):self.prove(data,raw)

    def test_entry_requires_one_global_executable_start_label(self):
        for info,index,address,size in ((0x00,1,BASE+64,0),
                                         (0x10,2,BASE+0x2000,0),
                                         (0x10,1,BASE+64,4)):
            data,raw,loc=fixture();off=loc['_start'][0]
            struct.pack_into('<BBHQQ',data,off+4,info,0,index,address,size)
            with self.subTest(info=info,index=index,address=address,size=size),self.assertRaises(ValueError):
                self.prove(data,raw)
        data,raw,_=fixture([('_start',0x10,1,BASE+64,0)])
        with self.assertRaises(ValueError):self.prove(data,raw)


if __name__=='__main__':unittest.main()
