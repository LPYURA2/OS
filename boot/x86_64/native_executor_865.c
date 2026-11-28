/* Authenticated native execution core, no test workload dependency unless
 * the explicitly selected CI observer flag is present. */
#include "native_executor_865.h"
#if defined(STYLEOS_OWNED_SHELL_865) && !defined(STYLEOS_NATIVE_PINNED_CATALOG)
bool s865_sample_wait_allowed(uint32_t,uint64_t,uint64_t);
static struct {uint64_t generation,window;} input_wait70[PS_MAX_SLOTS];
#endif
#include <styleos/fx_guest_849.h>
#include <styleos/arch.h>
#ifdef STYLEOS_OWNED_SHELL_865
#include "native_deep_drop.h"
#include <styleos/acpi_madt_806.h>
#include <styleos/user_preempt_848.h>
#include <styleos/user_session_runtime.h>
#include <styleos/native_window_channel.h>
#include <styleos/native_runtime_865.h>
#include <styleos/native_file_abi.h>
#ifdef STYLEOS_LIVE_VOLUME_39
static struct {uint64_t generation,request;} file_wait95[PS_MAX_SLOTS];
#endif
#endif
#define V 0x50u
#define BIT 0x10000u
#define COUNT 5000000u
_Alignas(16) struct gate848 idt848[256];
struct dpr_service *runtime860;
struct rt862_bank bank862;
struct s847_raw pending860;
uint64_t cr2862,frame860,lapic848;
bool receipt860,active848,armed848;
static struct rt858_live live848(void){return rt864_arch_capture();}
static uint64_t root848(void){uint64_t v;__asm__ volatile("mov %%cr3,%0":"=r"(v));return v;}
static uint64_t flags848(void){uint64_t v;__asm__ volatile("pushfq;popq %0":"=r"(v));return v;}
static uint32_t rd848(unsigned off){return *(volatile uint32_t *)(uintptr_t)(lapic848+off);}
static uint32_t pending848(unsigned base){uint32_t v=0;for(unsigned i=0;i<8;i++)v|=rd848(base+16*i);return v;}
static void wr848(unsigned off,uint32_t n){*(volatile uint32_t *)(uintptr_t)(lapic848+off)=n;}
static void fld848(const char *k,uint64_t v){arch_putc(' ');arch_puts(k);arch_putc('=');arch_puts("0x");for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)i)&15]);}
static _Noreturn void fail848(const char *s){__asm__ volatile("cli");arch_puts("STYLEOS S848 FAIL ");arch_puts(s);arch_putc('\n');
#if !defined(STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW) && !defined(STYLEOS_PROVISIONED_STORAGE_864)
 __asm__ volatile("outl %0,%1"::"a"(0x27u),"Nd"((uint16_t)0xf4));
#endif
 arch_halt();}
static void gate848(unsigned i,void(*p)(void),uint16_t cs,uint8_t ist,uint8_t type){uint64_t v=(uintptr_t)p;idt848[i]=(struct gate848){(uint16_t)v,cs,ist,type,(uint16_t)(v>>16),(uint32_t)(v>>32),0};}

extern void s865_drain_wait(void),s865_drain_entry(void),s865_drain_cli(void);
volatile uint64_t s865_drain_root,s865_drain_lapic,s865_drain_seen;
static bool timer_only865(unsigned base){
 for(unsigned i=0;i<8;i++)if(rd848(base+16*i)&~(i==2?BIT:0u))return false;
 return true;
}
/* A real pending timer IRQ must be delivered and acknowledged, never
 * discarded via a synthetic EOI. Only the owned safe root can drain it. */
static bool drain865(uint64_t safe,uint16_t cs,unsigned id){
 if(root848()!=safe||(flags848()&0x200)||armed848||!receipt860||
    rd848(0x320)!=(BIT|V)||rd848(0x390)||rd848(0x380)||
    pending848(0x100)||!timer_only865(0x200)||rd848(0x80)||
    s865_drain_root||s865_drain_lapic||s865_drain_seen)return false;
 if(!rd848(0x220))return true;
 struct gate848 saved=idt848[V];
 s865_drain_root=safe;s865_drain_lapic=lapic848;
 gate848(V,s865_drain_entry,cs,0,0x8e);
 __asm__ volatile("":::"memory");
 for(unsigned attempt=0;attempt<256&&!s865_drain_seen;attempt++)s865_drain_wait();
 __asm__ volatile("":::"memory");
 idt848[V]=saved;
 bool ok=s865_drain_seen==1&&root848()==safe&&!(flags848()&0x200)&&
    !pending848(0x100)&&!pending848(0x200)&&!rd848(0x390)&&!rd848(0x380)&&rd848(0x320)==(BIT|V);
 s865_drain_root=0;s865_drain_lapic=0;s865_drain_seen=0;
 if(ok){arch_puts("STYLEOS S865 TIMER-DRAIN");fld848("id",id);fld848("safe",safe);fld848("pc",(uintptr_t)s865_drain_cli);fld848("vector",V);arch_putc('\n');}
 return ok;
}

