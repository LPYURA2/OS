"""Architecture-neutral host validator for the current x86-64 TEA preview ELF.

Structural checks are not code authentication, code safety or a native loader.
"""
import struct


class ELFPolicyError(ValueError):
    pass


def verify_elf64_x86(data):
    if len(data) < 120 or data[:7] != b'\x7fELF\x02\x01\x01':
        raise ELFPolicyError('invalid ELF64 little-endian header')
    try:
        typ, machine, version = struct.unpack_from('<HHI',data,16)
        entry, phoff, _, _, ehsize, phentsz, phnum = struct.unpack_from('<QQQIHHH',data,24)
    except struct.error as exc:
        raise ELFPolicyError('truncated ELF header') from exc
    if (typ,machine,version) != (2,62,1):
        raise ELFPolicyError('unsupported ELF type/machine')
    if (ehsize,phentsz)!=(64,56) or not 1 <= phnum <= 16 or phoff < 64:
        raise ELFPolicyError('invalid program header geometry')
    if phoff + phentsz * phnum > len(data):
        raise ELFPolicyError('program header outside ELF')
    entry_mapped=False
    for i in range(phnum):
        try:
            kind,flags,off,vaddr,paddr,filesz,memsz,align=struct.unpack_from('<IIQQQQQQ',data,phoff+i*phentsz)
        except struct.error as exc:
            raise ELFPolicyError('corrupt segment') from exc
        if kind in (2,3):
            raise ELFPolicyError('dynamic linking/interpreter unsupported')
        if kind == 0x6474e551 and flags & 1:
            raise ELFPolicyError('executable stack forbidden')
        if kind!=1: continue
        if filesz > memsz or off > len(data) or filesz > len(data)-off:
            raise ELFPolicyError('segment exceeds payload')
        if vaddr > (1<<64)-1 - memsz:
            raise ELFPolicyError('segment address overflow')
        if flags & 2 and flags & 1:
            raise ELFPolicyError('writable executable segment')
        if align not in (0,1) and (align & (align-1) or vaddr % align != off % align):
            raise ELFPolicyError('invalid alignment')
        if flags & 1 and vaddr <= entry < vaddr+filesz:
            entry_mapped=True
    if not entry_mapped:
        raise ELFPolicyError('entry outside executable segment')
    return entry
