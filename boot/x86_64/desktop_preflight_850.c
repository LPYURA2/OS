/* Terminal QEMU standard-VGA diagnostic, after EBS and before MOV CR3.
 * Not called by Desktop or a live display driver. No MMIO access. */
#include <styleos/desktop_root_850.h>
#include <styleos/arch.h>
#include <styleos/ist_guard_plan_785.h>
#ifdef STYLEOS_TEST_RUNTIME_IRQ_859
#include <styleos/runtime_irq_859.h>
#endif
#ifdef STYLEOS_TEST_RUNTIME_ARENA_858
#include <styleos/physical_arena_844.h>
bool s845_extent(struct boot_range *);
struct s844_arena *s845_arena(void);
void s844_guest_run(uint64_t);
#ifdef STYLEOS_TEST_DESKTOP_COLD_863
bool s844_runtime_start(uint64_t);
#define S863_INITIAL_ARENA_NEXT 1u
#else
#define S863_INITIAL_ARENA_NEXT 4u
#endif
#endif
#ifdef STYLEOS_TEST_RUNTIME_ENTRY_858
#include <styleos/runtime_entry_858.h>
#include <styleos/desktop_process_runtime.h>
#include <styleos/user_session_runtime.h>
#include <styleos/native_window_channel.h>
#endif
#ifdef STYLEOS_TEST_DESKTOP_FRAME_852
#include <styleos/desktop_v0.h>
#ifdef STYLEOS_TEST_DESKTOP_INTERACTIVE_863
#include <styleos/desktop_interactive_863.h>
#if defined(STYLEOS_TEST_DESKTOP_RTC_RUNTIME_863) || defined(STYLEOS_TEST_DESKTOP_CLOCK_SERVICE_863)
#include <styleos/rtc_runtime_863.h>
#ifdef STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW
#include <styleos/pointer_clock_863.h>
#endif
#endif
#ifdef STYLEOS_TEST_DESKTOP_LIVE_863
#include <styleos/desktop_sessions.h>
#endif
#endif
#endif
#ifdef STYLEOS_TEST_DESKTOP_RTC_854
#include <styleos/rtc_x86_854.h>
static struct rtc_x86_854 rtc854;
#endif
#ifdef STYLEOS_TEST_DESKTOP_TIME_855
#include <styleos/pit_x86_855.h>
static struct pit_x86_855 pit855;
#endif
#if defined(STYLEOS_TEST_STORAGE_BUSMASTER_857) || defined(STYLEOS_TEST_DESKTOP_STORAGE_SERVICE_863)
#include <styleos/busmaster_857.h>
#endif
static _Alignas(4096) uint64_t tables[PG_TABLE_PAGES][512];
static struct s850_desktop_root composed;
static uint64_t read_cr3(void){uint64_t v;__asm__ volatile("mov %%cr3,%0":"=r"(v));return v;}
static uint64_t flags(void){uint64_t v;__asm__ volatile("pushfq;popq %0":"=r"(v));return v;}
static uint32_t in32(uint16_t p){uint32_t v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static uint16_t in16(uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
static void out32(uint16_t p,uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p):"memory");}
static void out16(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p):"memory");}
static uint32_t cfgaddr(unsigned dev,unsigned fn,unsigned off){return UINT32_C(0x80000000)|(dev<<11)|(fn<<8)|(off&0xfcu);}
static uint32_t cfg(unsigned dev,unsigned fn,unsigned off){out32(0xcf8,cfgaddr(dev,fn,off));return in32(0xcfc);}
static void cfgwrite(unsigned dev,unsigned fn,unsigned off,uint32_t v){out32(0xcf8,cfgaddr(dev,fn,off));out32(0xcfc,v);}
static uint16_t cmd(unsigned dev,unsigned fn){out32(0xcf8,cfgaddr(dev,fn,4));return in16(0xcfc);}
static void cmdwrite(unsigned dev,unsigned fn,uint16_t v){out32(0xcf8,cfgaddr(dev,fn,4));out16(0xcfc,v);}
#if defined(STYLEOS_TEST_STORAGE_ATA_READ_857) || defined(STYLEOS_TEST_DESKTOP_STORAGE_SERVICE_863)
#include <styleos/ata_diag_857.h>
struct ata857_live {uint64_t root;unsigned ops;};
static bool ata857_live_check(struct ata857_live *x){
 uint16_t cs;__asm__ volatile("mov %%cs,%0":"=r"(cs));
 if(!x||!x->root||read_cr3()!=x->root||(flags()&0x200)||(cs&3)||x->ops>=200000u)return false;
 ++x->ops;
 uint32_t old=in32(0xcf8);
 bool ok=cfg(5,0,0)==0x70108086u&&cfg(5,0,4)==0x02800003u&&cfg(5,0,8)==0x01018000u&&cfg(5,0,12)==0&&cfg(5,0,0x40)==0&&cfg(5,0,0x20)==0x60e1u;
 for(unsigned i=0;i<4;i++)if(cfg(5,0,0x10+4*i)!=0)ok=false;
 out32(0xcf8,old);if(!ok||in32(0xcf8)!=old||read_cr3()!=x->root||(flags()&0x200))return false;
 const uint16_t ports[4]={0x60e0,0x60e2,0x60e8,0x60ea};
 for(unsigned i=0;i<4;i++){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(ports[i]):"memory");if(v)return false;}
 return true;
}
static bool ata857_alt(void *ctx,uint8_t *v){if(!v||!ata857_live_check(ctx))return false;__asm__ volatile("inb %1,%0":"=a"(*v):"Nd"((uint16_t)0x376):"memory");return true;}
static bool ata857_data(void *ctx,uint16_t *v){if(!v||!ata857_live_check(ctx))return false;*v=in16(0x170);return true;}
static bool ata857_write(void *ctx,unsigned reg,uint8_t v){
 if(!ata857_allowed(reg,v)||!ata857_live_check(ctx))return false;
 uint16_t port=reg==8?0x376:(uint16_t)(0x170+reg);
 __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(port):"memory");return true;
}
#ifdef STYLEOS_TEST_STORAGE_ATA_READ_857
static const struct ata857_ops ata857_io={ata857_alt,ata857_write,ata857_data};
static void ata857_hex(const uint16_t *w){static const char hex[]="0123456789abcdef";for(unsigned i=0;i<ATA857_WORDS;i++){for(unsigned b=0;b<2;b++){unsigned v=(w[i]>>(8*b))&255u;arch_putc(hex[v>>4]);arch_putc(hex[v&15]);}}}
#endif
#if defined(STYLEOS_TEST_STORAGE_PIO_RUNTIME_863) || defined(STYLEOS_TEST_DESKTOP_STORAGE_SERVICE_863)
#include <styleos/ata_runtime_863.h>
#include <styleos/storage_runtime_863.h>
struct pio863 {struct ata857_live *live;struct ata_runtime_863 ata;bool writable;};
static bool pio_reg863(void *ctx,unsigned r,uint8_t v){
 struct pio863 *p=ctx;
 if(!p||!p->live)return false;
 if(ata857_allowed(r,v))return ata857_write(p->live,r,v);
 if(!p->writable||!p->ata.busy||r!=7||(v!=0x30&&v!=0xe7)||!ata857_live_check(p->live))return false;
 __asm__ volatile("outb %0,%1"::"a"(v),"Nd"((uint16_t)0x177):"memory");return true;
}
static bool pio_status863(void *ctx,uint8_t *v){struct pio863*p=ctx;return p&&ata857_alt(p->live,v);}
static bool pio_readword863(void *ctx,uint16_t *v){struct pio863*p=ctx;return p&&ata857_data(p->live,v);}
static bool pio_writeword863(void *ctx,uint16_t v){
 struct pio863*p=ctx;if(!p||!p->writable||!p->ata.busy||!ata857_live_check(p->live))return false;
 out16(0x170,v);return true;
}
/* Pace only the already-issued WRITE/FLUSH completion on the bounded
 * QEMU PIO fixture. Every next status sample still revalidates PCI/DMA.
 * PAUSE count is not a calibrated wall-clock deadline. */
static bool pio_pause863(void *ctx){
 struct pio863*p=ctx;
 if(!p||!p->live||!p->writable||!p->ata.busy||
    (p->ata.phase!=ATA863_WRITE_COMPLETE&&p->ata.phase!=ATA863_FLUSH)||
    read_cr3()!=p->live->root||(flags()&0x200))return false;
 for(unsigned i=0;i<256;i++)__asm__ volatile("pause":::"memory");
 return read_cr3()==p->live->root&&!(flags()&0x200);
}
static const struct ata_runtime_ops_863 pio_ops863={{pio_status863,pio_reg863,pio_readword863},pio_writeword863,pio_pause863};
static bool pio_observe863(void *ctx,struct storage_obs_863 *o){
 struct pio863*p=ctx;
 if(!p||!o||!p->ata.bound||p->ata.quarantined||!ata857_live_check(p->live))return false;
 *o=(struct storage_obs_863){.root=read_cr3(),.epoch=1,.device=UINT64_C(0x701080862800),
 .sectors=4096,.post_ebs=true,.single_cpu=true,.kernel_context=true,.exclusive_pio=true,
 .dma_denied=true,.disposable_data=true,.flush_supported=p->ata.flush_supported};return true;
}
static bool pio_read863(void *ctx,uint32_t lba,uint8_t *out){struct pio863*p=ctx;return ata_runtime_read_863(&p->ata,lba,out);}
static bool pio_write863(void *ctx,uint32_t lba,const uint8_t *in){
 struct pio863*p=ctx;
#ifdef STYLEOS_TEST_DESKTOP_STORAGE_SERVICE_863
 /* Four legacy note slots, two nine-sector banks per slot; never LBA 0. */
 return p->writable&&lba>=1&&lba<=72&&ata_runtime_write_863(&p->ata,lba,in);
#else
 return p->writable&&lba==2049&&ata_runtime_write_863(&p->ata,lba,in);
#endif
}
static bool pio_flush863(void *ctx){struct pio863*p=ctx;return p->writable&&ata_runtime_flush_863(&p->ata);}
static const struct storage_ops_863 store_ops863={pio_observe863,pio_read863,pio_write863,pio_flush863};
#endif
#endif
static void field(const char *k,uint64_t v){arch_putc(' ');arch_puts(k);arch_putc('=');paging_hex(v);}
static _Noreturn void fail(const char *s){
 arch_puts("STYLEOS S850 FAIL ");arch_puts(s);arch_putc('\n');
#ifndef STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW
 out32(0xf4,0x11);
#endif
 arch_halt();
}
/* Same narrow reversible S795 probe, restricted further to the standard
 * QEMU VGA fixture. No probe on an unknown device or driver-owned BAR.
 * Restore the command, BAR, and CF8 on every path following a write. */
