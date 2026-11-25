from trace_irq_859 import events,active_root
root=0x5f36000
sample='''Servicing hardware INT=0x50
  3: v=50 e=0000 i=0 cpl=0 IP=0038:0000000005db0001 pc=0000000005db0001
CR0=80010033 CR2=0000000000000000 CR3=0000000005f36000 CR4=00000668
check_exception old: 0xffffffff new 0xd
  4: v=0d e=0010 i=0 cpl=0 IP=0038:0000000005db1234 pc=0000000005db1234
CR0=80010033 CR2=0000000000000000 CR3=0000000005f36000 CR4=00000668
  5: v=0e e=0002 i=0 cpl=0 IP=0038:0000000005db1235 pc=0000000005db1235
CR0=80010033 CR2=0000000000000000 CR3=0000000007801000 CR4=00000668
  6: v=ff e=0000 i=0 cpl=0 IP=0038:0000000005db1236 pc=0000000005db1236
'''
e=events(sample,root);assert len(e)==2 and [x['vector'] for x in e]==[0x50,0xd] and e[1]['raw_error_field']==0x10
assert not events(sample,0x1000)
assert events(sample,0x7801000)[0]['vector']==14
assert not events('',root)
assert active_root(b'STYLEOS S851 ACTIVE root=0x0000000005f36000 gdt=0x1\n')==root
for bad in (b'',b'STYLEOS S851 ACTIVE root=0x0000000005f36000\n'*2):
    try:active_root(bad)
    except ValueError:continue
    raise AssertionError('ambiguous/missing root accepted')
print('7 synthetic trace parser checks PASS; no guest evidence')
