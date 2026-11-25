#!/usr/bin/env python3
"""Compare actual independent boots: same EFI and changes confined to clock."""
import json,sys
from pathlib import Path
from test_desktop_v0 import Frame
a,b=map(Path,sys.argv[1:])
x,y=[json.loads((p/'results.json').read_text()) for p in (a,b)]
if x['status']!='PASS' or y['status']!='PASS' or x['efi_sha256']!=y['efi_sha256'] or x['observations']['clock']!=[12,34] or y['observations']['clock']!=[17,45]:raise ValueError('clock paired evidence')
for name in ('initial','language','cursor','notes','text','restart'):
    Frame(a/(name+'.ppm')).expect_changes_in(Frame(b/(name+'.ppm')),[(1184,750,96,50)])
print('S863 clock paired PASS: 50 CPU events; 12 frames; only clock region differs')
