"""Independent ELF, container and finite raw-table proofs for T-P1.8B."""
from pathlib import Path
import hashlib
import struct
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.disk_app_site import (COMMON, describe_disk_app, validate_app_bytes,
                                 build_app_metadata, verify_table_closure)

BASE = 0x40080000
PAGE = 4096
FLAGS = ['--target=aarch64-none-elf', '-DSTYLEOS_EXPECT_ARCH=2',
         '-DSTYLEOS_TEST_DISK_APP=1', '-mgeneral-regs-only', '-mstrict-align']
PH, SYM, STRINGS, SH, TEXT = 64, 0x200, 0x1000, 0x2000, 0x4000


def fixture(extra_symbols=()):
    """Construct bytes independently of build.py and all production helpers."""
    image_size = 0x6d000
    raw = bytearray(0x1800)
    struct.pack_into('<I', raw, 0, 0x14000010)
    struct.pack_into('<QQQ', raw, 8, 0x80000, image_size, 0)
    struct.pack_into('<I', raw, 56, 0x644d5241)
    struct.pack_into('<III', raw, 64, 0xd503201f, 0xd503205f, 0x17ffffff)
    data = bytearray(TEXT + 0x2000)
    data[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', data, 16, 2, 183, 1, BASE,
                     PH, SH, 0, 64, 56, 2, 64, 6, 5)
    struct.pack_into('<IIQQQQQQ', data, PH, 1, 5, TEXT,
                     BASE, BASE, len(raw), len(raw), PAGE)
    struct.pack_into('<IIQQQQQQ', data, PH + 56, 1, 6, len(data),
                     BASE+0x2000, BASE+0x2000, 0, image_size-0x2000, PAGE)
    data[TEXT:TEXT+len(raw)] = raw
    symbols = [
        ('diagnostic', 0x01, 2, BASE+0xb000, 12400),
        ('styleos_bootpool_storage', 0x11, 2, BASE+0x2000, 0x8000),
        ('styleos_bootpool_metadata', 0x11, 2, BASE+0xa000, PAGE),
        ('image_start', 0x10, 1, BASE, 0),
        ('_start', 0x10, 1, BASE+64, 0),
        ('__image_end', 0x10, 2, BASE+image_size, 0),
        ('__bss_start', 0x10, 2, BASE+0x2000, 0),
        ('__bss_end', 0x10, 2, BASE+image_size, 0),
        ('__stack_bottom', 0x10, 2, BASE+0xe080, 0),
        ('__stack_top', 0x10, 2, BASE+0x1e080, 0),
        ('aarch64_vectors', 0x11, 1, BASE+0x800, 2048),
        ('styleos_disk_app_vectors', 0x11, 1, BASE+0x1000, 2048),
        ('styleos_vblk_control', 0x11, 2, BASE+0x1f000, PAGE),
        ('styleos_vblk_queue', 0x11, 2, BASE+0x20000, PAGE),
        ('styleos_vblk_request', 0x11, 2, BASE+0x21000, PAGE),
        ('styleos_disk_app_control', 0x11, 2, BASE+0x22000, 16384),
        ('styleos_disk_app_storage_workspace', 0x11, 2, BASE+0x26000, PAGE),
        ('styleos_disk_app_tables', 0x11, 2, BASE+0x27000, 2*32*PAGE),
        ('styleos_disk_app_entry_stack', 0x11, 2, BASE+0x67000, 16384),
        ('styleos_disk_app_code', 0x11, 2, BASE+0x6b000, PAGE),
        ('styleos_disk_app_user_stack', 0x11, 2, BASE+0x6c000, PAGE),
        *extra_symbols,
    ]
    strings = b'\0' + b'\0'.join(name.encode() for name, *_ in symbols) + b'\0'
    names = b'\0.text\0.bss\0.symtab\0.strtab\0.shstrtab\0'
    data[STRINGS:STRINGS+len(strings)] = strings
    data[0x1800:0x1800+len(names)] = names
    locations = {}
    for n, (name, info, index, address, size) in enumerate(symbols, 1):
        offset = SYM + n*24
        struct.pack_into('<IBBHQQ', data, offset, strings.index(name.encode()),
                         info, 0, index, address, size)
        locations.setdefault(name, []).append(offset)
    sections = [(0,0,0,0,0,0,0,0,0,0),
        (names.index(b'.text'),1,6,BASE,TEXT,len(raw),0,0,0x800,0),
        (names.index(b'.bss'),8,3,BASE+0x2000,len(data),image_size-0x2000,0,0,PAGE,0),
        (names.index(b'.symtab'),2,0,0,SYM,(len(symbols)+1)*24,4,2,8,24),
        (names.index(b'.strtab'),3,0,0,STRINGS,len(strings),0,0,1,0),
        (names.index(b'.shstrtab'),3,0,0,0x1800,len(names),0,0,1,0)]
    for n, section in enumerate(sections):
        struct.pack_into('<IIQQQQIIQQ', data, SH+n*64, *section)
    return data, raw, locations


