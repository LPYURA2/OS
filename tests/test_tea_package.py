import pathlib
import struct
import sys
import unittest
import zipfile

ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from tea_package import verify_elf64_x86, ELFPolicyError

class ElfBoundaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with zipfile.ZipFile(ROOT/'build/756/Documents.tea') as z:
            cls.data=z.read('payload.elf')

    def test_genuine_freestanding_elf(self):
        self.assertEqual(verify_elf64_x86(self.data),0x10010000)

    def test_malformed_magic(self):
        raw=bytearray(self.data);raw[0]=0
        with self.assertRaises(ELFPolicyError):verify_elf64_x86(raw)

    def test_wrong_cpu(self):
        raw=bytearray(self.data);struct.pack_into('<H',raw,18,183)
        with self.assertRaisesRegex(ELFPolicyError,'machine'):verify_elf64_x86(raw)

    def test_entry_not_executable(self):
        raw=bytearray(self.data);struct.pack_into('<Q',raw,24,0)
        with self.assertRaisesRegex(ELFPolicyError,'entry'):verify_elf64_x86(raw)

    def test_writable_code_rejected(self):
        raw=bytearray(self.data);phoff=struct.unpack_from('<Q',raw,32)[0];phnum=struct.unpack_from('<H',raw,56)[0]
        for i in range(phnum):
            row=phoff+i*56
            kind,flags=struct.unpack_from('<II',raw,row)
            if kind==1 and flags&1:
                struct.pack_into('<I',raw,row+4,flags|2)
                break
        else:self.fail('ELF missing executable segment')
        with self.assertRaisesRegex(ELFPolicyError,'writable executable'):verify_elf64_x86(raw)

    def test_dynamic_interpreter_rejected(self):
        raw=bytearray(self.data);phoff=struct.unpack_from('<Q',raw,32)[0]
        struct.pack_into('<I',raw,phoff,3)
        with self.assertRaises(ELFPolicyError):verify_elf64_x86(raw)

if __name__=='__main__': unittest.main()
