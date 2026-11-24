#include <styleos/ecam_quarantine_827.h>
#include <styleos/pci_mcfg_813.h>
#include <stdint.h>
static uint64_t le(const uint8_t *p, unsigned n){
    uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;
}
static int intersects(struct boot_range a,struct boot_range b){return a.start<b.end&&b.start<a.end;}
static int contains(struct boot_range a,struct boot_range b){return a.start<=b.start&&b.end<=a.end;}
/* For the observed QEMU DRAC only: one fixed DWord Memory
 * template followed by EndTag, no resource source or translation. */
static int drac_extent(const uint8_t *p,size_t n,struct boot_range *r){
    if(!p||n!=28||p[0]!=0x87||p[1]!=23||p[2]!=0||p[3]!=0||p[4]!=12||p[5]!=1||p[26]!=0x79||p[27]!=0)return 0;
    uint64_t lo=le(p+10,4),hi=le(p+14,4),len=le(p+22,4);
    if(le(p+6,4)||le(p+18,4)||lo>hi||!len||hi-lo+1!=len)return 0;
    return bm_extent(lo,len,r)==BM_OK;
}
static int span_disjoint(const void *a,size_t an,const void *b,size_t bn){
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return a&&b&&an&&bn&&x<=UINTPTR_MAX-an&&y<=UINTPTR_MAX-bn&&
        (x+an<=y||y+bn<=x);
}
int ecam827_quarantine(const void *mcfg,size_t mcfg_size,const void *drac,size_t drac_size,
    const void *map,size_t map_size,size_t stride,uint32_t version,
    struct ecam827_report *out,struct ecam827_work *work){
    if(!out||!work)return 0;
    /* Refuse overlap of output and scratch: failure must leave output unchanged. */
    if(!span_disjoint(out,sizeof(*out),work,sizeof(*work))||
       !span_disjoint(work,sizeof(*work),mcfg,mcfg_size)||
       !span_disjoint(work,sizeof(*work),drac,drac_size)||
       !span_disjoint(work,sizeof(*work),map,map_size)||
       !span_disjoint(out,sizeof(*out),mcfg,mcfg_size)||
       !span_disjoint(out,sizeof(*out),drac,drac_size)||
       !span_disjoint(out,sizeof(*out),map,map_size))return 0;
    struct pci813_mcfg m={0};struct boot_range d;
    if(!pci813_mcfg_parse(mcfg,mcfg_size,&m)||m.count!=1||m.windows[0].segment!=0||
        !drac_extent(drac,drac_size,&d)||d.start!=m.windows[0].physical_first||d.end!=m.windows[0].physical_end)return 0;
    struct ecam827_report *r=&work->candidate;
    *r=(struct ecam827_report){0};r->ecam=d;
    if(bm_uefi_parse(&r->diagnostic,map,map_size,stride,version)!=BM_OK)return 0;
    const uint8_t *bytes=map;
    for(size_t i=0;i<map_size;i+=stride){
        const uint8_t *p=bytes+i;struct boot_range v;
        /* Parser already checked extent and nonoverlap; retain a local check. */
        uint64_t pages=le(p+24,8);
        if(!pages||pages>UINT64_MAX/BOOT_MEMORY_PAGE||bm_extent(le(p+8,8),pages*BOOT_MEMORY_PAGE,&v)!=BM_OK)return 0;
        if(!intersects(v,d))continue;
        uint64_t kind=le(p,4),attr=le(p+32,8);
        if((kind!=0&&kind!=11)||(attr&(UINT64_C(1)<<63)))return 0;
        uint64_t lo=v.start>d.start?v.start:d.start,hi=v.end<d.end?v.end:d.end;
        r->explicit_pages+=(hi-lo)/BOOT_MEMORY_PAGE;
    }
    uint64_t total=(d.end-d.start)/BOOT_MEMORY_PAGE;
    if(r->explicit_pages>total)return 0;
    r->gap_pages=total-r->explicit_pages;
    /* Add a software exclusion even when firmware map has gaps. This is
     * a denial of candidate allocation, not proof of platform ownership. */
    if(bm_add(&r->diagnostic,d.start,d.end-d.start,1)!=BM_OK||bm_normalize(&r->diagnostic)!=BM_OK)return 0;
    int covered=0;
    for(size_t i=0;i<r->diagnostic.nr_reserved;i++)if(contains(r->diagnostic.reserved[i],d))covered=1;
    if(!covered)return 0;
    for(size_t i=0;i<r->diagnostic.nr_candidates;i++)if(intersects(r->diagnostic.candidates[i],d))return 0;
    *out=*r;return 1;
}
