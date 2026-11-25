"""Bind the INT3 return label to the real linked AMD64 PE image and map."""
import re
from fault_site import _range, _unpack


def bp_site(data, map_text, symbol="s851_bp_return"):
    if _range(data, 0, 64, 'DOS header')[:2] != b'MZ':
        raise ValueError('DOS magic')
    pe, = _unpack('<I', data, 60, 'PE pointer')
    if pe < 64 or _range(data, pe, 4, 'PE signature') != b'PE\0\0':
        raise ValueError('PE signature')
    machine, count, _, _, _, opt_size, _ = _unpack('<HHIIIHH', data, pe+4, 'COFF header')
    opt = _range(data, pe+24, opt_size, 'optional header')
    if machine != 0x8664 or not 0 < count <= 96 or opt_size < 112 or _unpack('<H', opt, 0, 'magic')[0] != 0x20b:
        raise ValueError('AMD64 PE32+ required')
    base, = _unpack('<Q', opt, 24, 'image base')
    size, headers = _unpack('<II', opt, 56, 'image size')
    start = pe+24+opt_size
    if not size or headers < start+40*count or headers > len(data):
        raise ValueError('header bounds')
    _range(data, start, count*40, 'section headers')
    if '\0' in map_text:
        raise ValueError('map NUL')
    bases = re.findall(r'^\s*Preferred load address is ([0-9a-fA-F]{16})\s*$', map_text, re.M)
    rows = [line for line in map_text.splitlines() if symbol in line.split()]
    if len(bases) != 1 or int(bases[0], 16) != base or len(rows) != 1:
        raise ValueError('map provenance')
    m = re.fullmatch(r'\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+'+re.escape(symbol)+r'\s+([0-9a-fA-F]{16})\s+boot_x86_64_desktop_preflight_850\.o\s*', rows[0])
    if not m:
        raise ValueError('map symbol row')
    seg, rel, absolute = (int(v,16) for v in m.groups())
    rva = absolute-base
    if not 0 < rva < size or not 1 <= seg <= count or rel < 1:
        raise ValueError('symbol bounds')
    matches=[]
    for i in range(count):
        name, vs, va, rs, ro, _, _, _, _, flags = _unpack('<8sIIIIIIHHI', data,start+40*i,'section')
        if va+(vs or rs)>size or (rs and ro<headers):
            raise ValueError('section bounds')
        if rs: _range(data,ro,rs,'section bytes')
        if va <= rva < va+(vs or rs): matches.append((i+1,name,va,rs,ro,flags))
    if len(matches)!=1:
        raise ValueError('symbol section ambiguity')
    number,name,va,rs,ro,flags=matches[0]
    if number!=seg or rva-va!=rel or rel>=rs or name.rstrip(b'\0')!=b'.text' or not flags&0x20000000 or flags&0x80000000:
        raise ValueError('symbol must be in executable non-writable section')
    if _range(data,ro+rel-1,1,'INT3')!=b'\xcc':
        raise ValueError('return symbol does not immediately follow INT3')
    return {'symbol':symbol,'rva':rva,'preferred_base':base,'int3_offset':ro+rel-1}
