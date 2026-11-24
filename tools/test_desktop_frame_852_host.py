#!/usr/bin/env python3
"""Compile real first-frame code against guarded RAM. No QEMU claims."""
import argparse,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCES='desktop_shell desktop_draw desktop_display desktop_sessions desktop_text_app desktop_stylefs desktop_extra_volume desktop_volume_io desktop_store stylefs native_desktop_bridge native_window_channel process_runtime window_service user_session_runtime system_apps desktop_frame_io_852'.split()
def reference(output,cc='cc',sanitize=False):
    output=Path(output).resolve();output.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DSTYLEOS_TEST_DESKTOP_FRAME_852=1','-I',str(ROOT/'include'),*(['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if sanitize else []),*[str(ROOT/'kernel'/f'{s}.c') for s in SOURCES],str(ROOT/'tests/test_desktop_frame_852.c'),'-o',str(output)],check=True)
    values=tuple(int(v) for v in subprocess.check_output([str(output)],text=True).split());assert len(values)==4
    return dict(zip(('writes','fnv','first','last'),values))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);p.add_argument('--cc',default='cc');p.add_argument('--sanitize',action='store_true');a=p.parse_args();print(json.dumps(reference(a.output,a.cc,a.sanitize)))
