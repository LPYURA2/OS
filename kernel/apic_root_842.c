#include <styleos/apic_root_842.h>
#define PAGE UINT64_C(4096)
#define NX (UINT64_C(1)<<63)
static bool overlap(uint64_t a,uint64_t b,uint64_t c,uint64_t d){return a<d&&c<b;}
static bool valid_cache(const struct s842_cache *c){
    if(!c||c->bits<36||c->bits>52||c->count>CACHE800_MAX_VAR||c->count!=(c->cap&255u)||(c->pat&255u)!=6)return false;
    for(unsigned i=0;i<8;i++){unsigned t=(c->pat>>(8*i))&255u;if(t!=0&&t!=1&&t!=4&&t!=5&&t!=6&&t!=7)return false;}
    return true;
}
static bool physical_type(const struct s842_cache *c,uint64_t p,unsigned type){
    struct cache800_result a,b;
    return cache800_physical_type(p,c->bits,c->cap,c->def,c->pairs,c->count,&a)&&
        cache800_physical_type(p+PAGE-1,c->bits,c->cap,c->def,c->pairs,c->count,&b)&&a.mtrr_enabled&&b.mtrr_enabled&&a.type==type&&b.type==type;
}
static uint64_t cache_flags(unsigned i){return ((i&1)?8u:0u)|((i&2)?16u:0u)|((i&4)?128u:0u);}
static bool device(const struct s842_root *r,uint64_t p){return p==r->lapic||p==r->ioapic;}
static bool expected(const struct s842_root *r,uint64_t va,uint64_t *value){
    const struct pg_plan *p=&r->plan;
    for(size_t i=0;i<p->count;i++){
        const struct pg_mapping *m=&p->maps[i];
        if(va>=m->va&&va-m->va<m->bytes){
            uint64_t pa=m->pa+(va-m->va);
            *value=pa|1u|((m->flags&PG_WRITE)?2u:0u)|((m->flags&PG_EXEC)?0:NX);
            if(device(r,pa))*value|=cache_flags(r->uc_index);
            return true;
        }
    }
    return false;
}
/* Walk the entire bounded detached tree; reject extra leaves, repeated table
 * links, inherited table pages and permission/cache changes. A/D may mutate. */
