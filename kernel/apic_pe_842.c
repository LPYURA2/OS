#include <styleos/apic_pe_842.h>
#define P UINT64_C(4096)
#if defined(STYLEOS_TEST_PHYSICAL_ARENA_844) || defined(STYLEOS_TEST_RUNTIME_ARENA_858)
bool s844_admit_efi(const struct boot_memory_evidence *,const struct boot_info *,struct boot_range *);
#endif
static bool inside(struct boot_range a,struct boot_range b){return a.start<a.end&&b.start<=a.start&&a.end<=b.end;}
static bool overlap(struct boot_range a,struct boot_range b){return a.start<b.end&&b.start<a.end;}
static bool reserved_ram(const struct boot_memory_map *m,struct boot_range r,bool loader_proved){
 bool ram=false,reserved=false;
 if(!m||m->nr_ram>BOOT_MEMORY_INPUT_MAX||m->nr_reserved>BOOT_MEMORY_INPUT_MAX||m->nr_candidates>BOOT_MEMORY_RANGE_MAX||r.start>=r.end)return false;
 for(size_t i=0;i<m->nr_ram;i++){if(m->ram[i].start>=m->ram[i].end)return false;ram|=inside(r,m->ram[i]);}
 for(size_t i=0;i<m->nr_reserved;i++){if(m->reserved[i].start>=m->reserved[i].end)return false;reserved|=inside(r,m->reserved[i]);}
 for(size_t i=0;i<m->nr_candidates;i++){if(m->candidates[i].start>=m->candidates[i].end||overlap(r,m->candidates[i]))return false;}
 return (ram||loader_proved)&&reserved;
}
static bool build_pe(const struct s790_low_request *pe,
 const struct boot_memory_map *map,const struct s842_cache *cache,
 const struct acpi806_topology *t,uint64_t msr,struct s842_root *out,bool loader_proved,struct boot_range extra){
 if(!out)return false;
 *out=(struct s842_root){0};
 if(!pe||!cache||!t||cache->bits<36||cache->bits>52||t->enabled_lapics!=1||t->ioapic_count!=1||t->iso_count>ACPI806_MAX_ISO)return false;
 /* xAPIC only. MSR base must match the validated MADT; no invented IOAPIC pin count. */
 uint64_t mask=((UINT64_C(1)<<cache->bits)-1)&~(P-1);
 if(!(msr&(1u<<11))||(msr&(1u<<10))||(msr&mask)!=t->lapic_physical||(msr&~(mask|UINT64_C(0x900))))return false;
 uint64_t lapic=t->lapic_physical,io=t->ioapic[0].physical;
 if(!lapic||!io||lapic==io||(lapic|io)&(P-1)||t->ioapic[0].id>15)return false;
 if(pe->boot_stack.end>UINT64_MAX-(P-1)||pe->boot_stack.start>=pe->boot_stack.end||pe->owned_tables_pa>UINT64_MAX-PG_TABLE_BYTES)return false;
 struct boot_range stack={pe->boot_stack.start&~(P-1),(pe->boot_stack.end+P-1)&~(P-1)};
 struct boot_range tables={pe->owned_tables_pa,pe->owned_tables_pa+PG_TABLE_BYTES};
 if(!inside(tables,pe->image)||overlap(tables,stack)||!reserved_ram(map,pe->image,loader_proved)||!reserved_ram(map,stack,loader_proved))return false;
 struct s790_low_result low;
 if(!s790_prepare_owned_low(pe,&low)||!s790_audit_owned_low(&low,pe->required_exec,pe->required_write,pe->required_rsp))return false;
 low.plan.pa_bits=cache->bits;
 if(extra.start||extra.end){
  if(!extra.start||extra.start>=extra.end||low.plan.count>=PG_MAX_MAPS-2)return false;
  low.plan.maps[low.plan.count++]=(struct pg_mapping){extra.start,extra.start,extra.end-extra.start,PG_WRITE};
 }
 return s842_build(&low.plan,map,cache,lapic,io,out);
}