bool s848_active(void){return active848;}
unsigned s848_dispatch_raw(struct s847_raw *f,uint64_t cr2,uint64_t root){
 int id=runtime860?runtime860->dispatcher.current:-1;
 if(!active848||id<0||(unsigned)id>=S846_ROOT_COUNT||root!=root848()||root!=bank862.slots[id].root||receipt860||(flags848()&0x200))fail848("862-origin");
 /* Validate actual non-IRQ frame before retiring our timer. The
  * classifier's armed argument describes the post-retirement contract.
  * Never discard a pending or in-service IRQ to manufacture a syscall. */
 enum rt865_frame_kind kind=s875_classify_native_frame(f,(uintptr_t)f,f->vector==14?cr2:0,
    root,bank862.safe,bank862.cs,bank862.ss,f->vector==V&&armed848,rt862_extent(&bank862,(unsigned)id));
 if(kind==RT865_INVALID)fail848("865-frame-contract");
 if(f->vector!=V){
  if(!armed848||rd848(0x320)!=V||rd848(0x380)!=COUNT||
     pending848(0x100)||!timer_only865(0x200))fail848("865-timer-cancel-origin");
  /* Explicit CI injection: let a real timer expire during the first
   * returning trap. Never needed by the retirement algorithm itself. */
#ifdef STYLEOS_NATIVE_EXECUTOR_OBSERVERS
  if(s865_test_force_timer((unsigned)id,f->vector,f->gpr[0])){
   unsigned spins=0;
   while(!(rd848(0x220)&BIT)&&spins++<10000000u)__asm__ volatile("pause");
   if(!(rd848(0x220)&BIT)||rd848(0x390)||!timer_only865(0x200))fail848("865-race-test-not-observed");
  }
#endif
  uint32_t remaining=rd848(0x390);
  if(remaining>COUNT)fail848("865-timer-cancel-count");
  wr848(0x320,BIT|V);wr848(0x380,0);
  uint32_t after=rd848(0x390),initial=rd848(0x380),lvt=rd848(0x320);
  uint32_t isr=pending848(0x100),irr=pending848(0x200);
  if(after||initial||lvt!=(BIT|V)||isr||!timer_only865(0x200))fail848("865-timer-cancel-pending");
  armed848=false;
  arch_puts("STYLEOS S865 TIMER-CANCEL");fld848("id",(unsigned)id);fld848("root",root);
  fld848("vector",f->vector);fld848("remaining",remaining);fld848("initial",initial);
  fld848("current",after);fld848("lvt",lvt);fld848("isr",isr);fld848("irr",irr);arch_putc('\n');
 }
 if(!s849_guest_suspend((unsigned)id,root,kind==RT865_TERMINAL||kind==RT865_USER_FAULT))fail848("fx-suspend");
 /* Authenticate the current owner against the actual scheduler and bank.
  * The event is classified by the CPU frame, never by user program bytes. */
 const struct ps_slot *owner=&runtime860->dispatcher.slots[id];
 const struct rt862_bank_slot *context=&bank862.slots[id];
 const struct pr_entry *running=pr_find(runtime860->dispatcher.registry,owner->pid);
 if(!running||running->state!=PR_RUNNING||!owner->pid||
    owner->pid!=context->pid||owner->handle!=context->handle||
    owner->user_root!=root||context->root!=root||
    running->scheduler_handle!=owner->handle||running->page_root!=root||
    runtime860->dispatcher.safe_root!=bank862.safe)fail848("865-current-owner");
 uint32_t isr=0;
 if(f->vector==V){
  isr=rd848(0x120);
  if(!armed848||rd848(0x390)||rd848(0x320)!=V||!(isr&BIT))
    fail848("865-irq-origin");
  /* A completed one-shot has zero current count but retains its initial
   * count register. Retire that configuration before EOI and safe return. */
  wr848(0x320,BIT|V);wr848(0x380,0);wr848(0xb0,0);
  if(rd848(0x380)||rd848(0x390)||pending848(0x100)||pending848(0x200)||rd848(0x320)!=(BIT|V))
    fail848("eoi-mask");
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
  arch_puts("STYLEOS NATIVE IRQ-RETIRE");fld848("slot",(unsigned)id);fld848("root",root);
  fld848("initial",rd848(0x380));fld848("current",rd848(0x390));fld848("lvt",rd848(0x320));
  fld848("isr",pending848(0x100));fld848("irr",pending848(0x200));arch_putc('\n');
#endif
  armed848=false;
 }else if(armed848||rd848(0x390)||rd848(0x320)!=(BIT|V)||
          pending848(0x100)||!timer_only865(0x200))fail848("865-nonirq-timer");
 /* Only authenticated receipt is consumed by the scheduler on safe root.
  * No expected instruction PC, GPR signature or per-task tick budget. */
 pending860=*f;frame860=(uintptr_t)f;
 cr2862=f->vector==14?cr2:0;receipt860=true;

#ifdef STYLEOS_NATIVE_EXECUTOR_OBSERVERS
 s865_test_observe_trap((unsigned)id,kind,f,root,isr);
#endif
 return 0;
}
struct rt865_slice run865(const struct rt862_selection *ticket,
 const struct up_context *ctx,const struct desc848 *g0,const struct desc848 *i0,
 const struct desc848 *g,const struct desc848 *d,
 const struct rt858_live *original,bool (*audit)(void *),void *opaque){
 if(!ticket||!ctx||!g0||!i0||!g||!d||!original||!audit)fail848("865-dispatch-argument");
 const uint64_t safe=ticket->safe_root;
 const unsigned id=ticket->slot;
 if(!active848||!runtime860||id>=S846_ROOT_COUNT||root848()!=safe||
    flags848()&0x200||receipt860||frame860||cr2862||armed848||
    rd848(0x320)!=(BIT|V)||rd848(0x390)||pending848(0x100)||pending848(0x200))
  fail848("865-dispatch-boundary");
 /* Re-derive the context from the authenticated bank, never trust a
  * caller-provided replacement PC/register file. */
 struct up_context expected={0};
 if(!rt862_bank_enter(&bank862,runtime860,ticket,safe,&expected)||
    ctx->pc!=expected.pc||ctx->sp!=expected.sp||ctx->status!=expected.status||
    ctx->cs!=expected.cs||ctx->ss!=expected.ss)fail848("865-dispatch-ticket");
 for(unsigned i=0;i<15;i++)if(ctx->gpr[i]!=expected.gpr[i])fail848("865-dispatch-register");
 const struct pr_entry *running=pr_find(runtime860->dispatcher.registry,ticket->pid);
 if(!running||running->state!=PR_RUNNING||running->scheduler_handle!=ticket->handle||
    running->page_root!=ticket->child_root)fail848("865-dispatch-owner");
 struct rt858_live boundary=live848();
 if(boundary.root!=safe||boundary.gdt!=original->gdt||boundary.idt!=original->idt||
    boundary.gdt_limit!=original->gdt_limit||boundary.idt_limit!=original->idt_limit||
    boundary.tr!=original->tr||boundary.cs!=original->cs||!audit(opaque)||
    g->base!=g0->base||g->limit!=g0->limit+16u||
    d->base!=(uintptr_t)idt848||d->limit!=sizeof idt848-1)fail848("865-dispatch-descriptors");
 const uint64_t before=running->cpu_ticks;
 const uint64_t resident=running->resident_bytes;
 __asm__ volatile("lgdt %0;lidt %1"::"m"(*g),"m"(*d):"memory");
 if(!s849_guest_dispatch(id,ticket->child_root))fail848("fx-dispatch");
 armed848=true;wr848(0x320,V);wr848(0x380,COUNT);
 uint64_t sp0,sp1;
 __asm__ volatile("mov %%rsp,%0":"=r"(sp0));
 s847_enter(&expected,ticket->child_root,safe);
 __asm__ volatile("mov %%rsp,%0":"=r"(sp1));
 if(root848()!=safe||(flags848()&0x200)||sp0!=sp1||armed848||!receipt860||
    rd848(0x390)||rd848(0x320)!=(BIT|V)||pending848(0x100)||!timer_only865(0x200))
  fail848("865-safe-return");
 if(pending860.vector!=V){if(!drain865(safe,original->cs,id))fail848("865-timer-drain");}
 else if(pending848(0x200))fail848("865-irq-duplicate");
 enum rt865_frame_kind kind=s875_classify_native_frame(&pending860,frame860,cr2862,
     ticket->child_root,safe,bank862.cs,bank862.ss,pending860.vector==V,rt862_extent(&bank862,id));
 if(kind==RT865_INVALID)fail848("865-receipt-kind");
 enum pr_state state=kind==RT865_USER_FAULT?PR_FAULTED:kind==RT865_TERMINAL?(pending860.vector==128?PR_EXITED:PR_FAULTED):PR_READY;
 __asm__ volatile("lidt %0;lgdt %1"::"m"(*i0),"m"(*g0):"memory");
 struct rt858_live returned=live848();
 if(returned.root!=safe||returned.gdt!=original->gdt||returned.idt!=original->idt||
    returned.gdt_limit!=original->gdt_limit||returned.idt_limit!=original->idt_limit||
    returned.tr!=original->tr||returned.cs!=original->cs||(returned.rflags&0x200)||
    !audit||!audit(opaque))fail848("861-timer-restore");
 struct rt860_tick t={.slot=id,.dispatch=ticket->dispatch,.pid=ticket->pid,
  .handle=ticket->handle,.child_root=ticket->child_root,.safe_root=root848(),
  .frame_va=frame860,.vector=pending860.vector,.isr=pending860.vector==V,
  .eoi=pending860.vector==V,.safe_flags=flags848(),.frame=pending860};
 if(rt863_bank_event(&bank862,runtime860,ticket,safe,&t,cr2862)!=PS_OK)fail848("863-event-commit");
#ifdef STYLEOS_OWNED_SHELL_865
 /* Only after actual CPL3 trap, safe-root return, scheduler commit and
  * full mapping audit. Never accept caller-supplied PID/root/generation. */
 if(kind==RT865_SYSCALL && (pending860.gpr[0]==S865_CALL_SHELL_REDRAW||
     pending860.gpr[0]==S865_CALL_SHELL_EVENT||pending860.gpr[0]==S865_CALL_SHELL_DECIDE)){
  const struct pr_entry *caller=pr_find(runtime860->dispatcher.registry,ticket->pid);
  struct rt862_bank_slot *slot=&bank862.slots[id];
  if(!caller||caller->state!=PR_READY||caller->scheduler_handle!=ticket->handle||
     caller->page_root!=ticket->child_root||slot->pid!=ticket->pid||
     slot->handle!=ticket->handle||slot->root!=ticket->child_root||
     runtime860->dispatcher.current!=-1||root848()!=safe||(flags848()&0x200))
   fail848("865-shell-controller-owner");
  uint64_t reply=0,op=pending860.gpr[0];
  if(op==S865_CALL_SHELL_DECIDE){
   if(!pending860.gpr[3])reply=s865_shell_controller_decide(ticket->pid,ticket->handle,
    ticket->child_root,pending860.gpr[1],pending860.gpr[2]);
  }else if(!pending860.gpr[1]&&!pending860.gpr[2]&&!pending860.gpr[3]){
   if(op==S865_CALL_SHELL_EVENT)reply=s865_shell_controller_event(ticket->pid,ticket->handle,ticket->child_root);
   else reply=s865_shell_controller_request(ticket->pid,ticket->handle,ticket->child_root);
  }
  slot->context.gpr[0]=reply;
  arch_puts("STYLEOS NATIVE SHELL-CALL");fld848("pid",ticket->pid);fld848("gen",ticket->handle);
  fld848("root",ticket->child_root);fld848("op",op);fld848("reply",reply);arch_putc('\n');
 }
 const uint64_t operation=pending860.gpr[0];
 if(kind==RT865_SYSCALL&&operation>=NF91_READ&&operation<=NF102_WRESULT){
  const struct pr_entry *caller=pr_find(runtime860->dispatcher.registry,ticket->pid);
  struct rt862_bank_slot *slot=&bank862.slots[id];
  if(!caller||caller->state!=PR_READY||caller->scheduler_handle!=ticket->handle||
     caller->page_root!=ticket->child_root||slot->pid!=ticket->pid||
     slot->handle!=ticket->handle||slot->root!=ticket->child_root||
     runtime860->dispatcher.current!=-1||root848()!=safe||(flags848()&0x200))
   fail848("865-file-owner");
  uint64_t value=0,status=NF91_UNAVAILABLE;
#ifdef STYLEOS_LIVE_VOLUME_39
  const bool wait=operation==NF95_READ_WAIT||operation==NF95_WRITE_WAIT;
  const uint64_t mapped=operation==NF95_READ_WAIT?NF91_READ:
                        operation==NF95_WRITE_WAIT?NF91_WRITE:operation;
  status=s91_native_file_call(id,ticket->pid,ticket->handle,ticket->child_root,
     mapped,pending860.gpr[1],pending860.gpr[2],pending860.gpr[3],&value);
  if(wait&&status==PO64_OK){
   if(!value||file_wait95[id].generation||input_wait70[id].generation||
      rt865_bank_wait_state(&bank862,runtime860,safe,id,ticket->pid,ticket->handle,ticket->child_root,true)!=PS_OK)
    fail848("95-file-park");
   file_wait95[id].generation=ticket->handle;file_wait95[id].request=value;
   state=PR_WAITING;
   arch_puts("STYLEOS FILE95 PARK");fld848("pid",ticket->pid);fld848("gen",ticket->handle);
   fld848("request",value);arch_putc('\n');
  }
#endif
  slot->context.gpr[0]=status;slot->context.gpr[3]=value;
#if defined(STYLEOS_FILE_PROBE_92) || defined(STYLEOS_FILE_PROBE_94) || defined(STYLEOS_FILE_PROBE_103)
  arch_puts("STYLEOS FILE92 CALL");fld848("pid",ticket->pid);fld848("op",operation);
  fld848("status",status);fld848("value",value);arch_putc('\n');
#endif
 }
 const bool drop=operation>=DD_CALL_OFFER && operation<=DD_CALL_CANCEL;
 if(kind==RT865_SYSCALL && (drop||pending860.gpr[0]==NWC_CALL_PIXEL||pending860.gpr[0]==NWC_CALL_TILE||pending860.gpr[0]==NWC_CALL_INPUT||pending860.gpr[0]==NWC_CALL_FRAME||operation==NWC_CALL_PATCH||operation==NWC_CALL_FRAME_GEN||operation==NWC_CALL_TEXT)){
  const struct pr_entry *caller=pr_find(runtime860->dispatcher.registry,ticket->pid);
  struct rt862_bank_slot *slot=&bank862.slots[id];
  if(!caller||caller->state!=PR_READY||caller->scheduler_handle!=ticket->handle||
     caller->page_root!=ticket->child_root||slot->pid!=ticket->pid||
     slot->handle!=ticket->handle||slot->root!=ticket->child_root||
     runtime860->dispatcher.current!=-1||root848()!=safe||(flags848()&0x200))
   fail848("865-pixel-owner");
  enum nwc_status pixel=NWC_INVALID;
  if(drop){
   uint64_t result=0;
   enum dd_status status=native_drop_call(ticket,(unsigned)operation,pending860.gpr[1],
                                          pending860.gpr[2],pending860.gpr[3],&result);
   slot->context.gpr[0]=status;slot->context.gpr[3]=result;
   arch_puts("STYLEOS DD72 CALL");fld848("pid",ticket->pid);fld848("gen",ticket->handle);
   fld848("root",ticket->child_root);fld848("op",operation);fld848("id",pending860.gpr[1]);
   fld848("status",status);fld848("result",result);arch_putc('\n');
  }else if(pending860.gpr[0]==NWC_CALL_PIXEL){
   pixel=nwc_publish_pixel((struct nwc_service *)nwc_global(),
    ticket->pid,ticket->handle,ticket->child_root,pending860.gpr[1],pending860.gpr[2]);
#if defined(STYLEOS_NATIVE_PINNED_CATALOG) || defined(STYLEOS_FILE_PROBE_92) || defined(STYLEOS_FILE_PROBE_94) || defined(STYLEOS_FILE_PROBE_103)
   arch_puts("STYLEOS NATIVE PIXEL");fld848("pid",ticket->pid);fld848("gen",ticket->handle);
   fld848("root",ticket->child_root);fld848("window",pending860.gpr[1]);
   fld848("rgb",pending860.gpr[2]);fld848("status",pixel);arch_putc('\n');
#endif
  }else if(operation==NWC_CALL_TEXT){
   uint64_t generation=0,bytes=pending860.gpr[3];
   uint8_t wire[NWC_TEXT_WIRE_MAX]={0};
   if(bytes>=NWC_TEXT_HEADER&&bytes<=sizeof wire&&
      s865_native_copy_data(id,ticket->pid,ticket->handle,ticket->child_root,
                            pending860.gpr[2],wire,(size_t)bytes))
    pixel=nwc_text_wire((struct nwc_service *)nwc_global(),ticket->pid,ticket->handle,
      ticket->child_root,pending860.gpr[1],wire,(size_t)bytes,&generation);
   slot->context.gpr[3]=pixel==NWC_OK?generation:0;
   volatile uint8_t *wipe=wire;
   for(size_t j=0;j<sizeof wire;++j)wipe[j]=0;
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
   arch_puts("STYLEOS NATIVE TEXT-CALL");fld848("pid",ticket->pid);fld848("gen",ticket->handle);
   fld848("root",ticket->child_root);fld848("window",pending860.gpr[1]);fld848("bytes",bytes);
   fld848("status",pixel);fld848("generation",slot->context.gpr[3]);arch_putc('\n');
#endif
  }else if(operation==NWC_CALL_PATCH||operation==NWC_CALL_FRAME_GEN){
   uint64_t generation=0;
   if(operation==NWC_CALL_FRAME_GEN){
    if(!pending860.gpr[2]&&!pending860.gpr[3])
     pixel=nwc_frame_generation((struct nwc_service *)nwc_global(),
      ticket->pid,ticket->handle,ticket->child_root,pending860.gpr[1],&generation);
   }else{
    uint8_t wire[NWC_PATCH_BYTES]={0};
    if(pending860.gpr[3]==NWC_PATCH_BYTES&&s865_native_copy_data(id,ticket->pid,
       ticket->handle,ticket->child_root,pending860.gpr[2],wire,sizeof wire))
     pixel=nwc_patch_wire((struct nwc_service *)nwc_global(),ticket->pid,ticket->handle,
       ticket->child_root,pending860.gpr[1],wire,sizeof wire,&generation);
    volatile uint8_t *wipe=wire;
    for(size_t j=0;j<sizeof wire;++j)wipe[j]=0;
   }
   slot->context.gpr[3]=pixel==NWC_OK?generation:0;
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
   arch_puts("STYLEOS NATIVE PATCH-CALL");fld848("pid",ticket->pid);fld848("gen",ticket->handle);
   fld848("root",ticket->child_root);fld848("op",operation);fld848("window",pending860.gpr[1]);
   fld848("status",pixel);fld848("generation",slot->context.gpr[3]);arch_putc('\n');
#endif
  }else if(pending860.gpr[0]==NWC_CALL_FRAME){
   uint64_t count=pending860.gpr[3];
   struct nwc_rect rect[NWC_FRAME_RECTS]={0};
   if(count&&count<=NWC_FRAME_RECTS&&s865_native_copy_data(id,ticket->pid,ticket->handle,
      ticket->child_root,pending860.gpr[2],rect,(size_t)count*sizeof rect[0]))
    pixel=nwc_publish_frame((struct nwc_service *)nwc_global(),ticket->pid,ticket->handle,
      ticket->child_root,pending860.gpr[1],rect,(unsigned)count);
   arch_puts("STYLEOS NATIVE FRAME");fld848("pid",ticket->pid);fld848("gen",ticket->handle);
   fld848("root",ticket->child_root);fld848("window",pending860.gpr[1]);
   fld848("va",pending860.gpr[2]);fld848("count",count);fld848("status",pixel);
   fld848("first",pixel==NWC_OK?rect[0].rgb:0);arch_putc('\n');
   volatile unsigned char *wipe=(void *)rect;
   for(size_t j=0;j<sizeof rect;++j)wipe[j]=0;
  }else if(pending860.gpr[0]==NWC_CALL_INPUT){
   uint8_t wire[NWC_INPUT_BYTES]={0};
   volatile uint8_t *dst=0;
   if(pending860.gpr[3]==NWC_INPUT_BYTES)
    dst=s865_native_data_span(id,ticket->pid,ticket->handle,ticket->child_root,
                             pending860.gpr[2],NWC_INPUT_BYTES);
   /* Preflight destination before dequeuing; no failure point between
    * successful dequeue and bounded write to an audited private alias. */
   if(dst){
    pixel=nwc_receive_wire((struct nwc_service *)nwc_global(),ticket->pid,
                          ticket->handle,ticket->child_root,pending860.gpr[1],wire);
    if(pixel==NWC_OK)
     for(size_t j=0;j<NWC_INPUT_BYTES;++j)dst[j]=wire[j];
   }
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
   arch_puts("STYLEOS NATIVE INPUT");fld848("pid",ticket->pid);fld848("gen",ticket->handle);
   fld848("root",ticket->child_root);fld848("window",pending860.gpr[1]);
   fld848("va",pending860.gpr[2]);fld848("bytes",pending860.gpr[3]);fld848("status",pixel);
   fld848("seq",pixel==NWC_OK?((uint32_t)wire[4]|((uint32_t)wire[5]<<8)|((uint32_t)wire[6]<<16)|((uint32_t)wire[7]<<24)):0);
   arch_putc('\n');
#endif
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
   /* Bounded per-slot statistics; a new generation cannot inherit counts.
    * Count real authenticated input requests; emit only on delivery, not
    * during busy empty polling. Neither status nor scheduling is altered. */
   static struct {uint64_t generation,empty,delivered,other;} poll[PS_MAX_SLOTS];
   if(poll[id].generation!=ticket->handle){
    poll[id].generation=ticket->handle;poll[id].empty=0;poll[id].delivered=0;poll[id].other=0;
   }
   const bool visibility_wait=dst&&pixel==NWC_DENIED&&
    nwc_wait_condition((struct nwc_service *)nwc_global(),ticket->pid,ticket->handle,
                       ticket->child_root,pending860.gpr[1])==NWC_WAIT_BLOCKED;
   if(pixel==NWC_EMPTY||visibility_wait){
    if(pixel==NWC_EMPTY)++poll[id].empty;else ++poll[id].other;
    /* Preserve the original EMPTY or DENIED result for the approved retry
     * loop clients. Hidden but authentic ports are not revocations.
     * Single CPU + IF=0 serialize receive, park, enqueue and wake scan. */
    if(s865_sample_wait_allowed(ticket->pid,ticket->handle,ticket->child_root)){
     if(rt865_bank_wait_state(&bank862,runtime860,safe,id,ticket->pid,ticket->handle,ticket->child_root,true)!=PS_OK)
      fail848("70-input-park");
     input_wait70[id].generation=ticket->handle;input_wait70[id].window=pending860.gpr[1];
     state=PR_WAITING;
     arch_puts("STYLEOS TEA70 PARK");fld848("pid",ticket->pid);fld848("gen",ticket->handle);
     arch_putc('\n');
    }
   }
   else if(pixel==NWC_OK){
    ++poll[id].delivered;
    arch_puts("STYLEOS TEA70 INPUT-COUNTS");fld848("pid",ticket->pid);
    fld848("gen",ticket->handle);fld848("empty",poll[id].empty);
    fld848("delivered",poll[id].delivered);fld848("other",poll[id].other);
    fld848("kind",(uint32_t)wire[12]|((uint32_t)wire[13]<<8)|((uint32_t)wire[14]<<16)|((uint32_t)wire[15]<<24));
    fld848("code",(uint32_t)wire[16]|((uint32_t)wire[17]<<8)|((uint32_t)wire[18]<<16)|((uint32_t)wire[19]<<24));arch_putc('\n');
   }else ++poll[id].other;
#endif
   volatile uint8_t *wipe_wire=wire;
   for(size_t j=0;j<NWC_INPUT_BYTES;++j)wipe_wire[j]=0;
  }else{
   uint64_t shape=pending860.gpr[3];
   unsigned w=(unsigned)(shape&65535u),h=(unsigned)((shape>>16)&65535u);
   uint32_t pixels[NWC_TILE_PIXELS]={0};
   if(!(shape>>32)&&w&&h&&w<=NWC_TILE_WIDTH&&h<=NWC_TILE_HEIGHT&&
      s865_native_copy_data(id,ticket->pid,ticket->handle,ticket->child_root,
                           pending860.gpr[2],pixels,(size_t)w*h*sizeof pixels[0]))
    pixel=nwc_publish_block((struct nwc_service *)nwc_global(),ticket->pid,
       ticket->handle,ticket->child_root,pending860.gpr[1],pixels,w,h);
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
   arch_puts("STYLEOS NATIVE TILE");fld848("pid",ticket->pid);fld848("gen",ticket->handle);
   fld848("root",ticket->child_root);fld848("window",pending860.gpr[1]);
   fld848("va",pending860.gpr[2]);fld848("shape",shape);fld848("status",pixel);
   fld848("first",pixel==NWC_OK?pixels[0]:0);arch_putc('\n');
#endif
   volatile uint32_t *wipe_pixels=pixels;
   for(size_t j=0;j<NWC_TILE_PIXELS;++j)wipe_pixels[j]=0;
  }
  if(!drop)slot->context.gpr[0]=(uint64_t)pixel;
 }
#endif
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
 arch_puts("STYLEOS NATIVE EVENT");fld848("slot",id);fld848("pid",ticket->pid);fld848("root",ticket->child_root);
 fld848("vector",pending860.vector);fld848("error",pending860.error);fld848("rip",pending860.rip);
 fld848("sp",pending860.rsp);fld848("flags",pending860.rflags);fld848("rax",pending860.gpr[0]);fld848("state",state);arch_putc('\n');
#endif
 receipt860=false;frame860=0;cr2862=0;
 volatile unsigned char *wipe=(void *)&pending860;
 for(size_t j=0;j<sizeof pending860;j++)wipe[j]=0;
 const struct pr_entry *after=pr_find(runtime860->dispatcher.registry,ticket->pid);
 if(!after||after->state!=state||after->resident_bytes!=resident||
    after->cpu_ticks!=before+(kind==RT865_TICK)||runtime860->dispatcher.current!=-1)
  fail848("865-committed-state");
 return (struct rt865_slice){kind==RT865_USER_FAULT?RT865_TERMINAL:kind,state,before};
}

