#include <styleos/apic_pe_842.h>
#define P UINT64_C(4096)
static bool inside(struct boot_range a,struct boot_range b){return a.start<a.end&&b.start<=a.start&&a.end<=b.end;}
static bool overlap(struct boot_range a,struct boot_range b){return a.start<b.end&&b.start<a.end;}
static bool reserved_ram(const struct boot_memory_map *m,struct boot_range r){
 bool ram=false,reserved=false;
 if(!m||m->nr_ram>BOOT_MEMORY_INPUT_MAX||m->nr_reserved>BOOT_MEMORY_INPUT_MAX||m->nr_candidates>BOOT_MEMORY_RANGE_MAX||r.start>=r.end)return false;
 for(size_t i=0;i<m->nr_ram;i++){if(m->ram[i].start>=m->ram[i].end)return false;ram|=inside(r,m->ram[i]);}
 for(size_t i=0;i<m->nr_reserved;i++){if(m->reserved[i].start>=m->reserved[i].end)return false;reserved|=inside(r,m->reserved[i]);}
 for(size_t i=0;i<m->nr_candidates;i++){if(m->candidates[i].start>=m->candidates[i].end||overlap(r,m->candidates[i]))return false;}
 return ram&&reserved;
}
bool s842_build_pe(const struct s790_low_request *pe,
 const struct boot_memory_map *map,const struct s842_cache *cache,
 const struct acpi806_topology *t,uint64_t msr,struct s842_root *out){
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
 if(!inside(tables,pe->image)||overlap(tables,stack)||!reserved_ram(map,pe->image)||!reserved_ram(map,stack))return false;
 struct s790_low_result low;
 if(!s790_prepare_owned_low(pe,&low)||!s790_audit_owned_low(&low,pe->required_exec,pe->required_write,pe->required_rsp))return false;
 low.plan.pa_bits=cache->bits;
 return s842_build(&low.plan,map,cache,lapic,io,out);
}
