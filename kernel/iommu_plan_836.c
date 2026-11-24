#include <styleos/iommu_plan_836.h>
#define PAGE UINT64_C(4096)
#define LIMIT (UINT64_C(1)<<52)
static int separate(const void *a,size_t an,const void *b,size_t bn){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return a&&b&&x<=UINTPTR_MAX-an&&y<=UINTPTR_MAX-bn&&(x+an<=y||y+bn<=x);
}
static int equal(struct boot_range a,struct boot_range b){return a.start==b.start&&a.end==b.end;}
static int intersects(struct boot_range a,struct boot_range b){return a.start<b.end&&b.start<a.end;}
static int contains(const struct boot_range *a,size_t n,struct boot_range r){
 for(size_t i=0;i<n;i++)if(a[i].start<=r.start&&a[i].end>=r.end)return 1;
 return 0;
}
static int ranges(const struct boot_range *a,size_t n,int page,uint64_t *count){
 uint64_t sum=0;
 for(size_t i=0;i<n;i++){
  if(a[i].start>=a[i].end||a[i].end>LIMIT||(page&&((a[i].start|a[i].end)&(PAGE-1)))||
     (i&&(a[i-1].end>a[i].start||(page&&a[i-1].end==a[i].start))))return 0;
  sum+=(a[i].end-a[i].start)/PAGE;
 }
 if(count)*count=sum;
 return 1;
}
static int same_set(const struct boot_range *a,const struct boot_range *b,size_t n){
 for(size_t i=0;i<n;i++)if(!equal(a[i],b[i]))return 0;
 return 1;
}
static int append(struct op_output *p,struct boot_range r){
 size_t n=p->nr_protected;
 if(n&&r.start<=p->protected_ranges[n-1].end){
  if(r.end>p->protected_ranges[n-1].end)p->protected_ranges[n-1].end=r.end;
  return 1;
 }
 if(n==OP_RANGES_MAX)return 0;
 p->protected_ranges[p->nr_protected++]=r;return 1;
}
int s836_extend(const struct op_output *old,const struct s835_report *q,
                struct op_output *out,struct s836_work *work){
 if(!separate(out,sizeof *out,work,sizeof *work)||
    !separate(out,sizeof *out,old,sizeof *old)||!separate(out,sizeof *out,q,sizeof *q)||
    !separate(work,sizeof *work,old,sizeof *old)||!separate(work,sizeof *work,q,sizeof *q))return 0;
 const struct boot_memory_map *m=&q->diagnostic;
 uint64_t count,review;
 if(old->valid!=1||old->granted||old->nr_protected>OP_RANGES_MAX||old->nr_review>OP_RANGES_MAX||
    !ranges(old->protected_ranges,old->nr_protected,1,&count)||count!=old->protected_pages||
    !ranges(old->review,old->nr_review,1,&review)||review!=old->review_pages)return 0;
 if(!m->nr_ram||!m->nr_candidates||m->nr_ram>BOOT_MEMORY_INPUT_MAX||m->nr_reserved>BOOT_MEMORY_INPUT_MAX||m->nr_candidates>BOOT_MEMORY_RANGE_MAX||
    !ranges(m->ram,m->nr_ram,0,0)||!ranges(m->reserved,m->nr_reserved,1,0)||!ranges(m->candidates,m->nr_candidates,1,0))return 0;
 work->checked=*m;
 if(bm_normalize(&work->checked)!=BM_OK||work->checked.nr_ram!=m->nr_ram||work->checked.nr_reserved!=m->nr_reserved||work->checked.nr_candidates!=m->nr_candidates||
    !same_set(work->checked.ram,m->ram,m->nr_ram)||!same_set(work->checked.reserved,m->reserved,m->nr_reserved)||!same_set(work->checked.candidates,m->candidates,m->nr_candidates))return 0;
 struct boot_range a=q->aperture,c=q->cover;
 if(a.start==0&&a.end==0){if(c.start||c.end||q->explicit_pages||q->gap_pages)return 0;}
 else {
  if(a.start>=a.end||a.end>LIMIT||a.start!=(a.start&~(PAGE-1))||c.start!=a.start||c.end!=((a.end+PAGE-1)&~(PAGE-1))||
     !contains(m->reserved,m->nr_reserved,c))return 0;
  uint64_t pages=(c.end-c.start)/PAGE;
  if(q->explicit_pages>pages||q->gap_pages!=pages-q->explicit_pages)return 0;
 }
 /* No new review range may be invented or an existing one silently removed. */
 for(size_t i=0;i<old->nr_review;i++){
  struct boot_range r=old->review[i];if(!contains(m->candidates,m->nr_candidates,r))return 0;
  for(size_t j=0;j<old->nr_protected;j++)if(intersects(r,old->protected_ranges[j]))return 0;
  for(size_t j=0;j<m->nr_reserved;j++)if(intersects(r,m->reserved[j]))return 0;
 }
 struct op_output *p=&work->plan;*p=(struct op_output){0};
 size_t i=0,j=0;
 while(i<old->nr_protected||j<m->nr_reserved){
  struct boot_range r;
  if(j==m->nr_reserved||(i<old->nr_protected&&old->protected_ranges[i].start<=m->reserved[j].start))r=old->protected_ranges[i++];
  else r=m->reserved[j++];
  if(!append(p,r))return 0;
 }
 if(!ranges(p->protected_ranges,p->nr_protected,1,&p->protected_pages))return 0;
 p->nr_review=old->nr_review;p->review_pages=old->review_pages;
 for(i=0;i<p->nr_review;i++)p->review[i]=old->review[i];
 p->valid=1;p->granted=0;*out=*p;return 1;
}