static bool child_writable(const struct s846_root *r,uint64_t x,uint64_t n){
 if(!r||!r->ready||r->plan.count>PG_MAX_MAPS)return false;
 for(size_t i=0;i<r->plan.count;i++){
  const struct pg_mapping *m=&r->plan.maps[i];
  if(m->va==m->pa&&m->flags==PG_WRITE&&x>=m->va&&x-m->va<m->bytes&&n<=m->bytes-(x-m->va))return true;
 }
 return false;
}
enum ps_status s865_executor_launch(unsigned id,const struct s846_root *child,
 const struct s844_arena *arena,const struct s844_lease *lease,const struct rt865_launch_request *request,
 uint8_t *scratch,size_t capacity,bool (*audit)(void *),void *opaque,struct nte_loaded *image,uint32_t *pid){
 if(!runtime860||!bank862.safe||root848()!=bank862.safe||(flags848()&0x200)||
    armed848||receipt860||frame860||cr2862||runtime860->dispatcher.current!=-1||
    id>=S846_ROOT_COUNT||id>=PS_MAX_SLOTS||bank862.slots[id].pid||
    !child||!arena||!lease||!audit||!s844_lease_live(arena,*lease)||!audit(opaque))return PS_EVIDENCE;
 struct rt858_live live=live848();
 if(rt861_dispatcher(&live)!=runtime860||
    !child_writable(child,lapic848,4096)||
    !child_writable(child,live.gdt,4096)||
    !child_writable(child,(uintptr_t)idt848,sizeof idt848)||
    !s865_fx_guest_bind(id,child,lease,bank862.safe))return PS_EVIDENCE;
 enum ps_status result=rt865_launch_prepared(&bank862,runtime860,bank862.safe,id,child,arena,lease,request,
     scratch,capacity,audit,opaque,image,pid);
 if(result!=PS_OK&&!s865_fx_retire(id,child->built.root,lease->generation,bank862.safe))
  fail848("865-launch-fp-rollback");
 return result;
}
#ifdef STYLEOS_OWNED_SHELL_865
enum ps_status s875_executor_admit(unsigned id,const struct s846_root *child,
 const struct s844_arena *arena,const struct s844_lease *lease,const struct nai75_layout *image,
 const char *name,bool (*audit)(void *),void *opaque,uint32_t *pid){
 if(!s865_executor_idle()||runtime860->dispatcher.current!=-1||
    id>=S846_ROOT_COUNT||id>=PS_MAX_SLOTS||bank862.slots[id].pid||
    !child||!arena||!lease||!image||!name||!pid||!audit||
    !s844_lease_live(arena,*lease)||!audit(opaque))return PS_EVIDENCE;
 if(!image->stack_pages||image->stack_pages>NAI75_STACK_PAGES)return PS_EVIDENCE;
 struct rt858_live live=live848();
 if(rt861_dispatcher(&live)!=runtime860||
    !child_writable(child,lapic848,4096)||
    !child_writable(child,live.gdt,4096)||
    !child_writable(child,(uintptr_t)idt848,sizeof idt848)||
    !s865_fx_guest_bind(id,child,lease,bank862.safe))return PS_EVIDENCE;
 const struct up_context initial={.pc=image->entry,
    .sp=S846_STACK+(uint64_t)image->stack_pages*4096u,
    .status=2,.cs=bank862.cs,.ss=bank862.ss};
 enum ps_status status=rt875_bank_admit_image(&bank862,runtime860,bank862.safe,id,
     child,lease,image,name,&initial,pid);
 if(status!=PS_OK&&!s865_fx_retire(id,child->built.root,lease->generation,bank862.safe))
    fail848("875-launch-fp-rollback");
 return status;
}
#endif
static void retire865_identity(const struct rt862_selection *t,bool (*audit)(void *),
 bool (*retire)(void *,unsigned,uint64_t,uint64_t),void *opaque){
 if(!t||!audit||!retire||!runtime860||!active848||armed848||receipt860||
    frame860||cr2862||root848()!=t->safe_root||(flags848()&0x200)||
    runtime860->dispatcher.current!=-1||t->safe_root!=bank862.safe||
    t->slot>=S846_ROOT_COUNT)
  fail848("865-terminal-boundary");
 const unsigned id=t->slot;
 const struct ps_slot *s=&runtime860->dispatcher.slots[id];
 const struct pr_entry *p=pr_find(runtime860->dispatcher.registry,t->pid);
 if(!p||(p->state!=PR_EXITED&&p->state!=PR_FAULTED)||
    s->pid!=t->pid||s->handle!=t->handle||s->user_root!=t->child_root||
    bank862.slots[id].pid!=t->pid||bank862.slots[id].handle!=t->handle||
    bank862.slots[id].root!=t->child_root||!audit(opaque))
  fail848("865-terminal-owner");
 for(unsigned i=0;i<WS_MAX_WINDOWS;i++)if(runtime860->windows->windows[i].id&&
    runtime860->windows->windows[i].owner_pid==t->pid)fail848("865-terminal-window");
 if(!s865_fx_retire(id,t->child_root,t->handle,t->safe_root)||
    !retire(opaque,id,t->child_root,t->handle)||
    rt862_bank_release(&bank862,runtime860,t->safe_root,id,t->pid,t->handle,t->child_root)!=PS_OK||
    pr_find(runtime860->dispatcher.registry,t->pid)||bank862.slots[id].pid||
    runtime860->dispatcher.slots[id].pid||!audit(opaque))fail848("865-terminal-release");
}

