"""Actual QMP counter replay and field mutations."""
from copy import deepcopy
import json,sys
from pathlib import Path
from ata_stats_857 import verify_counts,ZERO
from block_fixture_856_verify import DEVICE
p=Path(sys.argv[1]);rows=[json.loads((p/(tag+'-stats.json')).read_text()) for tag in ('prelaunch','ready','post-read')]
assert verify_counts(*rows)['read_bytes']==2048
count=0
for i in range(3):
    for k in (*ZERO,'rd_operations','rd_bytes','account_failed','account_invalid'):
        for mode in ('missing','wrong','type'):
            a=deepcopy(rows);s=next(r['stats'] for r in a[i] if r.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE))
            if mode=='missing':del s[k]
            elif mode=='wrong':s[k]=not s[k] if type(s[k]) is bool else s[k]+1
            else:s[k]='invalid'
            try:verify_counts(*a)
            except ValueError:count+=1;continue
            raise AssertionError('accepted counter mutation '+k)
print('Actual QMP checkpoints: 1 positive /',count,'refusals PASS')
