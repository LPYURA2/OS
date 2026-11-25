"""Correlate exact fixture frontend counters at three paused checkpoints."""
from overlay_fixture_856 import verify_stats
from block_fixture_856_verify import DEVICE, need

ZERO=('wr_bytes','wr_operations','flush_operations','unmap_bytes','unmap_operations','zone_append_bytes','zone_append_operations','failed_rd_operations','invalid_rd_operations','failed_wr_operations','invalid_wr_operations','failed_flush_operations','invalid_flush_operations','failed_unmap_operations','invalid_unmap_operations','failed_zone_append_operations','invalid_zone_append_operations')

def verify_counts(prelaunch,ready,post):
    for rows,n in ((prelaunch,0),(ready,0),(post,4)):
        verify_stats(rows)
        s=next(r['stats'] for r in rows if r.get('qdev') in (DEVICE,'/machine/peripheral/'+DEVICE))
        for k,v in (*((k,0) for k in ZERO),('rd_operations',n),('rd_bytes',512*n)):
            need(type(s.get(k)) is int and s[k]==v,'fixture counter '+k)
        need(s.get('account_failed') is True and s.get('account_invalid') is True,'accounting disabled')
    return {'read_operations':4,'read_bytes':2048,'write_operations':0,'write_bytes':0,'scope':'paused-checkpoints'}
