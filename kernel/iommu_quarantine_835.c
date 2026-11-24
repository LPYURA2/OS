#include <styleos/iommu_quarantine_835.h>
static int intersects(struct boot_range a,struct boot_range b){return a.start<b.end&&b.start<a.end;}
static int disjoint(const void *a,size_t an,const void *b,size_t bn){
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return a&&b&&an&&bn&&x<=UINTPTR_MAX-an&&y<=UINTPTR_MAX-bn&&(x+an<=y||y+bn<=x);
}
static uint64_t le(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
int s835_quarantine(const struct s835_input *in,const void *mcfg,size_t mn,const void *drac,size_t dn,
 const void *map,size_t n,size_t stride,uint32_t version,struct s835_report *out,struct s835_work *work){
    if(!disjoint(out,sizeof *out,work,sizeof *work))return 0;
    const void *inputs[]={in,mcfg,drac,map};size_t lengths[]={sizeof *in,mn,dn,n};
    for(unsigned i=0;i<4;i++)if(!disjoint(out,sizeof *out,inputs[i],lengths[i])||!disjoint(work,sizeof *work,inputs[i],lengths[i]))return 0;
    if(in->nr_others>S835_OTHER_MAX||in->backend>2)return 0;
    for(size_t i=0;i<in->nr_others;i++)if(in->others[i].start>=in->others[i].end)return 0;
    /* This draft only accepts the exact observed S833 fixture extents.
     * Do not infer these sizes from an ACPI base on real hardware. */
    struct boot_range byte=in->aperture;
    if((in->backend==0&&(byte.start||byte.end))||
       (in->backend==1&&(byte.start!=UINT64_C(0xfed90000)||byte.end!=UINT64_C(0xfed90230)))||
       (in->backend==2&&(byte.start!=UINT64_C(0xfed80000)||byte.end!=UINT64_C(0xfed84000))))return 0;
    if(!ecam827_quarantine(mcfg,mn,drac,dn,map,n,stride,version,&work->ecam,&work->ecam_work))return 0;
    struct s835_report *r=&work->result;
    *r=(struct s835_report){0};r->aperture=byte;r->diagnostic=work->ecam.diagnostic;
    if(!in->backend){*out=*r;return 1;}
    if(byte.end>UINT64_MAX-4095)return 0;
    r->cover=(struct boot_range){byte.start,(byte.end+4095)&~UINT64_C(4095)};
    if(intersects(r->cover,work->ecam.ecam))return 0;
    for(size_t i=0;i<in->nr_others;i++)if(intersects(r->cover,in->others[i]))return 0;
    for(size_t i=0;i<r->diagnostic.nr_candidates;i++)if(intersects(r->cover,r->diagnostic.candidates[i]))return 0;
    const uint8_t *p=map;
    for(size_t i=0;i<n;i+=stride){
        struct boot_range v;uint64_t pages=le(p+i+24,8);
        if(!pages||pages>UINT64_MAX/4096||bm_extent(le(p+i+8,8),pages*4096,&v)!=BM_OK)return 0;
        if(!intersects(v,r->cover))continue;
        uint64_t type=le(p+i,4),attr=le(p+i+32,8);
        if((type!=0&&type!=11)||(attr>>63))return 0;
        uint64_t a=v.start>r->cover.start?v.start:r->cover.start,b=v.end<r->cover.end?v.end:r->cover.end;
        r->explicit_pages+=(b-a)/4096;
    }
    uint64_t total=(r->cover.end-r->cover.start)/4096;
    if(r->explicit_pages>total)return 0;r->gap_pages=total-r->explicit_pages;
    if(bm_add(&r->diagnostic,r->cover.start,r->cover.end-r->cover.start,1)!=BM_OK||bm_normalize(&r->diagnostic)!=BM_OK)return 0;
    if(r->diagnostic.nr_candidates!=work->ecam.diagnostic.nr_candidates)return 0;
    for(size_t i=0;i<r->diagnostic.nr_candidates;i++){
        struct boot_range a=r->diagnostic.candidates[i],b=work->ecam.diagnostic.candidates[i];
        if(a.start!=b.start||a.end!=b.end||intersects(a,r->cover))return 0;
    }
    int covered=0;for(size_t i=0;i<r->diagnostic.nr_reserved;i++)if(r->diagnostic.reserved[i].start<=r->cover.start&&r->cover.end<=r->diagnostic.reserved[i].end)covered=1;
    if(!covered)return 0;*out=*r;return 1;
}
