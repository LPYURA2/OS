#include <styleos/physical_arena_844.h>
#define P BOOT_MEMORY_PAGE
#define LOW (UINT64_C(1)<<47)
static bool overlap(struct boot_range a,struct boot_range b){return a.start<b.end&&b.start<a.end;}
static bool inside(struct boot_range a,struct boot_range b){return a.start<a.end&&b.start<=a.start&&a.end<=b.end;}
static bool valid(struct boot_range r){return r.start<r.end;}
static bool object(const void *v,size_t n,struct boot_range *r){
 uint64_t x=(uintptr_t)v;if(!x||n>UINT64_MAX-x)return false;
 *r=(struct boot_range){x,x+n};return true;
}
static bool base_ok(uint64_t x){return x>=P&&!(x&(P-1))&&x<=LOW-S844_BYTES;}
static bool separated(const struct s844_arena *a,uint64_t base){
 struct boot_range r;return object(a,sizeof *a,&r)&&!overlap(r,(struct boot_range){base,base+S844_BYTES});
}
static bool shape(const struct s844_arena *a,uint32_t *mask){
 if(!a||((uintptr_t)a%_Alignof(struct s844_arena))||!a->ready||!base_ok(a->base)||!a->next||!separated(a,a->base))return false;
 uint32_t used=0;
 for(unsigned i=0;i<S844_PAGES;i++){
  const struct s844_lease *s=&a->slots[i];
  if(!s->generation){if(s->pa||s->owner||s->pages)return false;continue;}
  if(s->pa!=a->base+i*P||!s->owner||!s->pages||s->pages>S844_PAGES-i||s->generation>=a->next)return false;
  for(unsigned j=0;j<i;j++)if(a->slots[j].generation==s->generation)return false;
  uint32_t m=((UINT32_C(1)<<s->pages)-1)<<i;
  if(used&m)return false;
  used|=m;
 }
 *mask=used;return true;
}
static void zero(uint64_t pa,unsigned pages){
 volatile unsigned char *v=(volatile unsigned char *)(uintptr_t)pa;
 for(size_t i=0;i<(size_t)pages*P;i++)v[i]=0;
}
bool s844_init(struct s844_arena *a,uint64_t base){
 if(!a||((uintptr_t)a%_Alignof(struct s844_arena))||a->ready||!base_ok(base)||!separated(a,base))return false;
 *a=(struct s844_arena){.base=base,.next=1,.ready=true};return true;
}
bool s844_count(const struct s844_arena *a,unsigned *out){
 uint32_t used;struct boot_range r,m;
 if(!shape(a,&used)||!object(out,sizeof *out,&r)||((uintptr_t)out%_Alignof(unsigned))||!object(a,sizeof *a,&m)||
 overlap(r,m)||overlap(r,(struct boot_range){a->base,a->base+S844_BYTES}))return false;
 unsigned n=0;for(unsigned i=0;i<S844_PAGES;i++)n+=(used>>i)&1u;*out=n;return true;
}
bool s844_acquire(struct s844_arena *a,uint32_t owner,uint32_t pages,struct s844_lease *out){
 uint32_t used;struct boot_range r,m;
 if(!shape(a,&used)||!owner||!pages||pages>S844_PAGES||a->next==UINT64_MAX||
 !object(out,sizeof *out,&r)||!object(a,sizeof *a,&m)||((uintptr_t)out%_Alignof(struct s844_lease))||
 overlap(r,m)||overlap(r,(struct boot_range){a->base,a->base+S844_BYTES}))return false;
 for(unsigned i=0;i<=S844_PAGES-pages;i++){
  uint32_t bits=((UINT32_C(1)<<pages)-1)<<i;if(used&bits)continue;
  struct s844_lease s={a->base+i*P,a->next,owner,pages};
  zero(s.pa,pages);a->slots[i]=s;a->next++;*out=s;return true;
 }
 return false;
}
bool s844_release(struct s844_arena *a,uint32_t owner,struct s844_lease v){
 uint32_t used;
 if(!shape(a,&used)||!owner||owner!=v.owner||!v.generation||
 v.pa<a->base||v.pa>=a->base+S844_BYTES||(v.pa&(P-1)))return false;
 const unsigned i=(unsigned)((v.pa-a->base)/P);
 const struct s844_lease *s=&a->slots[i];
 if(s->pa!=v.pa||s->generation!=v.generation||s->owner!=owner||s->pages!=v.pages)return false;
 zero(v.pa,v.pages);a->slots[i]=(struct s844_lease){0};return true;
}
static uint64_t le(const unsigned char *p,unsigned n){
 uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8u*i);return v;
}
static bool descriptor(const unsigned char *p,struct boot_range *r){
 uint64_t x=le(p+8,8),n=le(p+24,8);
 if((x&(P-1))||!n||n>(UINT64_MAX-x)/P)return false;
 *r=(struct boot_range){x,x+n*P};return true;
}
/* The input is the trusted, post-EBS source buffer already admitted by
 * S842, not an arbitrary pointer supplied by a process. Numeric evidence
 * does not substitute for the boot adapter's actual AllocatePages receipt. */