static bool capture(const struct desktop_surface *s,struct pci794_bar *out,uint32_t *mask,unsigned *dev_out,unsigned *fn_out,uint16_t *command_out){
 uint32_t old=in32(0xcf8);unsigned matches=0,dev=0,fn=0;struct pci794_bar bar={0};struct pci794_device device={0};
 for(unsigned d=0;d<32;d++)for(unsigned f=0;f<8;f++){
  uint32_t words[16];words[0]=cfg(d,f,0);if((words[0]&0xffff)==0xffff)continue;
  for(unsigned i=1;i<16;i++)words[i]=cfg(d,f,4*i);
  struct pci794_device candidate;struct pci794_bar b;
  if(pci794_parse_header(words,&candidate)&&pci794_find_framebuffer_bar(&candidate,s->base,&b)){matches++;dev=d;fn=f;bar=b;device=candidate;}
 }
 bool ok=false;uint32_t size_mask=0;uint16_t before=0;
 if(matches==1&&device.vendor==0x1234&&device.device==0x1111&&device.class_code==3&&device.subclass==0&&bar.index==0&&bar.width==32&&bar.memory&&bar.base==s->base){
  uint32_t low=cfg(dev,fn,0x10);before=cmd(dev,fn);
  if(low==bar.raw_lo&&(before&2u)){
   cmdwrite(dev,fn,(uint16_t)(before&~7u));bool disabled=(cmd(dev,fn)&7u)==0;
   if(disabled){cfgwrite(dev,fn,0x10,UINT32_MAX);size_mask=cfg(dev,fn,0x10);}
   cfgwrite(dev,fn,0x10,low);cmdwrite(dev,fn,before);
   bool restored=cfg(dev,fn,0x10)==low&&cmd(dev,fn)==before;
   struct pci795_aperture aperture;
   ok=disabled&&restored&&pci795_decode_aperture(&bar,size_mask,0,s->base,s->bytes,&aperture);
  }
 }
 out32(0xcf8,old);if(in32(0xcf8)!=old||!ok)return false;
 *out=bar;*mask=size_mask;*dev_out=dev;*fn_out=fn;*command_out=before;return true;
}
#ifdef STYLEOS_TEST_DESKTOP_SWITCH_851
/* Terminal QEMU-only experiment. Transition IDT has no IST dependency and
 * uses the common CPL0 stack until the new TSS is loaded. It never returns
 * from an unexpected exception, and does not claim general NMI/DF recovery. */
struct s851_dt {uint16_t limit;uint64_t base;} __attribute__((packed));
struct s851_gate {uint16_t lo,cs;uint8_t ist,attr;uint16_t mid;uint32_t hi,zero;} __attribute__((packed));
_Static_assert(sizeof(struct s851_gate)==16,"IDT gate");
static _Alignas(4096) struct s851_gate bridge[256];
volatile uint64_t s851_frame[5],s851_frame_address;
void irq807_unexpected(void);void s851_bp(void);
__asm__(".text\n.global s851_bp\ns851_bp:\n"
 "pushq %rax\nleaq 8(%rsp),%rax\nmovq %rax,s851_frame_address(%rip)\n"
 "movq 8(%rsp),%rax\nmovq %rax,s851_frame(%rip)\n"
 "movq 16(%rsp),%rax\nmovq %rax,s851_frame+8(%rip)\n"
 "movq 24(%rsp),%rax\nmovq %rax,s851_frame+16(%rip)\n"
 "movq 32(%rsp),%rax\nmovq %rax,s851_frame+24(%rip)\n"
 "movq 40(%rsp),%rax\nmovq %rax,s851_frame+32(%rip)\n"
 "popq %rax\niretq\n");
static struct s851_gate make_gate(uint64_t fn,uint16_t cs,uint8_t ist){return (struct s851_gate){(uint16_t)fn,cs,ist,0x8e,(uint16_t)(fn>>16),(uint32_t)(fn>>32),0};}
static uint64_t reg4(void){uint64_t v;__asm__ volatile("mov %%cr4,%0":"=r"(v));return v;}
static uint64_t read_msr(uint32_t n){uint32_t lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(n));return ((uint64_t)hi<<32)|lo;}
static bool same_cache(const struct s842_cache *c){if(read_msr(0x277)!=c->pat||read_msr(0xfe)!=c->cap||read_msr(0x2ff)!=c->def)return false;for(unsigned i=0;i<c->count;i++)if(read_msr(0x200+2*i)!=c->pairs[i].base||read_msr(0x201+2*i)!=c->pairs[i].mask)return false;return true;}
static bool identity(uint64_t va,unsigned rights){struct pg_lookup x;return pg_lookup(&composed.plan,va,&x)==PG_OK&&x.pa==va&&x.flags==rights;}
static bool absent(uint64_t va){struct pg_lookup x;return pg_lookup(&composed.plan,va,&x)==PG_RANGE;}
static uint64_t bytes64(const unsigned char *p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
#ifdef STYLEOS_TEST_RUNTIME_ROOTS_861
#include "runtime_roots_861.inc"
#endif
#ifdef STYLEOS_TEST_DESKTOP_INTERACTIVE_863
static const struct dpr_service *shell_d863;
static struct s842_cache shell_cache863;
static uint64_t shell_root863;
static unsigned shell_events863;
#ifdef STYLEOS_TEST_DESKTOP_LIVE_863
static struct pg_experiment *live_p863;
static const struct s846_guard *live_guard863;
static struct s844_arena *live_arena863;
static const struct desktop_surface *live_surface863;
static unsigned live_phase863;
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
static unsigned pump_calls863;
static uint64_t pump_arena_base863;
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
static bool task_requested863;
static unsigned task_batches863;
#else
static unsigned pump_gate863;
static uint64_t pump_frame863,pump_sessions863;
#endif
#endif
static uint64_t live_hash863(const volatile unsigned char *p,size_t n){
 uint64_t h=UINT64_C(14695981039346656037);
 for(size_t i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}return h;
}
#endif
#ifdef STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW
/* This is a CPUID-visible fixture limit, not a general SMP topology proof.
 * Never infer physical CPU count or authorize AP startup from this check. */
static bool preview_single_cpu863(void){
 uint32_t a,b,c,d;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"0"(1),"2"(0));
 unsigned logical=(b>>16)&255u;
 return logical==1||(!(d&(1u<<28))&&logical==0);
}
#endif
static bool shell_fixture863(void){
 uint32_t a,b,c,d;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"0"(1),"2"(0));
 if(!(c&(1u<<31)))return false;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"0"(0x40000000),"2"(0));
 if(b!=0x54474354u||c!=0x43544743u||d!=0x47435447u)return false;
 uint32_t old=in32(0xcf8),lpc=cfg(31,0,0),cls=cfg(31,0,8);out32(0xcf8,old);
 return in32(0xcf8)==old&&lpc==0x29188086u&&(cls>>16)==0x0601;
}
void s863_shell_guard(void){
 if(!shell_root863||read_cr3()!=shell_root863||(flags()&0x200)||dpr_global_state()!=shell_d863)fail("863-shell-root");
}
#if defined(STYLEOS_TEST_DESKTOP_RTC_RUNTIME_863) || defined(STYLEOS_TEST_DESKTOP_CLOCK_SERVICE_863)
/* Only this opt-in Q35/TCG diagnostic owns CMOS. The runner fixes one vCPU;
 * APs are not started. No other CMOS backend is linked. As in S854 we
 * establish a NEW masked-NMI index shadow, never infer inherited port 70.
 * This mask is deliberately retained until diagnostic VM exit. */
/* Own a complete page so the inherited-root attestation can check an
 * exact, page-aligned interval before switching. */
static _Alignas(4096) union {
 struct rtc_runtime_863 state;
 unsigned char page[4096];
} rtc_page863;
_Static_assert(sizeof rtc_page863==4096,"RTC page extent");
#define rtc_runtime863 (rtc_page863.state)
static bool rtc_preflight863,rtc_admitted863;
#ifdef STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW
static _Alignas(4096) union {struct pc863_clock clock;unsigned char page[4096];} pointer_page863;
_Static_assert(sizeof pointer_page863==4096,"pointer clock extent");
static bool pointer_preflight863,pointer_attempted863;
#endif
#ifdef STYLEOS_TEST_DESKTOP_RTC_RUNTIME_863
static unsigned rtc_samples863;
#endif
static bool rtc_observe863(void *ctx,struct rtc_runtime_observation_863 *o)
{
 uint16_t cs;__asm__ volatile("mov %%cs,%0":"=r"(cs));
 if(ctx!=&rtc_runtime863||!o||!rtc_admitted863||!shell_root863||
    dpr_global_state()!=shell_d863)return false;
 *o=(struct rtc_runtime_observation_863){.root=read_cr3(),.epoch=1,
   .rflags=flags(),.cs=cs,.q35_tcg=true,.single_cpu=true,
   .post_ebs=true,.exclusive_cmos=true,.known_shadow=true};
 return true;
}
static bool rtc_index863(void *ctx,uint8_t v)
{
 if(ctx!=&rtc_runtime863||!rtc_admitted863||!rtc_runtime863.owner.held)return false;
 __asm__ volatile("outb %0,$0x70"::"a"(v):"memory");return true;
}
static bool rtc_data863(void *ctx,uint8_t *v)
{
 if(ctx!=&rtc_runtime863||!v||!rtc_admitted863||!rtc_runtime863.owner.held)return false;
 __asm__ volatile("inb $0x71,%0":"=a"(*v)::"memory");return true;
}
static const struct rtc_runtime_ops_863 rtc_ops863={rtc_observe863,rtc_index863,rtc_data863};
static void rtc_admit863(void)
{
 uint32_t a,b,c,d;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"0"(1),"2"(0));
 if(!rtc_preflight863||rtc_admitted863||!shell_fixture863()||((d&(1u<<28))&&((b>>16)&255u)!=1)||
    read_cr3()!=shell_root863||(flags()&0x200)||
    !identity((uintptr_t)&rtc_page863,PG_WRITE)||
    !identity((uintptr_t)&rtc_page863+sizeof rtc_page863-1,PG_WRITE)||
    !identity((uintptr_t)rtc_runtime_bind_863,PG_EXEC)||
    !identity((uintptr_t)rtc_runtime_read_863,PG_EXEC)||
    !identity((uintptr_t)rtc_observe863,PG_EXEC)||
    !identity((uintptr_t)rtc_index863,PG_EXEC)||
    !identity((uintptr_t)rtc_data863,PG_EXEC))fail("863-rtc-admit");
 const uint8_t initial=0x8d;
 __asm__ volatile("outb %0,$0x70"::"a"(initial):"memory");
 rtc_admitted863=true;
 if(!rtc_runtime_bind_863(&rtc_runtime863,&rtc_ops863,&rtc_runtime863,shell_root863,1,initial))fail("863-rtc-bind");
}
#ifdef STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW
/* Read-only TSC calibrated against three consecutive edges from the
 * already admitted RTC service. No PIT programming or new port grant. */