static bool audit_table(const struct s842_root *r,size_t table,unsigned level,uint64_t prefix,uint32_t *seen,uint64_t *leaves){
    if(table>=r->built.used||(*seen&(UINT32_C(1)<<table)))return false;
    *seen|=UINT32_C(1)<<table;
    uint64_t mask=((UINT64_C(1)<<r->plan.pa_bits)-1)&~(PAGE-1);
    for(unsigned i=0;i<512;i++){
        uint64_t e=r->plan.tables[table][i],va=prefix|((uint64_t)i<<(12+9*(level-1)));
        if(!e)continue;
        if(level==1){uint64_t want;if(!expected(r,va,&want)||(e&~UINT64_C(0x60))!=want)return false;(*leaves)++;}
        else{uint64_t pa=e&mask;if((e&~(mask|UINT64_C(0x20)))!=3u||pa<r->plan.tables_pa||pa-r->plan.tables_pa>=r->built.used*PAGE)return false;
            if(!audit_table(r,(size_t)((pa-r->plan.tables_pa)/PAGE),level-1,va,seen,leaves))return false;}
    }
    return true;
}
static bool shape(const struct s842_root *r,const struct s842_cache *c){
    if(!r||!valid_cache(c)||r->plan.arch!=PG_X86||r->plan.pa_bits!=c->bits||!r->plan.tables||!r->plan.tables_pa||!r->plan.capacity||r->plan.capacity>PG_TABLE_PAGES||r->plan.count<3||r->plan.count>PG_MAX_MAPS||r->uc_index>7||((c->pat>>(8*r->uc_index))&255u)!=0)return false;
    if(!r->lapic||!r->ioapic||r->lapic==r->ioapic||(r->lapic|r->ioapic)&(PAGE-1)||r->lapic>=UINT64_C(1)<<32||r->ioapic>=UINT64_C(1)<<32||!physical_type(c,r->lapic,0)||!physical_type(c,r->ioapic,0))return false;
    const uint64_t limit=UINT64_C(1)<<c->bits,table_bytes=r->plan.capacity*PAGE;
    if(((uintptr_t)r->plan.tables|r->plan.tables_pa)&(PAGE-1)||r->plan.tables_pa>=limit||table_bytes>limit-r->plan.tables_pa||table_bytes>UINTPTR_MAX-(uintptr_t)r->plan.tables)return false;
    if(overlap(r->plan.tables_pa,r->plan.tables_pa+table_bytes,r->lapic,r->lapic+PAGE)||overlap(r->plan.tables_pa,r->plan.tables_pa+table_bytes,r->ioapic,r->ioapic+PAGE))return false;
    uint64_t pages=0;bool tables_covered=false;
    for(size_t i=0;i<r->plan.count;i++){
        const struct pg_mapping *m=&r->plan.maps[i];
        if(!m->bytes||(m->va|m->pa|m->bytes)&(PAGE-1)||m->va!=m->pa||m->pa>=limit||m->bytes>limit-m->pa||m->va>=(UINT64_C(1)<<47)||m->bytes>(UINT64_C(1)<<47)-m->va||(m->flags!=0&&m->flags!=PG_WRITE&&m->flags!=PG_EXEC))return false;
        if(m->bytes/PAGE>PG_TABLE_PAGES*512u-pages)return false;
        pages+=m->bytes/PAGE;
        if(i<r->plan.count-2){
            if(overlap(m->pa,m->pa+m->bytes,r->lapic,r->lapic+PAGE)||overlap(m->pa,m->pa+m->bytes,r->ioapic,r->ioapic+PAGE))return false;
            for(uint64_t off=0;off<m->bytes;off+=PAGE)if(!physical_type(c,m->pa+off,6))return false;
            if(m->flags==PG_WRITE&&m->pa<=r->plan.tables_pa&&r->plan.tables_pa+table_bytes<=m->pa+m->bytes)tables_covered=true;
        }
        else if(m->pa!=(i==r->plan.count-2?r->lapic:r->ioapic)||m->bytes!=PAGE||m->flags!=PG_WRITE)return false;
        for(size_t j=0;j<i;j++){const struct pg_mapping *n=&r->plan.maps[j];if(overlap(m->pa,m->pa+m->bytes,n->pa,n->pa+n->bytes))return false;}
    }
    return tables_covered;
}
bool s842_audit(const struct s842_root *r,const struct s842_cache *c){
    if(!shape(r,c)||!r->ready||r->built.error!=PG_OK||r->built.root!=r->plan.tables_pa||!r->built.used||r->built.used>r->plan.capacity)return false;
    uint32_t seen=0;uint64_t leaves=0,want=0;
    for(size_t i=0;i<r->plan.count;i++)want+=r->plan.maps[i].bytes/PAGE;
    uint32_t all=r->built.used==32?UINT32_MAX:(UINT32_C(1)<<r->built.used)-1;
    return audit_table(r,0,4,0,&seen,&leaves)&&seen==all&&leaves==want;
}
static bool safe_device(const struct boot_memory_map *m,uint64_t pa){
    if(!m||m->nr_ram>BOOT_MEMORY_INPUT_MAX||m->nr_candidates>BOOT_MEMORY_RANGE_MAX)return false;
    for(size_t i=0;i<m->nr_ram;i++)if(m->ram[i].start>=m->ram[i].end||overlap(pa,pa+PAGE,m->ram[i].start,m->ram[i].end))return false;
    for(size_t i=0;i<m->nr_candidates;i++)if(m->candidates[i].start>=m->candidates[i].end||overlap(pa,pa+PAGE,m->candidates[i].start,m->candidates[i].end))return false;
    return true;
}
static bool patch(struct s842_root *r,uint64_t va){
    size_t table=0;uint64_t mask=((UINT64_C(1)<<r->plan.pa_bits)-1)&~(PAGE-1);
    for(unsigned level=4;level>1;level--){uint64_t e=r->plan.tables[table][(va>>(12+9*(level-1)))&511u],pa=e&mask;
        if((e&~mask)!=3u||pa<r->plan.tables_pa||pa-r->plan.tables_pa>=r->built.used*PAGE)return false;
        table=(size_t)((pa-r->plan.tables_pa)/PAGE);}
    uint64_t *entry=&r->plan.tables[table][(va>>12)&511u];if(*entry!=(va|NX|3u))return false;
    *entry|=cache_flags(r->uc_index);return true;
}
bool s842_build(const struct pg_plan *ram,const struct boot_memory_map *memory,const struct s842_cache *cache,uint64_t lapic,uint64_t ioapic,struct s842_root *out){
    if(!out)return false;
    *out=(struct s842_root){0};
    if(!ram||!valid_cache(cache)||!ram->count||ram->count>PG_MAX_MAPS-2)return false;
    struct s842_root r={.plan=*ram,.lapic=lapic,.ioapic=ioapic,.uc_index=8};
    for(unsigned i=0;i<8;i++)if(((cache->pat>>(8*i))&255u)==0){r.uc_index=(uint8_t)i;break;}
    r.plan.maps[r.plan.count++]=(struct pg_mapping){lapic,lapic,PAGE,PG_WRITE};
    r.plan.maps[r.plan.count++]=(struct pg_mapping){ioapic,ioapic,PAGE,PG_WRITE};
    if(!shape(&r,cache)||!safe_device(memory,lapic)||!safe_device(memory,ioapic))return false;
    /* Storage is detached and not published: patch exact UC leaves only after
     * the common builder succeeds. No access through temporary WB entries. */
    r.built=pg_build(&r.plan);
    if(r.built.error!=PG_OK||!patch(&r,lapic)||!patch(&r,ioapic))return false;
    r.ready=true;if(!s842_audit(&r,cache))return false;*out=r;return true;
}
