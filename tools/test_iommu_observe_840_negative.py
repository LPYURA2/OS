#!/usr/bin/env python3
"""Synthetic S840 records on an independently verified original S837 fixture.
Not a hardware run; exercise ordered replay and cross-stage binding only."""
import argparse,re
from pathlib import Path
from iommu_guest_837_verify import run as verify837
from iommu_observe_840_verify import replay,POLICY
from test import ownership_report
from uefi_map_826 import parse
from ecam_quarantine_827_verify import need

def test(folder):
    report=verify837(folder)
    need(report['status']=='PASS','parent')
    mode='intel';p=folder/mode
    raw=(p/'serial.log').read_bytes();efi=(p/'final-map.bin').read_bytes()
    lines=raw.decode('ascii').splitlines()
    owner=ownership_report(raw,'x86_64');s=owner['state_before'];src=owner['source_data']
    maps=parse(efi,src['stride'],src['version'])
    table={}
    for w in owner['walks']:
        for step in w['steps']:
            table[step['table'],(w['va']>>(39-9*step['level']))&511]=step['entry']
    root=s['root'];bits=s['bits'];pat=s['pat'];mask=((1<<bits)-1)&~4095
    pdpt=table[root,0]&mask
    # Use one known low RAM table to make a bounded synthetic target leaf.
    # No device memory is touched and none of these records are hardware evidence.
    child=table[pdpt,0]&mask
    need((pdpt,3) not in table,'fixture PDPT slot already sampled')
    table[pdpt,3]=child|3
    a=0xfed90000;slot=(a>>21)&511
    need((child,slot) not in table,'fixture PD slot already sampled')
    table[child,slot]=(a&~((1<<21)-1))|0x83
    need((pat&255)==6,'fixture WB PAT')
    def line(tag,**fields):return 'STYLEOS S840 '+tag+''.join(f' {k}=0x{v:016x}' for k,v in fields.items())
    obs=[line('CPU',root=root,pat=pat,bits=bits,cap=1,**{'def':0x806},count=1),
         line('MTRR',index=0,base=0x80000000,mask=(mask&~((1<<31)-1))|0x800)]
    def walk(va,reader):
        pa=root
        for level in (4,3,2,1):
            k=(va>>(12+9*(level-1)))&511;e=reader(pa,k)
            if not e&1:return (0,0,0,0)
            if level==1 or level in (2,3) and e&128:
                size=1<<(12+9*(level-1));idx=((e>>(7 if level==1 else 12))&1)*4+((e>>4)&1)*2+((e>>3)&1)
                return ((e&mask&~(size-1))+(va&(size-1)),e,size,(pat>>(8*idx))&255)
            pa=e&mask
        raise AssertionError('walk')
    def record(tag,pa,k):
        e=table[pa,k];obs.append(line(tag,table=pa,slot=k,entry=e));return e
    def target(pa,k):
        f=walk(pa,lambda p,i:record('IDENTITY',p,i));l=walk(pa+4095,lambda p,i:record('IDENTITY',p,i))
        need(f[0]==pa and l[0]==pa+4095 and f[1:]==l[1:] and f[3]==6,'synthetic table identity')
        return record('READ',pa,k)
    f=walk(a,target);l=walk(a+4095,target)
    obs += [line('RESULT',accepted=1,pages=1),line('PAGE',address=a,first=f[0],last=l[0],leaf=f[1],size=f[2],pat=f[3],mtrr=0,matches=1),POLICY]
    anchor=lines.index('STYLEOS S837 POLICY detached=yes allocator-integrated=no')+1
    def packet(o):return ('\n'.join(lines[:anchor]+o+lines[anchor:])+'\n').encode('ascii')
    need(replay(packet(obs),efi,mode)['status']=='OBSERVED','synthetic positive')
    mutations=[]
    for tag,field in [('CPU','root'),('CPU','bits'),('MTRR','index'),('IDENTITY','entry'),('IDENTITY','slot'),('READ','table'),('RESULT','accepted'),('RESULT','pages'),('PAGE','first'),('PAGE','mtrr')]:
        k=next(i for i,x in enumerate(obs) if x.startswith('STYLEOS S840 '+tag+' '))
        bad=obs.copy()
        bad[k]=re.sub(r' '+field+r'=0x([0-9a-f]{16})',lambda m:f' {field}=0x{(int(m[1],16)^0x100):016x}',bad[k],count=1)
        need(bad!=obs,'mutation');mutations.append(bad)
    mutations += [obs[:-1],obs[:2]+obs[3:],obs+[POLICY],obs[:-1]+[POLICY.replace('dma-grants=no','dma-grants=yes')]]
    for i,bad in enumerate(mutations):
        try:replay(packet(bad),efi,mode)
        except (ValueError,KeyError,IndexError):continue
        raise AssertionError(f'forgery accepted: {i}')
    print(f'S840 synthetic host: 1 positive + {len(mutations)} refusals PASS; not guest evidence')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--input',type=Path,required=True);test(p.parse_args().input)
