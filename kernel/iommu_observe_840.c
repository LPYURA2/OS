#include <styleos/iommu_collect_839.h>
#include <styleos/arch.h>
/* Q35/OVMF diagnostic only. No target VA or MMIO dereference. New table
 * pages require full baseline reservation, EFI BootServicesData, and live
 * endpoint identity walks through previously proved S837 table pages. */
struct context {const struct boot_info *boot;const struct op_input *in;const struct s839_snapshot *snapshot;};
static void field(const char *k,uint64_t v){
 arch_putc(' ');arch_puts(k);arch_puts("=0x");
 for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)i)&15u]);
}
static uint64_t msr(uint32_t n){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return ((uint64_t)d<<32)|a;}
static uint32_t caps(void){uint32_t a=1,b,c,d;__asm__ volatile("cpuid":"+a"(a),"=b"(b),"=c"(c),"=d"(d):"c"(0));return d;}
static uint64_t le(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8u*i);return v;}
static bool safe(const struct context *c,uint64_t pa){
 if(!pa||(pa&4095u)||pa>=UINT64_C(0x08000000)||!c->snapshot)return false;
 struct cache800_result lo,hi;const struct s839_snapshot *s=c->snapshot;
 if(!cache800_physical_type(pa,s->physical_bits,s->cap,s->def,s->pairs,s->count,&lo)||
    !cache800_physical_type(pa+4095,s->physical_bits,s->cap,s->def,s->pairs,s->count,&hi)||
    !lo.mtrr_enabled||!hi.mtrr_enabled||lo.type!=6||hi.type!=6)return false;
 const struct boot_memory_map *m=c->in->map;
 if(!m||m->nr_reserved>BOOT_MEMORY_INPUT_MAX||m->nr_candidates>BOOT_MEMORY_RANGE_MAX)return false;
 bool reserved=false;
 for(size_t i=0;i<m->nr_reserved;i++)if(m->reserved[i].start<=pa&&pa+4096<=m->reserved[i].end)reserved=true;
 if(!reserved)return false;
 for(size_t i=0;i<m->nr_candidates;i++)if(m->candidates[i].start<pa+4096&&pa<m->candidates[i].end)return false;
 const struct boot_info *b=c->boot;
 if(!b->memory_map||b->descriptor_size<40||b->descriptor_size>4096||
    !b->memory_map_size||b->memory_map_size%b->descriptor_size)return false;
 for(uint64_t off=0;off<b->memory_map_size;off+=b->descriptor_size){
  const uint8_t *p=(const uint8_t *)(uintptr_t)b->memory_map+off;
  uint64_t start=le(p+8,8),pages=le(p+24,8);
  if(le(p,4)==4&&le(p+32,8)&8u&&pages&&pages<=UINT64_MAX/4096u&&
     start<=pa&&pa-start<pages*4096u&&pages*4096u-(pa-start)>=4096u)return true;
 }
 return false;
}
static void record(const char *tag,uint64_t pa,uint16_t slot,uint64_t value){
 arch_puts("STYLEOS S840 ");arch_puts(tag);field("table",pa);field("slot",slot);field("entry",value);arch_putc('\n');
}
/* Bind sampled entries to the original S837 identity proof. Hardware A/D
 * changes are the only tolerated difference. */
