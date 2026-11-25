#ifndef STYLEOS_RUNTIME_IRQ_859_H
#define STYLEOS_RUNTIME_IRQ_859_H
#include <stdint.h>
#include <stdbool.h>
#include <styleos/apic_root_842.h>
#define IRQ859_VECTOR 0x50u
#define IRQ859_IST UINT64_C(0x400000003000)
struct irq859_expected {uint64_t root,ist,cs,wait_lo,wait_hi,rsp;};
struct irq859_observed {uint64_t root,frame,cs,rip,rsp,rflags,hits,isr,eoi;};
/* Pure check of trusted kernel observations; not a hardware authorization. */
static inline bool irq859_frame_ok(const struct irq859_expected *e,const struct irq859_observed *o){
 return e&&o&&e->root&&!(e->root&4095u)&&e->root<(UINT64_C(1)<<47)&&e->ist==IRQ859_IST&&e->cs&&e->cs<=0xffff&&!(e->cs&7u)&&e->wait_lo&&e->wait_lo<e->wait_hi&&e->wait_hi-e->wait_lo<=16&&e->wait_hi<(UINT64_C(1)<<47)&&e->rsp&&!(e->rsp&7u)&&e->rsp<(UINT64_C(1)<<47)&&o->root==e->root&&o->frame==e->ist+4096-16-40&&o->cs==e->cs&&o->rip>=e->wait_lo&&o->rip<e->wait_hi&&o->rsp==e->rsp&&(o->rflags&0x202)==0x202&&!(o->rflags&~UINT64_C(0xad7))&&o->hits==1&&o->isr==1&&o->eoi==1;
}
/* Narrow read-only check for the two already admitted S842 APIC pages.
 * Unlike pg_lookup, accept the exact audited UC leaf bits, not arbitrary PAT.
 * The caller binds root to live CR3 and the cache snapshot to live MSRs. */
static inline bool irq859_uc_page(const struct pg_plan *p,const struct pg_result *r,const struct s842_cache *c,uint64_t va){
 if(!p||!r||!c||!p->tables||p->arch!=PG_X86||p->pa_bits!=c->bits||c->bits<36||c->bits>52||!p->tables_pa||((p->tables_pa|(uintptr_t)p->tables)&4095u)||r->error!=PG_OK||r->root!=p->tables_pa||!r->used||r->used>p->capacity||p->capacity>PG_TABLE_PAGES||p->tables_pa>=(UINT64_C(1)<<c->bits)||p->capacity*UINT64_C(4096)>(UINT64_C(1)<<c->bits)-p->tables_pa||(va!=0xfee00000u&&va!=0xfec00000u)||c->count>CACHE800_MAX_VAR||c->count!=(c->cap&255u))return false;
 unsigned uc=0;while(uc<8&&((c->pat>>(8*uc))&255u))uc++;
 if(uc==8)return false;
 struct cache800_result a,b;
 if(!cache800_physical_type(va,c->bits,c->cap,c->def,c->pairs,c->count,&a)||!cache800_physical_type(va+4095,c->bits,c->cap,c->def,c->pairs,c->count,&b)||!a.mtrr_enabled||!b.mtrr_enabled||a.type||b.type)return false;
 uint64_t mask=((UINT64_C(1)<<c->bits)-1)&~UINT64_C(4095);size_t t=0;uint32_t seen=1;
 for(unsigned l=4;l>1;l--){uint64_t e=p->tables[t][(va>>(12+9*(l-1)))&511u],pa=e&mask;
  if((e&~(mask|UINT64_C(0x20)))!=3u||pa<p->tables_pa||pa-p->tables_pa>=r->used*UINT64_C(4096))return false;
  t=(size_t)((pa-p->tables_pa)/4096);if(seen&(UINT32_C(1)<<t))return false;seen|=UINT32_C(1)<<t;
 }
 uint64_t bits=((uc&1)?8u:0u)|((uc&2)?16u:0u)|((uc&4)?128u:0u);
 return (p->tables[t][(va>>12)&511u]&~UINT64_C(0x60))==(va|(UINT64_C(1)<<63)|3u|bits);
}
/* Q35 six-LVT fixture only. Accept the actual inherited ExtINT on LINT0
 * (or its already masked form), never an arbitrary unmasked source. */
#define IRQ859_LVT_COUNT 6u
static inline bool irq859_lvt_plan(uint32_t version,const uint32_t *before,uint32_t *masked){
 if(!before||!masked||((version>>16)&255u)!=5u)return false;
 for(unsigned i=0;i<IRQ859_LVT_COUNT;i++){
  if(i==3){if(before[i]!=0x700u&&before[i]!=0x10700u)return false;}
  else if(!(before[i]&0x10000u)||(before[i]&0x1000u))return false;
 }
 *masked=0x10700u;return true;
}
/* Terminal single-CPU test; no scheduling or reusable IRQ grant. */
void irq859_once(uint64_t root,uint64_t idt,uint16_t cs,uint64_t stack_lo,uint64_t stack_hi);
#endif
