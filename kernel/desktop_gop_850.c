#include <styleos/desktop_gop_850.h>
#define P UINT64_C(4096)
#define MAX_FB UINT64_C(16777216)
static bool overlap(struct boot_range a,struct boot_range b){return a.start<b.end&&b.start<a.end;}
static bool valid(struct boot_range a){return a.start<a.end;}
static uint64_t le(const unsigned char *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static bool desc(const unsigned char *p,struct boot_range *r){
 uint64_t base=le(p+8,8),n=le(p+24,8);
 if((base&(P-1))||!n||n>(UINT64_MAX-base)/P)return false;
 *r=(struct boot_range){base,base+n*P};return true;
}
static bool excluded(const struct boot_memory_evidence *e,struct boot_range fb){
 if(!e||!e->map||e->map->nr_ram>BOOT_MEMORY_INPUT_MAX||e->map->nr_candidates>BOOT_MEMORY_RANGE_MAX||e->map->nr_reserved>BOOT_MEMORY_INPUT_MAX)return false;
 const struct boot_range live[]={e->live.image,e->live.stack,e->live.handoff,e->live.source,e->workspace};
 for(unsigned i=0;i<sizeof live/sizeof live[0];i++)if(!valid(live[i])||overlap(fb,live[i]))return false;
 for(size_t i=0;i<e->map->nr_ram;i++)if(!valid(e->map->ram[i])||overlap(fb,e->map->ram[i]))return false;
 for(size_t i=0;i<e->map->nr_candidates;i++)if(!valid(e->map->candidates[i])||overlap(fb,e->map->candidates[i]))return false;
 /* Reserved may include device apertures, hence raw descriptor check below. */
 for(size_t i=0;i<e->map->nr_reserved;i++)if(!valid(e->map->reserved[i]))return false;
 return true;
}
static bool map_ok(const struct boot_info *b,const struct boot_memory_evidence *e,struct boot_range fb){
 if(!b||b->magic!=STYLEOS_BOOT_MAGIC||b->version!=1||b->size!=sizeof *b||b->arch!=STYLEOS_ARCH_X86_64||b->platform!=STYLEOS_PLATFORM_UEFI||b->flags!=STYLEOS_BOOT_EXITED_SERVICES||b->descriptor_version!=1||b->descriptor_size<40||b->descriptor_size>4096||(b->descriptor_size&7)||!b->memory_map||!b->memory_map_size||b->memory_map_size%b->descriptor_size||b->memory_map_size/b->descriptor_size>BOOT_MEMORY_INPUT_MAX||b->memory_map_size>UINTPTR_MAX-b->memory_map)return false;
 if(e->live.handoff.start!=(uintptr_t)b||e->live.handoff.end-e->live.handoff.start!=sizeof *b||e->live.source.start!=b->memory_map||e->live.source.end!=b->memory_map+b->memory_map_size)return false;
 const unsigned char *p=(const void *)(uintptr_t)b->memory_map;size_t stride=b->descriptor_size;
 for(size_t i=0;i<b->memory_map_size;i+=stride){
  struct boot_range a;if(!desc(p+i,&a))return false;
  if(overlap(a,fb)&&(le(p+i,4)!=11||(le(p+i+32,8)>>63)))return false;
  for(size_t j=0;j<i;j+=stride){struct boot_range c;if(!desc(p+j,&c)||overlap(a,c))return false;}
 }
 return true; /* gaps are not RAM grants: caller must prove exclusive BAR */
}
static bool cache_ok(const struct s842_cache *c){
 if(!c||c->bits<36||c->bits>52||c->count>CACHE800_MAX_VAR||c->count!=(c->cap&255u))return false;
 for(unsigned i=0;i<8;i++){unsigned t=(c->pat>>(8*i))&255u;if(t!=0&&t!=1&&t!=4&&t!=5&&t!=6&&t!=7)return false;}
 uint64_t high=((UINT64_C(1)<<c->bits)-1)&~(P-1);
 for(unsigned i=0;i<c->count;i++){
  uint64_t base=c->pairs[i].base,mask=c->pairs[i].mask;
  if((base&~(high|UINT64_C(0xff)))||(mask&~(high|UINT64_C(0x800))))return false;
  if(mask&0x800u){
   uint64_t holes=(high&~mask)>>12;
   if((holes&(holes+1))||(base&high&~mask))return false;
  }
 }
 return true;
}
static bool page_ok(const struct s850_gop_request *r,uint64_t pa){
 const struct s842_cache *c=r->cache;
 for(unsigned i=0;i<2;i++){
  uint64_t a=pa+(i?P-1:0);struct cache800_result m;struct cache801_translation t;
  if(!cache800_physical_type(a,c->bits,c->cap,c->def,c->pairs,c->count,&m)||!m.mtrr_enabled||m.type!=0||!cache801_walk(r->inherited_cr3,a,c->pat,c->bits,r->read_entry,r->read_context,&t)||!t.present||t.physical!=a||(t.pat_type!=0&&t.pat_type!=6))return false;
 }
 return true;
}
bool s850_gop_prepare(const struct s850_gop_request *r,struct s850_gop_plan *out){
 if(!out)return false;
 *out=(struct s850_gop_plan){0};
 if(!r||!r->memory||!r->read_entry||!r->inherited_cr3||!cache_ok(r->cache)||!desktop_surface_valid(r->surface))return false;
 const struct desktop_surface *s=r->surface;
 uint64_t bytes=UINT64_C(4)*s->stride*s->height;
 /* Require full pages within the reported GOP buffer, not merely BAR padding. */
 uint64_t rounded=(bytes+P-1)&~(P-1),base=s->base;
 if((base&(P-1))||!rounded||rounded>MAX_FB||rounded>s->bytes||base<UINT64_C(0x100000)||base>=(UINT64_C(1)<<47)||rounded>(UINT64_C(1)<<47)-base)return false;
 struct pci795_aperture bar;
 if(!r->bar||r->bar->base!=(((uint64_t)r->bar->raw_hi<<32)|(r->bar->raw_lo&~UINT32_C(15)))||r->bar->prefetch!=((r->bar->raw_lo&8u)!=0)||!pci795_decode_aperture(r->bar,r->mask_lo,r->mask_hi,base,s->bytes,&bar)||rounded>bar.bytes)return false;
 struct boot_range fb={base,base+rounded};
 if(!excluded(r->memory,fb)||!map_ok(r->boot,r->memory,fb))return false;
 unsigned idx=8;for(unsigned i=0;i<8;i++)if(((r->cache->pat>>(8*i))&255u)==0){idx=i;break;}
 if(idx==8)return false;
 for(uint64_t a=fb.start;a<fb.end;a+=P)if(!page_ok(r,a))return false;
 *out=(struct s850_gop_plan){.mapping={base,base,rounded,PG_WRITE},.cache_bits=((idx&1)?8u:0u)|((idx&2)?16u:0u)|((idx&4)?128u:0u),.pages=rounded/P,.bar_bytes=bar.bytes,.pat_index=(uint8_t)idx,.ready=true};
 return true;
}
