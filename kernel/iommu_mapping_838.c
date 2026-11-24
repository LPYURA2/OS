#include <styleos/iommu_mapping_838.h>
#define PAGE UINT64_C(4096)
#define LIMIT (UINT64_C(1)<<52)
static int valid_ranges(const struct boot_range *r,size_t n,uint64_t expected){
    uint64_t total=0;
    for(size_t i=0;i<n;i++){
        if(r[i].start>=r[i].end||r[i].end>LIMIT||((r[i].start|r[i].end)&(PAGE-1))||
           (i&&r[i-1].end>=r[i].start))return 0;
        total+=(r[i].end-r[i].start)/PAGE;
    }
    return total==expected;
}
static int covered(const struct boot_range *r,size_t n,struct boot_range cover){
    for(size_t i=0;i<n;i++)if(r[i].start<=cover.start&&r[i].end>=cover.end)return 1;
    return 0;
}
static int overlap(struct boot_range a,struct boot_range b){return a.start<b.end&&b.start<a.end;}
static int one_page(const struct s838_page *p,uint64_t a){
    const struct cache801_translation *f=&p->first,*l=&p->last;
    if(p->address!=a||!f->present||!l->present||!(f->leaf_entry&1)||
       f->physical!=a||l->physical!=a+PAGE-1||f->leaf_entry!=l->leaf_entry||
       f->page_size!=l->page_size||f->pat_index!=l->pat_index||f->pat_type!=l->pat_type)return 0;
    if(f->page_size!=PAGE&&f->page_size!=UINT64_C(0x200000)&&f->page_size!=UINT64_C(0x40000000))return 0;
    /* A reported translation must agree with its own raw leaf metadata. */
    uint64_t mask=(LIMIT-1)&~(PAGE-1), offset=f->page_size-1;
    if(f->page_size!=PAGE&&(!(f->leaf_entry&(UINT64_C(1)<<7))||
       (f->leaf_entry&offset&~UINT64_C(0x1fff))))return 0;
    if((f->leaf_entry&mask&~offset)+(a&offset)!=a)return 0;
    unsigned bit=f->page_size==PAGE?7u:12u;
    unsigned index=(unsigned)(((f->leaf_entry>>bit)&1u)<<2)|
                   (unsigned)(((f->leaf_entry>>4)&1u)<<1)|(unsigned)((f->leaf_entry>>3)&1u);
    if(f->pat_index!=index||(f->pat_type!=0&&f->pat_type!=6))return 0;
    return p->physical_cache.mtrr_enabled&&p->physical_cache.type==0&&
           p->physical_cache.matched<=CACHE800_MAX_VAR;
}
int s838_observe(const struct op_output *p,struct boot_range c,const struct s838_page *pages,size_t n){
    if(!p||p->valid!=1||p->granted||p->nr_protected>OP_RANGES_MAX||p->nr_review>OP_RANGES_MAX||
       !valid_ranges(p->protected_ranges,p->nr_protected,p->protected_pages)||
       !valid_ranges(p->review,p->nr_review,p->review_pages))return 0;
    for(size_t i=0,j=0;i<p->nr_protected&&j<p->nr_review;){
        struct boot_range x=p->protected_ranges[i],y=p->review[j];
        if(overlap(x,y))return 0;
        if(x.end<=y.start)i++;else j++;
    }
    if(!c.start&&!c.end)return n==0;
    /* Only the two independently observed S837 Q35 fixture covers. */
    if(!((c.start==UINT64_C(0xfed90000)&&c.end==UINT64_C(0xfed91000))||
         (c.start==UINT64_C(0xfed80000)&&c.end==UINT64_C(0xfed84000))))return 0;
    if(!pages||n!=(c.end-c.start)/PAGE||!covered(p->protected_ranges,p->nr_protected,c))return 0;
    for(size_t i=0;i<n;i++)if(!one_page(&pages[i],c.start+i*PAGE))return 0;
    return 1;
}
