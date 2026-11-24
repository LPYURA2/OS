#!/usr/bin/env python3
"""Replay every S841 table path independently, never infer absence from a hole."""
import argparse,json,re
from pathlib import Path
from iommu_observe_840_verify import run as parent,COVERS,POLICY as PARENT_POLICY
from ecam_quarantine_827_verify import need
from test import ownership_report
from uefi_map_826 import parse
from cache_800_verify import model
POLICY='STYLEOS S841 POLICY target-read=no mappings-changed=no dma-grants=no production-cr3=deny'
def replay(raw,efi,mode):
    lines=raw.decode('ascii').splitlines()
    obs=[s for s in lines if s.startswith('STYLEOS S841 ')]
    need(0<len(obs)<40000,'S841 record bound')
    i=lines.index(obs[0])
    need(lines[i:i+len(obs)]==obs and i==lines.index(PARENT_POLICY)+1 and i+len(obs)<lines.index('STYLEOS S823 PASS'),'S841 placement')
    owner=ownership_report(raw,'x86_64');src=owner['source_data'];maps=parse(efi,src['stride'],src['version'])
    def fields(line,tag,*keys):
        m=re.fullmatch('STYLEOS '+tag+''.join(' '+k+'=0x([0-9a-f]{16})' for k in keys),line)
        need(m is not None,'S841 schema '+tag);return tuple(int(v,16) for v in m.groups())
    root,pat,bits,cap,default,n=fields(next(x for x in lines if x.startswith('STYLEOS S840 CPU ')),'S840 CPU','root','pat','bits','cap','def','count')
    pairs=[]
    for x in [x for x in lines if x.startswith('STYLEOS S840 MTRR ')]:
        idx,b,m=fields(x,'S840 MTRR','index','base','mask')
        need(idx==len(pairs),'S841 MTRR order');pairs.append(dict(base=b,mask=m))
    need(len(pairs)==n,'S841 MTRR count')
    baseline=[];candidate=[]
    for line in lines:
        m=re.fullmatch(r'STYLEOS MEMMAP (RESERVED|CANDIDATE) 0x([0-9a-f]{16}) 0x([0-9a-f]{16})',line)
        if m:(baseline if m[1]=='RESERVED' else candidate).append((int(m[2],16),int(m[3],16)))
    known={w['pa'] for w in owner['walks'] if w['kind']=='TABLE' and w['pa']==w['va']}
    seen={}
    def record(pa,slot,e):
        key=(pa,slot);need(key not in seen or not (seen[key]^e)&~0x60,'S841 inconsistent table entry')
        seen[key]=e
    for w in owner['walks']:
        for s in w['steps']:
            record(s['table'],(w['va']>>(39-9*s['level']))&511,s['entry'])
    for x in lines:
        if x.startswith(('STYLEOS S840 READ ','STYLEOS S840 IDENTITY ')):
            pa,slot,e=fields(x,'S840 '+x.split()[2],'table','slot','entry');record(pa,slot,e)
    proof=dict(seen);mask=((1<<bits)-1)&~4095
    def safe(pa):
        if not(pa and pa%4096==0 and pa<0x8000000 and any(a<=pa and pa+4096<=b for a,b in baseline) and not any(a<pa+4096 and pa<b for a,b in candidate) and any(r['type']==4 and r['attributes']&8 and r['start']<=pa and pa+4096<=r['end_exclusive'] for r in maps)):return False
        return all((lambda t:t[0]==6 and t[2])(model(x,bits,cap,default,pairs)) for x in (pa,pa+4095))
    def identity(va):
        pa=root&~4095
        for level in (4,3,2,1):
            if pa not in known or not safe(pa):return None
            slot=(va>>(12+9*(level-1)))&511;e=proof.get((pa,slot))
            if e is None or not e&1 or level==4 and e&128:return None
            if level==1 or e&128:
                size=1<<(12+9*(level-1));idx=((e>>(7 if level==1 else 12))&1)*4+((e>>4)&1)*2+((e>>3)&1)
                return ((e&mask&~(size-1))+(va&(size-1)),e&~0x60,size,(pat>>(8*idx))&255)
            pa=e&mask
        return None
    def admitted(pa):
        if not safe(pa):return False
        f,l=identity(pa),identity(pa+4095)
        return f is not None and l is not None and f[0]==pa and l[0]==pa+4095 and f[1:]==l[1:] and f[3]==6
    cursor=visited=denied=0;aliases=[];leaves=0
    a,b=COVERS[mode]
    def take(tag,*keys):
        nonlocal cursor
        need(cursor<len(obs),'S841 truncated');v=fields(obs[cursor],'S841 '+tag,*keys);cursor+=1;return v
    def scan(pa,prefix,level):
        nonlocal visited,denied,leaves
        ok=visited<64 and admitted(pa)
        need(take('PATH','pa','prefix','level','admitted')==(pa,prefix,level,int(ok)),'S841 path admission')
        if not ok:denied+=1;return
        visited+=1;entries=[]
        for i in range(0,512,16):
            row=take('ROW','slot',*(['e']*16));need(row[0]==i,'S841 row order');entries.extend(row[1:])
            for j,e in enumerate(row[1:]):record(pa,i+j,e)
        for slot,e in enumerate(entries):
            if not e&1:continue
            need(not e&(((1<<52)-1)&~(mask|4095)) and not(level==4 and e&128),'S841 invalid physical bits')
            va=prefix|(slot<<(12+9*(level-1)))
            if level==1 or e&128:
                size=1<<(12+9*(level-1));need(level==1 or not e&(size-1)&~0x1fff,'S841 leaf alignment')
                lo=e&mask&~(size-1);hi=lo+size;leaves+=1
                if a<b and lo<b and a<hi:
                    start=max(a,lo);end=min(b,hi);v=va+start-lo
                    if v&(1<<47):v|=0xffff000000000000
                    idx=((e>>(7 if level==1 else 12))&1)*4+((e>>4)&1)*2+((e>>3)&1)
                    aliases.append(dict(physical=start,end=end,virtual=v,pat=(pat>>(8*idx))&255,identity=v==start))
            else:scan(e&mask,va,level-1)
    scan(root&~4095,0,4)
    need(take('TOTAL','paths','denied')==(visited,denied) and obs[cursor:]==[POLICY],'S841 totals/policy')
    need(visited>0,'S841 no root evidence')
    if a<b:need(any(x['identity'] and x['physical']==a and x['end']==b for x in aliases) or denied>0,'S841 expected identity or unresolved path')
    return dict(status='COMPLETE_INHERITED_ROOT' if not denied else 'PARTIAL',paths=visited,denied=denied,leaves=leaves,aliases=aliases,production_cr3_authorized=False)
def run(folder):
    prior=parent(folder);meta=json.loads((folder/'results.json').read_text());manifest=json.loads((folder/'build/manifest.json').read_text())
    need(meta.get('alias_census_841') is True and manifest.get('test_alias_census_841') is True,'S841 profile')
    modes={}
    for mode,row in prior['modes'].items():
        if row.get('status')=='SKIPPED':modes[mode]=row;continue
        p=folder/mode;modes[mode]=replay((p/'serial.log').read_bytes(),(p/'final-map.bin').read_bytes(),mode)
    return dict(stage=841,status='PASS',modes=modes,foundation_complete=False)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--input',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    r=run(a.input);a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
