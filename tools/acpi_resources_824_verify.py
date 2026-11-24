"""S824 bounded diagnostic decoder, not a resource allocator or DMA grant.
Replay S823 and reconcile captured, same-boot byte streams. No hardware access.
"""
import re
import hashlib
from acpica_firmware_823_verify import verify as parent
H = r'0x([0-9a-f]{16})'
POLICY = ('STYLEOS S824 POLICY observation-only=yes ecam=not-read '
          'dma-grants=no production-cr3=deny')
MAX64 = (1 << 64) - 1


def require(ok, why):
    if not ok:
        raise ValueError(why)


def le(b):
    return int.from_bytes(b, 'little')


def resources(raw, uefi=False):
    """Strict supported subset. Return descriptors, including empty ranges.
    Do not turn movable, translated or consumer descriptors into allocations.
    UEFI granularity has protocol-specific semantics; retain rather than infer.
    """
    require(2 <= len(raw) <= (1200 if uefi else 1024), 'resource size')
    out = []
    pos = 0
    while pos < len(raw):
        tag = raw[pos]
        if tag == 0x79:
            require(pos + 2 == len(raw), 'end tag extent')
            require(raw[pos+1] == 0 or sum(raw) % 256 == 0, 'end checksum')
            return out
        require(len(out) < 24 if uefi else len(out) < 64, 'too many descriptors')
        if tag & 0x80:
            require(pos + 3 <= len(raw), 'truncated header')
            size = le(raw[pos+1:pos+3]); start = pos + 3
        else:
            size = tag & 7; start = pos + 1
        require(start + size <= len(raw), 'truncated descriptor')
        b = raw[start:start+size]; pos = start+size
        if tag in (0x88, 0x87, 0x8a):
            width = {0x88: 2, 0x87: 4, 0x8a: 8}[tag]
            require((not uefi or tag == 0x8a) and size == 3+5*width, 'address descriptor shape')
            kind, flags, specific = b[:3]
            require(kind in (0, 1, 2) and flags & 0xf0 == 0, 'address flags/type')
            gran, lo, hi, trans, length = [le(b[3+i*width:3+(i+1)*width]) for i in range(5)]
            fixed = bool(uefi or flags & 0x0c == 0x0c)
            require(lo <= hi, 'inverted address limits')
            if length:
                require(length-1 <= hi-lo, 'address length')
                require(not fixed or length-1 == hi-lo or uefi, 'fixed range length')
            if kind == 2:
                require(not trans and 0 <= lo <= hi <= 255 and length > 0 and fixed, 'bus descriptor')
                require(uefi or length-1 == hi-lo, 'bus length')
            out.append(dict(tag=tag,kind=kind,flags=flags,specific=specific,
                            gran=gran,lo=lo,hi=hi,translation=trans,length=length,
                            fixed=fixed))
        elif not uefi and tag in (0x47, 0x4b):
            require(size == (7 if tag == 0x47 else 3), 'io descriptor shape')
            if tag == 0x47:
                lo,hi,length=le(b[1:3]),le(b[3:5]),b[6]
            else:
                lo=hi=le(b[:2]); length=b[2]
            require(lo<=hi and length and hi+length-1<=0xffff, 'small io extent')
            out.append(dict(tag=tag,kind=1,lo=lo,hi=hi+length-1,length=length,
                            translation=0,fixed=lo==hi,flags=None))
        elif not uefi and tag == 0x86:
            require(size==9, 'fixed memory descriptor shape')
            lo,length=le(b[1:5]),le(b[5:9]);hi=lo+length-1
            require(length and hi<=0xffffffff, 'fixed memory extent')
            out.append(dict(tag=tag,kind=0,lo=lo,hi=hi,length=length,
                            translation=0,fixed=True,flags=None))
        else:
            raise ValueError('unsupported resource descriptor '+hex(tag))
    raise ValueError('missing end tag')


def bus_set(desc):
    result=set()
    for v in desc:
        if v['kind']!=2: continue
        part=set(range(v['lo'],v['lo']+v['length']))
        require(not result & part and part and max(part)<=255, 'bus overlap')
        result.update(part)
    require(result,'no bus ranges')
    return result


def mcfg(raw):
    require(60<=len(raw)<=556 and raw[:4]==b'MCFG' and le(raw[4:8])==len(raw)
            and sum(raw)%256==0 and raw[36:44]==bytes(8) and (len(raw)-44)%16==0,'MCFG header')
    windows=[]; buses=set()
    for p in range(44,len(raw),16):
        e=raw[p:p+16];base=le(e[:8]);seg=le(e[8:10]);lo,hi=e[10:12]
        start=base+(lo<<20);end=base+((hi+1)<<20)
        require(base and base%(1<<20)==0 and lo<=hi and end<=MAX64 and e[12:]==bytes(4),'MCFG window')
        for w in windows:
            require(not (start<w['end'] and w['start']<end),'MCFG physical overlap')
            require(not (seg==w['segment'] and lo<=w['last'] and w['first']<=hi),'MCFG bus overlap')
        windows.append(dict(segment=seg,first=lo,last=hi,start=start,end=end))
        if seg==0: buses.update(range(lo,hi+1))
    return windows,buses