void s865_retire_terminal(const struct rt862_selection *t,bool (*audit)(void *),
 bool (*retire)(void *,unsigned,uint64_t,uint64_t),void *opaque){
 if(!t||!runtime860||t->dispatch!=runtime860->dispatcher.dispatches)
  fail848("865-terminal-ticket");
 retire865_identity(t,audit,retire,opaque);
}

#ifdef STYLEOS_OWNED_SHELL_865
/* Persistent descriptor/APIC ownership under the already admitted safe
 * root. Never arm the timer or enable IF while executing the shell. */
static struct desc848 native_g0,native_i0,native_g,native_d;
static struct rt858_live native_original;
extern void s847_bad_entry(void),s848_irq_entry(void),s864_ud_entry(void),s864_gp_entry(void);
extern void s865_de_entry(void),s865_db_entry(void);
static uint8_t native_in(uint16_t p){uint8_t x;__asm__ volatile("inb %1,%0":"=a"(x):"Nd"(p));return x;}
static uint64_t native_apic(void){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(0x1b));return ((uint64_t)d<<32)|a;}
bool s865_executor_boot(uint64_t safe,uint64_t lapic,bool (*audit)(void *),void *opaque){
 struct acpi806_topology topo={0};
 if(!safe||!lapic||active848||runtime860||root848()!=safe||(flags848()&0x200)||
    !audit||!audit(opaque)||!acpi806_copy_capture(&topo)||topo.enabled_lapics!=1||
    topo.lapic_physical!=lapic||native_in(0x21)!=0xff||native_in(0xa1)!=0xff)return false;
 const uint64_t apic=native_apic();
 if(!(apic&0x800)||(apic&0x400)||(apic&UINT64_C(0xfffff000))!=lapic)return false;
 native_original=live848();runtime860=rt861_dispatcher(&native_original);
 if(!runtime860||pr_snapshot(runtime860->dispatcher.registry,0,0,true))return false;
 lapic848=lapic;
 uint32_t lvt=rd848(0x320),initial=rd848(0x380),current=rd848(0x390),div=rd848(0x3e0),svr=rd848(0xf0);
 if(!s848_timer_admit(lvt,initial,current,div,pending848(0x100),pending848(0x200))||rd848(0x320)!=lvt)return false;
 wr848(0x380,0);
 if(rd848(0x380)||rd848(0x390)||pending848(0x100)||pending848(0x200))return false;
 native_g0=(struct desc848){native_original.gdt_limit,native_original.gdt};
 native_i0=(struct desc848){native_original.idt_limit,native_original.idt};
 struct s847_selectors sel={0};
 if(!s847_gdt_append((void *)(uintptr_t)native_g0.base,4096,native_g0.limit,native_original.tr,native_original.cs,&sel))return false;
 for(unsigned j=0;j<256;j++)gate848(j,s847_bad_entry,native_original.cs,3,0x8e);
 gate848(V,s848_irq_entry,native_original.cs,1,0x8e);
 gate848(128,s847_call_entry,native_original.cs,1,0xee);
 gate848(14,s847_pf_entry,native_original.cs,2,0x8e);
 gate848(0,s865_de_entry,native_original.cs,2,0x8e);
 gate848(1,s865_db_entry,native_original.cs,2,0x8e);
 gate848(6,s864_ud_entry,native_original.cs,2,0x8e);
 gate848(13,s864_gp_entry,native_original.cs,2,0x8e);
 native_g=(struct desc848){sel.limit,native_g0.base};
 native_d=(struct desc848){sizeof idt848-1,(uintptr_t)idt848};
 wr848(0x80,0);wr848(0xf0,(svr&~255u)|0x1ffu);wr848(0x320,BIT|V);wr848(0x3e0,3);
 if(rd848(0x320)!=(BIT|V)||rd848(0x3e0)!=3||rd848(0x80)||rd848(0xf0)!=((svr&~255u)|0x1ffu)||
    !s865_fx_guest_open(safe)||!rt862_bank_open(&bank862,runtime860,safe,sel.cs,sel.ss))return false;
 active848=true;
 return s865_executor_idle()&&audit(opaque);
}
bool s865_executor_idle(void){
 if(!active848||!runtime860||root848()!=bank862.safe||(flags848()&0x200)||
    receipt860||frame860||cr2862||armed848||rd848(0x320)!=(BIT|V)||
    rd848(0x380)||rd848(0x390)||pending848(0x100)||pending848(0x200))return false;
 struct rt858_live live=live848();
 return rt861_dispatcher(&live)==runtime860;
}
enum ps_status s865_executor_stop(unsigned id,uint32_t pid,uint64_t generation,
 uint64_t child,bool authorized,bool (*audit)(void *),
 bool (*retire)(void *,unsigned,uint64_t,uint64_t),void *opaque){
 if(!authorized||!pid||!generation||id>=S846_ROOT_COUNT||!audit||!retire)
  return PS_EVIDENCE;
 if(!s865_executor_idle()||runtime860->dispatcher.current!=-1)return PS_STATE;
 if(!audit(opaque))return PS_EVIDENCE;
 const uint64_t safe=root848();
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
 const struct pr_entry *target=pr_find(runtime860->dispatcher.registry,pid);
 const bool parked=target&&target->state==PR_WAITING;
 bool input_parked=input_wait70[id].generation==generation;
#ifdef STYLEOS_LIVE_VOLUME_39
 const bool file_parked=file_wait95[id].generation==generation&&file_wait95[id].request!=0;
 if(parked&&input_parked==file_parked)return PS_EVIDENCE;
#else
 if(parked&&!input_parked)return PS_EVIDENCE;
#endif
 if(parked&&(target->scheduler_handle!=generation||target->page_root!=child))
  return PS_EVIDENCE;
#endif
 enum ps_status status=rt865_bank_stop_ready(&bank862,runtime860,safe,id,pid,
                                             generation,child,authorized);
 if(status!=PS_OK)return status;
 (void)usr_reconcile((struct usr_runtime *)usr_global());
 (void)nwc_reconcile((struct nwc_service *)nwc_global());
 native_drop_reconcile();
#ifdef STYLEOS_LIVE_VOLUME_39
  s88_native_file_reconcile();
#endif
 /* Cleanup identity only; dispatch zero is not a trap or dispatch ticket. */
 const struct rt862_selection identity={id,0,pid,generation,child,safe};
 retire865_identity(&identity,audit,retire,opaque);
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
 /* Retire broker state synchronously, even when there are no surviving
  * processes and the next supervisor poll would take its empty fast path. */
 input_wait70[id].generation=0;input_wait70[id].window=0;
#ifdef STYLEOS_LIVE_VOLUME_39
 file_wait95[id].generation=0;file_wait95[id].request=0;
 if(file_parked){
  arch_puts("STYLEOS FILE95 WAIT-RETIRED");fld848("pid",pid);
  fld848("gen",generation);fld848("request",file_wait95[id].request);arch_putc('\n');
 }
#endif
 if(parked&&input_parked){
  arch_puts("STYLEOS TEA70 WAIT-RETIRED");fld848("pid",pid);
  fld848("gen",generation);fld848("wait-gen",input_wait70[id].generation);
  fld848("wait-window",input_wait70[id].window);arch_putc('\n');
 }
#endif
 if(!s865_executor_idle())fail848("865-stop-return");
#ifdef STYLEOS_NATIVE_PINNED_CATALOG
 arch_puts("STYLEOS NATIVE FORCED-STOP");fld848("slot",id);fld848("pid",pid);
 fld848("gen",generation);fld848("root",child);arch_putc('\n');
#endif
 return PS_OK;
}
void s865_executor_poll(bool (*audit)(void *),bool (*retire)(void *,unsigned,uint64_t,uint64_t),void *opaque){
 if(!s865_executor_idle()||!audit||!retire)fail848("native-poll-boundary");
 native_drop_reconcile();
 /* Live-volume reconciliation already occurs before the empty fast path
  * in s865_native_pump; terminal cleanup remains synchronous below. */
 if(!pr_snapshot(runtime860->dispatcher.registry,0,0,true))return;
 if(!audit(opaque))fail848("native-poll-audit");
#ifdef STYLEOS_LIVE_VOLUME_39
 /* The volume pump ran at the safe supervisor boundary before this scan.
  * Waiters never retain user addresses or execute while parked. */
 for(unsigned j=0;j<PS_MAX_SLOTS;++j){
  const struct rt862_bank_slot *s=&bank862.slots[j];
  if(!file_wait95[j].generation)continue;
  if(!s->pid||s->handle!=file_wait95[j].generation)fail848("95-wait-generation");
  const struct pr_entry *p=pr_find(runtime860->dispatcher.registry,s->pid);
  if(!p||p->state!=PR_WAITING||input_wait70[j].generation)fail848("95-wait-identity");
  const uint64_t request=file_wait95[j].request;
  if(s95_native_file_wait(s->pid,s->handle,s->root,request)==PO64_BUSY)continue;
  if(rt865_bank_wait_state(&bank862,runtime860,bank862.safe,j,s->pid,s->handle,s->root,false)!=PS_OK)
   fail848("95-file-wake");
  file_wait95[j].generation=0;file_wait95[j].request=0;
  arch_puts("STYLEOS FILE95 WAKE");fld848("pid",s->pid);fld848("gen",s->handle);
  fld848("request",request);arch_putc('\n');
 }
#endif
#ifndef STYLEOS_NATIVE_PINNED_CATALOG
 /* Queue readiness is checked after the trusted producer. An exact live
  * port with a hidden window/locked session stays parked; restore plus
  * deliverable input wakes it. Revocation still wakes to observe refusal. */
 for(unsigned j=0;j<PS_MAX_SLOTS;++j){
  const struct rt862_bank_slot *s=&bank862.slots[j];
  if(!input_wait70[j].generation)continue;
  if(!s->pid||s->handle!=input_wait70[j].generation){
   input_wait70[j].generation=0;input_wait70[j].window=0;continue;
  }
  const struct pr_entry *p=pr_find(runtime860->dispatcher.registry,s->pid);
  if(!p||p->state!=PR_WAITING)fail848("70-wait-identity");
  const enum nwc_wait_condition condition=nwc_wait_condition((struct nwc_service *)nwc_global(),
                                         s->pid,s->handle,s->root,input_wait70[j].window);
  if(condition==NWC_WAIT_EMPTY||condition==NWC_WAIT_BLOCKED)continue;
  if(rt865_bank_wait_state(&bank862,runtime860,bank862.safe,j,s->pid,s->handle,s->root,false)!=PS_OK)
   fail848("70-input-wake");
  input_wait70[j].generation=0;input_wait70[j].window=0;
  arch_puts("STYLEOS TEA70 WAKE");fld848("pid",s->pid);fld848("gen",s->handle);arch_putc('\n');
 }
#endif
 unsigned id;uint32_t pid;
 enum ps_status status=dpr_dispatch(runtime860,&id,&pid);
 if(status==PS_EMPTY)return;
 if(status!=PS_OK||id>=S846_ROOT_COUNT)fail848("native-poll-select");
 const struct ps_slot *owner=&runtime860->dispatcher.slots[id];
 const struct rt862_selection ticket={id,runtime860->dispatcher.dispatches,pid,owner->handle,owner->user_root,bank862.safe};
 struct up_context ctx={0};
 if(!rt862_bank_enter(&bank862,runtime860,&ticket,bank862.safe,&ctx))fail848("native-poll-context");
 struct rt865_slice result=run865(&ticket,&ctx,&native_g0,&native_i0,&native_g,&native_d,&native_original,audit,opaque);
 if(result.kind==RT865_TERMINAL){
  (void)usr_reconcile((struct usr_runtime *)usr_global());
  (void)nwc_reconcile((struct nwc_service *)nwc_global());
  native_drop_reconcile();
#ifdef STYLEOS_LIVE_VOLUME_39
  s88_native_file_reconcile();
#endif
  s865_retire_terminal(&ticket,audit,retire,opaque);
 }
 if(!s865_executor_idle())fail848("native-poll-return");
}
#endif

/* Native trap entry, no historical cooperative fallback. */
bool s848_active(void);
unsigned s848_dispatch_raw(struct s847_raw *,uint64_t,uint64_t);
unsigned s847_dispatch_raw(struct s847_raw *f,uint64_t cr2,uint64_t root)
{
 if(s848_active())return s848_dispatch_raw(f,cr2,root);
 __asm__ volatile("cli":::"memory");
 arch_puts("STYLEOS RUNTIME FAIL unexpected inactive trap\n");
 arch_halt();
}