static uint64_t pointer_counter863(void){
 uint32_t lo,hi;__asm__ volatile("lfence;rdtsc;lfence":"=a"(lo),"=d"(hi)::"memory");
 return ((uint64_t)hi<<32)|lo;
}
void s863_pointer_init(void){
 s863_shell_guard();
 if(pointer_attempted863)return;
 pointer_attempted863=true;
 uint32_t a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"0"(1),"2"(0));
 if(!pointer_preflight863||!rtc_admitted863||!preview_single_cpu863()||
    !same_cache(&shell_cache863)||!(d&(1u<<4))||!(d&(1u<<26))||
    !identity((uintptr_t)&pointer_page863,PG_WRITE)||
    !identity((uintptr_t)&pointer_page863+4095,PG_WRITE))goto unavailable;
 struct pc863_edge edges[3]={0};unsigned n=0,prev=86400;uint64_t last=0;
 for(unsigned i=0;i<2000000u&&n<3;i++){
  struct rtc_time_854 t={0};uint64_t before=pointer_counter863();
  bool ok=rtc_runtime_read_863(&rtc_runtime863,&t);
  uint64_t after=pointer_counter863();
  if(after<before||before<last||rtc_runtime863.quarantined)goto unavailable;
  if(!ok)continue;
  unsigned sec=(unsigned)t.hour*3600u+(unsigned)t.min*60u+t.sec;
  if(prev!=86400&&sec!=prev){
   if(sec!=(prev+1)%86400||!last)goto unavailable;
   edges[n++]=(struct pc863_edge){last,after,sec};
  }
  prev=sec;last=before;
 }
 if(n!=3||!same_cache(&shell_cache863)||
    !pc863_init(&pointer_page863.clock,edges))goto unavailable;
 s863_shell_guard();
 for(unsigned i=0;i<3;i++){
  arch_puts("STYLEOS S863T EDGE");field("root",shell_root863);field("n",i);
  field("second",edges[i].second);field("lo",edges[i].lo);field("hi",edges[i].hi);arch_putc('\n');
 }
 uint64_t stamp=pc863_sample(&pointer_page863.clock,pointer_counter863());
 if(!stamp)goto unavailable;
 arch_puts("STYLEOS S863T POINTER");field("root",shell_root863);
 field("min-hz",pointer_page863.clock.min_hz);field("max-hz",pointer_page863.clock.max_hz);
 field("hz",pointer_page863.clock.mono.hz);field("stamp",stamp);arch_putc('\n');
 return;
unavailable:
 arch_puts("STYLEOS S863T POINTER UNAVAILABLE\n");
}
uint64_t s863_pointer_read(void){
 s863_shell_guard();
 if(!pointer_page863.clock.mono.ready||pointer_page863.clock.mono.faulted)return 0;
 if(!same_cache(&shell_cache863)){pointer_page863.clock.mono.faulted=true;return 0;}
 return pc863_sample(&pointer_page863.clock,pointer_counter863());
}
bool s863_pointer_near(uint64_t a,uint64_t b){
 s863_shell_guard();return pc863_near(&pointer_page863.clock,a,b);
}
#endif
#ifdef STYLEOS_TEST_DESKTOP_RTC_RUNTIME_863
static void rtc_sample863(void)
{
 struct rtc_time_854 t={0};bool ok=false;
 s863_shell_guard();
 if(!rtc_admitted863||!same_cache(&shell_cache863)||rtc_samples863>=3)fail("863-rtc-context");
 for(unsigned i=0;i<4096;i++){
    if(rtc_runtime_read_863(&rtc_runtime863,&t)){ok=true;break;}
    if(rtc_runtime863.quarantined)break;
    __asm__ volatile("pause":::"memory");
 }
 if(!ok||rtc_runtime863.owner.held||rtc_runtime863.owner.shadow!=0x8d||
    rtc_runtime863.quarantined||rtc_runtime863.owner.quarantined||
    t.hour>23||t.min>59||t.sec>59||!same_cache(&shell_cache863))fail("863-rtc-read");
 s863_shell_guard();
 arch_puts("STYLEOS S863R READ");field("root",shell_root863);field("n",++rtc_samples863);
 field("dispatches",shell_d863->dispatcher.dispatches);field("hour",t.hour);
 field("minute",t.min);field("second",t.sec);field("shadow",rtc_runtime863.owner.shadow);arch_putc('\n');
}
#endif
#ifdef STYLEOS_TEST_DESKTOP_CLOCK_SERVICE_863
/* Only admitted Q35 fixture. Leave caller outputs unchanged on refusal. */
bool s863_shell_clock(unsigned *hour,unsigned *minute){
 static bool displayed;
 static unsigned last_hour,last_min;
 if(!hour||!minute||hour==minute||!rtc_admitted863)return false;
 s863_shell_guard();
 if(!same_cache(&shell_cache863)||rtc_runtime863.quarantined)return false;
 struct rtc_time_854 t={0};bool ok=false;
 unsigned attempts=displayed?1:4096;
 for(unsigned i=0;i<attempts;i++){
  if(rtc_runtime_read_863(&rtc_runtime863,&t)){ok=true;break;}
  if(rtc_runtime863.quarantined)break;
  __asm__ volatile("pause":::"memory");
 }
 if(!ok||rtc_runtime863.owner.held||rtc_runtime863.owner.shadow!=0x8d||
    rtc_runtime863.quarantined||rtc_runtime863.owner.quarantined||
    t.hour>23||t.min>59||!same_cache(&shell_cache863))return false;
 s863_shell_guard();
 *hour=t.hour;*minute=t.min;
 if(!displayed||last_hour!=t.hour||last_min!=t.min){
  arch_puts("STYLEOS S863C CLOCK");field("root",shell_root863);
  field("dispatches",shell_d863->dispatcher.dispatches);field("hour",t.hour);
  field("minute",t.min);field("shadow",rtc_runtime863.owner.shadow);arch_putc('\n');
  displayed=true;last_hour=t.hour;last_min=t.min;
 }
 return true;
}
#endif
#endif
#ifdef STYLEOS_TEST_DESKTOP_STORAGE_SERVICE_863
static _Alignas(4096) union {
 struct {struct ata857_live live;struct pio863 pio;struct storage_runtime_863 store;bool ready;} s;
 unsigned char page[4096];
} disk863;
_Static_assert(sizeof disk863==4096,"disk service page");
static bool disk_preflight863;
static struct bm857_snapshot disk_snapshot863(void){
 uint32_t old=in32(0xcf8);
 struct bm857_snapshot v={.root=read_cr3(),.id=cfg(5,0,0),.class_revision=cfg(5,0,8),.command_status=cfg(5,0,4),.bar4=cfg(5,0,0x20)};
 out32(0xcf8,old);
 if(in32(0xcf8)!=old||v.id!=0x70108086u||v.class_revision!=0x01018000u||v.bar4!=0x60e1u)fail("863-shell-storage-pci");
 const uint16_t ports[]={0x60e0,0x60e2,0x60e8,0x60ea};
 for(unsigned i=0;i<4;i++)__asm__ volatile("inb %1,%0":"=a"(v.bm[i]):"Nd"(ports[i]):"memory");
 return v;
}
static void disk_admit863(void){
 uint32_t a,b,c,d;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"0"(1),"2"(0));
 if((d&(1u<<28))&&((b>>16)&255u)!=1)fail("863-shell-storage-cpu");
 s863_shell_guard();
 if(!disk_preflight863||disk863.s.ready||!shell_fixture863()||!same_cache(&shell_cache863)||
    !identity((uintptr_t)&disk863,PG_WRITE)||!identity((uintptr_t)&disk863+4095,PG_WRITE)||
    !identity((uintptr_t)storage_commit_863,PG_EXEC)||!identity((uintptr_t)storage_read_863,PG_EXEC)||
    desk_input_init()!=3)fail("863-shell-storage-admit");
 /* Host must pause and attest the exact isolated overlay before acknowledging. */
 arch_puts("STYLEOS S863S WAIT\n");
 bool ack=false;
 for(unsigned i=0;i<20000000u;i++){
  struct desk_event e={0};if(!desk_input_next(&e)){__asm__ volatile("pause");continue;}
  if(e.mouse||e.key!=0x30||e.modifiers)fail("863-shell-storage-ack");
  ack=true;break;
 }
 if(!ack||!shell_fixture863()||!same_cache(&shell_cache863))fail("863-shell-storage-host");
 struct bm857_snapshot before=disk_snapshot863();
 uint16_t word=0;
 if(!bm857_plan(&before,shell_root863,&word))fail("863-shell-storage-dma");
 uint32_t old=in32(0xcf8);
 cmdwrite(5,0,word);out32(0xcf8,old);
 struct bm857_snapshot after=disk_snapshot863();
 if(!bm857_confirm(&after,shell_root863))fail("863-shell-storage-master");
 disk863.s.live.root=shell_root863;
 disk863.s.pio.live=&disk863.s.live;
 uint8_t super[512];
 if(!ata_runtime_bind_863(&disk863.s.pio.ata,&pio_ops863,&disk863.s.pio)||
    !ata_runtime_read_863(&disk863.s.pio.ata,0,super))fail("863-shell-storage-identify");
 static const char magic[]="STYLEOS-DESK-V01";
 for(unsigned i=0;i<512;i++)if(super[i]!=(i<sizeof magic-1?(uint8_t)magic[i]:0))fail("863-shell-storage-signature");
 disk863.s.pio.writable=true;
 if(!storage_bind_863(&disk863.s.store,&store_ops863,&disk863.s.pio,shell_root863,1,UINT64_C(0x701080862800),0,4096,true))fail("863-shell-storage-bind");
 disk863.s.ready=true;
 arch_puts("STYLEOS S863S ADMIT");field("root",shell_root863);field("first-write",1);field("last-write",72);field("dma",0);arch_putc('\n');
}
static bool disk_ready863(void){
 if(!disk863.s.ready||disk863.s.store.quarantined||disk863.s.pio.ata.quarantined)return false;
 s863_shell_guard();
 if(!same_cache(&shell_cache863)||!ata857_live_check(&disk863.s.live)){
  disk863.s.store.quarantined=true;disk863.s.pio.ata.quarantined=true;return false;
 }
 return true;
}
void s863_storage_document(bool saved,unsigned slot,const char *data,size_t bytes){
 s863_shell_guard();
 if(!disk_ready863()||slot>=DESK_DOCS||!data||bytes>DESK_TEXT_LIMIT)fail("863-shell-storage-document");
 uint64_t hash=UINT64_C(14695981039346656037);
 for(size_t i=0;i<bytes;i++){hash^=(uint8_t)data[i];hash*=UINT64_C(1099511628211);}
 arch_puts(saved?"STYLEOS S863S SAVED":"STYLEOS S863S OPENED");
 field("root",shell_root863);field("slot",slot);field("bytes",bytes);field("hash",hash);arch_putc('\n');
}
uint32_t s863_disk_probe(void){return disk_ready863()?4096:0;}
bool s863_disk_read(uint32_t lba,uint8_t out[512]){
 return out&&lba<4096&&disk_ready863()&&storage_read_863(&disk863.s.store,lba,out);
}
bool s863_disk_write(uint32_t lba,const uint8_t in[512]){
 if(!in||lba<1||lba>72||!disk_ready863())return false;
 /* Every successful write already includes hardware flush and readback. */
 if(!storage_commit_863(&disk863.s.store,lba,in)){
  arch_puts("STYLEOS S863S REFUSAL");field("root",shell_root863);field("lba",lba);
  field("store",disk863.s.store.fault);field("ata",disk863.s.pio.ata.fault);
  field("phase",disk863.s.pio.ata.phase);field("polls",disk863.s.pio.ata.status_polls);
  field("status",disk863.s.pio.ata.last_status);field("ops",disk863.s.live.ops);
  field("pending-flush",disk863.s.pio.ata.pending_flush);arch_putc('\n');return false;
 }
 arch_puts("STYLEOS S863S COMMIT");field("root",shell_root863);field("lba",lba);arch_putc('\n');
 return true;
}
bool s863_disk_flush(void){
 /* No queued writes exist after verified commit; check actual live transport. */
 return disk_ready863()&&!disk863.s.pio.ata.pending_flush&&!disk863.s.store.busy;
}
#endif
static void shell_boundary863(void){
 s863_shell_guard();unsigned used;
#ifdef STYLEOS_TEST_DESKTOP_COLD_863
 unsigned dispatches=0,next=1,generation=1;
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
 if(live_phase863==2){dispatches=S863_DISPATCHES_PER_BATCH*task_batches863;next=1+2*task_batches863;generation=1+2*task_batches863;}
#endif
#else
 unsigned dispatches=20,next=5,generation=8;
#endif
#ifdef STYLEOS_TEST_DESKTOP_LIVE_863
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
 if(live_phase863==1){
  struct rt858_live v=capture861();
  if(rt861_dispatcher(&v)!=shell_d863||!same_cache(&shell_cache863)||nwc_port_count(nwc_global()))fail("863-pump-boundary");
  /* s845_arena is an empty-arena handoff, not an active-arena query.
   * Use only the already admitted pointer retained before the shell began. */
  unsigned dispatches=shell_d863->dispatcher.dispatches;
  if(!live_arena863||live_arena863->base!=pump_arena_base863||
     !s844_count(live_arena863,&used)||
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
     dispatches<S863_DISPATCHES_PER_BATCH*task_batches863||dispatches>S863_DISPATCHES_PER_BATCH*(task_batches863+1)||
#else
     dispatches>20||
#endif
     live_arena863->next!=S863_INITIAL_ARENA_NEXT+2*((dispatches+S863_DISPATCHES_PER_BATCH-1)/S863_DISPATCHES_PER_BATCH)||
     (used!=0&&used!=6))fail("863-pump-arena");
  if(!used){
   if(dispatches%S863_DISPATCHES_PER_BATCH||s845_arena()!=live_arena863||pr_snapshot(pr_global(),0,0,true)||ws_snapshot(ws_global(),0,0))fail("863-pump-empty");
  }else if(!dispatches||s845_arena())fail("863-pump-occupied");
  return;
 }
#else
 if(live_phase863==1)fail("863-live-reentry");
#endif
 if(!live_phase863){dispatches=0;next=1;generation=S863_INITIAL_ARENA_NEXT;}
#endif
 if(!boundary861(shell_d863,&shell_cache863)||shell_d863->dispatcher.dispatches!=dispatches||pr_global()->next_pid!=next||ws_global()->next_id!=next||nwc_port_count(nwc_global())||!s844_count(s845_arena(),&used)||used||s845_arena()->next!=generation)fail("863-shell-boundary");
}
#ifdef STYLEOS_TEST_DESKTOP_LIVE_863
/* Called only from the completed input/render boundary of the running shell.
 * Reuse the bounded S862 trial, not an independent scheduler or new grant. */