static bool consistent(const struct op_input *in,uint64_t pa,uint16_t slot,uint64_t entry){
 for(size_t i=0;i<in->nr_walks;i++){
  const struct op_walk *w=&in->walks[i];
  if(w->levels>4)return false;
  for(unsigned j=0;j<w->levels;j++)
   if(w->tables[j]==pa&&((w->va>>(39u-9u*j))&511u)==slot&&
      ((w->entries[j]^entry)&~UINT64_C(0x60)))return false;
 }
 return true;
}
static bool known(void *ptr,uint64_t pa,uint16_t slot,uint64_t *v){
 const struct context *c=ptr;
 if(!v||slot>=512||!safe(c,pa))return false;
 for(size_t i=0;i<c->in->nr_walks;i++){
  const struct op_walk *w=&c->in->walks[i];
  if(w->kind==OP_WALK_TABLE&&w->va==pa&&w->pa==pa){
   *v=((const volatile uint64_t *)(uintptr_t)pa)[slot];record("IDENTITY",pa,slot,*v);return consistent(c->in,pa,slot,*v);
  }
 }
 return false;
}
static bool target(void *ptr,uint64_t pa,uint16_t slot,uint64_t *v){
 const struct context *c=ptr;const struct op_snapshot *s=&c->in->snapshot;
 struct cache801_translation a={0},b={0};
 if(!v||slot>=512||!safe(c,pa)||
    !cache801_walk(s->root,pa,s->pat,(uint8_t)s->pa_bits,known,ptr,&a)||
    !cache801_walk(s->root,pa+4095,s->pat,(uint8_t)s->pa_bits,known,ptr,&b)||
    !a.present||!b.present||a.physical!=pa||b.physical!=pa+4095||
    a.leaf_entry!=b.leaf_entry||a.page_size!=b.page_size||a.pat_type!=6||b.pat_type!=6){
  record("DENY",pa,slot,0);return false;
 }
 *v=((const volatile uint64_t *)(uintptr_t)pa)[slot];record("READ",pa,slot,*v);return consistent(c->in,pa,slot,*v);
}
#ifdef STYLEOS_TEST_ALIAS_CENSUS_841
/* Separate diagnostic profile. Scan each reachable table path, not just
 * unique physical pages: a shared page can create distinct virtual aliases.
 * No raw data/MMIO read; use exactly the S840 RAM admission gates. */
