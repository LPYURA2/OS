"""Decode every executable task-profile byte and reject non-integer state use.

This helper deliberately requires LLVM's real instruction decoder.  Compiler
options are recorded and checked as an additional constraint, never used as a
substitute for examining the linked image.  Callers supply already-validated
file-backed executable spans; the ARM Image's 60 data-header bytes are excluded
by the caller while its first branch remains in the proof.
"""
from __future__ import annotations

import hashlib
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess


X86_INTEGER = frozenset('''
adc add and bsf bsr bswap bt btc btr bts call clc cld cli clts cmc cmp
cmpxchg cpuid cqo cqto cwd cwde cdq cdqe dec div endbr64 enter hlt idiv imul
in inc ins int int3 invlpg iret iretq jmp jecxz jrcxz lahf lea leave lgdt lidt lldt lmsw lock
lods loop loope loopne lret ltr mov movabs movs movsx movsxd movzx mul neg nop not
or out outs pause pop popf popfq push pushf pushfq rcl rcr rdmsr rdtsc rdtscp
ret retf rol ror sahf sal sar sbb scas sgdt shl shld shr shrd sidt sldt smsw
stc std sti stos str sub syscall sysret test ud2 verr verw xadd xchg xlat
xor wrmsr
'''.split())
X86_PREFIXES = frozenset(('rep', 'repe', 'repz', 'repne', 'repnz', 'lock',
                          'data16', 'addr32', 'rex64', 'rex.w',
                          'cs', 'ds', 'es', 'fs', 'gs', 'ss'))
ARM_INTEGER = frozenset('''
adc adcs add adds adr adrp and ands asr b bfc bfi bfxil bic bics bl blr br
brk cbnz cbz ccmn ccmp cinc cinv cls clz cmn cmp cneg csel cset csetm csinc
csinv csneg dc dmb dsb eon eor eret extr hint hlt hvc ic isb ldadd ldadda
ldaddal ldaddl ldar ldarb ldarh ldaxp ldaxr ldaxrb ldaxrh ldnp ldp ldpsw ldr
ldrb ldrh ldrsb ldrsh ldrsw ldur ldurb ldurh ldursb ldursh ldursw ldxp ldxr
ldxrb ldxrh lsl lsr madd mneg mov movk movn movz mrs msr msub mul mvn neg
negs ngc ngcs nop orn orr prfm rbit ret rev rev16 rev32 ror sbc sbcs sbfiz
sbfm sbfx sdiv sev sevl smaddl smnegl smsubl smulh smull stlr stlrb stlrh
stlxp stlxr stlxrb stlxrh stnp stp str strb strh stur sturb sturh stxp stxr
stxrb stxrh sub subs svc sxtb sxth sxtw sys sysl tbnz tbz tlbi tst ubfiz ubfm
ubfx udf udiv umaddl umnegl umsubl umulh umull uxtb uxth wfe wfi yield
'''.split())
ARM_CONDITIONS = frozenset(('eq', 'ne', 'cs', 'hs', 'cc', 'lo', 'mi', 'pl',
                            'vs', 'vc', 'hi', 'ls', 'ge', 'lt', 'gt', 'le', 'al', 'nv'))


def _flags(arch, flags):
    if isinstance(flags, str):
        try:
            flags = shlex.split(flags)
        except ValueError as error:
            raise ValueError('malformed task compiler flags') from error
    if (not isinstance(flags, (tuple, list)) or not flags or
            any(not isinstance(flag, str) or not flag or '\x00' in flag for flag in flags)):
        raise ValueError('task compiler flags must be a nonempty string or list')
    flags = list(flags)
    required = ({'-mno-red-zone', '-mno-sse', '-mno-mmx', '-msoft-float'}
                if arch == 'x86_64' else {'-mgeneral-regs-only'})
    if not required.issubset(flags):
        raise ValueError('missing required integer-only task compiler flags')
    for flag in flags:
        if flag.startswith('@'):
            raise ValueError('opaque compiler response files cannot prove task flags')
        if arch == 'x86_64':
            # Positive feature overrides invalidate the claim even if an older
            # negative flag also appears.  The linked decoder is a second gate.
            if (flag in {'-mred-zone', '-mhard-float', '-mno-soft-float',
                         '-m80387', '-mfp-ret-in-387'} or
                    re.match(r'-m(?:sse|ssse|avx|mmx|3dnow|fma|f16c|xop)', flag) or
                    flag.startswith(('-march=', '-mcpu=', '-mfpmath=',
                                     '-Xclang', '-mllvm'))):
                raise ValueError(f'conflicting task compiler flag {flag}')
        elif (flag in {'-mno-general-regs-only', '-mhard-float'} or
              flag.startswith(('-march=', '-mcpu=', '-mfpu=', '-mfloat-abi=',
                               '-Xclang', '-mllvm'))):
            raise ValueError(f'conflicting task compiler flag {flag}')
    return flags