def app_file(marker=0x12345678, exit_code=37):
    code = struct.pack('<7I', 0xb5000060, 0xd2a24021, 0xf9400022,
        0xd2800013 | ((marker & 0xffff) << 5),
        0xf2a00013 | ((marker >> 16) << 5),
        0xd2800000 | (exit_code << 5), 0xd4000041)
    header = struct.pack('<8sIHHIIII', b'STYAPP1\0', 1, 183, 32, 28, 0, 0, 0)
    result = bytearray(header + code)
    struct.pack_into('<I', result, 28, zlib.crc32(result) & 0xffffffff)
    return result


def checksum(data):
    result = bytearray(data)
    result[28:32] = bytes(4)
    struct.pack_into('<I', result, 28, zlib.crc32(result) & 0xffffffff)
    return result


def app_elf():
    data = bytearray(0x1100)
    data[:16] = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<HHIQQQIHHHHHH', data, 16, 2, 183, 1, 0,
                     64, 0x800, 0, 64, 56, 1, 64, 5, 4)
    struct.pack_into('<IIQQQQQQ', data, 64, 1, 5, 0x1000, 0, 0, 28, 28, PAGE)
    strings = b'\0disk_app_entry\0disk_app_fault\0disk_app_normal\0disk_app_exit\0disk_app_end\0'
    names = b'\0.text\0.symtab\0.strtab\0.shstrtab\0'
    data[0x400:0x400+len(strings)] = strings
    data[0x500:0x500+len(names)] = names
    symbols = [('disk_app_entry',0),('disk_app_fault',8),('disk_app_normal',12),
               ('disk_app_exit',24),('disk_app_end',28)]
    for n, (name, value) in enumerate(symbols, 1):
        struct.pack_into('<IBBHQQ', data, 0x200+n*24, strings.index(name.encode()),
                         0x10, 0, 1, value, 0)
    sections = [(0,0,0,0,0,0,0,0,0,0),
                (names.index(b'.text'),1,6,0,0x1000,28,0,0,4,0),
                (names.index(b'.symtab'),2,0,0,0x200,6*24,3,1,8,24),
                (names.index(b'.strtab'),3,0,0,0x400,len(strings),0,0,1,0),
                (names.index(b'.shstrtab'),3,0,0,0x500,len(names),0,0,1,0)]
    for n, section in enumerate(sections):
        struct.pack_into('<IIQQQQIIQQ', data, 0x800+n*64, *section)
    data[0x1000:0x101c] = app_file()[32:]
    return data


