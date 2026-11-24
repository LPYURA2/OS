/* Isolated CPL3 test. Reuse S846 audited UC LAPIC and S847 entry. */
#include <styleos/user_preempt_848.h>
#include <styleos/acpi_madt_806.h>
#include <styleos/arch.h>
#ifdef STYLEOS_TEST_FX_CONTEXT_849
#include <styleos/fx_guest_849.h>
#endif
#define P UINT64_C(4096)
#define V 0x50u
#define BIT 0x10000u
#define COUNT 5000000u
struct desc848 {uint16_t limit;uint64_t base;}__attribute__((packed));
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
 if(f->vector==V){
  uint32_t isr=rd848(0x120);uint64_t p=word848(S846_DATA);
  if(!armed848||rd848(0x390)||f->rip<sites848[id][0]||f->rip>=sites848[id][1]||f->rsp!=S846_STACK+P||p<=progress848[id]||word848(S846_DATA+8))fail848("tick-progress");
  for(unsigned i=0;i<15;i++)if(f->gpr[i]!=pattern848((unsigned)id,i))fail848("gpr");
  struct s848_irq_evidence e={root,(uintptr_t)f,flags848(),isr};
  if(!s848_timer_suspend(&ctl848,f,&e))fail848("tick-model");
  wr848(0x320,BIT|V);wr848(0xb0,0);
  if((rd848(0x120)&BIT)||(rd848(0x220)&BIT)||rd848(0x320)!=(BIT|V))fail848("eoi-mask");
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
void s848_trial(const struct s846_root child[2],const struct s844_lease leases[2],uint64_t safe,uint64_t admitted_lapic){
 struct desc848 g0={0},i0={0},check={0};uint16_t tr,cs;struct acpi806_topology topo={0};uint64_t apic=msr848(0x1b);
 if(root848()!=safe||(flags848()&0x200)||active848||!acpi806_copy_capture(&topo)||topo.enabled_lapics!=1||topo.lapic_physical!=admitted_lapic||!(apic&0x800)||(apic&0x400)||(apic&UINT64_C(0xfffff000))!=admitted_lapic||in848(0x21)!=0xff||in848(0xa1)!=0xff)fail848("apic-admission");
 lapic848=admitted_lapic;
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
 active848=true;
 arch_puts("STYLEOS S848 READY");fld848("safe",safe);fld848("a",child[0].built.root);fld848("b",child[1].built.root);fld848("lapic",lapic848);fld848("cs",sel.cs);fld848("ss",sel.ss);fld848("a-loop-end",sites848[0][1]);fld848("b-loop-end",sites848[1][1]);fld848("a-fault",sites848[0][2]);fld848("b-exit",sites848[1][2]);fld848("count",COUNT);arch_putc('\n');
 for(unsigned step=0;step<10;step++){
  static const unsigned order[10]={0,1,0,1,0,1,0,1,1,1};unsigned id;struct up_context ctx;uint64_t sp0,sp1;
  if(!s848_user_dispatch(&ctl848,root848(),&id,&ctx)||id!=order[step]||armed848||rd848(0x390)||(rd848(0x120)&BIT)||(rd848(0x220)&BIT))fail848("dispatch");
#ifdef STYLEOS_TEST_FX_CONTEXT_849
  if(!s849_guest_dispatch(id,child[id].built.root))fail848("fx-dispatch");
#endif
  bool terminal=step==6||step==9;
  if(terminal)put848(leases[id].pa+P+8,1);
  else{armed848=true;wr848(0x320,V);wr848(0x380,COUNT);}
  __asm__ volatile("mov %%rsp,%0":"=r"(sp0));s847_enter(&ctx,child[id].built.root,safe);__asm__ volatile("mov %%rsp,%0":"=r"(sp1));
  uint64_t a=word848(leases[id].pa+P),b=word848(leases[1-id].pa+P);
  if(root848()!=safe||(flags848()&0x200)||sp0!=sp1||armed848||ctl848.current!=-1||ctl848.slot[id].state!=(terminal?(id?S847_EXITED:S847_FAULTED):S847_READY)||b!=progress848[1-id]||a<progress848[id]||(!terminal&&a!=progress848[id]))fail848("safe-return");
  progress848[id]=a;
  arch_puts("STYLEOS S848 BACK");fld848("step",step);fld848("id",id);fld848("safe",safe);fld848("state",ctl848.slot[id].state);fld848("a",progress848[0]);fld848("b",progress848[1]);fld848("stack-same",1);arch_putc('\n');
 }
 unsigned id;struct up_context ctx;if(ticks848!=8||local848[0]!=3||local848[1]!=5||s848_user_dispatch(&ctl848,safe,&id,&ctx))fail848("final");
#ifdef STYLEOS_TEST_FX_CONTEXT_849
 if(!s849_guest_finish(safe))fail848("fx-finish");
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