__attribute__((noinline)) void s863_live_run(void){
 shell_boundary863();
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
 if(live_phase863==1||task_batches863>=S863_LAUNCH_LIMIT)fail("863-task-phase");
#else
 if(live_phase863)fail("863-live-phase");
#endif
 if(!live_p863||!live_guard863||!live_arena863||!live_surface863||live_arena863!=s845_arena()||live_surface863->stride!=live_surface863->width||live_surface863->format!=DESKTOP_BGR_BYTES)fail("863-live-admission");
 const struct desk_sessions *sessions=desk_sessions_global();
#ifndef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
 if(!sessions->notes.dirty||!sessions->notes.length||!desk_sessions_pid(sessions,DESK_SESSION_NOTES))fail("863-live-draft");
#endif
 uint64_t draft=live_hash863((const volatile unsigned char *)sessions,sizeof *sessions);
 size_t bytes=(size_t)live_surface863->width*live_surface863->height*4;
 if(bytes>live_surface863->bytes)fail("863-live-frame-range");
 uint64_t frame=live_hash863((const volatile unsigned char *)live_surface863->base,bytes);
 live_phase863=1;
 arch_puts("STYLEOS S863L BEGIN");field("root",shell_root863);field("dispatches",shell_d863->dispatcher.dispatches);field("frame",frame);field("sessions",draft);arch_putc('\n');
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
#ifndef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
 pump_frame863=frame;pump_sessions863=draft;
#endif
}
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
/* Request from the actual keyboard action, not from the UART observer.
 * Only the fixed built-in native fixture is currently authorized. */
bool s863_request_tasks(void){
 s863_shell_guard();
 shell_boundary863();
 bool accept=live_phase863!=1&&!task_requested863&&task_batches863<S863_LAUNCH_LIMIT;
 if(accept)task_requested863=true;
 arch_puts("STYLEOS S863B REQUEST");field("root",shell_root863);field("completed",task_batches863);
 field("dispatches",shell_d863->dispatcher.dispatches);field("accepted",accept);arch_putc('\n');
 return accept;
}
#endif
/* Called once per actual shell poll after input/render. One hardware dispatch
 * or one terminal cleanup; no preserved execution stack and no nested shell. */