def _decoder():
    configured = os.environ.get('LLVM_OBJDUMP')
    if configured:
        decoder = shutil.which(configured)
        if not decoder:
            raise ValueError('LLVM_OBJDUMP does not name an executable LLVM objdump')
        return decoder
    for candidate in ('llvm-objdump', *(f'llvm-objdump-{n}' for n in range(22, 13, -1))):
        decoder = shutil.which(candidate)
        if decoder:
            return decoder
    raise ValueError('task proof requires llvm-objdump; set LLVM_OBJDUMP')


def _run(command):
    try:
        result = subprocess.run(command, check=False, capture_output=True,
                                text=True, timeout=30)
    except (OSError, subprocess.SubprocessError, UnicodeError) as error:
        raise ValueError(f'cannot run task instruction decoder: {error}') from error
    if result.returncode or result.stderr.strip():
        raise ValueError('LLVM task instruction decoder failed: ' +
                         (result.stderr.strip() or f'exit {result.returncode}'))
    return result.stdout


def _integer(arch, instruction, raw):
    instruction = instruction.strip().lower()
    # Symbol annotations and decoder comments describe addresses, not register
    # operands.  In particular ARM's '#' must remain: it introduces immediates.
    semantic = re.sub(r'<[^>]*>', '', instruction)
    semantic = semantic.split('//' if arch == 'aarch64' else '#', 1)[0]
    semantic = semantic.split(';', 1)[0]
    fields = semantic.split()
    if not fields:
        raise ValueError('decoded instruction is missing')
    mnemonic = fields[0]
    if arch == 'x86_64':
        while mnemonic in X86_PREFIXES:
            fields = fields[1:]
            if not fields:
                raise ValueError('decoded instruction contains only a prefix')
            mnemonic = fields[0]
        allowed = (mnemonic in X86_INTEGER or
                   (mnemonic[-1:] in 'bwlq' and mnemonic[:-1] in X86_INTEGER) or
                   re.fullmatch(r'mov(?:[sz][bw][wlq]|slq)', mnemonic) or
                   re.fullmatch(r'(?:j|set|cmov)(?:a|ae|b|be|c|e|g|ge|l|le|na|nae|nb|nbe|nc|ne|ng|nge|nl|nle|no|np|ns|nz|o|p|pe|po|s|z)[bwlq]?', mnemonic))
        if (not allowed or re.search(r'(?<![a-z0-9_])%?(?:(?:xmm|ymm|zmm|mm)\d+|st(?:\([0-7]\))?|mxcsr|xcr\d*)(?![a-z0-9_])', semantic)):
            raise ValueError(f'non-integer or unsupported x86 instruction: {instruction}')
    else:
        # LLVM preserves ELF $d mapping symbols even under -D.  The assembly
        # emits such mapping records for alignment holes in its vector table.
        # Admit only a complete, explicitly declared all-zero 32-bit word;
        # nonzero literal data never substitutes for instruction decoding.
        if re.fullmatch(r'\.word\s+(?:0x0+|0)', semantic.strip()) and raw == bytes(4):
            return 'zero-padding'
        allowed = mnemonic in ARM_INTEGER or (mnemonic.startswith('b.') and
                                               mnemonic[2:] in ARM_CONDITIONS)
        if (not allowed or re.search(r'(?<![a-z0-9_])(?:[vqdsbhzp][0-9]+(?:\.[0-9]+[bhsdq])?|za(?:[0-9]+[hv]?)?|zt[0-9]+|fpcr|fpsr|fpmr|svcr|(?:zcr|smcr)_el[123]|s3_3_c4_c4_[01]|s3_3_c4_c2_2)(?![a-z0-9_])', semantic)):
            raise ValueError(f'non-integer or unsupported AArch64 instruction: {instruction}')
        if mnemonic == 'udf' and raw != bytes(4):
            raise ValueError('only all-zero AArch64 padding may decode as udf')
    return 'instruction'


