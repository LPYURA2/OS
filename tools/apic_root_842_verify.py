"""Strict independent UART replay of actual S842 terminal IRQ experiment."""
import re
from irq_807_verify import verify as parent
def verify(raw,rc):
    try:
        ok,obs=parent(raw,rc)
        if not ok:raise ValueError(obs)
        lines=raw.splitlines()
        def fields(tag,keys):
            prefix=('STYLEOS S842 '+tag+' ').encode()
            xs=[x for x in lines if x.startswith(prefix)]
            if len(xs)!=1:raise ValueError('duplicate/missing '+tag)
            raw=xs[0][len(prefix):]
            m=re.fullmatch(b' '.join(k.encode()+b'=0x([0-9a-f]{16})' for k in keys),raw)
            if not m:raise ValueError('schema '+tag)
            return dict(zip(keys,(int(v,16) for v in m.groups()))),lines.index(xs[0])
        a,i=fields('READY',('old','new','low-table','low-used','lapic','ioapic','uc-index','pat','ist'))
        b,j=fields('IRQ',('before','during','after','frame','ist'))
        pol=b'STYLEOS S842 POLICY isolated-own-root=yes pit-restored=no production-cr3=deny dma-grants=no'
        if lines.count(pol)!=1 or not i<j<lines.index(pol)<lines.index(b'STYLEOS S807 PASS'):raise ValueError('order/policy')
        if len([x for x in lines if x.startswith(b'STYLEOS S842 ')])!=3:raise ValueError('extra evidence')
        if any(b' FAIL ' in x or b'STYLEOS S790 DF ' in x for x in lines):raise ValueError('guest failure')
        if not all(a[k]>0 and a[k]%4096==0 for k in ('old','new','low-table','lapic','ioapic','ist')) or len({a[k] for k in ('old','new','low-table','lapic','ioapic')})!=5:raise ValueError('root/device provenance')
        if not 4<=a['low-used']<=32 or not 0<=a['uc-index']<8 or (a['pat']>>(8*a['uc-index']))&255:raise ValueError('cache/type')
        if not a['new']==b['before']==b['during']==b['after'] or a['ist']!=b['ist'] or not b['ist']<=b['frame']<=b['ist']+4096-40 or b['frame']%8:raise ValueError('actual IRQ root/IST')
        if (a['lapic'],a['ioapic'])!=(obs['topology']['lapic-physical'],obs['topology']['ioapic-physical']):raise ValueError('route device mismatch')
        def mem(tag):
            xs=[x for x in lines if x.startswith(('STYLEOS MEMMAP '+tag+' ').encode())]
            if len(xs)!=1:raise ValueError('memory evidence '+tag)
            _,_,_,lo,hi=xs[0].split()
            return int(lo,16),int(hi,16)
        lo,hi=mem('IMAGE');sl,sh=mem('STACK')
        if not lo<=sl<sh<=hi or not lo<=a['new']<a['new']+4096<=hi or not lo<=a['low-table']<a['low-table']+32*4096<=hi:raise ValueError('owned image')
        obs.update(root=a,irq=b,production_cr3_authorized=False)
        return True,obs
    except (ValueError,IndexError,TypeError,KeyError) as e:return False,str(e)
