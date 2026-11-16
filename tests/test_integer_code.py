"""Fail-closed task instruction proof, using controlled decoder output."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.integer_code import _flags, _instructions, verify_integer_code

X86_FLAGS = ['-mno-red-zone', '-mno-sse', '-mno-mmx', '-msoft-float']
ARM_FLAGS = ['-mgeneral-regs-only']


class IntegerCodeTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='styleos-integer-code-')
        self.addCleanup(temporary.cleanup)
        self.binary = Path(temporary.name) / 'kernel.elf'

    def prove(self, arch, raw, output, flags=None):
        self.binary.write_bytes(b'header' + raw)
        span = dict(start=0x1000, end=0x1000 + len(raw), offset=6, section='.text')
        complete = lambda text: subprocess.CompletedProcess([], 0, text, '')
        with patch('tools.integer_code._decoder', return_value='/tool/llvm-objdump'), \
                patch('tools.integer_code.subprocess.run', side_effect=[
                    complete('LLVM version 19.1.7\n'), complete(output)]) as run:
            result = verify_integer_code(arch, self.binary, [span], flags or
                                         (X86_FLAGS if arch == 'x86_64' else ARM_FLAGS))
            self.assertEqual(run.call_args_list[1].args[0][1:3],
                             ['-D', '--disassemble-zeroes'])
            return result

    def test_x86_complete_bytes_flags_hash_and_command(self):
        raw = bytes.fromhex('4889d8c3')
        proof, instructions = self.prove('x86_64', raw,
            '1000: 48 89 d8\tmovq %rbx, %rax\n1003: c3\tretq\n')
        self.assertEqual(proof['decoded_bytes'], 4)
        self.assertEqual(proof['instruction_count'], 2)
        self.assertEqual(proof['compiler_flags'], X86_FLAGS)
        self.assertEqual(proof['executable_sha256'], hashlib.sha256(raw).hexdigest())
        self.assertEqual(proof['decoder_commands'][0][-1], 'kernel.elf')
        self.assertEqual(instructions[0x1000]['bytes'], '4889d8')

    def test_arm_both_llvm_raw_formats_and_zero_padding(self):
        for output in ('1000: 1f 20 03 d5 nop\n1004: 00 00 00 00 udf #0\n',
                       '1000: d503201f nop\n1004: 00000000 udf #0\n'):
            with self.subTest(output=output):
                proof, instructions = self.prove('aarch64', bytes.fromhex('1f2003d500000000'), output)
                self.assertEqual(proof['decoded_bytes'], 8)
                self.assertEqual(instructions[0x1000]['bytes'], '1f2003d5')

    def test_x86_far_return_decoder_alias_is_integer_only(self):
        proof, instructions = self.prove('x86_64', bytes.fromhex('48cb'),
                                          '1000: 48 cb lretq\n')
        self.assertEqual(proof['instruction_count'], 1)
        self.assertEqual(instructions[0x1000]['bytes'], '48cb')

    def test_arm_declared_zero_words_are_recorded_as_padding(self):
        proof, instructions = self.prove('aarch64', bytes(4),
                                          '1000: 00000000 .word 0x00000000')
        self.assertEqual(proof['zero_padding_bytes'], 4)
        self.assertEqual(proof['instruction_count'], 0)
        self.assertEqual(instructions[0x1000]['kind'], 'zero-padding')

    def test_rejects_x86_fp_simd_mmx_and_state_operations(self):
        for instruction in ('movaps %xmm0, %xmm1', 'pxor %mm0, %mm0',
                            'fld1', 'fninit', 'emms', 'vzeroupper',
                            'fxsave (%rax)', 'ldmxcsr (%rax)',
                            'movq %mm0, %rax', 'xsave (%rax)'):
            with self.subTest(instruction=instruction), self.assertRaises(ValueError):
                self.prove('x86_64', b'\x90', '1000: 90 ' + instruction)

    def test_rejects_arm_fp_simd_sve_and_fp_control(self):
        for instruction in ('fadd d0, d0, d1', 'mov v0.16b, v1.16b',
                            'ldr q0, [x0]', 'ldr d0, [x0]', 'str s0, [x0]',
                            'add z0.s, p0/m, z0.s, z1.s',
                            'ldr za[w12, #0], [x0]',
                            'mrs x0, fpcr', 'msr fpsr, x0',
                            'msr svcr, x0', 'msr zcr_el1, x0',
                            'mrs x0, S3_3_C4_C4_0'):
            with self.subTest(instruction=instruction), self.assertRaises(ValueError):
                self.prove('aarch64', bytes.fromhex('1f2003d5'),
                           '1000: d503201f ' + instruction)

    def test_rejects_undefined_unknown_or_data_directives(self):
        for arch, raw, output in (
                ('x86_64', b'\x90', '1000: 90 <unknown>'),
                ('x86_64', b'\x90', '1000: 90 .byte 0x90'),
                ('aarch64', bytes.fromhex('01000000'), '1000: 00000001 udf #1'),
                ('aarch64', bytes.fromhex('1f2003d5'), '1000: d503201f .word 0xd503201f')):
            with self.subTest(output=output), self.assertRaises(ValueError):
                self.prove(arch, raw, output)

    def test_rejects_gap_overlap_skip_and_incorrect_raw_bytes(self):
        for output in ('1000: 90 nop\n',
                       '1001: 90 nop\n1002: 90 nop\n',
                       '1000: 90 nop\n1000: 90 nop\n',
                       '1000: 91 nop\n1001: 90 nop\n',
                       '1000: 90\n1001: 90 nop\n', ''):
            with self.subTest(output=output), self.assertRaises(ValueError):
                self.prove('x86_64', bytes.fromhex('9090'), output)

    def test_integer_aliases_prefixes_and_control_registers(self):
        for instruction in ('movzbl (%rax), %ecx', 'cmovneq %rcx, %rax',
                            'rep stosq %rax, %es:(%rdi)', 'nopw %cs:(%rax,%rax)',
                            'jmpq *%rax', 'movq %cr3, %rax', 'lidt (%rax)'):
            with self.subTest(instruction=instruction):
                _instructions('1000: 90 ' + instruction, 'x86_64', 0x1000, b'\x90')
        for instruction in ('mrs x0, sctlr_el1', 'msr cptr_el2, xzr',
                            'dc ivac, x0', 'b.eq 0x1000', 'ldp x19, x20, [x0]'):
            with self.subTest(instruction=instruction):
                _instructions('1000: d503201f ' + instruction, 'aarch64', 0x1000,
                              bytes.fromhex('1f2003d5'))

    def test_symbol_names_and_comments_are_not_register_operands(self):
        _instructions('1000: 90 jmp 0x1000 <xmm0> # %xmm1',
                      'x86_64', 0x1000, b'\x90')
        _instructions('1000: d503201f b 0x1000 <d0> // v0.16b',
                      'aarch64', 0x1000, bytes.fromhex('1f2003d5'))
        # Immediate syntax is retained when stripping decoder comments.
        _instructions('1000: d503201f add x0, x0, #0x1 // d0',
                      'aarch64', 0x1000, bytes.fromhex('1f2003d5'))

    def test_requires_arch_integer_flags(self):
        for arch, flags in (('x86_64', []), ('aarch64', []),
                            ('x86_64', X86_FLAGS[:-1]),
                            ('aarch64', ['-mno-outline-atomics']),
                            ('x86_64', None), ('x86_64', ['-mno-sse', 4])):
            with self.subTest(arch=arch, flags=flags), self.assertRaises(ValueError):
                _flags(arch, flags)
        self.assertEqual(_flags('x86_64', ' '.join(X86_FLAGS)), X86_FLAGS)

    def test_rejects_feature_overrides_even_with_required_flags(self):
        for flag in ('-mred-zone', '-msse2', '-mavx', '-mfma', '-mhard-float',
                     '-mno-soft-float', '-mfpmath=387', '@options.rsp',
                     '-march=native', '-mcpu=haswell', '-Xclang'):
            with self.subTest(flag=flag), self.assertRaises(ValueError):
                _flags('x86_64', X86_FLAGS + [flag])
        for flag in ('-mno-general-regs-only', '-march=armv8-a+simd', '-mcpu=cortex-a72'):
            with self.subTest(flag=flag), self.assertRaises(ValueError):
                _flags('aarch64', ARM_FLAGS + [flag])

    def test_missing_decoder_is_explicit_failure(self):
        self.binary.write_bytes(b'\x90')
        with patch.dict('os.environ', {}, clear=True), \
                patch('tools.integer_code.shutil.which', return_value=None), \
                self.assertRaisesRegex(ValueError, 'requires llvm-objdump'):
            verify_integer_code('x86_64', self.binary, [], X86_FLAGS)

    def test_wrong_decoder_version_and_stderr_fail_closed(self):
        self.binary.write_bytes(b'\x90')
        for result in (subprocess.CompletedProcess([], 0, 'GNU objdump 2.40', ''),
                       subprocess.CompletedProcess([], 0, 'LLVM version 19', 'warning'),
                       subprocess.CompletedProcess([], 1, '', 'failed')):
            with self.subTest(result=result), \
                    patch('tools.integer_code._decoder', return_value='objdump'), \
                    patch('tools.integer_code.subprocess.run', return_value=result), \
                    self.assertRaises(ValueError):
                verify_integer_code('x86_64', self.binary, [], X86_FLAGS)

    def test_invalid_or_overlapping_spans_fail_before_decode(self):
        self.binary.write_bytes(b'\x90')
        span = dict(start=0x1000, end=0x1001, offset=0, section='.text')
        for spans in ([], [dict(span, offset=1)], [dict(span, end=0x1000)],
                      [dict(span, section='')], [span, span]):
            with self.subTest(spans=spans), \
                    patch('tools.integer_code._decoder', return_value='llvm-objdump'), \
                    patch('tools.integer_code._run', side_effect=[
                        'LLVM version 19.1', '1000: 90 nop']), \
                    self.assertRaises(ValueError):
                verify_integer_code('x86_64', self.binary, spans, X86_FLAGS)


if __name__ == '__main__':
    unittest.main()