def data(line, prefix, limit):
    m=re.fullmatch(prefix+r'([0-9a-f]+)',line)
    require(m and len(m[1])%2==0 and len(m[1])<=2*limit,'record shape')
    return bytes.fromhex(m[1])


def verify(raw, rc, mode):
    ok,d=parent(raw,rc,mode)
    if not ok: return False,{}
    try:
        lines=raw.decode('ascii').replace('\r','').splitlines()
        rec=[x for x in lines if x.startswith('STYLEOS S824 ')]
        require(len(rec)==d['uefi_roots']+4 and rec[-1]==POLICY,'record count/policy')
        crs=data(rec[0],r'STYLEOS S824 CRS data=',1024)
        table=data(rec[1],r'STYLEOS S824 MCFG data=',4096)
        require(len(crs)==d['acpica823_crs_bytes'],'CRS length differs from parent')
        require(lines.index('STYLEOS S821 PASS')<lines.index(rec[0])<lines.index('STYLEOS S823 PASS')<lines.index(rec[1])<lines.index(rec[-1])<next(i for i,x in enumerate(lines) if x.startswith('STYLEOS S816 PLATFORM')),'record ordering')
        decoded=resources(crs); aml_buses=bus_set(decoded)
        windows,mcfg_buses=mcfg(table)
        uefi=[]; all_roots=[]; uefi_buses=None; raw_files={'crs.bin':crs,'mcfg.bin':table}
        for idx,line in enumerate(rec[2:-2]):
            m=re.fullmatch(r'STYLEOS S824 UEFI index='+H+r' segment='+H+r' data=([0-9a-f]+)',line)
            require(m and int(m[1],16)==idx and int(m[2],16)<=65535 and len(m[3])%2==0 and len(m[3])<=2400,'UEFI record')
            seg=int(m[2],16); blob=bytes.fromhex(m[3]); desc=resources(blob,True); bset=bus_set(desc)
            require(all(seg!=s or not bset & b for s,b in all_roots),'UEFI overlap')
            all_roots.append((seg,bset));uefi.append(dict(segment=seg,descriptors=desc))
            raw_files[f'uefi-root-{idx}.bin']=blob
            if seg==0:
                require(uefi_buses is None,'multiple segment zero roots');uefi_buses=bset
        bitmap=data(rec[-2],r'STYLEOS S824 BUSES data=',32)
        require(len(bitmap)==32,'bus bitmap length')
        live={i for i in range(256) if bitmap[i//8] & (1<<(i%8))}
        require(0 in live and len(live)==d['buses'] and uefi_buses is not None,'live bus count')
        require(live<=aml_buses and live<=uefi_buses and live<=mcfg_buses,'live bus coverage')
        require(0 in aml_buses,'PCI0 root')
        # Cross-check exact MCFG root base against the independently computed S816 record.
        old=next(x for x in lines if x.startswith('STYLEOS S816 MCFG '))
        base=int(re.search(r'root-base='+H,old)[1],16)
        require(sum(w['segment']==0 and w['first']==0 and w['start']==base for w in windows)==1,'MCFG root mismatch')
        # Diagnostic conflicts only: compare fixed, untranslated memory intervals.
        # No claims about translated/movable/consumer descriptors or reserved ECAM.
        conflicts=[]; unresolved=0
        for source,descs in [('aml',decoded)]+[(f'uefi{i}',r['descriptors']) for i,r in enumerate(uefi)]:
            candidates=[]
            for i,v in enumerate(descs):
                if v['kind']!=0 or not v['length']: continue
                if not v['fixed'] or v['translation'] or (source=='aml' and (v['flags'] is None or v['flags']&1)):
                    unresolved+=1;continue
                lo=v['lo'];end=lo+v['length']
                for j,a,b in candidates:
                    if lo<b and a<end: conflicts.append([source,j,i,'overlap'])
                candidates.append((i,lo,end))
                for w in windows:
                    if lo<w['end'] and w['start']<end: conflicts.append([source,i,'ecam',w['segment']])
        raw_files['live-buses.bin']=bitmap
        d.update(s824_live_buses=sorted(live),s824_aml_buses=sorted(aml_buses),
                 s824_uefi_buses=sorted(uefi_buses),s824_mcfg_buses=sorted(mcfg_buses),
                 s824_crs_descriptors=decoded,s824_uefi_roots=uefi,s824_mcfg_windows=windows,
                 s824_resource_conflicts=conflicts,s824_unresolved_memory_descriptors=unresolved,
                 s824_seg_default=not d['acpica823_firmware_seg_present'],
                 s824_bbn_default=not d['acpica823_firmware_bbn_present'],
                 s824_live_bus_coverage=True,s824_resource_ownership=False,
                 s824_raw_sha256={k:hashlib.sha256(v).hexdigest() for k,v in raw_files.items()},
                 _s824_raw=raw_files)
        return True,d
    except (ValueError, TypeError, IndexError, KeyError, StopIteration):
        return False,{}
