"""Unit-only mutations of in-memory artificial second round; NOT new guest evidence."""
import gzip,json,re,sys
from pathlib import Path
from runtime_recycle_862_verify import verify, trace_check, recycle_site, HEADER
from runtime_sparse_862_verify import sparse_site, verify as sparse_verify, trace_check as sparse_trace

def main(p,build):
    r=json.loads((p/'results.json').read_text());raw=(p/'serial.log').read_bytes();ref=json.loads((p/'reference.json').read_text());trace=gzip.decompress((p/'cpu-trace.log.gz').read_bytes()).decode('ascii');old=p/'build/x86_64'
    site=sparse_site((old/'BOOTX64.EFI').read_bytes(),old/'kernel.map');ok,o=sparse_verify(raw,33,site,ref);assert ok,o;assert o==r['observations'];assert json.loads(json.dumps(sparse_trace(trace,o)))==r['cpu_events']
    new=recycle_site((build/'BOOTX64.EFI').read_bytes(),build/'kernel.map')
    try:recycle_site((old/'BOOTX64.EFI').read_bytes(),old/'kernel.map')
    except ValueError:pass
    else:raise AssertionError('old binary admitted')
    assert not verify(raw,33,new,ref)[0]
    # Use old actual PE sites for old capture and new required symbol keys ONLY
    # for the artificial validator exercise. Never persist an artificial capture.
    site.update({k:new[k] for k in ('s849_guest_bind_leases_rva','stale862_rva')})
    roots=o['join_roots'];safe=roots['safe'];lines=raw.splitlines(keepends=True);begin=next(i for i,x in enumerate(lines) if x.startswith(b'STYLEOS S861 ROOTS '));prefix=lines[:begin];original=lines[begin:]
    def marker(tag,keys,vals):return ('STYLEOS S862R '+tag+' '+' '.join(f'{k}=0x{v:016x}' for k,v in zip(keys,vals))+'\n').encode()
    def shift(line,fields):
        for k,n in fields.items():
            pat=rb'(?<= )'+k.encode()+rb'=0x([0-9a-f]{16})'
            line=re.sub(pat,lambda m:k.encode()+b'=0x'+f'{int(m[1],16)+n:016x}'.encode() if int(m[1],16) or k=='dispatches' else m[0],line)
        return line
    samples=[]
    for epoch in range(2):
        gen=4+2*epoch;out=[marker('BEGIN',('round','safe','dispatches','next'),(epoch,safe,10*epoch,gen))]
        if epoch:out.append(marker('REUSE',('pa-a','pa-b','old-a','old-b','new-a','new-b','stale-leases','stale-root'),(roots['pa-a'],roots['pa-b'],4,5,6,7,2,1)))
        for x in original:
            if epoch:
                for tag,fields in [(b'S861 ROOTS',{'gen-a':2,'gen-b':2}),(b'S861 CLEAN',{'next':2}),(b'S860 READY',{'pid-a':2,'pid-b':2,'handle-a':2,'handle-b':2,'win-a':2,'win-b':2}),(b'S861T BOUNDARY',{'pid':2}),(b'S860 EVENT',{'pid':2,'handle':2,'dispatch':10,'window':2}),(b'S862B ENTER',{'pid':2,'dispatch':10}),(b'S862B COMMIT',{'pid':2}),(b'S862S ADMIT',{'pid':2,'handle':2,'dispatches':10}),(b'S862S RELEASE',{'pid':2,'handle':2}),(b'S860 CLEAN',{'dispatches':10})]:
                    if x.startswith(b'STYLEOS '+tag+b' '):x=shift(x,fields);break
                if x.startswith(b'STYLEOS S862B ENTER step=0x0000000000000000 '):out.append(marker('STALE',('slot','root','old-pid','new-pid','old-handle','new-handle','old-dispatch','new-dispatch','refused'),(0,roots['a'],1,3,4,6,1,11,3)))
            out.append(x)
            if x.startswith(b'STYLEOS S849 READY '):out.append(marker('FX',('safe','gen-a','gen-b','refused'),(safe,gen,gen+1,2*epoch)))
        out.append(marker('DONE',('round','safe','dispatches','next','next-pid','next-window','used'),(epoch,safe,10*(epoch+1),gen+2,3+2*epoch,3+2*epoch,0)));samples.append(b''.join(out))
    artificial=b''.join(prefix)+b''.join(samples);ok,obs=verify(artificial,33,site,ref);assert ok,obs
    h=list(HEADER.finditer(trace));start=next(i for i,m in enumerate(h) if int(m[1])==r['cpu_events'][0][0]);second=trace[h[start+1].start():]
    def bump(m):
        a,b=m.span(1);return m[0][:a-m.start()]+str(int(m[1])+12)+m[0][b-m.start():]
    fake_trace=trace+HEADER.sub(bump,second)
    assert len(trace_check(fake_trace,obs))==25
    n=0
    for h in re.finditer(rb'^STYLEOS S862R [^\r\n]+',artificial,re.M):
        for f in re.finditer(rb'=0x([0-9a-f]{16})',h[0]):
            a,z=(h.start()+i for i in f.span(1));v=int(artificial[a:z],16)
            for bit in (0,63):
                assert not verify(artificial[:a]+f'{v^(1<<bit):016x}'.encode()+artificial[z:],33,site,ref)[0];n+=1
        for x in [artificial[:h.start()]+artificial[h.end():],artificial[:h.start()]+h[0]+b'\n'+artificial[h.start():]]:
            assert not verify(x,33,site,ref)[0];n+=1
    for x in (trace, fake_trace+second):
        try:trace_check(x,obs)
        except ValueError:n+=1
        else:raise AssertionError('bad artificial trace accepted')
    print('UNIT ONLY:',n,'sampled refusals PASS; previous real sparse proof intact; artificial two-round UART/trace accepted only as a unit fixture; no new hardware proof.')
if __name__=='__main__':main(Path(sys.argv[1]),Path(sys.argv[2]))