def tables(site, user=False):
    base = site['objects']['tables']['start'] + (32*PAGE if user else 0)
    addresses = list(range(base, base+32*PAGE, PAGE))
    pages = {pa: [0]*512 for pa in addresses}
    next_page = 1

    def insert(va, descriptor):
        nonlocal next_page
        pa = base
        for shift in (39,30,21):
            index = (va >> shift) & 511
            if not pages[pa][index]:
                pages[pa][index] = addresses[next_page] | 3
                next_page += 1
            pa = pages[pa][index] & ~0xfff
        pages[pa][(va >> 12) & 511] = descriptor

    for pa in range(site['image_start'] & -PAGE, (site['image_end']+PAGE-1) & -PAGE, PAGE):
        insert(pa, pa | 0x40000000000703)
    insert(0x09000000, 0x09000000 | 0x60000000000607)
    if user:
        insert(0x10000000, site['objects']['code']['start'] | 0x200000000007c3)
        insert(0x11000000, site['objects']['user_stack']['start'] | 0x60000000000743)
    return base, addresses, pages, insert


class DiskAppSiteTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='styleos-disk-app-site-')
        self.addCleanup(temp.cleanup)
        self.folder = Path(temp.name)

    def prove(self, data=None, raw=None, flags=None, arch='aarch64', map_path=None):
        default, image, _ = fixture()
        binary, raw_path = self.folder/'kernel.elf', self.folder/'styleos.bin'
        binary.write_bytes(default if data is None else data)
        raw_path.write_bytes(image if raw is None else raw)
        return describe_disk_app(arch, binary, map_path, raw_path, FLAGS if flags is None else flags)

    def test_whole_static_bss_resources_identity_and_raw_bytes_bound(self):
        site = self.prove()
        self.assertEqual(site['image_entry_target'], BASE+64)
        self.assertEqual(site['image_size'], 0x6d000)
        self.assertEqual(site['binary_sha256'], hashlib.sha256(fixture()[0]).hexdigest())
        self.assertEqual(set(site['objects']), set(COMMON) | {'stack','vectors','disk_app_vectors','pool','pool_metadata'})
        self.assertEqual(site['objects']['tables']['bytes'], 262144)
        self.assertEqual(site['objects']['code']['bytes'], PAGE)
        self.assertNotIn('integer_only', site)

    def test_profile_flags_architecture_and_integer_compiler_contract(self):
        invalid = [[], FLAGS[1:], FLAGS[:-1], FLAGS[:-2], FLAGS+['-DSTYLEOS_TEST_STORAGE=1'],
                   FLAGS+['-DSTYLEOS_TEST_DISK_APP=1'], FLAGS+['-USTYLEOS_TEST_DISK_APP'],
                   FLAGS+['--target=x86_64-none-elf'], FLAGS+['-DSTYLEOS_EXPECT_ARCH=1'],
                   FLAGS+['-D','STYLEOS_TEST_TASKS=1'], FLAGS+['-mno-general-regs-only'],
                   FLAGS+['-mno-strict-align'], FLAGS+['@opaque.rsp'],
                   FLAGS+['-Xclang','-target-feature'], FLAGS+['-march=armv8-a+simd']]
        for flags in invalid:
            with self.subTest(flags=flags), self.assertRaises(ValueError):
                self.prove(flags=flags)
        with self.assertRaises(ValueError): self.prove(arch='x86_64')
        with self.assertRaises(ValueError): self.prove(map_path=self.folder/'kernel.map')

    def test_every_full_object_size_alignment_and_disjointness(self):
        for name, size, alignment in COMMON.values():
            for field, value in ((16,size-1),(8,BASE+1)):
                data, raw, locations = fixture()
                struct.pack_into('<Q', data, locations[name][0]+field, value)
                with self.subTest(name=name,field=field), self.assertRaises(ValueError):
                    self.prove(data,raw)
        data,raw,locations=fixture()
        struct.pack_into('<Q',data,locations['styleos_disk_app_code'][0]+8,BASE+0x6c000)
        with self.assertRaises(ValueError): self.prove(data,raw)

    def test_copy_destination_must_be_complete_zero_filled_bss(self):
        for index in (1,):
            data,raw,locations=fixture()
            struct.pack_into('<H',data,locations['styleos_disk_app_code'][0]+6,index)
            struct.pack_into('<Q',data,locations['styleos_disk_app_code'][0]+8,BASE+0x1000)
            with self.assertRaises(ValueError): self.prove(data,raw)
        for offset, value in ((PH+56+24,BASE+0x3000),(PH+56+4,4),(PH+56+4,7),
                              (PH+56+40,0x6a000),(PH+40,0x2000)):
            data,raw,_=fixture()
            struct.pack_into('<I' if offset == PH+56+4 else '<Q',data,offset,value)
            with self.subTest(offset=offset), self.assertRaises(ValueError): self.prove(data,raw)
        data,raw,locations=fixture()
        struct.pack_into('<Q',data,locations['__bss_end'][0]+8,BASE+0x6c000)
        with self.assertRaises(ValueError): self.prove(data,raw)

    def test_old_payloads_and_externally_compiled_app_labels_forbidden_in_boot(self):
        for name in ('styleos_user_code_a','styleos_user_a_fault','styleos_tasks_control',
                     'styleos_paging_tables','styleos_storage_control','disk_app_entry','disk_app_fault'):
            data,raw,_=fixture([(name,0x11,2,BASE+0x6b000,PAGE)])
            with self.subTest(name=name), self.assertRaises(ValueError): self.prove(data,raw)

    def test_original_and_own_vectors_full_bytes_and_stack_endpoints(self):
        for name in ('aarch64_vectors','styleos_disk_app_vectors'):
            data,raw,locations=fixture()
            struct.pack_into('<Q',data,locations[name][0]+16,1024)
            with self.subTest(name=name), self.assertRaises(ValueError): self.prove(data,raw)
        data,raw,locations=fixture()
        struct.pack_into('<Q',data,locations['__stack_top'][0]+8,BASE+0x1e070)
        with self.assertRaises(ValueError): self.prove(data,raw)
        for offset in (0,16,56,100,0x900,0x1700):
            data,raw,_=fixture();raw[offset]^=1
            with self.subTest(offset=offset), self.assertRaises(ValueError): self.prove(data,raw)
        data,raw,_=fixture();raw.append(0)
        with self.assertRaises(ValueError): self.prove(data,raw)

    def test_full_closed_integer_payload_decode_and_variant_binding(self):
        for marker, exit_code in ((0,0),(0x12345678,37),(0xffffffff,65535)):
            raw=app_file(marker,exit_code); metadata=validate_app_bytes(raw)
            self.assertEqual(metadata['marker'],marker)
            self.assertEqual(metadata['exit_code'],exit_code)
            self.assertEqual(metadata['file_sha256'],hashlib.sha256(raw).hexdigest())
            self.assertEqual(metadata['sites']['fault']['user_pc'],0x10000008)
            self.assertEqual(metadata['sites']['exit']['saved_pc'],0x1000001c)
            self.assertEqual(sum(len(bytes.fromhex(row['bytes'])) for row in metadata['instructions']),28)

    def test_header_length_overflow_entry_reserved_and_crc_refusals(self):
        for offset, fmt, values in ((8,'<I',(0,2,0xffffffff)),(12,'<H',(0,62)),
            (14,'<H',(0,31,65535)),(16,'<I',(0,24,32,0xffffffff)),
            (20,'<I',(1,4,28,0xffffffff)),(24,'<I',(1,0xffffffff))):
            for value in values:
                data=app_file();struct.pack_into(fmt,data,offset,value)
                with self.subTest(offset=offset,value=value), self.assertRaises(ValueError):
                    validate_app_bytes(checksum(data))
        for data in (app_file()[:31],app_file()[:-1],app_file()+b'\0',bytes(1025)):
            with self.assertRaises(ValueError): validate_app_bytes(data)
        data=app_file();data[0]^=1
        with self.assertRaises(ValueError): validate_app_bytes(checksum(data))
        data=app_file();data[35]^=1
        with self.assertRaises(ValueError): validate_app_bytes(data)

    def test_every_non_immediate_opcode_bit_and_instruction_order_is_closed(self):
        # Mutation keeps a valid container checksum, testing the decoder itself.
        for index in range(7):
            for bit in range(32):
                if index in (3,4,5) and 5 <= bit <= 20:
                    continue
                data=app_file();data[32+index*4+bit//8]^=1 << (bit%8)
                with self.subTest(instruction=index,bit=bit), self.assertRaises(ValueError):
                    validate_app_bytes(checksum(data))
        for offset in range(0,28,4):
            data=app_file();struct.pack_into('<I',data,32+offset,0xd503201f)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                validate_app_bytes(checksum(data))
        for word in (0x4e201c00,0xd69f03e0,0xd61f0000,0xf9400042,0xd4000021):
            data=app_file();struct.pack_into('<I',data,40,word)
            with self.subTest(word=word), self.assertRaises(ValueError):
                validate_app_bytes(checksum(data))

    def test_external_elf_source_and_full_code_are_independently_bound(self):
        binary, source, raw = self.folder/'app.elf',self.folder/'app.S',self.folder/'app.bin'
        binary.write_bytes(app_elf());source.write_text('external assembly example\n');raw.write_bytes(app_file()[32:])
        metadata=build_app_metadata(binary,source,raw)
        self.assertEqual(metadata['file_hex'],app_file().hex())
        self.assertEqual(metadata['binary_sha256'],hashlib.sha256(app_elf()).hexdigest())
        raw.write_bytes(app_file()[32:-1])
        with self.assertRaises(ValueError): build_app_metadata(binary,source,raw)
        for offset,fmt,value in ((64+24,'<Q',4),(64+4,'<I',7),(64+40,'<Q',32),
                                  (0x200+3*24+8,'<Q',16),(0x800+64+8,'<Q',7)):
            data=app_elf();struct.pack_into(fmt,data,offset,value);binary.write_bytes(data)
            with self.subTest(offset=offset), self.assertRaises(ValueError): build_app_metadata(binary,source)

    def test_complete_safe_and_user_raw_descriptor_closure(self):
        site=self.prove()
        for user in (False,True):
            root,addresses,pages,_=tables(site,user)
            report=verify_table_closure(root,addresses,pages,site,user)
            self.assertEqual(report['user_leaf_count'],2 if user else 0)
            self.assertFalse(report['virtio_mmio_mapped'])
            self.assertFalse(report['fault_va_mapped'])
            raw={pa:struct.pack('<512Q',*page) for pa,page in pages.items()}
            self.assertEqual(report,verify_table_closure(root,addresses,raw,site,user))

    def test_table_reserve_omissions_loops_hidden_descriptors_and_blocks_fail(self):
        site=self.prove();root,addresses,pages,_=tables(site)
        with self.assertRaises(ValueError): verify_table_closure(root,addresses[:-1],pages,site)
        pages[addresses[-1]][511]=1
        with self.assertRaises(ValueError): verify_table_closure(root,addresses,pages,site)
        for descriptor in (root|3,0x50000000|3,0x40000001,addresses[1]|0x1000000000000003):
            root,addresses,pages,_=tables(site);pages[root][0]=descriptor
            with self.subTest(descriptor=descriptor),self.assertRaises(ValueError):
                verify_table_closure(root,addresses,pages,site)

    def test_no_extra_user_alias_foreign_rights_or_virtio_after_io(self):
        site=self.prove()
        for va,descriptor in ((0x0a000000,0x0a000000|0x60000000000607),
                              (0x12010000,site['objects']['code']['start']|0x60000000000743),
                              (0x10000000,site['objects']['code']['start']|0x20000000000743),
                              (0x11000000,site['objects']['user_stack']['start']|0x20000000000743),
                              (BASE,BASE|0x40000000000743)):
            root,addresses,pages,insert=tables(site,True);insert(va,descriptor)
            with self.subTest(va=va,descriptor=descriptor),self.assertRaises(ValueError):
                verify_table_closure(root,addresses,pages,site,True)


if __name__=='__main__': unittest.main()