def _instructions(output, arch, start, expected):
    """Parse real decoder records and prove gap-free file-byte agreement."""
    result, cursor = {}, start
    for line in output.splitlines():
        match = re.match(r'^\s*([0-9a-fA-F]+):\s+(.*?)\s*$', line)
        if not match:
            continue
        address, fields = int(match[1], 16), match[2].split()
        if not fields:
            continue
        # LLVM uses four space-separated bytes or one printed little-endian
        # word for AArch64, depending on version.  x86 records use byte tokens.
        if arch == 'aarch64' and re.fullmatch(r'[0-9a-fA-F]{8}', fields[0]):
            raw = bytes.fromhex(fields.pop(0))
            native = expected[address - start:address - start + 4]
            if raw != native and raw[::-1] == native:
                raw = raw[::-1]
        else:
            parts = []
            maximum = 4 if arch == 'aarch64' else 15
            while fields and len(parts) < maximum and re.fullmatch(r'[0-9a-fA-F]{2}', fields[0]):
                parts.append(fields.pop(0))
            raw = bytes.fromhex(''.join(parts))
        if not raw or not fields:
            raise ValueError('unrecognized LLVM instruction record')
        if address != cursor or (arch == 'aarch64' and len(raw) != 4):
            raise ValueError('LLVM instruction coverage has a gap, overlap or wrong width')
        offset = address - start
        if offset < 0 or raw != expected[offset:offset + len(raw)]:
            raise ValueError('LLVM decoded bytes disagree with the linked executable')
        instruction = ' '.join(fields)
        kind = _integer(arch, instruction, raw)
        result[address] = {'bytes': raw.hex(), 'instruction': instruction, 'kind': kind}
        cursor += len(raw)
    if cursor != start + len(expected):
        raise ValueError('LLVM did not decode the complete executable span')
    return result


def verify_integer_code(arch, binary, spans, compiler_flags):
    """Return ``(proof, instructions_by_native_pc)`` or raise ``ValueError``.

    Each span has ``start``, ``end`` (native objdump addresses), file ``offset``
    and ``section``.  The caller proves that these spans cover every executable
    section and that executable entrypoints are instruction boundaries.
    """
    if arch not in ('x86_64', 'aarch64'):
        raise ValueError('unsupported task instruction architecture')
    flags = _flags(arch, compiler_flags)
    decoder = _decoder()
    version = _run([decoder, '--version']).strip()
    if not re.search(r'\bLLVM version \d+', version):
        raise ValueError('task instruction decoder is not LLVM objdump')
    binary = Path(binary)
    try:
        data = binary.read_bytes()
    except OSError as error:
        raise ValueError(f'cannot read task executable: {error}') from error
    if not isinstance(spans, list) or not spans:
        raise ValueError('task instruction proof has no executable spans')
    proof_spans, commands, instructions, previous = [], [], {}, None
    digest = hashlib.sha256()
    for span in spans:
        try:
            start, end, offset, section = (span[key] for key in ('start', 'end', 'offset', 'section'))
        except (KeyError, TypeError) as error:
            raise ValueError('malformed executable task span') from error
        if (any(type(value) is not int for value in (start, end, offset)) or
                not 0 <= start < end < 1 << 64 or offset < 0 or
                end - start > len(data) - offset or
                not isinstance(section, str) or not section or '\x00' in section or
                (previous is not None and start < previous) or
                (arch == 'aarch64' and (start % 4 or end % 4))):
            raise ValueError('invalid or overlapping executable task span')
        previous = end
        raw = data[offset:offset + end - start]
        command = [decoder, '-D', '--disassemble-zeroes', f'--section={section}',
                   f'--start-address={start}', f'--stop-address={end}', str(binary)]
        decoded = _instructions(_run(command), arch, start, raw)
        instructions.update(decoded)
        commands.append([Path(decoder).name, *command[1:-1], binary.name])
        proof_spans.append(dict(start=start, end=end, offset=offset, section=section,
                                bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
        digest.update(raw)
    return (dict(compiler_flags=flags, decoder_version=version,
                 decoder_commands=commands, executable_spans=proof_spans,
                 executable_sha256=digest.hexdigest(),
                 decoded_bytes=sum(span['bytes'] for span in proof_spans),
                 zero_padding_bytes=sum(len(bytes.fromhex(i['bytes'])) for i in instructions.values()
                                        if i['kind'] == 'zero-padding'),
                 instruction_count=sum(i['kind'] == 'instruction' for i in instructions.values())),
            instructions)
