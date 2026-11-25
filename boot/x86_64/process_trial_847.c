/* Terminal, single CPU, integer-only CPL3 experiment under audited S846 roots. */
#include <styleos/process_entry_847.h>
#include <styleos/arch.h>
#define P UINT64_C(4096)
#ifdef STYLEOS_TEST_RUNTIME_USER_861
#include <styleos/runtime_entry_858.h>
#include <styleos/runtime_tick_860.h>
static struct dpr_service *joined847;
static uint32_t pid847[2],win847[2];
static struct rt858_live live847(void){
 struct rt858_live v={0};struct {uint16_t limit;uint64_t base;}__attribute__((packed)) g,d;
 __asm__ volatile("sgdt %0;sidt %1;str %2;mov %%cs,%3;mov %%rsp,%4;mov %%cr0,%5;mov %%cr4,%6;mov %%cr3,%7;pushfq;popq %8":"=m"(g),"=m"(d),"=r"(v.tr),"=r"(v.cs),"=r"(v.rsp),"=r"(v.cr0),"=r"(v.cr4),"=r"(v.root),"=r"(v.rflags)::"memory");
 uint32_t lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000080));v.efer=((uint64_t)hi<<32)|lo;
 v.gdt=g.base;v.idt=d.base;v.gdt_limit=g.limit;v.idt_limit=d.limit;return v;
}
#endif
struct d847 {uint16_t limit;uint64_t base;}__attribute__((packed));
struct gate847 {uint16_t low,cs;uint8_t ist,type;uint16_t mid;uint32_t hi,zero;}__attribute__((packed));
static _Alignas(16) struct gate847 idt847[256];
static struct s847_control ctl847;
static unsigned events847;
static uint64_t pcs847[2][4];
extern void s847_bad_entry(void);
extern const unsigned char s847_0_start[],s847_0_end[],s847_0_ok[],s847_0_deny[],s847_0_yield[],s847_0_fault[];
extern const unsigned char s847_1_start[],s847_1_end[],s847_1_ok[],s847_1_deny[],s847_1_yield[],s847_1_exit[];
static void fld(const char *key,uint64_t v){arch_putc(' ');arch_puts(key);arch_putc('=');arch_puts("0x");for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)i)&15]);}
static _Noreturn void fail847(const char *s){arch_puts("STYLEOS S847 FAIL ");arch_puts(s);arch_putc('\n');__asm__ volatile("cli;outl %0,%1"::"a"(0x26u),"Nd"((uint16_t)0xf4));arch_halt();}
static uint64_t cr3_847(void){uint64_t v;__asm__ volatile("mov %%cr3,%0":"=r"(v));return v;}
static uint64_t read847(uint64_t p){return *(volatile uint64_t *)(uintptr_t)p;}
static bool rw_image847(const struct s846_root *r,uint64_t x,uint64_t n){
 for(size_t i=0;i<r->plan.count;i++){const struct pg_mapping *m=&r->plan.maps[i];if(m->va==m->pa&&m->flags==PG_WRITE&&x>=m->va&&x-m->va<m->bytes&&n<=m->bytes-(x-m->va))return true;}
 return false;
}
static void gate847(unsigned n,void (*entry)(void),uint16_t cs,uint8_t ist,uint8_t type){uint64_t p=(uintptr_t)entry;idt847[n]=(struct gate847){(uint16_t)p,cs,ist,type,(uint16_t)(p>>16),(uint32_t)(p>>32),0};}
#ifdef STYLEOS_TEST_USER_PREEMPT_848
bool s848_active(void);
unsigned s848_dispatch_raw(struct s847_raw *,uint64_t,uint64_t);
#endif
unsigned s847_dispatch_raw(struct s847_raw *f,uint64_t cr2,uint64_t root){
#ifdef STYLEOS_TEST_USER_PREEMPT_848
 if(s848_active())return s848_dispatch_raw(f,cr2,root);
#endif
 static const unsigned ids[8]={0,0,0,1,1,1,0,1};
 static const unsigned stages[8]={0,1,2,0,1,2,3,3};
 unsigned n=events847;if(n>=8||ctl847.current!=(int)ids[n])fail847("event-order");
 unsigned id=ids[n],stage=stages[n];uint64_t call=f->gpr[0];
#ifdef STYLEOS_TEST_RUNTIME_USER_861
 if(!joined847||joined847->dispatcher.current!=(int)id||joined847->dispatcher.slots[id].pid!=pid847[id])fail847("861-trap-owner");
 arch_puts("STYLEOS S861U FRAME");fld("n",n);fld("flags",f->rflags);arch_putc('\n');
#endif
 enum s847_action action=stage<2?S847_RETURN:stage==2?S847_YIELD:id?S847_EXIT:S847_FAULT;
 uint64_t expect=stage==0?0:stage==1?3:0;
 if(root!=ctl847.slot[id].root||cr3_847()!=root||f->rip!=pcs847[id][stage]||
 (action==S847_FAULT?(f->vector!=14||f->error!=5||cr2!=S846_IST):(f->vector!=128||f->error||call!=(stage<2?3:stage==2?1:2))))fail847("trap-evidence");
 if(stage>=2&&(f->gpr[11]!=0x847000u+id||f->gpr[5]!=S846_DATA))fail847("register-continuity");
 struct s847_decision d;
 if(!s847_apply_raw(&ctl847,f,(uintptr_t)f,cr2,root,&d)||d.action!=action||d.result!=expect)fail847("trap-decision");
 arch_puts("STYLEOS S847 TRAP");fld("n",n);fld("id",id);fld("stage",stage);fld("root",root);fld("vector",f->vector);fld("error",f->error);fld("cr2",f->vector==14?cr2:0);fld("rip",f->rip);fld("rsp",f->rsp);fld("cs",f->cs);fld("ss",f->ss);fld("frame",(uintptr_t)f);fld("call",f->vector==14?0:call);fld("action",d.action);fld("result",d.result);arch_putc('\n');
 events847++;return d.action==S847_RETURN?1u:0u;
}
void s847_trial(const struct s846_root child[2],const struct s844_lease leases[2],uint64_t root
#ifdef STYLEOS_TEST_RUNTIME_USER_861
 ,bool (*audit)(void *),void *opaque
#endif
){
 struct d847 oldg={0},oldi={0},now={0};uint16_t tr,cs;uint64_t flags;
 __asm__ volatile("sgdt %0;sidt %1;str %2;mov %%cs,%3;pushfq;popq %4":"=m"(oldg),"=m"(oldi),"=r"(tr),"=r"(cs),"=r"(flags));
 if(flags&0x200||cr3_847()!=root||!rw_image847(&child[0],oldg.base,P)||!rw_image847(&child[1],oldg.base,P)||!rw_image847(&child[0],(uintptr_t)idt847,sizeof idt847)||!rw_image847(&child[1],(uintptr_t)idt847,sizeof idt847))fail847("descriptor-map");
#ifdef STYLEOS_TEST_RUNTIME_USER_861
 struct rt858_live initial=live847();joined847=rt861_dispatcher(&initial);
 if(!joined847||!audit||!audit(opaque))fail847("861-join");
 for(unsigned i=0;i<2;i++){
  if(!leases[i].generation||leases[i].pages!=3||dpr_admit(joined847,i,leases[i].generation,child[i].built.root,0,i?"S861 B":"S861 A",&pid847[i])!=PS_OK||pr_account(pr_global(),pid847[i],leases[i].generation,0,3*P)!=PR_OK||ws_open_app(ws_global(),pr_global(),pid847[i],i?"S861 B":"S861 A",(struct ws_bounds){0,0,400,300},&win847[i])!=WS_OK)fail847("861-admit");
 }
 if(ws_set_minimized(ws_global(),pid847[0],win847[0],true)!=WS_OK)fail847("861-minimize");
 arch_puts("STYLEOS S861U READY");fld("safe",root);fld("pid-a",pid847[0]);fld("pid-b",pid847[1]);fld("win-a",win847[0]);fld("win-b",win847[1]);fld("idt",(uintptr_t)idt847);arch_putc('\n');
#endif
 const unsigned char *starts[2]={s847_0_start,s847_1_start},*ends[2]={s847_0_end,s847_1_end};
 const unsigned char *sites[2][4]={{s847_0_ok,s847_0_deny,s847_0_yield,s847_0_fault},{s847_1_ok,s847_1_deny,s847_1_yield,s847_1_exit}};
 for(unsigned i=0;i<2;i++){
  uintptr_t lo=(uintptr_t)starts[i],hi=(uintptr_t)ends[i];if(hi<=lo||hi-lo>P)fail847("code-extent");
  volatile unsigned char *dest=(void *)(uintptr_t)leases[i].pa;for(size_t j=0;j<P;j++)dest[j]=j<hi-lo?starts[i][j]:0;
  for(unsigned j=0;j<4;j++){uintptr_t x=(uintptr_t)sites[i][j];if(x<lo||x>=hi)fail847("site");pcs847[i][j]=S846_CODE+x-lo;}
  for(size_t j=0;j<hi-lo;j++)if(dest[j]!=starts[i][j])fail847("code-copy");
 }
 struct s847_selectors sel;
 if(!s847_gdt_append((void *)(uintptr_t)oldg.base,P,oldg.limit,tr,cs,&sel)||!s847_init(&ctl847,root,child[0].built.root,child[1].built.root,sel.cs,sel.ss))fail847("gdt");
 for(unsigned i=0;i<256;i++)gate847(i,s847_bad_entry,cs,3,0x8e);
 gate847(128,s847_call_entry,cs,1,0xee);gate847(14,s847_pf_entry,cs,2,0x8e);
 struct d847 g={sel.limit,oldg.base},d={sizeof idt847-1,(uintptr_t)idt847};
 __asm__ volatile("lgdt %0;lidt %1"::"m"(g),"m"(d):"memory");
 __asm__ volatile("sgdt %0":"=m"(now));if(now.base!=g.base||now.limit!=g.limit)fail847("gdtr");
 __asm__ volatile("sidt %0":"=m"(now));if(now.base!=d.base||now.limit!=d.limit)fail847("idtr");
#ifdef STYLEOS_TEST_RUNTIME_USER_861
 __asm__ volatile("lidt %0;lgdt %1"::"m"(oldi),"m"(oldg):"memory");
#endif
 arch_puts("STYLEOS S847 READY");fld("safe",root);fld("cs",sel.cs);fld("ss",sel.ss);fld("tr",tr);fld("limit",sel.limit);for(unsigned i=0;i<2;i++)for(unsigned j=0;j<4;j++){const char *keys[2][4]={{"a-ok","a-deny","a-yield","a-fault"},{"b-ok","b-deny","b-yield","b-exit"}};fld(keys[i][j],pcs847[i][j]);}arch_putc('\n');
 for(unsigned step=0;step<4;step++){
  unsigned id;struct up_context c;uint64_t before_sp,after_sp;
#ifdef STYLEOS_TEST_RUNTIME_USER_861
  struct rt858_live boundary=live847();unsigned chosen;uint32_t pid;
  if(rt861_dispatcher(&boundary)!=joined847||!audit(opaque)||dpr_dispatch(joined847,&chosen,&pid)!=PS_OK||chosen!=step%2||pid!=pid847[chosen])fail847("861-dispatch");
  __asm__ volatile("lgdt %0;lidt %1"::"m"(g),"m"(d):"memory");
#endif
  if(!s847_dispatch(&ctl847,cr3_847(),&id,&c)||id!=step%2)fail847("dispatch");
  __asm__ volatile("mov %%rsp,%0":"=r"(before_sp));s847_enter(&c,child[id].built.root,root);__asm__ volatile("mov %%rsp,%0":"=r"(after_sp));
  uint64_t value=read847(leases[id].pa+P),foreign=read847(leases[1-id].pa+P);
  if(cr3_847()!=root||before_sp!=after_sp||ctl847.current!=-1||ctl847.slot[id].state!=(step<2?S847_READY:id?S847_EXITED:S847_FAULTED)||value!=(step==3?0x847101u:0x847000u+id)||foreign!=(step==0?0x846d1001u:0x847000u+1-id))fail847("return");
#ifdef STYLEOS_TEST_RUNTIME_USER_861
  __asm__ volatile("lidt %0;lgdt %1"::"m"(oldi),"m"(oldg):"memory");
  struct rt858_live returned=live847();
  if(returned.root!=root||returned.gdt!=initial.gdt||returned.idt!=initial.idt||returned.gdt_limit!=initial.gdt_limit||returned.idt_limit!=initial.idt_limit||returned.tr!=initial.tr||returned.cs!=initial.cs||(returned.rflags&0x200)||!audit(opaque))fail847("861-restore");
  const struct pr_entry *p=pr_find(pr_global(),pid847[id]);
  if(!p||p->state!=PR_RUNNING||p->resident_bytes!=3*P||!ws_find(ws_global(),win847[id])||dpr_observe(joined847,id,leases[id].generation,child[id].built.root,root,step<2?PS_YIELD:id?PS_EXIT:PS_FAULT)!=PS_OK||rt861_dispatcher(&returned)!=joined847)fail847("861-event");
  p=pr_find(pr_global(),pid847[id]);if(!p||p->state!=(step<2?PR_READY:id?PR_EXITED:PR_FAULTED)||p->cpu_ticks||p->resident_bytes!=3*P||((ws_find(ws_global(),win847[id])!=0)!=(step<2))||(step<3&&!ws_find(ws_global(),win847[1]))||(step<2&&!ws_find(ws_global(),win847[0])->minimized))fail847("861-lifetime");
  arch_puts("STYLEOS S861U EVENT");fld("step",step);fld("id",id);fld("pid",pid847[id]);fld("handle",leases[id].generation);fld("safe",returned.root);fld("gdt",returned.gdt);fld("idt",returned.idt);fld("dispatch",joined847->dispatcher.dispatches);fld("resident",p->resident_bytes);fld("windows",ws_snapshot(ws_global(),0,0));arch_putc('\n');
#endif
  arch_puts("STYLEOS S847 BACK");fld("step",step);fld("id",id);fld("root",root);fld("state",ctl847.slot[id].state);fld("value",value);fld("foreign",foreign);fld("stack-same",1);arch_putc('\n');
 }
 unsigned id;struct up_context c;if(events847!=8||s847_dispatch(&ctl847,root,&id,&c))fail847("terminal");
 __asm__ volatile("lidt %0;lgdt %1"::"m"(oldi),"m"(oldg):"memory");
 volatile unsigned char *tail=(void *)(uintptr_t)(oldg.base+oldg.limit+1u);for(unsigned i=0;i<16;i++)tail[i]=0;
 __asm__ volatile("sgdt %0":"=m"(now));if(now.base!=oldg.base||now.limit!=oldg.limit)fail847("restore-gdt");
 __asm__ volatile("sidt %0":"=m"(now));if(now.base!=oldi.base||now.limit!=oldi.limit)fail847("restore-idt");
 arch_puts("STYLEOS S847 POLICY cpl3=yes user-fault-contained=yes range-only=yes if=0 production-cr3=deny\n");
}
