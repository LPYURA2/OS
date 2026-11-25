#!/usr/bin/env python3
"""Eight fresh, independent save/reboot pairs on one linked cold image.
No retry of failed cases; keep all evidence and fail if ANY pair fails.
This is a focused storage stability probe, not full KFA-11 acceptance.
"""
import argparse,hashlib,json,subprocess,sys,time
from pathlib import Path
from test import ROOT
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);p.add_argument('--mixed',action='store_true');args=p.parse_args();mixed=args.mixed;out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    build=out/'build'
    with (out/'compile.log').open('w') as log:
        subprocess.run([sys.executable,str(ROOT/'tools/build.py'),'--arch','x86_64',*(['--test-desktop-cold-tasks-863','--mixed-cold-tasks-864'] if mixed else ['--test-desktop-cold-863']),'--output',str(build)],check=True,stdout=log,stderr=subprocess.STDOUT)
    paths={'boot_sha256':build/'x86_64/styleos.img','efi_sha256':build/'x86_64/BOOTX64.EFI','map_sha256':build/'x86_64/kernel.map'}
    expected={k:sha(v) for k,v in paths.items()}
    records=[];good=[]
    for i in range(8):
        case=out/f'pair-{i+1:02d}';log=out/f'pair-{i+1:02d}.log';start=time.monotonic()
        row={'pair':i+1,'status':'FAIL'}
        try:
            if {k:sha(v) for k,v in paths.items()}!=expected:raise ValueError('shared image changed before case')
            with log.open('w') as f:
                proc=subprocess.run([sys.executable,str(ROOT/'tools/test_desktop_storage_service_863.py'),*(['--tasks','--mixed'] if mixed else ['--cold']),'--build-from',str(build),'--output',str(case)],stdout=f,stderr=subprocess.STDOUT,timeout=180)
            row['returncode']=proc.returncode
            if proc.returncode:raise ValueError('guest pair rejected')
            r=json.loads((case/'results.json').read_text())
            if r['status']!='PASS' or not r['cold_owned_root'] or r['cold_task_fixture']!=mixed or r.get('mixed_task_fixture',False)!=mixed or any(r[k]!=v for k,v in expected.items()):raise ValueError('same linked image required')
            if r['first']['writes']!=2 or r['second']['writes']!=0:raise ValueError('write counts')
            if len(r['first']['cpu_events'])!=(29 if mixed else 1) or len(r['second']['cpu_events'])!=(29 if mixed else 1):raise ValueError('cold CPU proof')
            if sha(case/'legacy.raw')!=r['base_sha256'] or sha(case/'after-write.raw')!=r['media_sha256'] or sha(case/'after-reboot.raw')!=r['media_sha256']:raise ValueError('media hash')
            if {k:sha(v) for k,v in paths.items()}!=expected:raise ValueError('shared image changed after case')
            row.update(status='PASS',base_sha256=r['base_sha256'],media_sha256=r['media_sha256'])
            good.append(row)
        except (OSError,ValueError,KeyError,subprocess.TimeoutExpired) as exc:row['error']=repr(exc)
        row['wall_seconds']=round(time.monotonic()-start,3);records.append(row)
        (out/'results.json').write_text(json.dumps(dict(status='PASS' if len(good)==8 else 'FAIL',expected_pairs=8,mixed_tasks=mixed,passed=len(good),failed=sum(x['status']=='FAIL' for x in records),linked=expected,records=records),indent=2))
        print('S863 stress pair',i+1,row['status'],flush=True)
    if len(good)!=8 or len({(x['base_sha256'],x['media_sha256']) for x in good})!=1:raise SystemExit('storage stress failed: no retries allowed')
    print('S863 storage stability PASS: 8 independent pairs, 16 boots, 16 verified sector commits, zero reboot writes'+('; 384 mixed task dispatches' if mixed else ''))
if __name__=='__main__':main()