bool s842_build_pe(const struct s790_low_request *pe,
 const struct boot_memory_map *map,const struct s842_cache *cache,
 const struct acpi806_topology *t,uint64_t msr,struct s842_root *out){
 return build_pe(pe,map,cache,t,msr,out,false,(struct boot_range){0});
}
static uint64_t le(const unsigned char *p,unsigned count){
 uint64_t v=0;for(unsigned i=0;i<count;i++)v|=(uint64_t)p[i]<<(8u*i);return v;
}
static bool descriptor(const unsigned char *p,struct boot_range *r){
 uint64_t base=le(p+8,8),pages=le(p+24,8);
 if((base&(P-1))||!pages||pages>(UINT64_MAX-base)/P)return false;
 *r=(struct boot_range){base,base+pages*P};return true;
}
bool s842_build_pe_efi(const struct s790_low_request *pe,
 const struct boot_memory_evidence *e,const struct boot_info *b,
 const struct s842_cache *cache,const struct acpi806_topology *t,
 uint64_t msr,struct s842_root *out){
 if(!out)return false;
 *out=(struct s842_root){0};
 if(!pe||!e||!b||!t||!e->map||b->magic!=STYLEOS_BOOT_MAGIC||b->version!=1||
 b->arch!=STYLEOS_ARCH_X86_64||b->platform!=STYLEOS_PLATFORM_UEFI||b->flags!=STYLEOS_BOOT_EXITED_SERVICES||
 b->size!=sizeof(*b)||e->live.handoff.start!=(uintptr_t)b||e->live.handoff.end-e->live.handoff.start!=sizeof(*b)||
 e->live.image.start!=pe->image.start||e->live.image.end!=pe->image.end||
 e->live.stack.start!=pe->boot_stack.start||e->live.stack.end!=pe->boot_stack.end||
 !inside(pe->boot_stack,pe->image)||!inside(e->live.handoff,pe->image)||
 b->descriptor_version!=1||b->descriptor_size<40||b->descriptor_size>4096||(b->descriptor_size&7)||
 !b->memory_map||!b->memory_map_size||b->memory_map_size%b->descriptor_size||
 b->memory_map_size/b->descriptor_size>BOOT_MEMORY_INPUT_MAX||
 b->memory_map_size>UINT64_MAX-b->memory_map||e->live.source.start!=b->memory_map||
 e->live.source.end!=b->memory_map+b->memory_map_size||
 !reserved_ram(e->map,e->live.source,true)||
 t->ioapic_count!=1)return false;
 const unsigned char *map=(const unsigned char *)(uintptr_t)b->memory_map;
 const size_t stride=b->descriptor_size,size=(size_t)b->memory_map_size;
 if((pe->image.start|pe->image.end)&(P-1)||pe->image.start>=pe->image.end)return false;
 uint64_t covered=0,source_covered=0;
 struct boot_range dev[2]={{t->lapic_physical,(uint64_t)t->lapic_physical+P},
 {t->ioapic[0].physical,(uint64_t)t->ioapic[0].physical+P}};
 for(size_t i=0;i<size;i+=stride){
  struct boot_range a;if(!descriptor(map+i,&a))return false;
  for(size_t j=0;j<i;j+=stride){struct boot_range c;if(!descriptor(map+j,&c)||overlap(a,c))return false;}
  unsigned type=(unsigned)le(map+i,4);uint64_t attr=le(map+i+32,8);
  if(overlap(a,pe->image)){
   if((type!=1&&type!=2)||!(attr&8u)||(attr>>63))return false;
   uint64_t lo=a.start>pe->image.start?a.start:pe->image.start,hi=a.end<pe->image.end?a.end:pe->image.end;
   covered+=hi-lo;
  }
  if(overlap(a,e->live.source)){
   if(type!=2||!(attr&8u)||(attr>>63))return false;
   uint64_t lo=a.start>e->live.source.start?a.start:e->live.source.start,hi=a.end<e->live.source.end?a.end:e->live.source.end;
   source_covered+=hi-lo;
  }
  for(unsigned j=0;j<2;j++)if(overlap(a,dev[j])&&(type!=11||!inside(dev[j],a)))return false;
 }
 if(covered!=pe->image.end-pe->image.start||source_covered!=e->live.source.end-e->live.source.start)return false;
 struct boot_range extra={0};
#if defined(STYLEOS_TEST_PHYSICAL_ARENA_844) || defined(STYLEOS_TEST_RUNTIME_ARENA_858)
 if(!s844_admit_efi(e,b,&extra))return false;
#endif
 return build_pe(pe,e->map,cache,t,msr,out,true,extra);
}
