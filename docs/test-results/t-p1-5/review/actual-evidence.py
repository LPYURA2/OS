from pathlib import Path
import json,re,sys
sys.path.insert(0,'/workspace/OS')
from tools.test import verified_ownership_site
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path('/workspace/OS/build/t-p1-5-integration')
folder=root/'test-results'
hexpat=r'0x([0-9a-f]{16})'
def fields(line): return {k:int(v,16) for k,v in re.findall(r' ([a-z0-9_]+)=0x([0-9a-f]{16})',line)}
def inside(rs,a,b): return any(x<=a<b<=y for x,y in rs)
rows=[]
for log in sorted(p for p in folder.glob('*-ownership*.log') if p.name.startswith(('qemu-','integration-'))):
 data=log.read_text(); lines=data.splitlines(); meta=json.loads(log.with_suffix('.json').read_text())
 assert meta['status']=='PASS' and meta['requested_stop'] and meta['settled'] and not meta['natural_exit']
 arch=meta['arch']; negative=meta['mode']=='ownership-invalid'
 binary=verified_ownership_site(root/('ownership-invalid' if negative else 'ownership'),arch,negative)
 assert meta['ownership_site']==binary
 mmap={k:[] for k in ('RAM','RESERVED','CANDIDATE')}
 for line in lines:
  m=re.fullmatch(r'STYLEOS MEMMAP (RAM|RESERVED|CANDIDATE) '+hexpat+' '+hexpat,line)
  if m: mmap[m[1]].append((int(m[2],16),int(m[3],16)))
 if negative:
  assert [x for x in lines if x.startswith('STYLEOS OWNERSHIP')]==['STYLEOS OWNERSHIP ERROR: required']
  assert meta['ownership_plan'] is None and meta['refusal']=='required'
  rows.append(dict(job=log.stem,negative=True,refusal='required',partial_plan=False))
  continue
 plan=meta['ownership_plan']; assert plan['arch']==arch and plan['granted']==0
 claimre=r'STYLEOS OWNERSHIP CLAIM kind=([A-Z_]+) proof=([A-Z_]+) va='+hexpat+' pa='+hexpat+' bytes='+hexpat
 claims=[]
 for line in lines:
  m=re.fullmatch(claimre,line)
  if m: claims.append((m[1],m[2],*[int(x,16) for x in m.groups()[2:]]))
 assert len(claims)==len(plan['claims'])
 protected=[];review=[]
 for line in lines:
  m=re.fullmatch(r'STYLEOS OWNERSHIP (PROTECTED|REVIEW) '+hexpat+' '+hexpat+'( reason=no-transfer)?',line)
  if m:
   a,b=int(m[2],16),int(m[3],16);assert a<b and (a|b)&4095==0
   if m[1]=='PROTECTED': assert not m[4];protected.append((a,b))
   else: assert m[4];review.append((a,b))
 assert protected==[(x['start'],x['end']) for x in plan['protected']]
 assert review==[(x['start'],x['end']) for x in plan['review']]
 assert all(x['reason']=='no-transfer' for x in plan['review'])
 # Event sweep across reported and independently derived expected membership.
 intervals=[mmap['RESERVED']+[(pa&~4095,(pa+size+4095)&~4095) for _,_,_,pa,size in claims],mmap['CANDIDATE'],protected,review]
 events={}
 for category,rs in enumerate(intervals):
  for a,b in rs:
   events.setdefault(a,[0]*4)[category]+=1;events.setdefault(b,[0]*4)[category]-=1
 active=[0]*4
 for coordinate,changes in sorted(events.items()):
  active=[a+b for a,b in zip(active,changes)]
  assert active[2]==bool(active[0]), (log.name,coordinate,active)
  assert active[3]==bool(active[1] and not active[0]),(log.name,coordinate,active)
 before=fields(next(x for x in lines if x.startswith('STYLEOS OWNERSHIP STATE ' ) and not x.startswith('STYLEOS OWNERSHIP STATE AFTER')))
 after=fields(next(x for x in lines if x.startswith('STYLEOS OWNERSHIP STATE AFTER')))
 assert before.keys()==after.keys() and all(before[k]==after[k] for k in before if k!='rflags')
 assert ((before['rflags']^after['rflags'])&~0x8d5)==0
 assert before['root']==(0 if arch=='aarch64' else before['root'])
 unresolved=fields(next(x for x in lines if x.startswith('STYLEOS OWNERSHIP UNRESOLVED')))['flags']
 assert unresolved==(3 if arch=='x86_64' else 0) and plan['unresolved']['flags']==unresolved
 if arch=='x86_64':
  assert before['root'] and not before['root']&4095
  tables={pa for k,_,_,pa,_ in claims if k=='OLD_TABLE'};assert before['root'] in tables
  tracekeys=set();entry_values={};i=0
  while i<len(lines):
   m=re.fullmatch(r'STYLEOS OWNERSHIP WALK kind=(GDT|TABLE) va='+hexpat+' pa='+hexpat+' levels='+hexpat,lines[i])
   if not m:i+=1;continue
   kind,va,pa,levels=m[1],*[int(x,16) for x in m.groups()[1:]]
   assert (kind,va,pa) not in tracekeys;tracekeys.add((kind,va,pa));table=before['root']
   for level in range(levels):
    i+=1;step=fields(lines[i]);entry=step['entry'];assert step['level']==level and step['table']==table and table in tables
    assert inside(mmap['RESERVED'],table,table+4096)
    assert all(not (a<table+4096 and table<b) for a,b in mmap['CANDIDATE'])
    shift=39-level*9;slot=table,(va>>shift)&511
    if slot in entry_values:assert not (entry_values[slot]^entry)&~0x60
    entry_values[slot]=entry
    assert entry&1 and not entry&0x7800000000000000
    mask=((1<<before['bits'])-1)&0x000ffffffffff000
    assert not (entry&0x000ffffffffff000)&~mask
    huge=level<3 and bool(entry&128);leaf=huge or level==3
    assert leaf==(level==levels-1)
    if leaf:
     shift=shift if huge else 12;offset=(1<<shift)-1
     if huge:assert not entry&0x000ffffffffff000&offset&~(1<<12)
     actual=(entry&mask&~offset)+(va&offset);assert actual==pa
     patindex=(entry>>3&3)|((entry>>(12 if huge else 7)&1)<<2)
     assert before['pat']>>(patindex*8)&255==6
    else:assert not entry&0x18;table=entry&mask
   i+=1
  expected={('GDT',va,pa) for k,_,va,pa,_ in claims if k=='GDT'}|{('TABLE',x,x) for x in tables}
  assert tracekeys==expected
  assert plan['unresolved']['dependencies']==['inherited-table-set-incomplete','other-firmware-state']
 else:
  assert not plan['walks'] and not plan['unresolved']['dependencies']
  assert {k for k,_,_,_,_ in claims}=={'IMAGE','STACK','HANDOFF','SOURCE_BUFFER','MAP_WORKSPACE','OWN_WORKSPACE','BOOT_POOL','POOL_METADATA','VECTORS','MMIO'}
  assert any(k=='MMIO' and pa==0x9000000 and size==4096 for k,_,_,pa,size in claims)
 assert plan['totals']==dict(protected=sum(b-a for a,b in protected)//4096,review=sum(b-a for a,b in review)//4096,granted=0)
 assert lines.count('STYLEOS OWNERSHIP PASS')==1
 rows.append(dict(job=log.stem,negative=False,phase=plan['phase'],root=hex(before['root']),unresolved=unresolved,granted=0,protected_ranges=len(protected),review_ranges=len(review),walks=len(plan['walks']),ram=mmap['RAM']))
assert len(rows)==7
Path('/workspace/scratch/styleos-t-p1-5/review/actual-evidence.json').write_text(json.dumps(rows,indent=2)+'\n')
print(json.dumps(dict(status='PASS',jobs=len(rows),positive=sum(not x['negative'] for x in rows),negative=sum(x['negative'] for x in rows),details=rows),indent=2))