#define S841_MAX_PATHS 64u
struct census {struct context *ctx;unsigned visited,denied;};
static bool quiet_known(void *ptr,uint64_t pa,uint16_t slot,uint64_t *v){
 const struct context *c=ptr;
 if(!v||slot>=512||!safe(c,pa))return false;
 for(size_t i=0;i<c->in->nr_walks;i++){
  const struct op_walk *w=&c->in->walks[i];
  if(w->kind==OP_WALK_TABLE&&w->va==pa&&w->pa==pa){
   *v=((const volatile uint64_t *)(uintptr_t)pa)[slot];
   arch_puts("STYLEOS S841 PROOF");field("table",pa);field("slot",slot);field("entry",*v);arch_putc('\n');
   return consistent(c->in,pa,slot,*v);
  }
 }
 return false;
}
static bool admit841(struct context *c,uint64_t pa){
 const struct s839_snapshot *s=c->snapshot;
 struct cache801_translation a={0},b={0};
 return safe(c,pa)&&cache801_walk(s->root,pa,s->pat,s->physical_bits,quiet_known,c,&a)&&
  cache801_walk(s->root,pa+4095,s->pat,s->physical_bits,quiet_known,c,&b)&&
  a.present&&b.present&&a.physical==pa&&b.physical==pa+4095&&a.leaf_entry==b.leaf_entry&&
  a.page_size==b.page_size&&a.pat_type==6&&b.pat_type==6;
}
static int scan841(struct census *q,uint64_t pa,uint64_t prefix,unsigned level){
 struct context *c=q->ctx;
 bool admitted=q->visited<S841_MAX_PATHS&&admit841(c,pa);
 arch_puts("STYLEOS S841 PATH");field("pa",pa);field("prefix",prefix);field("level",level);
 if(!admitted){
  field("admitted",0);arch_putc('\n');q->denied++;return 1;
 }
 field("admitted",1);arch_putc('\n');q->visited++;
 /* Read and emit a bounded row, then follow exactly its child entries.
  * Four recursive levels; no full-page stack frame or compiler stack probe. */
 uint64_t mask=((UINT64_C(1)<<c->snapshot->physical_bits)-1)&~UINT64_C(4095);
 uint64_t entries[16];
 for(unsigned base=0;base<512;base+=16){
  for(unsigned j=0;j<16;j++){
   unsigned i=base+j;
   entries[j]=((const volatile uint64_t *)(uintptr_t)pa)[i];
   if(!consistent(c->in,pa,(uint16_t)i,entries[j]))return 0;
  }
  arch_puts("STYLEOS S841 ROW");field("slot",base);
  for(unsigned j=0;j<16;j++)field("e",entries[j]);
  arch_putc('\n');
  for(unsigned j=0;j<16;j++){
   unsigned i=base+j;uint64_t e=entries[j];
   if(!(e&1))continue;
   if(e&(((UINT64_C(1)<<52)-1)&~(mask|UINT64_C(4095))))return 0;
   if(level==4&&(e&128))return 0;
   if(level==1||(e&128)){
    uint64_t size=UINT64_C(1)<<(12u+9u*(level-1u));
    if(level==3&&!(c->in->snapshot.features&2u))return 0;
    if(level>1&&(e&(size-1)&~UINT64_C(0x1fff)))return 0;
    continue;
   }
   if(!scan841(q,e&mask,prefix|((uint64_t)i<<(12u+9u*(level-1u))),level-1))return 0;
  }
 }
 return 1;
}
static int census841(struct context *ctx){
 const struct op_snapshot *s=&ctx->in->snapshot;
 if(s->aux0&(UINT64_C(1)<<12))return 0; /* no LA57 */
 struct census q={ctx,0,0};
 if(!scan841(&q,ctx->snapshot->root&~UINT64_C(4095),0,4))return 0;
 arch_puts("STYLEOS S841 TOTAL");field("paths",q.visited);field("denied",q.denied);arch_putc('\n');
 arch_puts("STYLEOS S841 POLICY target-read=no mappings-changed=no dma-grants=no production-cr3=deny\n");
 return 1;
}
#endif
int s840_observe_guest(const struct boot_info *boot,const struct op_input *in,const struct op_output *plan,struct boot_range cover){
 struct op_snapshot now;
 if(!boot||!in||!plan||in->nr_walks>OP_WALKS_MAX||!op_arch_unchanged(in,&now))return 0;
 if((caps()&((1u<<5)|(1u<<12)|(1u<<16)))!=((1u<<5)|(1u<<12)|(1u<<16)))return 0;
 if(now.mode&(UINT64_C(3)<<29))return 0;
 struct s839_snapshot s={.root=now.root,.pat=now.pat,.cap=msr(0xfe),.def=msr(0x2ff),.physical_bits=(uint8_t)now.pa_bits};
 s.count=(uint8_t)(s.cap&255u);
 if(s.count>CACHE800_MAX_VAR)return 0;
 arch_puts("STYLEOS S840 CPU");field("root",s.root);field("pat",s.pat);field("bits",s.physical_bits);field("cap",s.cap);field("def",s.def);field("count",s.count);arch_putc('\n');
 for(uint8_t i=0;i<s.count;i++){
  s.pairs[i].base=msr(0x200u+2u*i);s.pairs[i].mask=msr(0x201u+2u*i);
  arch_puts("STYLEOS S840 MTRR");field("index",i);field("base",s.pairs[i].base);field("mask",s.pairs[i].mask);arch_putc('\n');
 }
 struct s839_result result={0};struct context ctx={boot,in,&s};
 int ok=s839_collect(plan,cover,&s,target,&ctx,&result);
 if(!op_arch_unchanged(in,&now)||msr(0xfe)!=s.cap||msr(0x2ff)!=s.def)return 0;
 for(uint8_t i=0;i<s.count;i++)if(msr(0x200u+2u*i)!=s.pairs[i].base||msr(0x201u+2u*i)!=s.pairs[i].mask)return 0;
 arch_puts("STYLEOS S840 RESULT");field("accepted",ok);field("pages",result.count);arch_putc('\n');
 if(ok)for(size_t i=0;i<result.count;i++){
  const struct s838_page *p=&result.pages[i];
  arch_puts("STYLEOS S840 PAGE");field("address",p->address);field("first",p->first.physical);field("last",p->last.physical);
  field("leaf",p->first.leaf_entry);field("size",p->first.page_size);field("pat",p->first.pat_type);
  field("mtrr",p->physical_cache.type);field("matches",p->physical_cache.matched);arch_putc('\n');
 }
 arch_puts("STYLEOS S840 POLICY target-read=no mappings-changed=no dma-grants=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_ALIAS_CENSUS_841
 if(!census841(&ctx)||!op_arch_unchanged(in,&now)||msr(0xfe)!=s.cap||msr(0x2ff)!=s.def)return 0;
 for(uint8_t i=0;i<s.count;i++)if(msr(0x200u+2u*i)!=s.pairs[i].base||msr(0x201u+2u*i)!=s.pairs[i].mask)return 0;
#endif
 return 1;
}
