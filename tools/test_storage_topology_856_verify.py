from storage_topology_856_verify import verify,POLICY
ROOT=0x1000

def rec(tag,**v):return ('STYLEOS S856 '+tag+' ').encode()+b' '.join(f'{k.replace("_","-")}=0x{n:016x}'.encode() for k,n in dict(root=ROOT,**v).items())
def fixture(with_ide):
    rows=[rec('PCI',bdf=0,id=0x29c08086,**{'class':0x06000002})];devices=[{'slot':0,'function':0,'id':{'device':0x29c0,'vendor':0x8086},'class_info':{'class':0x0600}}]
    if with_ide:
        rows.extend([rec('PCI',bdf=40,id=0x70108086,**{'class':0x01018000}),rec('IDE',bdf=40,command_status=1,header=0,timing=0x80000000,bar0=0,bar1=0,bar2=0,bar3=0,bar4=0xc001)])
        devices.append({'slot':5,'function':0,'id':{'device':0x7010,'vendor':0x8086},'class_info':{'class':0x0101}})
    rows.extend([rec('TOTAL',functions=len(devices),ide=int(with_ide),cf8_before=0x80000000,cf8_after=0x80000000),POLICY]);return rows,[{'bus':0,'devices':devices}]
for has in (False,True):
    rows,qmp=fixture(has);res=verify(b'\n'.join(rows),ROOT,qmp);assert res['compatible_static_candidates']==([40] if has else []) and res['admission'] is False
rows,qmp=fixture(True);raw=b'\n'.join(rows);bad=[(raw,0,qmp),(raw,0x2000,qmp),(raw,ROOT,[]),(raw+b'\n'+POLICY,ROOT,qmp),(raw+b'\nSTYLEOS S850 FAIL x',ROOT,qmp),(raw.replace(b'ata-access=no',b'ata-access=yes'),ROOT,qmp)]
for i in range(len(rows)):bad.append((b'\n'.join(rows[:i]+rows[i+1:]),ROOT,qmp))
for a,b in [(b'functions=0x0000000000000002',b'functions=0x0000000000000003'),(b'cf8-after=0x0000000080000000',b'cf8-after=0x0000000000000000'),(b'ide=0x0000000000000001',b'ide=0x0000000000000000'),(b'id=0x0000000070108086',b'id=0x0000000070118086'),(b'bdf=0x0000000000000028',b'bdf=0x0000000000000100')]:assert a in raw;bad.append((raw.replace(a,b),ROOT,qmp))
for item in bad:
    try:verify(*item)
    except ValueError:continue
    raise AssertionError('accepted mutation')
for a,b in [(b'command-status=0x0000000000000001',b'command-status=0x0000000000000005'),(b'timing=0x0000000080000000',b'timing=0x0000000000000000'),(b'bar2=0x0000000000000000',b'bar2=0x0000000000000201')]:
    assert a in raw;res=verify(raw.replace(a,b),ROOT,qmp);assert not res['compatible_static_candidates'] and not res['admission']
print(f'S856 topology synthetic: 2 positive, {len(bad)} rejected mutations, 3 blocked static candidates PASS; no hardware evidence')
