from requester_plan_829 import plan
from test_iommu_evidence_828 import table,drhd,scope,ivhd,entry
rows=[(8,1,0,0),(16,2,1,2),(0x108,2,2,2),(0x210,1,0,0)]
def test(raw,b):
    p=plan(raw,b,rows)
    assert not any(p[k] for k in ('ownership','mmio_authorized','dma_authorized','production_cr3_authorized'))
    assert all(not v['ownership'] and not v['dma_authorized'] for v in p['requesters'])
    return {v['rid']:v for v in p['requesters']},p['blockers']
n=0
r,b=test(b'',0);assert all(v['state']=='NO_CLAIM' for v in r.values());n+=1
r,b=test(table(b'DMAR',[drhd(b'')]),1);assert not b and all(v['claims']==[0] for v in r.values());n+=1
r,b=test(table(b'DMAR',[drhd(scope(1,0,1,0),flags=0),drhd(b'',base=0xfeda0000)]),1);assert r[8]['claims']==[0] and r[16]['claims']==[1];n+=1
r,b=test(table(b'DMAR',[drhd(scope(2,0,2,0),flags=0),drhd(b'',base=0xfeda0000)]),1);assert r[8]['claims']==[1] and all(r[k]['claims']==[0] for k in (16,0x108,0x210));n+=1
r,b=test(table(b'DMAR',[drhd(scope(2,0,2,0),flags=0),drhd(scope(1,0,2,0,1,0,2,0),base=0xfeda0000,flags=0)]),1);assert r[0x210]['state']=='CONFLICT';n+=1
r,b=test(table(b'DMAR',[drhd(b''),drhd(b'',base=0xfeda0000)]),1);assert 'multiple-include-all' in b and all(v['state']=='BLOCKED' for v in r.values());n+=1
r,b=test(table(b'DMAR',[drhd(scope(1,0,31,7))]),1);assert 'unresolved-records' in b and all(v['state']=='BLOCKED' for v in r.values());n+=1
r,b=test(table(b'IVRS',[ivhd(entry(2,8)+entry(0x42,16,8))]),2);assert r[16]['state']=='CANDIDATE' and r[16]['claims']==[0];n+=1
r,b=test(table(b'IVRS',[ivhd(entry(0x42,16,8))]),2);assert r[16]['state']=='UNRESOLVED_ALIAS';n+=1
r,b=test(table(b'IVRS',[ivhd(entry(0x42,16,16))]),2);assert r[16]['state']=='UNRESOLVED_ALIAS';n+=1
r,b=test(table(b'IVRS',[ivhd(entry(0x42,16,8)+entry(0x42,8,16))]),2);assert all(r[k]['state']=='UNRESOLVED_ALIAS' for k in (8,16));n+=1
r,b=test(table(b'IVRS',[ivhd(entry(0x42,16,8)+entry(2,0x999))]),2);assert r[16]['state']=='UNRESOLVED_ALIAS';n+=1
r,b=test(table(b'IVRS',[ivhd(entry(0x42,16,8)),ivhd(entry(2,8),base=0xfed90000)]),2);assert r[16]['state']=='UNRESOLVED_ALIAS';n+=1
r,b=test(table(b'IVRS',[ivhd(entry(1,0)+entry(2,8))]),2);assert 'amd-all-device-semantics-unresolved' in b and r[8]['state']=='BLOCKED';n+=1

# Extended and nonzero data-setting policy is not silently reduced to SELECT.
r,b=test(table(b'IVRS',[ivhd(entry(0x46,8,0))]),2);assert 'amd-extended-semantics-unresolved' in b and r[8]['state']=='BLOCKED';n+=1
q=bytearray(entry(2,8));q[3]=1
r,b=test(table(b'IVRS',[ivhd(q)]),2);assert 'amd-device-policy-unresolved' in b and r[8]['state']=='BLOCKED';n+=1
# Even a directly claimed live requester cannot be admitted while the claimed
# AMD controller requester ID is absent or any alias group is unresolved.
q=bytearray(ivhd(entry(2,8)));q[4:6]=(0xfff).to_bytes(2,'little')
r,b=test(table(b'IVRS',[q]),2);assert 'amd-unit-requester-not-observed' in b and r[8]['state']=='BLOCKED';n+=1
r,b=test(table(b'IVRS',[ivhd(entry(2,8)+entry(0x42,16,16))]),2);assert 'amd-unresolved-alias-group' in b and r[8]['state']=='BLOCKED';n+=1
# Unobserved direct and alias sources are retained in the report.
p=plan(table(b'IVRS',[ivhd(entry(2,8)+entry(2,0x999)+entry(0x42,0x888,8))]),2,rows)
assert p['unobserved_direct']==[0x999] and p['unobserved_alias']==[0x888];n+=1
# Independent topology guard: the older decoder accepts these sibling windows.
for badrows in ([(8,2,1,3),(16,2,2,3),(0x108,1,0,0),(0x208,1,0,0)],[(8,2,2,3),(0x208,2,1,1),(0x108,1,0,0)]):
    try:plan(table(b'DMAR',[drhd(b'')]),1,badrows)
    except ValueError:n+=1
    else:raise AssertionError('unsafe bridge windows accepted')
# Memory exceptions and multiple IVHD descriptor forms remain global blockers.
import struct
rmrr=struct.pack('<HH',1,24)+bytes(20)
r,b=test(table(b'DMAR',[drhd(b''),rmrr]),1);assert 'memory-exceptions' in b and all(v['state']=='BLOCKED' for v in r.values());n+=1
ivmd=struct.pack('<BBH',0x20,0,32)+bytes(28)
r,b=test(table(b'IVRS',[ivhd(entry(2,8)),ivmd]),2);assert 'memory-exceptions' in b and r[8]['state']=='BLOCKED';n+=1
iv11=bytearray(ivhd(b''));iv11[0]=0x11;iv11[2:4]=(40).to_bytes(2,'little');iv11.extend(bytes(16))
r,b=test(table(b'IVRS',[ivhd(entry(2,8)),iv11]),2);assert 'multiple-descriptor-forms' in b and r[8]['state']=='BLOCKED';n+=1
print(f'{n}/{n} synthetic requester-plan scenarios PASS; no ownership or hardware grants')
