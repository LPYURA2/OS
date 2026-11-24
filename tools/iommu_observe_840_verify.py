#!/usr/bin/env python3
"""Independent ordered replay of live S840 table reads, bound to S837 and QMP."""
import argparse,json,re
from pathlib import Path
from iommu_guest_837_verify import run as parent
from test import ownership_report
from uefi_map_826 import parse
from cache_800_verify import model
from ecam_quarantine_827_verify import need
POLICY='STYLEOS S840 POLICY target-read=no mappings-changed=no dma-grants=no production-cr3=deny'
COVERS={'disabled':(0,0),'intel':(0xfed90000,0xfed91000),'bridge-intel':(0xfed90000,0xfed91000),'bridge-two-hop-intel':(0xfed90000,0xfed91000),'amd':(0xfed80000,0xfed84000)}
def replay(raw,efi,mode):
    lines=raw.decode('ascii').splitlines()
    obs=[s for s in lines if s.startswith('STYLEOS S840 ')]
    need(0<len(obs)<1000,'S840 records')
    i=lines.index(obs[0]);need(lines[i:i+len(obs)]==obs and i==lines.index('STYLEOS S837 POLICY detached=yes allocator-integrated=no')+1 and i+len(obs)<lines.index('STYLEOS S823 PASS'),'S840 placement')
    owner=ownership_report(raw,'x86_64');state=owner['state_before'];src=owner['source_data']
    maps=parse(efi,src['stride'],src['version'])
    known={w['pa'] for w in owner['walks'] if w['kind']=='TABLE' and w['pa']==w['va']}
    baseline=[];candidate=[]
    for line in lines:
        m=re.fullmatch(r'STYLEOS MEMMAP (RESERVED|CANDIDATE) 0x([0-9a-f]{16}) 0x([0-9a-f]{16})',line)
        if m:(baseline if m[1]=='RESERVED' else candidate).append((int(m[2],16),int(m[3],16)))
    cursor=0
    def take(tag,*fields):
        nonlocal cursor
        need(cursor<len(obs),'S840 truncated')
        m=re.fullmatch('STYLEOS S840 '+tag+''.join(' '+k+'=0x([0-9a-f]{16})' for k in fields),obs[cursor])
        need(m is not None,'S840 schema '+tag);cursor+=1
        return tuple(int(x,16) for x in m.groups())
    root,pat,bits,cap,default,num=take('CPU','root','pat','bits','cap','def','count')
    need((root,pat,bits)==(state['root'],state['pat'],state['bits']) and 36<=bits<=52 and num==cap&255 and num<=16 and not state['mode']&(3<<29),'S840 CPU binding')
    need(all((pat>>(8*j))&255 in (0,1,4,5,6,7) for j in range(8)),'S840 PAT')
    pairs=[]
    for j in range(num):
        idx,b,m=take('MTRR','index','base','mask');need(idx==j,'MTRR order');pairs.append(dict(base=b,mask=m))
    seen={}
    def safe(pa):
        return bool(pa and pa%4096==0 and pa<0x8000000 and any(a<=pa and pa+4096<=b for a,b in baseline) and not any(a<pa+4096 and pa<b for a,b in candidate) and any(r['type']==4 and r['attributes']&8 and r['start']<=pa and pa+4096<=r['end_exclusive'] for r in maps))
    def read(tag,pa,slot):
        a,b,e=take(tag,'table','slot','entry');need((a,b)==(pa,slot),'S840 read path')
        if tag=='DENY':need(e==0,'denied value');return None
        # Accessed/dirty bits can change due to the diagnostic's own CPU reads.
        key=(pa,slot);need(key not in seen or (seen[key]^e)&~0x60==0,'S840 unstable entry');seen[key]=e
        return e
    def identity(pa,slot):
        return read('IDENTITY',pa,slot) if safe(pa) and pa in known else None
    mask=((1<<bits)-1)&~4095
    def walk(va,reader):
        pa=root&~4095
        if not pa or pa&~mask:return None
        for level in (4,3,2,1):
            slot=(va>>(12+9*(level-1)))&511;e=reader(pa,slot)
            if e is None:return None
            if not e&1:return (0,0,0,0)
            if level==4 and e&128:return None
            if level==1 or level in (2,3) and e&128:
                size=1<<(12+9*(level-1))
                if e&((size-1)&~4095&(~4096 if level>1 else -1)):return None
                idx=((e>>(7 if level==1 else 12))&1)*4+((e>>4)&1)*2+((e>>3)&1)
                return ((e&mask&~(size-1))+(va&(size-1)),e,size,(pat>>(8*idx))&255)
            pa=e&mask
            if not pa or e&128:return None
        return None
    def target(pa,slot):
        admitted=False
        if safe(pa):
            a=walk(pa,identity)
            if a is not None:
                b=walk(pa+4095,identity)
                admitted=b is not None and a[0]==pa and b[0]==pa+4095 and a[1:]==b[1:] and a[3]==6
        return read('READ' if admitted else 'DENY',pa,slot)
    a,b=COVERS[mode];pages=[];valid=True
    covers=[s for s in lines if s.startswith('STYLEOS S835 COVER ')]
    need(covers==[f'STYLEOS S835 COVER start=0x{a:016x} end=0x{b:016x}'],'S840 cover')
    for va in range(a,b,4096):
        f=walk(va,target)
        if f is None:valid=False;break
        l=walk(va+4095,target)
        if l is None:valid=False;break
        if default&~0xcff or va>=1<<bits or any(p['base']&255 not in (0,1,4,5,6) for p in pairs):
            valid=False;break
        mt,matched,enabled=model(va,bits,cap,default,pairs)
        # Compare every page, but continue reading the whole cover before
        # deciding whether the final detached collector report is publishable.
        if f[0]!=va or l[0]!=va+4095 or f[1:]!=l[1:] or f[3] not in (0,6) or not enabled or mt!=0:valid=False
        pages.append((va,f[0],l[0],f[1],f[2],f[3],mt,matched))
    accepted,count=take('RESULT','accepted','pages')
    need((accepted,count)==(int(valid),len(pages) if valid else 0),'S840 collector result')
    if valid:
        for page in pages:need(take('PAGE','address','first','last','leaf','size','pat','mtrr','matches')==page,'S840 page report')
    need(obs[cursor:]==[POLICY],'S840 policy/trailing records')
    return {'status':'OBSERVED' if valid and a!=b else 'NO_APERTURE' if valid else 'UNSUPPORTED','pages':count,'target_mmio_read':False,'production_cr3_authorized':False}
def run(folder):
    meta=json.loads((folder/'results.json').read_text())
    manifest=json.loads((folder/'build/manifest.json').read_text())
    need(meta.get('guest_observation_840') is True and manifest.get('test_guest_observation_840') is True,'S840 profile')
    prior=parent(folder);modes={}
    for mode,row in prior['modes'].items():
        if row.get('status')=='SKIPPED':modes[mode]=row;continue
        p=folder/mode
        modes[mode]=replay((p/'serial.log').read_bytes(),(p/'final-map.bin').read_bytes(),mode)
    need(set(modes)==set(COVERS) and modes['disabled']['status']=='NO_APERTURE','S840 mode coverage')
    # A successful diagnostic still distinguishes unavailable mappings from observations.
    need(any(r['status']=='OBSERVED' for r in modes.values()),'S840 no nonempty hardware observation')
    return {'stage':840,'status':'PASS','modes':modes,'foundation_complete':False}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--input',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    r=run(a.input);a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