__attribute__((noinline)) void s863_shell_pump(void){
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
 if(task_requested863){task_requested863=false;s863_live_run();}
 if(live_phase863!=1)return;
#else
 if(live_phase863!=1||pump_gate863==1||pump_gate863==2)return;
#endif
 shell_boundary863();
 const struct desk_sessions *sessions=desk_sessions_global();
 size_t bytes=(size_t)live_surface863->width*live_surface863->height*4;
 uint64_t draft=live_hash863((const volatile unsigned char *)sessions,sizeof *sessions);
 uint64_t frame=live_hash863((const volatile unsigned char *)live_surface863->base,bytes);
 unsigned before=shell_d863->dispatcher.dispatches;
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
 if(pump_calls863>=S863_POLLS_PER_BATCH*(task_batches863+1))fail("863-pump-admission");
#else
 if(pump_calls863>=22||frame!=pump_frame863||draft!=pump_sessions863)fail("863-pump-admission");
#endif
 bool done=guest861(live_p863,&shell_cache863,live_guard863,live_arena863,live_surface863,frame);
 unsigned after=shell_d863->dispatcher.dispatches,used;
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
 const unsigned end_dispatch=S863_DISPATCHES_PER_BATCH*(task_batches863+1);
#else
 const unsigned end_dispatch=20;
#endif
 if(after<before||after-before>1||!s844_count(live_arena863,&used)||(done!=(after==end_dispatch&&used==0))||(!done&&after==before&&used!=0))fail("863-pump-result");
 arch_puts("STYLEOS S863P SLICE");field("n",++pump_calls863);field("before",before);field("after",after);field("used",used);field("processes",pr_snapshot(pr_global(),0,0,true));field("windows",ws_snapshot(ws_global(),0,0));field("done",done);arch_putc('\n');
#ifndef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
 if(pump_calls863==1){pump_gate863=1;arch_puts("STYLEOS S863P WAIT\n");}
#endif
#else
 guest861(live_p863,&shell_cache863,live_guard863,live_arena863,live_surface863,frame);
#endif
 if(live_hash863((const volatile unsigned char *)sessions,sizeof *sessions)!=draft||live_hash863((const volatile unsigned char *)live_surface863->base,bytes)!=frame)fail("863-live-state");
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
 shell_boundary863();
 if(!done)return;
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
 if(pump_calls863!=S863_POLLS_PER_BATCH*(task_batches863+1))fail("863-pump-final");
 ++task_batches863;
#else
 if(pump_calls863!=22||pump_gate863!=3)fail("863-pump-final");
#endif
#endif
 live_phase863=2;shell_boundary863();
 arch_puts("STYLEOS S863L DONE");field("root",shell_root863);field("dispatches",shell_d863->dispatcher.dispatches);field("frame",frame);field("sessions",draft);arch_putc('\n');
}
#endif
void s863_shell_observe(const struct desk_event *ev,unsigned x,unsigned y,unsigned active,bool ru,unsigned restarts,size_t text,uint64_t text_hash,bool text_dirty,unsigned note_session,size_t writes,unsigned inputs,unsigned batch){
 shell_boundary863();
 if(inputs!=3||batch>32||(!ev)!=(!batch))fail("863-shell-input");
 if(!ev){
#ifdef STYLEOS_TEST_DESKTOP_RTC_RUNTIME_863
  rtc_sample863();
#endif
  arch_puts("STYLEOS S863 READY");field("root",shell_root863);field("dispatches",shell_d863->dispatcher.dispatches);field("next-pid",pr_global()->next_pid);field("next-window",ws_global()->next_id);field("input",inputs);field("x",x);field("y",y);field("writes",writes);arch_putc('\n');return;}
 if(shell_events863==UINT32_MAX)fail("863-shell-counter");
 arch_puts("STYLEOS S863 EVENT");field("root",shell_root863);field("n",++shell_events863);field("batch",batch);field("mouse",ev->mouse);field("key",ev->key);field("dx",(uint64_t)(int64_t)ev->dx);field("dy",(uint64_t)(int64_t)ev->dy);field("buttons",ev->buttons);field("modifiers",ev->modifiers);field("x",x);field("y",y);field("active",active);field("ru",ru);field("restarts",restarts);field("text",text);field("text-hash",text_hash);field("text-dirty",text_dirty);field("note-session",note_session);field("writes",writes);arch_putc('\n');
#if defined(STYLEOS_TEST_DESKTOP_LIVE_863) && !defined(STYLEOS_TEST_DESKTOP_COLD_TASKS_863)
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
 if(live_phase863==1){
  if((pump_gate863!=1&&pump_gate863!=2)||batch!=1||!ev->mouse||ev->key||ev->dy||ev->buttons||ev->modifiers||ev->dx!=(pump_gate863==1?20:-20)||!writes||writes>340)fail("863-pump-input");
  const struct pr_entry *waiting=pr_find(pr_global(),1);
  const struct desk_sessions *sessions=desk_sessions_global();
  if(shell_d863->dispatcher.dispatches!=1||pr_snapshot(pr_global(),0,0,true)!=1||ws_snapshot(ws_global(),0,0)!=1||!waiting||waiting->state!=PR_READY||waiting->cpu_ticks!=1||x!=(pump_gate863==1?680u:660u)||y!=600||live_hash863((const volatile unsigned char *)sessions,sizeof *sessions)!=pump_sessions863)fail("863-pump-input-state");
 #ifdef STYLEOS_TEST_DESKTOP_RTC_RUNTIME_863
  if(pump_gate863==1)rtc_sample863();
 #endif
  if(++pump_gate863==3)arch_puts("STYLEOS S863P RESUME\n");
  return;
 }
#endif
 if(batch==1&&!ev->mouse&&ev->key==0x43&&!ev->modifiers)s863_live_run();
#endif
#ifndef STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW
 if(batch==1&&!ev->mouse&&ev->key==0x58&&!ev->modifiers){
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
#ifdef STYLEOS_TEST_DESKTOP_AUTONOMOUS_863
  if(!task_batches863||task_requested863||live_phase863!=2)fail("863-task-incomplete");
#else
  if(task_batches863!=2||task_requested863)fail("863-task-incomplete");
#endif
#endif
#ifdef STYLEOS_TEST_DESKTOP_LIVE_863
  if(live_phase863!=2)fail("863-live-incomplete");
#endif
 #ifdef STYLEOS_TEST_DESKTOP_RTC_RUNTIME_863
  rtc_sample863();
  if(rtc_samples863!=3)fail("863-rtc-count");
 #endif
  arch_puts("STYLEOS S863 EXIT");field("root",shell_root863);field("events",shell_events863);field("dispatches",shell_d863->dispatcher.dispatches);field("used",0);arch_putc('\n');
  out32(0xf4,0x10);arch_halt();
 }
#endif
}
#endif
static _Noreturn void s851_switch(struct pg_experiment *p,const struct s842_cache *cache,const struct s846_guard *guard,const struct desktop_surface *s,uint64_t gdt,uint64_t idt,uint64_t tss,uint64_t rsp0,uint16_t limit,uint16_t selector){
 uint16_t cs,tr;uint64_t rsp,cr0;__asm__ volatile("mov %%cs,%0;str %1;mov %%rsp,%2;mov %%cr0,%3":"=r"(cs),"=r"(tr),"=r"(rsp),"=r"(cr0));
 uint64_t cr4=reg4(),target=composed.built.root,bridge_pa=(uintptr_t)bridge;
 if(!cs||(cs&7)||tr||flags()&0x200||!(cr0&(UINT64_C(1)<<16))||!(read_msr(0xc0000080)&(UINT64_C(1)<<11))||cr4&((UINT64_C(1)<<12)|(UINT64_C(1)<<17))||read_cr3()!=p->old_root||target==p->old_root||!same_cache(cache)||!selector||(selector&7)||limit!=selector+15||limit>=4096||cs+7>limit)fail("851-mode");
 if(arch_paging_attest_image_writable(p,bridge_pa,sizeof bridge)!=PG_OK||!identity(bridge_pa,PG_WRITE)||!identity((uintptr_t)s851_bp,PG_EXEC)||!identity((uintptr_t)irq807_unexpected,PG_EXEC)||!identity(rsp,PG_WRITE)||!identity((uintptr_t)s851_frame,PG_WRITE)||!identity((uintptr_t)&s851_frame_address,PG_WRITE))fail("851-bridge-closure");
 const unsigned char *ts=(const void *)(uintptr_t)tss,*gd=(const void *)(uintptr_t)gdt;uint64_t lo=bytes64(gd+selector),hi=bytes64(gd+selector+8);
 if(((lo>>40)&255)!=0x89||((lo&0xffff)|(((lo>>48)&15)<<16))!=103||((lo>>16&0xffffff)|(lo>>56<<24)|(hi<<32))!=tss||hi>>16||bytes64(ts+4)!=rsp0+4096-16)fail("851-descriptor");
 for(unsigned i=0;i<3;i++){uint64_t va=S846_IST+3*i*4096;struct pg_lookup x;if(bytes64(ts+36+8*i)!=va+4096-16||pg_lookup(&composed.plan,va,&x)!=PG_OK||x.pa!=guard->pa[i]||x.flags!=PG_WRITE||!absent(va-4096)||!absent(va+4096))fail("851-ist");}
 for(unsigned i=0;i<256;i++)bridge[i]=make_gate((uintptr_t)irq807_unexpected,cs,0);
 ((struct s851_gate *)(uintptr_t)idt)[3]=make_gate((uintptr_t)s851_bp,cs,1);
 struct s851_dt b={4095,bridge_pa},g={limit,gdt},d={4095,idt},live_g,live_d;
 arch_puts("STYLEOS S851 BRIDGE");field("idt",bridge_pa);field("old",p->old_root);field("new",target);field("cs",cs);field("selector",selector);arch_putc('\n');
 /* IDT with IST=0 first. Any unexpected vector exits on the common stack;
  * after new CR3 the new GDT and bridge remain valid, then LTR and final IDT. */
 __asm__ volatile("cli;lidt %0;lgdt %1;mov %2,%%cr4;mov %3,%%cr3;ltr %4;lidt %5"::"m"(b),"m"(g),"r"(cr4&~(UINT64_C(1)<<7)),"r"(target),"rm"(selector),"m"(d):"memory");
 __asm__ volatile("sgdt %0;sidt %1;str %2":"=m"(live_g),"=m"(live_d),"=r"(tr)::"memory");
 if(read_cr3()!=target||reg4()&(UINT64_C(1)<<7)||flags()&0x200||live_g.base!=gdt||live_g.limit!=limit||live_d.base!=idt||live_d.limit!=4095||tr!=selector||((volatile const unsigned char *)gd)[selector+5]!=0x8b||!same_cache(cache))fail("851-live-descriptors");
 arch_puts("STYLEOS S851 ACTIVE");field("root",read_cr3());field("gdt",live_g.base);field("idt",live_d.base);field("tr",tr);field("pat",read_msr(0x277));field("pge",(reg4()>>7)&1);arch_putc('\n');
 uint64_t expected_pc,before_rsp;__asm__ volatile("leaq s851_bp_return(%%rip),%0;mov %%rsp,%1;int3;.global s851_bp_return;s851_bp_return:":"=&r"(expected_pc),"=&r"(before_rsp)::"memory");
 if(read_cr3()!=target||s851_frame_address!=S846_IST+4096-16-40||s851_frame[0]!=expected_pc||s851_frame[1]!=cs||s851_frame[2]&0x200||s851_frame[3]!=before_rsp)fail("851-real-ist");
 arch_puts("STYLEOS S851 BP");field("root",read_cr3());field("frame",s851_frame_address);field("rip",s851_frame[0]);field("rsp",s851_frame[3]);field("cs",s851_frame[1]);field("flags",s851_frame[2]);arch_putc('\n');
 uint64_t offsets[2]={0,4*((uint64_t)(s->height-1)*s->stride+s->width-1)};
 for(unsigned i=0;i<2;i++){
  if(offsets[i]>composed.gop.mapping.bytes-4||offsets[i]>s->bytes-4)fail("851-pixel-bounds");
  volatile uint32_t *pixel=(void *)(s->base+offsets[i]);uint32_t old=*pixel,want=old^0x00ffffffu;
  *pixel=want;__asm__ volatile("mfence":::"memory");uint32_t got=*pixel;
  *pixel=old;__asm__ volatile("mfence":::"memory");uint32_t restored=*pixel;
  if(got!=want||restored!=old||read_cr3()!=target)fail("851-pixel");
  arch_puts("STYLEOS S851 PIXEL");field("id",i);field("root",target);field("address",(uintptr_t)pixel);field("old",old);field("write",want);field("read",got);field("restored",restored);arch_putc('\n');
 }
 arch_puts("STYLEOS S851 POLICY terminal=yes own-root=yes hardware-ist=yes framebuffer-restored=yes firmware-return=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_RUNTIME_ARENA_858
 /* Reuse the existing EFI reservation and full S842/S850 tree audits.
  * This checks the live owned root before touching any external pages. */
 struct boot_range arena858;
 if(!s845_extent(&arena858)||arena858.end!=arena858.start+S844_BYTES||read_cr3()!=target||(flags()&0x200)||!same_cache(cache))fail("858-arena-context");
 for(uint64_t a=arena858.start;a<arena858.end;a+=4096)if(!identity(a,PG_WRITE))fail("858-arena-map");
#ifdef STYLEOS_TEST_DESKTOP_COLD_863
 if(!s844_runtime_start(target))fail("863-cold-arena");
#else
 s844_guest_run(target);
#endif
 if(!s845_arena()||s845_arena()->base!=arena858.start||read_cr3()!=target||(flags()&0x200)||!same_cache(cache))fail("858-arena-live");
#endif
#ifdef STYLEOS_TEST_RUNTIME_ENTRY_858
 /* Initial service bootstrap only, after hardware proof. No legacy device
  * initialization, tasks, IRQ enabling or production-root authorization. */
 struct rt858_expected expected858={p->old_root,target,gdt,idt,p->evidence->live.stack.start,p->evidence->live.stack.end,limit,selector,cs};
 struct rt858_live live858={0};
 __asm__ volatile("sgdt %0;sidt %1;str %2;mov %%cs,%3;mov %%rsp,%4;mov %%cr0,%5":"=m"(live_g),"=m"(live_d),"=r"(live858.tr),"=r"(live858.cs),"=r"(live858.rsp),"=r"(live858.cr0)::"memory");
 live858.root=read_cr3();live858.gdt=live_g.base;live858.idt=live_d.base;live858.gdt_limit=live_g.limit;live858.idt_limit=live_d.limit;live858.cr4=reg4();live858.efer=read_msr(0xc0000080);live858.rflags=flags();
 if(!identity((uintptr_t)rt858_start,PG_EXEC)||!same_cache(cache)||!rt858_start(&expected858,&live858)||!rt858_ready(target)||read_cr3()!=target||(flags()&0x200))fail("858-runtime-entry");
 arch_puts("STYLEOS S858 RUNTIME");field("root",read_cr3());field("old",p->old_root);field("gdt",live858.gdt);field("idt",live858.idt);field("tr",live858.tr);field("cs",live858.cs);field("rsp",live858.rsp);field("cr0",live858.cr0);field("cr4",live858.cr4);field("efer",live858.efer);field("flags",live858.rflags);field("gdt-limit",live858.gdt_limit);field("idt-limit",live858.idt_limit);field("safe-root",dpr_global_state()->dispatcher.safe_root);field("processes",pr_snapshot(pr_global(),0,0,true));field("windows",ws_snapshot(ws_global(),0,0));field("session-next",usr_global()->next_session);field("selected",usr_global()->selected_session);field("ports",nwc_port_count(nwc_global()));arch_putc('\n');
#endif
#ifdef STYLEOS_TEST_DESKTOP_FRAME_852
 /* Bound the diagnostic workload to the admitted standard VGA mode. */
 if(s->width!=1280||s->height!=800||s->stride!=1280||s->format!=DESKTOP_BGR_BYTES||
    s->bytes!=UINT64_C(4096000)||composed.gop.mapping.bytes<s->bytes||
    !identity((uintptr_t)desk_first_frame_852,PG_EXEC)||read_cr3()!=target)fail("852-frame-contract");
 size_t writes=desk_first_frame_852(s);
 if(writes!=(size_t)s->width*s->height||read_cr3()!=target||flags()&0x200)fail("852-frame-write");
 __asm__ volatile("mfence":::"memory");
 const volatile uint32_t *fb=(const volatile uint32_t *)s->base;
 uint64_t hash=UINT64_C(14695981039346656037);
 for(size_t i=0;i<writes;i++){
    uint32_t pixel=fb[i];
    for(unsigned b=0;b<4;b++){hash^=(pixel>>(8*b))&255u;hash*=UINT64_C(1099511628211);}
 }
 if(read_cr3()!=target||flags()&0x200||!same_cache(cache))fail("852-frame-readback");
 arch_puts("STYLEOS S852 FRAME");field("root",target);field("width",s->width);field("height",s->height);field("writes",writes);field("fnv",hash);field("first",fb[0]);field("last",fb[writes-1]);arch_putc('\n');
 arch_puts("STYLEOS S852 POLICY first-frame=yes input=no storage=no app-admission=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_RUNTIME_ENTRY_858
 if(!rt858_ready(read_cr3())||(flags()&0x200)||!same_cache(cache))fail("858-runtime-frame");
 arch_puts("STYLEOS S858 POLICY runtime-services=yes empty-registry=yes frame=yes irq=no app-admission=no storage=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_RUNTIME_ARENA_858
 struct s844_arena *allocator858=s845_arena();unsigned used858;
 if(!allocator858||allocator858->base!=arena858.start||allocator858->next!=S863_INITIAL_ARENA_NEXT||!s844_count(allocator858,&used858)||used858||read_cr3()!=target||(flags()&0x200)||!same_cache(cache))fail("858-arena-after");
 for(uint64_t a=arena858.start;a<arena858.end;a+=4096)if(!identity(a,PG_WRITE))fail("858-arena-after-map");
 for(uint64_t a=arena858.start;a<arena858.end;a++)if(*(volatile const unsigned char *)(uintptr_t)a)fail("858-arena-after-zero");
 arch_puts("STYLEOS S858 ARENA");field("root",target);field("base",arena858.start);field("pages",S844_PAGES);field("used",used858);field("next",allocator858->next);field("zero",1);arch_putc('\n');
 arch_puts("STYLEOS S858 ARENA-POLICY bounded-efi=yes runtime-services=yes frame=yes reclaim=no irq=no dma-grant=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_RUNTIME_ROOTS_861
#if !defined(STYLEOS_TEST_DESKTOP_LIVE_863) && !defined(STYLEOS_TEST_DESKTOP_COLD_863)
 guest861(p,cache,guard,allocator858,s,hash);
#endif
#ifdef STYLEOS_TEST_DESKTOP_INTERACTIVE_863
 /* Existing shell only after two actual lifecycle rounds on this same root.
  * Storage/clock/reset are explicitly denied. No new task ABI or DMA grant. */
 if(!shell_fixture863()||!identity((uintptr_t)desktop_v0_run,PG_EXEC)||!identity((uintptr_t)desk_input_init,PG_EXEC)||!identity((uintptr_t)desk_input_next,PG_EXEC))fail("863-shell-admission");
#ifdef STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW
 if(!preview_single_cpu863())fail("863-shell-cpu");
#endif
 shell_root863=target;shell_d863=dpr_global_state();shell_cache863=*cache;
#ifdef STYLEOS_TEST_DESKTOP_LIVE_863
 live_p863=p;live_guard863=guard;live_arena863=allocator858;live_surface863=s;
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
 pump_arena_base863=arena858.start;
#endif
#endif
 shell_boundary863();
#ifdef STYLEOS_TEST_DESKTOP_STORAGE_SERVICE_863
 disk_admit863();
#endif
#if defined(STYLEOS_TEST_DESKTOP_RTC_RUNTIME_863) || defined(STYLEOS_TEST_DESKTOP_CLOCK_SERVICE_863)
 rtc_admit863();
#endif
#ifdef STYLEOS_TEST_DESKTOP_COLD_863
 arch_puts("STYLEOS S863B COLD");field("root",target);field("dispatches",shell_d863->dispatcher.dispatches);field("arena-next",allocator858->next);
 field("processes",pr_snapshot(pr_global(),0,0,true));field("windows",ws_snapshot(ws_global(),0,0));arch_putc('\n');
#endif
 arch_puts("STYLEOS S863 ADMIT");field("root",target);field("lpc",0x29188086);field("dispatches",shell_d863->dispatcher.dispatches);arch_putc('\n');
#ifdef STYLEOS_TEST_DESKTOP_AUTONOMOUS_863
 arch_puts("STYLEOS S863 POLICY q35-tcg=yes polled-input=yes storage=no clock=rtc-service reset=no dma-grant=no production-cr3=deny\n");
#elif defined(STYLEOS_TEST_DESKTOP_COLD_863)
 arch_puts("STYLEOS S863 POLICY q35-tcg=yes polled-input=yes storage=legacy-fixture clock=rtc-service reset=no dma-grant=no production-cr3=deny\n");
#elif defined(STYLEOS_TEST_DESKTOP_STORAGE_SERVICE_863)
 arch_puts("STYLEOS S863 POLICY q35-tcg=yes polled-input=yes storage=legacy-fixture clock=no reset=no dma-grant=no production-cr3=deny\n");
#elif defined(STYLEOS_TEST_DESKTOP_CLOCK_SERVICE_863)
 arch_puts("STYLEOS S863 POLICY q35-tcg=yes polled-input=yes storage=no clock=rtc-service reset=no dma-grant=no production-cr3=deny\n");
#else
 arch_puts("STYLEOS S863 POLICY q35-tcg=yes polled-input=yes storage=no clock=no reset=no dma-grant=no production-cr3=deny\n");
#endif
 desktop_v0_run(s);
#endif
#endif
#ifdef STYLEOS_TEST_RUNTIME_IRQ_859
 if(read_cr3()!=target||flags()&0x200||!same_cache(cache))fail("859-map-context");
 if(!irq859_uc_page(&composed.plan,&composed.built,cache,0xfee00000))fail("859-map-lapic-uc");
 if(!irq859_uc_page(&composed.plan,&composed.built,cache,0xfec00000))fail("859-map-ioapic-uc");
 if(!identity((uintptr_t)irq859_once,PG_EXEC)||!identity(idt,PG_WRITE))fail("859-map-image");
 irq859_once(target,idt,cs,p->evidence->live.stack.start,p->evidence->live.stack.end);
 if(!rt858_ready(target)||!s844_count(allocator858,&used858)||used858||allocator858->next!=4||read_cr3()!=target||(flags()&0x200)||!same_cache(cache))fail("859-post-runtime");
 for(uint64_t a=arena858.start;a<arena858.end;a++)if(*(volatile const unsigned char *)(uintptr_t)a)fail("859-arena-zero");
 arch_puts("STYLEOS S859 POLICY irq0=yes eoi=yes idt-restored=yes route-restored=yes pit-restored=no runtime-intact=yes arena-intact=yes scheduler-switch=no production-cr3=deny\n");
#endif
#endif
#endif
#ifdef STYLEOS_TEST_DESKTOP_INPUT_853
 /* This is a fixed Q35/TCG terminal fixture, not general ACPI resource admission. */
 uint32_t cp_a,cp_b,cp_c,cp_d;
 __asm__ volatile("cpuid":"=a"(cp_a),"=b"(cp_b),"=c"(cp_c),"=d"(cp_d):"0"(1),"2"(0));
 if(!(cp_c&(1u<<31)))fail("853-hypervisor");
 __asm__ volatile("cpuid":"=a"(cp_a),"=b"(cp_b),"=c"(cp_c),"=d"(cp_d):"0"(0x40000000),"2"(0));
 if(cp_b!=0x54474354u||cp_c!=0x43544743u||cp_d!=0x47435447u)fail("853-tcg");
 uint32_t cf8=in32(0xcf8),lpc=cfg(31,0,0),classreg=cfg(31,0,8);
 out32(0xcf8,cf8);
 if(in32(0xcf8)!=cf8||lpc!=0x29188086u||(classreg>>16)!=0x0601||
    !identity((uintptr_t)desk_input_init,PG_EXEC)||!identity((uintptr_t)desk_input_next,PG_EXEC)||
    !identity((uintptr_t)desk_cursor_853,PG_EXEC)||read_cr3()!=target||flags()&0x200)
     fail("853-fixture");
 unsigned input=desk_input_init();
 if(input!=3||read_cr3()!=target||flags()&0x200)fail("853-init");
 arch_puts("STYLEOS S853 READY");field("root",target);field("lpc",lpc);field("input",input);arch_putc('\n');
 struct desk_event event={0};bool key=false,mouse=false;
 for(unsigned i=0;i<20000000u;i++){
    if(!desk_input_next(&event)){__asm__ volatile("pause");continue;}
    if(event.mouse||event.key!=0x1e||event.modifiers)fail("853-key");
    key=true;break;
 }
 if(!key||read_cr3()!=target||flags()&0x200)fail("853-key-timeout");
 arch_puts("STYLEOS S853 KEY");field("root",target);field("key",event.key);field("modifiers",event.modifiers);arch_putc('\n');
 for(unsigned i=0;i<20000000u;i++){
    if(!desk_input_next(&event)){__asm__ volatile("pause");continue;}
    if(!event.mouse||event.dx!=20||event.dy!=0||event.buttons||event.modifiers)fail("853-mouse");
    mouse=true;break;
 }
 if(!mouse||read_cr3()!=target||flags()&0x200)fail("853-mouse-timeout");
 size_t moved=desk_cursor_853(&event);
 if(!moved||moved>340||read_cr3()!=target||flags()&0x200)fail("853-damage");
 __asm__ volatile("mfence":::"memory");
 uint64_t after=UINT64_C(14695981039346656037);
 for(size_t i=0;i<writes;i++){uint32_t pixel=fb[i];for(unsigned j=0;j<4;j++){after^=(pixel>>(8*j))&255u;after*=UINT64_C(1099511628211);}}
 if(after==hash||read_cr3()!=target||flags()&0x200||!same_cache(cache))fail("853-readback");
 arch_puts("STYLEOS S853 MOUSE");field("root",target);field("dx",event.dx);field("dy",event.dy);field("buttons",event.buttons);field("writes",moved);field("fnv",after);arch_putc('\n');
 arch_puts("STYLEOS S853 POLICY polling=yes irq=no storage=no clock=no reset=no app-admission=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_DESKTOP_RTC_854
 /* Dedicated single-vCPU terminal Q35 fixture: firmware already exited,
  * IF remains clear, no other live CMOS users, unexpected vectors fail-stop.
  * Establish a NEW index/NMI shadow; inherited port-70 state is unknown.
  * Legacy NMI mask remains set through terminal VM exit. No firmware return. */
 if(!identity((uintptr_t)rtc_x86_bind_854,PG_EXEC)||!identity((uintptr_t)rtc_x86_read_854,PG_EXEC)||
    !identity((uintptr_t)&rtc854,PG_WRITE)||read_cr3()!=target||flags()&0x200)fail("854-closure");
 const uint8_t initial_index=0x8d;
 __asm__ volatile("outb %0,$0x70"::"a"(initial_index):"memory");
 const struct rtc_x86_contract_854 contract={.expected_root=target,.known_index=initial_index,
     .terminal_fixture=true,.exclusive_cmos=true,.known_shadow=true,.post_ebs=true};
 if(!rtc_x86_bind_854(&rtc854,&contract))fail("854-bind");
 struct rtc_time_854 time={0};bool observed=false;
 for(unsigned i=0;i<4096;i++){
    if(rtc_x86_read_854(&rtc854,&time)){observed=true;break;}
    if(rtc854.owner.quarantined)break;
    __asm__ volatile("pause":::"memory");
 }
 if(!observed||rtc854.owner.held||rtc854.owner.quarantined||rtc854.owner.shadow!=initial_index||
    time.hour>23||time.min>59||time.sec>59||read_cr3()!=target||flags()&0x200||!same_cache(cache))fail("854-rtc");
 arch_puts("STYLEOS S854 RTC");field("root",target);field("hour",time.hour);field("minute",time.min);field("second",time.sec);field("shadow",rtc854.owner.shadow);arch_putc('\n');
 arch_puts("STYLEOS S854 POLICY terminal=yes rtc-read=yes data-write=no shadow=software nmi-mask=fixture irq=no storage=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_DESKTOP_TIME_855
 /* Q35/TCG single-vCPU diagnostic only. The count +/-2 PIT tick
  * envelope is a provisional fixture assumption, not a physical attestation.
  * Compare independently observed RTC second edges before accepting a result. */
 if(!identity((uintptr_t)pit_x86_bind_855,PG_EXEC)||!identity((uintptr_t)pit_x86_window_855,PG_EXEC)||
    !identity((uintptr_t)pit_x86_counter_855,PG_EXEC)||!identity((uintptr_t)&pit855,PG_WRITE)||
    read_cr3()!=target||flags()&0x200)fail("855-closure");
 const struct pit_x86_contract_855 pc={.root=target,.terminal=true,.exclusive=true,.post_ebs=true,.reference_attested=true};
 if(!pit_x86_bind_855(&pit855,&pc))fail("855-bind");
 struct calibration_window_855 windows[S855_CAL_SAMPLES];
 const uint16_t counts[S855_CAL_SAMPLES]={59659,47727,65535};
 for(unsigned i=0;i<S855_CAL_SAMPLES;i++){
    const struct pit_capture_contract_855 cc={.count=counts[i],.ref_ticks_min=(uint32_t)counts[i]-2,.ref_ticks_max=(uint32_t)counts[i]+1+(counts[i]!=65535),.max_polls=S855_CAPTURE_MAX_POLLS,.terminal=true,.exclusive=true,.reference_attested=true};
    if(!pit_x86_window_855(&pit855,&cc,&windows[i])||pit855.capture.busy||pit855.capture.quarantined)fail("855-capture");
    const struct calibration_window_855 *w=&windows[i];
    arch_puts("STYLEOS S855 PIT");field("root",target);field("id",i);field("count",counts[i]);field("start-before",w->start_before);field("start-after",w->start_after);field("end-before",w->end_before);field("end-after",w->end_after);field("min-ticks",w->ref_ticks_min);field("max-ticks",w->ref_ticks_max);arch_putc('\n');
 }
 struct calibration_result_855 frequency;
 if(!calibration_pit_855(windows,S855_CAL_SAMPLES,&frequency))fail("855-calibration");
 arch_puts("STYLEOS S855 CAL");field("root",target);field("min-hz",frequency.min_hz);field("max-hz",frequency.max_hz);field("estimate",frequency.estimate_hz);arch_putc('\n');
 /* RTC is a distinct emulated device, not an independent physical oscillator.
  * Bracket consecutive second changes, tolerating failed stable reads. */
 uint64_t before=0,after_read=0,last_before=0,edge_lo[2]={0},edge_hi[2]={0};
 unsigned prev=86400,edges=0,edge_sec[2]={0};
 for(unsigned i=0;i<2000000u&&edges<2;i++){
    if(!pit_x86_counter_855(&pit855,&before))fail("855-edge-before");
    bool valid=rtc_x86_read_854(&rtc854,&time);
    if(!pit_x86_counter_855(&pit855,&after_read)||after_read<before||before<last_before||rtc854.owner.quarantined)fail("855-edge-after");
    if(!valid)continue;
    unsigned sec=(unsigned)time.hour*3600u+(unsigned)time.min*60u+time.sec;
    if(prev!=86400&&sec!=prev){
        if(sec!=(prev+1)%86400||!last_before)fail("855-edge-order");
        edge_lo[edges]=last_before;edge_hi[edges]=after_read;edge_sec[edges]=sec;edges++;
    }
    prev=sec;last_before=before;
 }
 if(edges!=2||edge_lo[1]<=edge_hi[0]||edge_hi[1]<edge_lo[1])fail("855-edge-timeout");
 uint64_t min=edge_lo[1]-edge_hi[0],max=edge_hi[1]-edge_lo[0],hz=frequency.estimate_hz;
 if(min<hz-hz/20||max>hz+hz/20)fail("855-rtc-crosscheck");
 for(unsigned i=0;i<2;i++){arch_puts("STYLEOS S855 EDGE");field("root",target);field("id",i);field("second",edge_sec[i]);field("lo",edge_lo[i]);field("hi",edge_hi[i]);arch_putc('\n');}
 struct monotonic_855 mono={0};
 if(!monotonic_init_855(&mono,hz,edge_hi[0])||!monotonic_sample_855(&mono,edge_lo[1])||!monotonic_sample_855(&mono,edge_hi[1])||!mono.ns||mono.faulted||read_cr3()!=target||flags()&0x200||!same_cache(cache)||rtc854.owner.shadow!=initial_index)fail("855-monotonic");
 arch_puts("STYLEOS S855 MONO");field("root",target);field("origin",mono.origin);field("counter",mono.last_counter);field("ns",mono.ns);arch_putc('\n');
 arch_puts("STYLEOS S855 POLICY terminal=yes pit2=stopped reference=fixture rtc-crosscheck=yes irq=no storage=no production-cr3=deny\n");
#endif
#endif
#endif
#endif
#ifdef STYLEOS_TEST_STORAGE_TOPOLOGY_856
 /* Read-only PCI configuration census. No CFC writes, no ATA ports, no
  * controller admission. CF8 is restored before terminal completion. */
 uint32_t cf8_before=in32(0xcf8);unsigned functions=0,ide=0;
 if(read_cr3()!=target||(flags()&0x200))fail("856-context");
 for(unsigned d=0;d<32;d++)for(unsigned f=0;f<8;f++){
    uint32_t id=cfg(d,f,0);if((id&0xffffu)==0xffffu)continue;
    if(!(id&0xffffu))fail("856-vendor");
    uint32_t cls=cfg(d,f,8);functions++;
    arch_puts("STYLEOS S856 PCI");field("root",target);field("bdf",d*8+f);field("id",id);field("class",cls);arch_putc('\n');
    if((cls>>16)!=0x0101u)continue;
    uint32_t status=cfg(d,f,4),header=cfg(d,f,12),timing=cfg(d,f,0x40);
    uint32_t bars[5];for(unsigned i=0;i<5;i++)bars[i]=cfg(d,f,0x10+4*i);
    arch_puts("STYLEOS S856 IDE");field("root",target);field("bdf",d*8+f);field("command-status",status);field("header",header);field("timing",timing);
    field("bar0",bars[0]);field("bar1",bars[1]);field("bar2",bars[2]);field("bar3",bars[3]);field("bar4",bars[4]);arch_putc('\n');ide++;
 }
 out32(0xcf8,cf8_before);uint32_t cf8_after=in32(0xcf8);
 if(cf8_before!=cf8_after||read_cr3()!=target||(flags()&0x200)||!same_cache(cache))fail("856-restore");
 arch_puts("STYLEOS S856 TOTAL");field("root",target);field("functions",functions);field("ide",ide);field("cf8-before",cf8_before);field("cf8-after",cf8_after);arch_putc('\n');
 arch_puts("STYLEOS S856 POLICY pci-census=yes ata-access=no data-write=no dma=no admission=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_STORAGE_BMDMA_857
 /* Exact Q35/TCG fixture only. Harness independently checks BAR4 owner and
  * extent before resuming from S853 READY. Reads only four BMIDE registers.
  * Not a general port grant, ATA command, DMA stop or controller admission. */
 uint32_t old857=in32(0xcf8),id857=cfg(5,0,0),class857=cfg(5,0,8),cmd857=cfg(5,0,4),bar857=cfg(5,0,0x20);
 out32(0xcf8,old857);
 if(ide!=1||id857!=0x70108086u||class857>>8!=0x010180u||!(cmd857&1u)||bar857!=0x60e1u||in32(0xcf8)!=old857||read_cr3()!=target||(flags()&0x200))fail("857-bmdma-context");
 uint8_t samples857[2][4];
 for(unsigned pass=0;pass<2;pass++){
    const uint16_t ports[4]={0x60e0,0x60e2,0x60e8,0x60ea};
    for(unsigned i=0;i<4;i++)__asm__ volatile("inb %1,%0":"=a"(samples857[pass][i]):"Nd"(ports[i]):"memory");
 }
 uint32_t after857=cfg(5,0,4),bar_after857=cfg(5,0,0x20);out32(0xcf8,old857);
 if(after857!=cmd857||bar_after857!=bar857||in32(0xcf8)!=old857||read_cr3()!=target||(flags()&0x200)||!same_cache(cache))fail("857-bmdma-restore");
 for(unsigned i=0;i<4;i++)if(samples857[0][i]!=samples857[1][i])fail("857-bmdma-unstable");
 arch_puts("STYLEOS S857 BMIDE");field("root",target);field("bdf",40);field("bar4",bar857);field("pci-command",cmd857&0xffffu);field("primary-command",samples857[0][0]);field("primary-status",samples857[0][1]);field("secondary-command",samples857[0][2]);field("secondary-status",samples857[0][3]);field("samples",2);arch_putc('\n');
 arch_puts("STYLEOS S857 POLICY bmdma-read=yes bmdma-write=no ata-access=no dma-denied=no admission=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_STORAGE_BUSMASTER_857
 /* Terminal fixture only. Never re-enable bus master, even on failed readback. */
 struct bm857_snapshot before={target,id857,class857,cmd857,bar857,{samples857[0][0],samples857[0][1],samples857[0][2],samples857[0][3]}};
 uint16_t word857=0;
 if(!bm857_plan(&before,target,&word857)||read_cr3()!=target||(flags()&0x200)||!same_cache(cache))fail("857-busmaster-pre");
 /* Re-read identity, class, BAR and status immediately before the one command write. */
 uint32_t liveid=cfg(5,0,0),liveclass=cfg(5,0,8),livebar=cfg(5,0,0x20),livecmd=cfg(5,0,4);
 out32(0xcf8,old857);
 if(liveid!=id857||liveclass!=class857||livebar!=bar857||livecmd!=cmd857||in32(0xcf8)!=old857)fail("857-busmaster-live");
 const uint16_t bmports857[4]={0x60e0,0x60e2,0x60e8,0x60ea};
 for(unsigned i=0;i<4;i++){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(bmports857[i]):"memory");if(v)fail("857-busmaster-active");}
 cmdwrite(5,0,word857); /* outw CFC: never touch the W1C status word */
 struct bm857_snapshot post={0};
 post.root=read_cr3();post.id=cfg(5,0,0);post.class_revision=cfg(5,0,8);post.bar4=cfg(5,0,0x20);post.command_status=cfg(5,0,4);
 out32(0xcf8,old857);
 for(unsigned i=0;i<4;i++)__asm__ volatile("inb %1,%0":"=a"(post.bm[i]):"Nd"(bmports857[i]):"memory");
 if(!bm857_confirm(&post,target)||read_cr3()!=target||(flags()&0x200)||in32(0xcf8)!=old857||!same_cache(cache))fail("857-busmaster-post");
 arch_puts("STYLEOS S857 BUSMASTER");field("root",target);field("bdf",40);field("before",before.command_status);field("after",post.command_status);field("bar4",post.bar4);field("primary-command",post.bm[0]);field("primary-status",post.bm[1]);field("secondary-command",post.bm[2]);field("secondary-status",post.bm[3]);arch_putc('\n');
 arch_puts("STYLEOS S857 BUSMASTER-POLICY pci-master-cleared=yes ata-access=no dma-admission=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_STORAGE_ATA_READ_857
 /* Explicit Q35/TCG disposable fixture experiment. No generic ATA grant.
  * Host checks topology/attachment/ports before sending input; guest rechecks
  * PCI and BMIDE before each operation. Timing=0 is accepted ONLY here. */
 struct ata857_live io857={target,0};struct ata857_state ata857={0};uint16_t words857[ATA857_WORDS];
 if(!ata857_identify(&ata857,&ata857_io,&io857,words857))fail("857-ata-identify");
 arch_puts("STYLEOS S857 ATA-IDENTIFY");field("root",target);arch_puts(" bytes=");ata857_hex(words857);arch_putc('\n');
 const uint32_t lbas857[]={0,1,2048,4095};
 for(unsigned i=0;i<4;i++){
   if(!ata857_read(&ata857,&ata857_io,&io857,lbas857[i],words857))fail("857-ata-read");
   arch_puts("STYLEOS S857 ATA-SECTOR");field("root",target);field("lba",lbas857[i]);arch_puts(" bytes=");ata857_hex(words857);arch_putc('\n');
 }
 unsigned count857=io857.ops;for(unsigned i=0;i<ATA857_WORDS;i++)words857[i]=0xa5a5;
 if(ata857_read(&ata857,&ata857_io,&io857,4096,words857)||io857.ops!=count857)fail("857-ata-oob");
 for(unsigned i=0;i<ATA857_WORDS;i++)if(words857[i]!=0xa5a5)fail("857-ata-oob-buffer");
 if(!ata857_live_check(&io857)||!same_cache(cache))fail("857-ata-final");
 arch_puts("STYLEOS S857 ATA-REFUSE");field("root",target);field("lba",4096);field("io",0);field("unchanged",1);arch_putc('\n');
 arch_puts("STYLEOS S857 ATA-READY");field("root",target);arch_putc('\n');
 bool ack857=false;
#ifdef STYLEOS_TEST_STORAGE_PIO_RUNTIME_863
 bool write863=false;
#endif
 for(unsigned i=0;i<20000000u;i++){
   struct desk_event ev857={0};if(!desk_input_next(&ev857)){__asm__ volatile("pause");continue;}
    
#ifdef STYLEOS_TEST_STORAGE_PIO_RUNTIME_863
    if(ev857.mouse||(ev857.key!=0x30&&ev857.key!=0x13)||ev857.modifiers)fail("863-pio-ack");
    write863=ev857.key==0x30;ack857=true;break;
#else
    if(ev857.mouse||ev857.key!=0x30||ev857.modifiers)fail("857-ata-ack");ack857=true;break;
#endif
 }
 if(!ack857||!ata857_live_check(&io857)||!same_cache(cache))fail("857-ata-final-ack");
 arch_puts("STYLEOS S857 ATA-POLICY fixture-only=yes identify=yes read=yes data-write=no dma=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_STORAGE_PIO_RUNTIME_863
 /* Host attests exact paused disposable fixture, then B=commit or R=read-only. */
 struct pio863 pio={.live=&io857,.writable=write863};
 struct storage_runtime_863 store={0};
 uint8_t expected863[512],got863[512];uint16_t evidence863[256];
 uint32_t a863,b863,c863,d863;
 __asm__ volatile("cpuid":"=a"(a863),"=b"(b863),"=c"(c863),"=d"(d863):"0"(1),"2"(0));
 if((d863&(1u<<28))&&((b863>>16)&255u)!=1)fail("863-pio-cpu");
 for(unsigned i=0;i<512;i++)expected863[i]=(uint8_t)(i^0xa5);
 if(!ata_runtime_bind_863(&pio.ata,&pio_ops863,&pio)||
    !storage_bind_863(&store,&store_ops863,&pio,target,1,UINT64_C(0x701080862800),2049,1,write863))fail("863-pio-bind");
 unsigned oldops863=io857.ops;
 if(storage_read_863(&store,1,got863)||storage_commit_863(&store,1,expected863)||io857.ops!=oldops863)fail("863-pio-range");
 if(write863){if(!storage_commit_863(&store,0,expected863))fail("863-pio-commit");}
 else if(storage_commit_863(&store,0,expected863)||io857.ops!=oldops863)fail("863-pio-ro");
 if(!storage_read_863(&store,0,got863)||!same_cache(cache)||!ata857_live_check(&io857)||pio.ata.pending_flush)fail("863-pio-readback");
 for(unsigned i=0;i<512;i++)if(got863[i]!=expected863[i])fail("863-pio-bytes");
 for(unsigned i=0;i<256;i++)evidence863[i]=(uint16_t)(got863[2*i]|((uint16_t)got863[2*i+1]<<8));
 arch_puts("STYLEOS S863D DATA");field("root",target);field("write",write863);field("lba",2049);arch_puts(" bytes=");ata857_hex(evidence863);arch_putc('\n');
 arch_puts("STYLEOS S863D READY\n");
 bool done863=false;
 for(unsigned i=0;i<20000000u;i++){struct desk_event ev={0};if(!desk_input_next(&ev)){__asm__ volatile("pause");continue;}
    if(ev.mouse||ev.key!=0x2e||ev.modifiers)fail("863-pio-finish-key");done863=true;break;}
 if(!done863||!same_cache(cache)||!ata857_live_check(&io857))fail("863-pio-finish");
 arch_puts("STYLEOS S863D POLICY disposable=yes dma=no ordinary-cr3=deny\n");
#endif
#endif
#endif
#endif
#endif
 out32(0xf4,0x10);arch_halt();
}
#endif

extern bool s850_read_inherited_entry(void *,uint64_t,uint16_t,uint64_t *);
_Noreturn void s850_preflight(const struct s842_root *base,const struct s842_cache *cache,struct pg_experiment *p,const struct s846_guard *guard,uint64_t gdt,uint64_t idt,uint64_t tss,uint64_t rsp0
#ifdef STYLEOS_TEST_DESKTOP_SWITCH_851
,uint16_t gdt_limit,uint16_t tss_selector
#endif
){
#ifdef STYLEOS_TEST_DESKTOP_INTERACTIVE_863
 if(flags()&0x200)fail("863-platform-context");
#endif
 if(!p||!p->evidence||!guard||read_cr3()!=p->old_root||(flags()&0x200)||arch_paging_attest_image_writable(p,(uintptr_t)tables,sizeof tables)!=PG_OK)fail("context-storage");
#ifdef STYLEOS_TEST_DESKTOP_INTERACTIVE_863
 if(!shell_fixture863())fail("863-platform");
#ifdef STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW
 if(!preview_single_cpu863())fail("863-platform-cpu");
#endif
#endif
 const struct desktop_surface *surface=desktop_platform_surface();
 if(!desktop_surface_valid(surface))fail("surface-invalid");
 if(!desktop_surface_excludes_boot_memory(surface,p->evidence))fail("surface-overlap");
 const struct boot_info *boot=(const void *)(uintptr_t)p->evidence->live.handoff.start;
 struct pci794_bar bar;uint32_t mask;unsigned dev,fn;uint16_t command;
 if(!capture(surface,&bar,&mask,&dev,&fn,&command))fail("vga-bar");
 struct s850_gop_request req={boot,p->evidence,surface,&bar,mask,0,cache,p->old_root,s850_read_inherited_entry,p};
 if(!s850_desktop_build_guarded(base,&req,guard,tables,(uintptr_t)tables,&composed)||!s850_desktop_audit_guarded(base,&req,guard,&composed))fail("tree");
#ifdef STYLEOS_TEST_RUNTIME_ROOTS_861
 if(!preflight861(p,base,&req))fail("861-preflight");
#endif
 const uint64_t desc[]={gdt,idt,tss,rsp0};
 for(unsigned i=0;i<4;i++){
  struct pg_lookup x;
  if(arch_paging_attest_image_writable(p,desc[i],4096)!=PG_OK||pg_lookup(&composed.plan,desc[i],&x)!=PG_OK||x.pa!=desc[i]||x.flags!=PG_WRITE)fail("descriptor-closure");
 }
#ifdef STYLEOS_TEST_DESKTOP_STORAGE_SERVICE_863
 if(disk_preflight863||arch_paging_attest_image_writable(p,(uintptr_t)&disk863,sizeof disk863)!=PG_OK||
    !identity((uintptr_t)&disk863,PG_WRITE)||!identity((uintptr_t)&disk863+4095,PG_WRITE))fail("863-shell-storage-page");
 disk_preflight863=true;
#endif
#if defined(STYLEOS_TEST_DESKTOP_RTC_RUNTIME_863) || defined(STYLEOS_TEST_DESKTOP_CLOCK_SERVICE_863)
 /* The old walker is legal ONLY here, with the inherited CR3 active.
  * The detached plan has already passed the guarded-tree audit. */
 if(rtc_preflight863||
    arch_paging_attest_image_writable(p,(uintptr_t)&rtc_page863,sizeof rtc_page863)!=PG_OK||
    !identity((uintptr_t)&rtc_page863,PG_WRITE)||
    !identity((uintptr_t)&rtc_page863+sizeof rtc_page863-1,PG_WRITE))fail("863-rtc-preflight-page");
 rtc_preflight863=true;
#ifdef STYLEOS_DESKTOP_OWNED_ROOT_PREVIEW
 if(pointer_preflight863||
    arch_paging_attest_image_writable(p,(uintptr_t)&pointer_page863,sizeof pointer_page863)!=PG_OK||
    !identity((uintptr_t)&pointer_page863,PG_WRITE)||
    !identity((uintptr_t)&pointer_page863+4095,PG_WRITE))fail("863-pointer-preflight");
 pointer_preflight863=true;
#endif
#endif
 if(read_cr3()!=p->old_root||(flags()&0x200))fail("root-changed");
 arch_puts("STYLEOS S850 GOP");field("base",surface->base);field("bytes",surface->bytes);field("width",surface->width);field("height",surface->height);field("stride",surface->stride);field("format",surface->format);arch_putc('\n');
 arch_puts("STYLEOS S850 BAR");field("dev",dev);field("fn",fn);field("command",command);field("raw",bar.raw_lo);field("mask",mask);field("size",composed.gop.bar_bytes);arch_putc('\n');
 arch_puts("STYLEOS S850 ROOT");field("inherited",p->old_root);field("live",read_cr3());field("detached",composed.built.root);field("source",base->built.root);field("used",composed.built.used);field("pages",composed.gop.pages);field("pat-index",composed.gop.pat_index);field("cache-bits",composed.gop.cache_bits);field("pat",cache->pat);arch_putc('\n');
 arch_puts("STYLEOS S850 DESCRIPTORS");field("gdt",gdt);field("idt",idt);field("tss",tss);field("rsp0",rsp0);field("ist1",guard->pa[0]);field("ist2",guard->pa[1]);field("ist3",guard->pa[2]);arch_putc('\n');
 arch_puts("STYLEOS S850 POLICY bar-restored=yes detached-audited=yes descriptors-staged=yes desktop-root-active=no framebuffer-write=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_DESKTOP_SWITCH_851
 s851_switch(p,cache,guard,surface,gdt,idt,tss,rsp0,gdt_limit,tss_selector);
#else
 out32(0xf4,0x10);arch_halt();
#endif
}
