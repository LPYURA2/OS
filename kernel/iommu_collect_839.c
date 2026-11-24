#include <styleos/iommu_collect_839.h>
static int separate(const void *a,size_t an,const void *b,size_t bn){
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return a&&b&&x<=UINTPTR_MAX-an&&y<=UINTPTR_MAX-bn&&(x+an<=y||y+bn<=x);
}
int s839_collect(const struct op_output *plan,struct boot_range cover,
                 const struct s839_snapshot *snapshot,cache801_read_entry reader,
                 void *context,struct s839_result *out){
    if(!separate(plan,sizeof *plan,out,sizeof *out)||
       !s838_observe(plan,(struct boot_range){0,0},0,0)||
       (snapshot&&!separate(snapshot,sizeof *snapshot,out,sizeof *out)))return 0;
    struct s839_result result={0};
    if(!cover.start&&!cover.end){*out=result;return 1;}
    /* No callbacks for an unsupported fixture or unprotected cover. */
    if(!((cover.start==UINT64_C(0xfed90000)&&cover.end==UINT64_C(0xfed91000))||
         (cover.start==UINT64_C(0xfed80000)&&cover.end==UINT64_C(0xfed84000))))return 0;
    int protected=0;
    for(size_t i=0;i<plan->nr_protected;i++)
        if(plan->protected_ranges[i].start<=cover.start&&plan->protected_ranges[i].end>=cover.end)protected=1;
    if(!protected||!reader||!snapshot||
       snapshot->physical_bits<36||snapshot->physical_bits>52||
       snapshot->count>CACHE800_MAX_VAR||snapshot->count!=(uint8_t)(snapshot->cap&255u))return 0;
    result.count=(size_t)((cover.end-cover.start)/4096u);
    if(result.count>S839_MAX_PAGES)return 0;
    for(size_t i=0;i<result.count;i++){
        struct s838_page *p=&result.pages[i];
        p->address=cover.start+i*4096u;
        if(!cache801_walk(snapshot->root,p->address,snapshot->pat,snapshot->physical_bits,reader,context,&p->first)||
           !cache801_walk(snapshot->root,p->address+4095u,snapshot->pat,snapshot->physical_bits,reader,context,&p->last)||
           !cache800_physical_type(p->address,snapshot->physical_bits,snapshot->cap,snapshot->def,
                                 snapshot->pairs,snapshot->count,&p->physical_cache))return 0;
    }
    if(!s838_observe(plan,cover,result.pages,result.count))return 0;
    *out=result;return 1;
}