bool s844_efi_proof(const struct boot_memory_evidence *e,const struct boot_info *b,
 struct boot_range a,struct boot_range *desc,uint64_t *attr){
 if(!e||!b||!desc||!attr||!e->map||!base_ok(a.start)||a.end!=a.start+S844_BYTES||
 a.start<0x100000u||a.end>0x08000000u||
 b->magic!=STYLEOS_BOOT_MAGIC||b->version!=1||b->size!=sizeof *b||
 b->arch!=STYLEOS_ARCH_X86_64||b->platform!=STYLEOS_PLATFORM_UEFI||
 b->flags!=STYLEOS_BOOT_EXITED_SERVICES||b->descriptor_version!=1||
 b->descriptor_size<40||b->descriptor_size>4096||(b->descriptor_size&7)||
 !b->memory_map||!b->memory_map_size||b->memory_map_size%b->descriptor_size||
 b->memory_map_size/b->descriptor_size>BOOT_MEMORY_INPUT_MAX||
 b->memory_map_size>UINT64_MAX-b->memory_map||
 e->live.source.start!=b->memory_map||e->live.source.end!=b->memory_map+b->memory_map_size)return false;
 const struct boot_range protect[]={e->live.image,e->live.stack,e->live.handoff,e->live.source,e->workspace};
 for(unsigned i=0;i<sizeof protect/sizeof protect[0];i++)if(!valid(protect[i])||overlap(a,protect[i]))return false;
 const struct boot_memory_map *m=e->map;
 if(m->nr_ram>BOOT_MEMORY_INPUT_MAX||m->nr_reserved>BOOT_MEMORY_INPUT_MAX||m->nr_candidates>BOOT_MEMORY_RANGE_MAX)return false;
 bool reserved=false;
 for(size_t i=0;i<m->nr_reserved;i++){if(!valid(m->reserved[i]))return false;reserved|=inside(a,m->reserved[i]);}
 for(size_t i=0;i<m->nr_ram;i++)if(!valid(m->ram[i])||overlap(a,m->ram[i]))return false;
 for(size_t i=0;i<m->nr_candidates;i++)if(!valid(m->candidates[i])||overlap(a,m->candidates[i]))return false;
 if(!reserved)return false;
 const unsigned char *raw=(const void *)(uintptr_t)b->memory_map;
 size_t stride=b->descriptor_size,size=(size_t)b->memory_map_size;
 struct boot_range got={0};uint64_t flags=0;unsigned count=0;
 for(size_t i=0;i<size;i+=stride){
  struct boot_range r;if(!descriptor(raw+i,&r))return false;
  for(size_t j=0;j<i;j+=stride){struct boot_range x;if(!descriptor(raw+j,&x)||overlap(r,x))return false;}
  if(!overlap(a,r))continue;
  flags=le(raw+i+32,8);
  if(le(raw+i,4)!=2||!(flags&8u)||(flags>>63)||!inside(a,r))return false;
  got=r;count++;
 }
 if(count!=1)return false;
 *desc=got;*attr=flags;return true;
}
