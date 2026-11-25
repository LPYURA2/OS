"""Unit-only synthetic additions over the *prior* live capture; not guest evidence."""
import gzip,json,re,sys
from pathlib import Path
from runtime_pump_863_verify import site_check,verify,refusals,trace_check
from test_desktop_interactive_863 import EVENT

def main(p,build):
    r=json.loads((p/'results.json').read_text());raw=(p/'serial.log').read_bytes();obs=r['observations'];ref=r['reference'];fnv=r['frame_fnv'];site=site_check((build/'BOOTX64.EFI').read_bytes(),build/'kernel.map')
    old=p/'build/x86_64'
    try:site_check((old/'BOOTX64.EFI').read_bytes(),old/'kernel.map')
    except ValueError:pass
    else:raise AssertionError('previous binary accepted as pump')
    assert not verify(raw,33,site,ref,fnv)[0]
    # Unit-only record format test: actual prior CPU addresses require the prior
    # site; the six new link-presence keys are explicitly artificial here.
    site={**r['site'],**{k:site[k] for k in ('s863_shell_pump_rva','s863_shell_pump_call_rva','guest861_rva','guest861_call_rva','s848_trial_rva','s848_trial_call_rva')}}
    assert not verify(raw,33,site,ref,fnv)[0]
    def row(tag,keys,vals):return (tag+' '+' '.join(f'{k}=0x{v:016x}' for k,v in zip(keys,vals))).encode()
    def slice_row(vals):return row('STYLEOS S863P SLICE',('n','before','after','used','processes','windows','done'),vals)
    lines=[];step=epoch=0
    for line in raw.splitlines():
        if line.startswith(b'STYLEOS S863 EVENT '):
            m=re.search(rb' n=0x([0-9a-f]{16}) ',line);n=int(m[1],16)
            if n>=6:line=line[:m.start(1)]+f'{n+2:016x}'.encode()+line[m.end(1):]
        if line.startswith(b'STYLEOS S863 EXIT '):line=line.replace(b'events=0x0000000000000007',b'events=0x0000000000000009')
        lines.append(line)
        if line.startswith(b'STYLEOS S848 BACK '):
            base=epoch*10;n=epoch*11+step+1
            lines.append(slice_row((n,base+step,base+step+1,6,1 if step==0 else 2,1 if step in (0,7,8) else 0 if step==9 else 2,0)))
            if n==1:
                lines.append(b'STYLEOS S863P WAIT')
                for i,dx,x in [(6,20,680),(7,(1<<64)-20,660)]:
                    e=dict(obs['shell_events'][4]);e.update(n=i,mouse=1,key=0,dx=dx,x=x,writes=340)
                    lines.append(row('STYLEOS S863 EVENT',EVENT,[e[k] for k in EVENT]))
                lines.append(b'STYLEOS S863P RESUME')
            step+=1
        if line.startswith(b'STYLEOS S862R DONE '):
            assert step==10;lines.append(slice_row((11*(epoch+1),10*(epoch+1),10*(epoch+1),0,0,0,int(epoch==1))));step=0;epoch+=1
    assert epoch==2
    sample=b'\n'.join(lines)+b'\n';ok,o=verify(sample,33,site,ref,fnv);assert ok,o
    assert len(o['shell_slices'])==22
    trace=gzip.decompress((p/'cpu-trace.log.gz').read_bytes()).decode('ascii');assert json.loads(json.dumps(trace_check(trace,o)))==r['cpu_events']
    n=refusals(sample,33,site,ref,fnv)
    print('UNIT ONLY: artificial poll records over earlier original trace;',n,'refusals PASS; previous EFI and raw UART rejected; NOT new guest evidence')
if __name__=='__main__':main(Path(sys.argv[1]),Path(sys.argv[2]))
