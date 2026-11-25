/* Isolated CPL3 test. Reuse S846 audited UC LAPIC and S847 entry. */
#include <styleos/user_preempt_848.h>
#include <styleos/acpi_madt_806.h>
#include <styleos/arch.h>
#ifdef STYLEOS_TEST_FX_CONTEXT_849
#include <styleos/fx_guest_849.h>
#endif
#ifdef STYLEOS_TEST_RUNTIME_TICK_860
#include <styleos/runtime_tick_860.h>
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
#include <styleos/runtime_entry_858.h>
#ifdef STYLEOS_TEST_RUNTIME_SELECT_862
#include <styleos/runtime_select_862.h>
#endif
#endif
static struct dpr_service *runtime860;
static struct s847_raw pending860,saved860[2];
static uint64_t frame860,handle860[2];
static uint32_t pid860[2],win860[2];
static bool receipt860,have860[2];
#endif
#define P UINT64_C(4096)
#define V 0x50u
#define BIT 0x10000u
#define COUNT 5000000u
struct desc848 {uint16_t limit;uint64_t base;}__attribute__((packed));
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
static struct rt858_live live848(void){
 struct rt858_live v={0};struct desc848 g,d;
 __asm__ volatile("sgdt %0;sidt %1;str %2;mov %%cs,%3;mov %%rsp,%4;mov %%cr0,%5;mov %%cr4,%6;mov %%cr3,%7;pushfq;popq %8":"=m"(g),"=m"(d),"=r"(v.tr),"=r"(v.cs),"=r"(v.rsp),"=r"(v.cr0),"=r"(v.cr4),"=r"(v.root),"=r"(v.rflags)::"memory");
 uint32_t lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000080));v.efer=((uint64_t)hi<<32)|lo;
 v.gdt=g.base;v.idt=d.base;v.gdt_limit=g.limit;v.idt_limit=d.limit;return v;
}
#endif
struct gate848 {uint16_t lo,cs;uint8_t ist,type;uint16_t mid;uint32_t hi,zero;}__attribute__((packed));
static _Alignas(16) struct gate848 idt848[256];
static struct s847_control ctl848;
static uint64_t lapic848,sites848[2][3],progress848[2];
static unsigned ticks848,local848[2];
static bool active848,armed848;
extern void s848_irq_entry(void),s847_bad_entry(void);
extern const unsigned char s848_a_start[],s848_a_loop_end[],s848_a_fault[],s848_a_end[];
extern const unsigned char s848_b_start[],s848_b_loop_end[],s848_b_exit[],s848_b_end[];
static uint64_t root848(void){uint64_t v;__asm__ volatile("mov %%cr3,%0":"=r"(v));return v;}
static uint64_t flags848(void){uint64_t v;__asm__ volatile("pushfq;popq %0":"=r"(v));return v;}
static uint64_t msr848(uint32_t n){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return ((uint64_t)d<<32)|a;}
static uint32_t rd848(unsigned off){return *(volatile uint32_t *)(uintptr_t)(lapic848+off);}
static uint32_t pending848(unsigned base){uint32_t v=0;for(unsigned i=0;i<8;i++)v|=rd848(base+16*i);return v;}
static void wr848(unsigned off,uint32_t n){*(volatile uint32_t *)(uintptr_t)(lapic848+off)=n;}
static uint8_t in848(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static uint64_t word848(uint64_t p){return *(volatile uint64_t *)(uintptr_t)p;}
static void put848(uint64_t p,uint64_t v){*(volatile uint64_t *)(uintptr_t)p=v;}
static void fld848(const char *k,uint64_t v){arch_putc(' ');arch_puts(k);arch_putc('=');arch_puts("0x");for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)i)&15]);}
static _Noreturn void fail848(const char *s){__asm__ volatile("cli");arch_puts("STYLEOS S848 FAIL ");arch_puts(s);arch_putc('\n');__asm__ volatile("outl %0,%1"::"a"(0x27u),"Nd"((uint16_t)0xf4));arch_halt();}
static bool writable848(const struct s846_root *r,uint64_t x,uint64_t n){for(size_t i=0;i<r->plan.count;i++){const struct pg_mapping *m=&r->plan.maps[i];if(m->va==m->pa&&m->flags==PG_WRITE&&x>=m->va&&x-m->va<m->bytes&&n<=m->bytes-(x-m->va))return true;}return false;}
static void gate848(unsigned i,void(*p)(void),uint16_t cs,uint8_t ist,uint8_t type){uint64_t v=(uintptr_t)p;idt848[i]=(struct gate848){(uint16_t)v,cs,ist,type,(uint16_t)(v>>16),(uint32_t)(v>>32),0};}
static uint64_t pattern848(unsigned id,unsigned i){return i==5?S846_DATA:UINT64_C(0x8480000000000000)|((uint64_t)id<<32)|i;}
bool s848_active(void){return active848;}
unsigned s848_dispatch_raw(struct s847_raw *f,uint64_t cr2,uint64_t root){
 int id=ctl848.current;if(!active848||id<0||id>1||root!=root848()||root!=ctl848.slot[id].root)fail848("origin");
#ifdef STYLEOS_TEST_FX_CONTEXT_849
 if(!s849_guest_suspend((unsigned)id,root,f->vector!=V))fail848("fx-suspend");
#endif
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
 arch_puts("STYLEOS S861T FRAME");fld848("step",runtime860->dispatcher.dispatches-1);fld848("flags",f->rflags);arch_putc('\n');
#endif
 if(f->vector==V){
  uint32_t isr=rd848(0x120);uint64_t p=word848(S846_DATA);
  if(!armed848||rd848(0x390)||f->rip<sites848[id][0]||f->rip>=sites848[id][1]||f->rsp!=S846_STACK+P||p<=progress848[id]||word848(S846_DATA+8))fail848("tick-progress");
  for(unsigned i=0;i<15;i++)if(f->gpr[i]!=pattern848((unsigned)id,i))fail848("gpr");
  struct s848_irq_evidence e={root,(uintptr_t)f,flags848(),isr};
  if(!s848_timer_suspend(&ctl848,f,&e))fail848("tick-model");
  wr848(0x320,BIT|V);wr848(0xb0,0);
  if((rd848(0x120)&BIT)||(rd848(0x220)&BIT)||rd848(0x320)!=(BIT|V))fail848("eoi-mask");
#ifdef STYLEOS_TEST_RUNTIME_TICK_860
  /* Capture only after the real S849 FP suspension and S848 EOI check. */
  if(receipt860||!runtime860||runtime860->dispatcher.current!=id)fail848("860-receipt");
  pending860=*f;frame860=(uintptr_t)f;receipt860=true;
#endif
  armed848=false;progress848[id]=p;++ticks848;++local848[id];
  arch_puts("STYLEOS S848 TICK");fld848("n",ticks848);fld848("id",id);fld848("root",root);fld848("rip",f->rip);fld848("rsp",f->rsp);fld848("flags",f->rflags);fld848("frame",(uintptr_t)f);fld848("cs",f->cs);fld848("ss",f->ss);fld848("progress",p);fld848("isr",isr);fld848("eoi",1);
  const char *const names[]={"rax","rbx","rcx","rdx","rsi","rdi","rbp","r8","r9","r10","r11","r12","r13","r14","r15"};
  for(unsigned i=0;i<15;i++)fld848(names[i],f->gpr[i]);
  arch_putc('\n');return 0;
 }
 if(armed848||f->rip!=sites848[id][2]||word848(S846_DATA+8)!=1||f->rsp!=S846_STACK+P||word848(S846_DATA)<progress848[id]||local848[id]!=(id?5u:3u))fail848("terminal-origin");
 struct s847_decision d;if(id==0?(f->vector!=14||f->error!=5||cr2!=S846_IST):(f->vector!=128||f->gpr[0]!=S847_CALL_EXIT))fail848("terminal-trap");
 if(!s848_user_trap(&ctl848,f,(uintptr_t)f,cr2,root,flags848(),&d)||d.action!=(id?S847_EXIT:S847_FAULT))fail848("terminal-model");
 arch_puts("STYLEOS S848 END");fld848("id",id);fld848("root",root);fld848("rip",f->rip);fld848("vector",f->vector);fld848("error",f->error);fld848("cr2",id?0:cr2);fld848("state",ctl848.slot[id].state);fld848("ticks",ticks848);arch_putc('\n');return 0;
}
void s848_trial(const struct s846_root child[2],const struct s844_lease leases[2],uint64_t safe,uint64_t admitted_lapic
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
 ,bool (*audit)(void *),void *opaque
#endif
){
 struct desc848 g0={0},i0={0},check={0};uint16_t tr,cs;struct acpi806_topology topo={0};uint64_t apic=msr848(0x1b);
 if(root848()!=safe||(flags848()&0x200)||active848||!acpi806_copy_capture(&topo)||topo.enabled_lapics!=1||topo.lapic_physical!=admitted_lapic||!(apic&0x800)||(apic&0x400)||(apic&UINT64_C(0xfffff000))!=admitted_lapic||in848(0x21)!=0xff||in848(0xa1)!=0xff)fail848("apic-admission");
 lapic848=admitted_lapic;
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
 struct rt858_live original=live848();runtime860=rt861_dispatcher(&original);
 if(!runtime860||!audit||!audit(opaque))fail848("861-timer-join");
#endif
 __asm__ volatile("sgdt %0;sidt %1;str %2;mov %%cs,%3":"=m"(g0),"=m"(i0),"=r"(tr),"=r"(cs));
 for(unsigned i=0;i<2;i++)if(!writable848(&child[i],lapic848,P)||!writable848(&child[i],g0.base,P)||!writable848(&child[i],(uintptr_t)idt848,sizeof idt848))fail848("mapping");
 uint32_t lvt=rd848(0x320),div=rd848(0x3e0),tpr=rd848(0x80),svr=rd848(0xf0);
 /* Post-EBS, terminal trial only: retire a masked firmware timer without
  * restoring its elapsed phase. Refuse unmasked/deadline/pending sources. */
 uint32_t initial=rd848(0x380),current=rd848(0x390),isr=pending848(0x100),irr=pending848(0x200);
 arch_puts("STYLEOS S848 ADMIT");fld848("lvt",lvt);fld848("initial",initial);fld848("current",current);fld848("div",div);fld848("isr",isr);fld848("irr",irr);arch_putc('\n');
 if(!s848_timer_admit(lvt,initial,current,div,isr,irr)||rd848(0x320)!=lvt)fail848("timer-busy");
 wr848(0x380,0);
 initial=rd848(0x380);current=rd848(0x390);isr=pending848(0x100);irr=pending848(0x200);
 arch_puts("STYLEOS S848 STOP");fld848("lvt",rd848(0x320));fld848("initial",initial);fld848("current",current);fld848("isr",isr);fld848("irr",irr);arch_putc('\n');
 if(initial||current||isr||irr||rd848(0x320)!=lvt)fail848("timer-stop");
 const unsigned char *start[2]={s848_a_start,s848_b_start},*loop[2]={s848_a_loop_end,s848_b_loop_end},*end[2]={s848_a_end,s848_b_end},*term[2]={s848_a_fault,s848_b_exit};
 for(unsigned i=0;i<2;i++){
  uintptr_t lo=(uintptr_t)start[i],hi=(uintptr_t)end[i],mid=(uintptr_t)loop[i],last=(uintptr_t)term[i];if(lo>=mid||mid>last||last>=hi||hi-lo>P)fail848("code-extent");
  volatile unsigned char *dst=(void *)(uintptr_t)leases[i].pa;for(size_t j=0;j<3*P;j++)dst[j]=j<hi-lo?start[i][j]:0;
  for(size_t j=0;j<hi-lo;j++)if(dst[j]!=start[i][j])fail848("copy");
  sites848[i][0]=S846_CODE;sites848[i][1]=S846_CODE+mid-lo;sites848[i][2]=S846_CODE+last-lo;
 }
 struct s847_selectors sel;
 if(!s847_gdt_append((void *)(uintptr_t)g0.base,P,g0.limit,tr,cs,&sel)||!s847_init(&ctl848,safe,child[0].built.root,child[1].built.root,sel.cs,sel.ss))fail848("gdt");
 for(unsigned i=0;i<2;i++)for(unsigned j=0;j<15;j++)ctl848.slot[i].context.gpr[j]=pattern848(i,j);
 for(unsigned i=0;i<256;i++)gate848(i,s847_bad_entry,cs,3,0x8e);
 gate848(V,s848_irq_entry,cs,1,0x8e);gate848(128,s847_call_entry,cs,1,0xee);gate848(14,s847_pf_entry,cs,2,0x8e);
 struct desc848 g={sel.limit,g0.base},d={sizeof idt848-1,(uintptr_t)idt848};
 __asm__ volatile("lgdt %0;lidt %1"::"m"(g),"m"(d):"memory");
 wr848(0x80,0);wr848(0xf0,(svr&~255u)|0x1ffu);wr848(0x320,BIT|V);wr848(0x3e0,3); /* divide by 16 */
 if(rd848(0x320)!=(BIT|V)||rd848(0x3e0)!=3||rd848(0x80)||rd848(0xf0)!=((svr&~255u)|0x1ffu))fail848("setup");
#ifdef STYLEOS_TEST_FX_CONTEXT_849
 if(!s849_guest_init(child,safe))fail848("fx-mode");
#endif
#ifdef STYLEOS_TEST_RUNTIME_TICK_860
#ifndef STYLEOS_TEST_RUNTIME_TIMER_861
 if(dpr_global_start(safe)!=DPR_OK||!(runtime860=dpr_tick_global_860()))fail848("860-runtime-start");
#else
 __asm__ volatile("lidt %0;lgdt %1"::"m"(i0),"m"(g0):"memory");
#endif
 for(unsigned i=0;i<2;i++){
  if(!leases[i].generation||leases[i].pages!=3||leases[i].owner!=(i?S846_OWNER_B:S846_OWNER_A)||!child[i].ready)fail848("860-lease");
  handle860[i]=leases[i].generation;
  if(dpr_admit(runtime860,i,handle860[i],child[i].built.root,0,i?"S860 B":"S860 A",&pid860[i])!=PS_OK||pr_account(pr_global(),pid860[i],handle860[i],0,3*P)!=PR_OK||ws_open_app(ws_global(),pr_global(),pid860[i],i?"S860 B":"S860 A",(struct ws_bounds){0,0,400,300},&win860[i])!=WS_OK)fail848("860-admit");
 }
 if(ws_set_minimized(ws_global(),pid860[0],win860[0],true)!=WS_OK)fail848("860-minimize");
 arch_puts("STYLEOS S860 READY");fld848("safe",safe);fld848("pid-a",pid860[0]);fld848("pid-b",pid860[1]);fld848("handle-a",handle860[0]);fld848("handle-b",handle860[1]);fld848("win-a",win860[0]);fld848("win-b",win860[1]);arch_putc('\n');
#endif
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
 arch_puts("STYLEOS S861T READY");fld848("safe",safe);fld848("idt",(uintptr_t)idt848);fld848("cs",sel.cs);fld848("ss",sel.ss);fld848("limit",sel.limit);arch_putc('\n');
#endif
 active848=true;
 arch_puts("STYLEOS S848 READY");fld848("safe",safe);fld848("a",child[0].built.root);fld848("b",child[1].built.root);fld848("lapic",lapic848);fld848("cs",sel.cs);fld848("ss",sel.ss);fld848("a-loop-end",sites848[0][1]);fld848("b-loop-end",sites848[1][1]);fld848("a-fault",sites848[0][2]);fld848("b-exit",sites848[1][2]);fld848("count",COUNT);arch_putc('\n');
 for(unsigned step=0;step<10;step++){
  static const unsigned order[10]={0,1,0,1,0,1,0,1,1,1};unsigned id;struct up_context ctx;uint64_t sp0,sp1;
#ifdef STYLEOS_TEST_RUNTIME_TICK_860
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
  struct rt858_live boundary=live848();
  if(rt861_dispatcher(&boundary)!=runtime860||!audit(opaque))fail848("861-timer-before");
#endif
  unsigned chosen;uint32_t chosen_pid;
  if(receipt860||dpr_dispatch(runtime860,&chosen,&chosen_pid)!=PS_OK||chosen>1||chosen_pid!=pid860[chosen])fail848("860-dispatch");
#endif
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
  __asm__ volatile("lgdt %0;lidt %1"::"m"(g),"m"(d):"memory");
#endif
#ifdef STYLEOS_TEST_RUNTIME_SELECT_862
  /* The dispatcher owns selection; deliberately keep the legacy cursor at A.
   * The fixed order below remains a fixture assertion, not a selector. */
  const struct rt862_selection selection={.slot=chosen,.dispatch=runtime860->dispatcher.dispatches,.pid=chosen_pid,.handle=handle860[chosen],.child_root=child[chosen].built.root,.safe_root=safe};
  unsigned cursor=ctl848.next;
  if(!rt862_selected_context(&ctl848,&runtime860->dispatcher,&selection,root848(),&ctx)||ctl848.next!=cursor)fail848("862-selected-context");
  id=chosen;
  arch_puts("STYLEOS S862 SELECT");fld848("step",step);fld848("id",id);fld848("pid",chosen_pid);fld848("dispatch",selection.dispatch);fld848("cursor",cursor);fld848("safe",safe);fld848("root",selection.child_root);arch_putc('\n');
  if(id!=order[step]||armed848||rd848(0x390)||(rd848(0x120)&BIT)||(rd848(0x220)&BIT))fail848("862-fixture");
#else
  if(!s848_user_dispatch(&ctl848,root848(),&id,&ctx)||id!=order[step]||armed848||rd848(0x390)||(rd848(0x120)&BIT)||(rd848(0x220)&BIT))fail848("dispatch");
#endif
#ifdef STYLEOS_TEST_RUNTIME_TICK_860
  /* Legacy S848 control remains a context checker; actual registry chooses first. */
  if(id!=chosen)fail848("860-choice");
  if(have860[id]){
   const struct s847_raw *f=&saved860[id];
   if(ctx.pc!=f->rip||ctx.sp!=f->rsp||ctx.cs!=f->cs||ctx.ss!=f->ss||ctx.status!=f->rflags)fail848("860-resume-frame");
   for(unsigned j=0;j<15;j++)if(ctx.gpr[j]!=f->gpr[j])fail848("860-resume-gpr");
  }
#endif
#ifdef STYLEOS_TEST_FX_CONTEXT_849
  if(!s849_guest_dispatch(id,child[id].built.root))fail848("fx-dispatch");
#endif
  bool terminal=step==6||step==9;
  if(terminal)put848(leases[id].pa+P+8,1);
  else{armed848=true;wr848(0x320,V);wr848(0x380,COUNT);}
  __asm__ volatile("mov %%rsp,%0":"=r"(sp0));s847_enter(&ctx,child[id].built.root,safe);__asm__ volatile("mov %%rsp,%0":"=r"(sp1));
  uint64_t a=word848(leases[id].pa+P),b=word848(leases[1-id].pa+P);
  if(root848()!=safe||(flags848()&0x200)||sp0!=sp1||armed848||ctl848.current!=-1||ctl848.slot[id].state!=(terminal?(id?S847_EXITED:S847_FAULTED):S847_READY)||b!=progress848[1-id]||a<progress848[id]||(!terminal&&a!=progress848[id]))fail848("safe-return");
#ifdef STYLEOS_TEST_RUNTIME_TICK_860
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
  __asm__ volatile("lidt %0;lgdt %1"::"m"(i0),"m"(g0):"memory");
  struct rt858_live returned=live848();
  if(returned.root!=safe||returned.gdt!=original.gdt||returned.idt!=original.idt||returned.gdt_limit!=original.gdt_limit||returned.idt_limit!=original.idt_limit||returned.tr!=original.tr||returned.cs!=original.cs||(returned.rflags&0x200)||!audit(opaque))fail848("861-timer-restore");
#endif
  const struct pr_entry *pe=pr_find(pr_global(),pid860[id]);
  if(!pe||pe->state!=PR_RUNNING||pe->resident_bytes!=3*P||!ws_find(ws_global(),win860[id]))fail848("860-before-event");
  if(terminal){
   if(receipt860||dpr_observe(runtime860,id,handle860[id],child[id].built.root,safe,id?PS_EXIT:PS_FAULT)!=PS_OK)fail848("860-end");
   have860[id]=false;volatile unsigned char *zero=(void *)&saved860[id];for(size_t j=0;j<sizeof saved860[id];j++)zero[j]=0;
  }else{
   if(!receipt860)fail848("860-no-receipt");
   struct rt860_bounds bounds={{S846_CODE,S846_CODE+P},{S846_STACK,S846_STACK+P},{S846_IST,S846_IST+P},sel.cs,sel.ss};
   struct rt860_tick t={.slot=id,.dispatch=runtime860->dispatcher.dispatches,.pid=pid860[id],.handle=handle860[id],.child_root=child[id].built.root,.safe_root=root848(),.frame_va=frame860,.vector=V,.isr=1,.eoi=1,.safe_flags=flags848(),.frame=pending860};
   if(dpr_tick_860(runtime860,&bounds,&t,&saved860[id])!=PS_OK)fail848("860-tick");
   receipt860=false;have860[id]=true;frame860=0;volatile unsigned char *zero=(void *)&pending860;for(size_t j=0;j<sizeof pending860;j++)zero[j]=0;
  }
  pe=pr_find(pr_global(),pid860[id]);const struct ws_window *window=ws_find(ws_global(),win860[id]);
  if(!pe||pe->cpu_ticks!=local848[id]||pe->resident_bytes!=3*P||pe->state!=(terminal?(id?PR_EXITED:PR_FAULTED):PR_READY)||runtime860->dispatcher.current!=-1||(terminal?window!=0:(!window||window->minimized!=(id==0)))||ws_snapshot(ws_global(),0,0)!=(step<6?2u:step<9?1u:0u))fail848("860-event-state");
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
  struct rt858_live after=live848();if(rt861_dispatcher(&after)!=runtime860)fail848("861-timer-after");
  arch_puts("STYLEOS S861T BOUNDARY");fld848("step",step);fld848("safe",after.root);fld848("gdt",after.gdt);fld848("idt",after.idt);fld848("pid",pid860[id]);arch_putc('\n');
#endif
  arch_puts("STYLEOS S860 EVENT");fld848("step",step);fld848("id",id);fld848("pid",pid860[id]);fld848("handle",handle860[id]);fld848("dispatch",runtime860->dispatcher.dispatches);fld848("safe",safe);fld848("root",child[id].built.root);fld848("ticks",pe->cpu_ticks);fld848("state",pe->state);fld848("resident",pe->resident_bytes);fld848("window",window?window->id:0);fld848("minimized",window?window->minimized:0);arch_putc('\n');
#endif
  progress848[id]=a;
  arch_puts("STYLEOS S848 BACK");fld848("step",step);fld848("id",id);fld848("safe",safe);fld848("state",ctl848.slot[id].state);fld848("a",progress848[0]);fld848("b",progress848[1]);fld848("stack-same",1);arch_putc('\n');
 }
#ifdef STYLEOS_TEST_RUNTIME_SELECT_862
 if(ticks848!=8||local848[0]!=3||local848[1]!=5||ctl848.current!=-1||ctl848.slot[0].state!=S847_FAULTED||ctl848.slot[1].state!=S847_EXITED||ctl848.next!=0)fail848("862-final");
#else
 unsigned id;struct up_context ctx;if(ticks848!=8||local848[0]!=3||local848[1]!=5||s848_user_dispatch(&ctl848,safe,&id,&ctx))fail848("final");
#endif
#ifdef STYLEOS_TEST_FX_CONTEXT_849
 if(!s849_guest_finish(safe))fail848("fx-finish");
#ifdef STYLEOS_TEST_RUNTIME_TICK_860
 unsigned slot;uint32_t pid;
 if(receipt860||have860[0]||have860[1]||dpr_dispatch(runtime860,&slot,&pid)!=PS_EMPTY||pr_snapshot(pr_global(),0,0,true)!=2||ws_snapshot(ws_global(),0,0))fail848("860-final");
#endif
#endif
 active848=false;wr848(0x320,BIT|V);wr848(0x380,0);wr848(0x3e0,div);wr848(0x320,lvt);wr848(0x80,tpr);wr848(0xf0,svr);
 __asm__ volatile("lidt %0;lgdt %1"::"m"(i0),"m"(g0):"memory");volatile unsigned char *tail=(void *)(uintptr_t)(g0.base+g0.limit+1);for(unsigned i=0;i<16;i++)tail[i]=0;
 __asm__ volatile("sgdt %0":"=m"(check));if(check.base!=g0.base||check.limit!=g0.limit)fail848("restore-gdt");__asm__ volatile("sidt %0":"=m"(check));if(check.base!=i0.base||check.limit!=i0.limit||rd848(0x320)!=lvt||rd848(0x380)||rd848(0x390)||rd848(0x3e0)!=div||rd848(0x80)!=tpr||rd848(0xf0)!=svr||pending848(0x100)||pending848(0x200))fail848("restore-apic");
 arch_puts("STYLEOS S848 TOTAL");fld848("ticks",ticks848);fld848("a",local848[0]);fld848("b",local848[1]);fld848("config-restored",1);fld848("timer-stopped",1);arch_putc('\n');
#ifdef STYLEOS_TEST_FX_CONTEXT_849
 arch_puts("STYLEOS S848 POLICY cpl3-timer=yes integer-only=no single-cpu=yes firmware-timer-phase=discarded production-cr3=deny\n");
 arch_puts("STYLEOS S849 POLICY cpl3-timer=yes fx-legacy=yes single-cpu=yes production-cr3=deny\n");
#else
 arch_puts("STYLEOS S848 POLICY cpl3-timer=yes integer-only=yes single-cpu=yes firmware-timer-phase=discarded production-cr3=deny\n");
#endif
}

#ifdef STYLEOS_TEST_RUNTIME_TICK_860
/* Called only after S846 destroys child roots and returns scrubbed leases. */
void s860_reap(uint64_t safe){
 if(!runtime860||root848()!=safe||flags848()&0x200||runtime860->dispatcher.safe_root!=safe||receipt860||have860[0]||have860[1])fail848("860-reap-context");
 for(unsigned i=0;i<2;i++)if(dpr_release(runtime860,i,handle860[i])!=PS_OK)fail848("860-reap");
 if(pr_snapshot(pr_global(),0,0,true)||ws_snapshot(ws_global(),0,0)||runtime860->dispatcher.current!=-1||runtime860->dispatcher.dispatches!=10)fail848("860-reap-state");
 arch_puts("STYLEOS S860 CLEAN");fld848("safe",safe);fld848("dispatches",10);fld848("processes",0);fld848("windows",0);arch_putc('\n');
 arch_puts("STYLEOS S860 POLICY actual-cpl3-ticks=yes fp=yes existing-dispatcher=yes leases-released=yes production-cr3=deny\n");
}
#endif
