"""Unit tests only: artificial bank UART on previous real S862 capture."""
import gzip,json,re,sys
from pathlib import Path
from runtime_bank_862_verify import verify,trace_check,bank_site

def main(p,build,full=False):
    r=json.loads((p/'results.json').read_text());raw=(p/'serial.log').read_bytes();ref=json.loads((p/'reference.json').read_text());site=r['site'];obs=r['observations'];trace=gzip.decompress((p/'cpu-trace.log.gz').read_bytes()).decode('ascii')
    print('Linked new bank:',bank_site((build/'BOOTX64.EFI').read_bytes(),build/'kernel.map'))
    old=p/'build/x86_64'
    try:bank_site((old/'BOOTX64.EFI').read_bytes(),old/'kernel.map')
    except ValueError:pass
    else:raise AssertionError('old binary accepted')
    assert not verify(raw,r['qemu_exit_code'],site,ref)[0]
    def row(tag,keys,values):return ('STYLEOS S862B '+tag+' '+' '.join(f'{k}=0x{x:016x}' for k,x in zip(keys,values))+'\n').encode()
    saved=[(0x10000000,0x11001000,2)]*2;out=[];step=ti=0
    for line in raw.splitlines(keepends=True):
        if line.startswith(b'STYLEOS S862 SELECT '):
            e=obs['runtime_ticks'][step];pc,sp,f=saved[e['id']];out.append(row('ENTER',('step','id','pid','dispatch','safe','root','pc','sp','flags'),(step,e['id'],e['pid'],e['dispatch'],e['safe'],e['root'],pc,sp,f|0x200)));continue
        if line.startswith(b'STYLEOS S861T BOUNDARY '):
            e=obs['runtime_ticks'][step];i=e['id'];terminal=step in (6,9)
            if terminal:saved[i]=(0,0,0)
            else:
                t=obs['user_preempt_ticks'][ti];ti+=1;saved[i]=(t['rip'],t['rsp'],t['flags']&~0x200)
            out.append(row('COMMIT',('step','id','pid','state','pc','sp','flags','zero'),(step,i,e['pid'],e['state'],*saved[i],int(terminal))));step+=1
        if line.startswith(b'STYLEOS S860 CLEAN '):out.append(row('CLEAN',('safe','bank-zero','pending-zero'),(obs['active']['root'],1,1)))
        out.append(line)
    assert step==10
    sample=b''.join(out);ok,o=verify(sample,r['qemu_exit_code'],site,ref);assert ok,o
    assert json.loads(json.dumps(trace_check(trace,o)))==r['cpu_events'];n=0
    for h in re.finditer(rb'^STYLEOS S862B (?:ENTER|COMMIT|CLEAN) [^\r\n]+',sample,re.M):
        for f in re.finditer(rb'=0x([0-9a-f]{16})',h[0]):
            a,z=(h.start()+i for i in f.span(1));v=int(sample[a:z],16)
            for bit in (range(64) if full else (0,63)):
                assert not verify(sample[:a]+f'{v^(1<<bit):016x}'.encode()+sample[z:],r['qemu_exit_code'],site,ref)[0];n+=1
        for x in [sample[:h.start()]+sample[h.end():],sample[:h.start()]+h[0]+b'\n'+sample[h.start():]]:
            assert not verify(x,r['qemu_exit_code'],site,ref)[0];n+=1
    print('UNIT ONLY: artificial bank records,',n,'refusals PASS; old PE and UART rejected; NOT hardware bank execution')
if __name__=='__main__':main(Path(sys.argv[1]),Path(sys.argv[2]),'--full' in sys.argv[3:])
