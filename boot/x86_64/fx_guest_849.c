/* Isolated diagnostic: no control-register changes, no compiler-generated FP. */
#include <styleos/fx_guest_849.h>
#include <styleos/arch.h>
struct s849_image s849_kernel_image,s849_user_capture,s849_user_restore;
static struct s849_image kernel_check;
volatile uint64_t s849_live;
static struct s849_bank bank;
static uint64_t safe849;
static unsigned seen[S846_ROOT_COUNT];
#ifdef STYLEOS_TEST_RUNTIME_LOOP_862
extern const unsigned s862_loop_budget[S846_ROOT_COUNT];
#endif
static uint64_t cr3_849(void){uint64_t x;__asm__ volatile("mov %%cr3,%0":"=r"(x));return x;}
static uint64_t flags849(void){uint64_t x;__asm__ volatile("pushfq;popq %0":"=r"(x));return x;}
static uint32_t u32(const uint8_t *b){return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
static uint64_t u64(const uint8_t *b){return u32(b)|((uint64_t)u32(b+4)<<32);}
static void field(const char *k,uint64_t x){arch_putc(' ');arch_puts(k);arch_puts("=0x");for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(x>>(unsigned)i)&15]);}
static bool mapped(const struct s846_root *r,const void *v,size_t n){uint64_t x=(uintptr_t)v;for(size_t i=0;i<r->plan.count;i++){const struct pg_mapping *m=&r->plan.maps[i];if(m->va==m->pa&&m->flags==PG_WRITE&&x>=m->va&&x-m->va<m->bytes&&n<=m->bytes-(x-m->va))return true;}return false;}
static void scrub(void *p,size_t n){volatile uint8_t *b=p;while(n--)*b++=0;}
static bool allzero(const void *p,size_t n){const uint8_t *b=p;while(n--)if(*b++)return false;return true;}
#ifdef STYLEOS_TEST_FOUR_GUEST_864
bool s849_guest_init(const struct s846_root child[S846_ROOT_COUNT],const struct s844_lease leases[S846_ROOT_COUNT],uint64_t safe){
#else
bool s849_guest_init(const struct s846_root child[2],uint64_t safe){
#endif
 if(safe849||s849_live||cr3_849()!=safe||(flags849()&0x200))return false;
 for(unsigned i=0;i<S846_ROOT_COUNT;i++)if(!mapped(&child[i],&bank,sizeof bank)||!mapped(&child[i],&s849_kernel_image,sizeof s849_kernel_image)||!mapped(&child[i],&s849_user_capture,sizeof s849_user_capture)||!mapped(&child[i],&s849_user_restore,sizeof s849_user_restore)||!mapped(&child[i],&kernel_check,sizeof kernel_check)||!mapped(&child[i],(const void *)&s849_live,sizeof s849_live))return false;
 struct s849_caps c={0};uint32_t a,b,mask;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c.leaf1_ecx),"=d"(c.leaf1_edx):"a"(1),"c"(0));
 __asm__ volatile("mov %%cr0,%0;mov %%cr4,%1":"=r"(c.cr0),"=r"(c.cr4));
 arch_puts("STYLEOS S849 MODE");field("ecx",c.leaf1_ecx);field("edx",c.leaf1_edx);field("cr0",c.cr0);field("cr4",c.cr4);arch_putc('\n');
 if(!s849_admit(&c,&mask))return false;
 __asm__ volatile("fxsave64 %0":"=m"(s849_kernel_image)::"memory");
 c.mxcsr_mask=u32(s849_kernel_image.bytes+28);
#ifdef STYLEOS_TEST_FOUR_GUEST_864
 uint64_t roots[S846_ROOT_COUNT],generations[S846_ROOT_COUNT];
 for(unsigned i=0;i<S846_ROOT_COUNT;i++){
  if(child[i].lease.generation!=leases[i].generation||child[i].lease.pa!=leases[i].pa)return false;
  roots[i]=child[i].built.root;generations[i]=leases[i].generation;
 }
#ifdef STYLEOS_INTEGRATED_RUNTIME_864
 if(!s865_fx_open(&bank,&c))return false;
 for(unsigned i=0;i<S846_ROOT_COUNT;i++)if(!s865_fx_bind(&bank,i,roots[i],generations[i]))return false;
#else
 if(!s849_init_set(&bank,&c,roots,generations))return false;
#endif
 for(unsigned i=0;i<S846_ROOT_COUNT;i++){
  seen[i]=0;
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
  if(generations[i]>=5){
#else
  if(generations[i]>=8){
#endif
   struct s849_image rejected={0};
   if(s849_dispatch(&bank,i,roots[i],generations[i]-4,&rejected)||!allzero(&rejected,sizeof rejected)||bank.active!=-1)return false;
   arch_puts("STYLEOS S864 FX-STALE");field("id",i);field("gen",generations[i]-4);arch_putc('\n');
  }
 }
 if(
#else
 if(!s849_init(&bank,&c,child[0].built.root,child[1].built.root)||
#endif
 !s849_image_valid(&s849_kernel_image,bank.mxcsr_mask)||(bank.mxcsr_mask&0x3f80)!=0x3f80)return false;
#ifdef STYLEOS_TEST_RUNTIME_RECYCLE_862
 seen[0]=seen[1]=0;
#endif
 safe849=safe;arch_puts("STYLEOS S849 READY");field("mask",bank.mxcsr_mask);field("safe",safe);arch_putc('\n');return true;
}
#ifdef STYLEOS_TEST_RUNTIME_RECYCLE_862
/* The caller has audited the same two leases and their roots before this
 * operation. No generation can be reused without a fresh empty FP bank. */
bool s849_guest_bind_leases(const struct s844_lease leases[2],bool reused){
 if(!safe849||cr3_849()!=safe849||(flags849()&0x200)||bank.active!=-1||s849_live||!leases||
#ifdef STYLEOS_TEST_DESKTOP_AUTONOMOUS_863
 !leases[0].generation||leases[0].generation==UINT64_MAX||!(leases[0].generation&1u)||
 reused!=(leases[0].generation>1)
#else
 leases[0].generation!=
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
 (reused?3u:1u)
#else
 (reused?6u:4u)
#endif
#endif
 ||leases[1].generation!=leases[0].generation+1)return false;
 for(unsigned i=0;i<2;i++){
  if(bank.slot[i].generation!=1||!bank.slot[i].live||seen[i])return false;
  bank.slot[i].generation=leases[i].generation;
  if(reused){struct s849_image out={0};if(s849_dispatch(&bank,i,bank.slot[i].root,leases[i].generation-2,&out)||!allzero(&out,sizeof out)||bank.active!=-1)return false;}
 }
 arch_puts("STYLEOS S862R FX");field("safe",safe849);field("gen-a",leases[0].generation);field("gen-b",leases[1].generation);field("refused",reused?2:0);arch_putc('\n');return true;
}
#define FX_GEN(i) bank.slot[i].generation
#else
#ifdef STYLEOS_TEST_FOUR_GUEST_864
#define FX_GEN(i) bank.slot[i].generation
#else
#define FX_GEN(i) 1
#endif
#endif
bool s849_guest_dispatch(unsigned id,uint64_t root){
 if(!safe849||cr3_849()!=safe849||(flags849()&0x200)||s849_live||id>=S846_ROOT_COUNT||!s849_dispatch(&bank,id,root,FX_GEN(id),&s849_user_restore))return false;
 scrub(&s849_user_capture,sizeof s849_user_capture);s849_live=1;return true;
}
bool s849_guest_suspend(unsigned id,uint64_t root,bool terminal){
 if(!safe849||id>=S846_ROOT_COUNT||cr3_849()!=root||(flags849()&0x200)||s849_live!=2||!s849_suspend(&bank,id,root,FX_GEN(id),&s849_user_capture))return false;
 /* The kernel FP restore is an architectural check independent of the
  * user program. Validate the actual captured state through the bank above. */
 __asm__ volatile("fxsave64 %0":"=m"(kernel_check)::"memory");
 if(!s849_arch_equal(&s849_kernel_image,&kernel_check,bank.mxcsr_mask))return false;
 scrub(&kernel_check,sizeof kernel_check);
 const uint8_t *p=s849_user_capture.bytes;
 uint64_t x14=u64(p+160+14*16),x15=u64(p+160+15*16),st0=u64(p+32);
 uint32_t exp=(uint32_t)p[40]|((uint32_t)p[41]<<8);
 bool control=p[0]==0x7f&&p[1]==3&&!p[2]&&p[3]==0x38&&p[4]==0x80;
 bool other=allzero(p+160,14*16)&&!u64(p+160+14*16+8)&&!u64(p+160+15*16+8);
#ifdef STYLEOS_INTEGRATED_RUNTIME_864
 /* Report observed fixture properties, but the kernel's FP save/restore
  * must not depend on these user-controlled register values. The
  * external guest verifier still requires the precise test pattern. */
 arch_puts("STYLEOS S849 OBSERVED");field("id",id);field("root",root);field("n",seen[id]+1);field("control",control);field("other-zero",other);arch_putc('\n');
#else
 uint64_t tag=UINT64_C(0x8480000000000000)|((uint64_t)id<<32);
 if(!control||!other||x14!=(tag|13)||x15!=(tag|14)||
    st0!=(id?0:UINT64_C(0x8000000000000000))||exp!=(id?0:0x3fff)||
    u32(p+24)!=(id?0x3f80u:0x1f80u))return false;
#endif
 ++seen[id];arch_puts("STYLEOS S849 CAPTURE");field("id",id);field("root",root);field("n",seen[id]);field("xmm14",x14);field("xmm15",x15);field("mxcsr",u32(p+24));field("st0",st0);field("exp",exp);field("kernel-restored",1);field("terminal",terminal);arch_putc('\n');
 scrub(&s849_user_capture,sizeof s849_user_capture);scrub(&s849_user_restore,sizeof s849_user_restore);s849_live=0;
 /* Retirement happens later under the safe root. */
 return true;
}
bool s849_guest_finish(uint64_t safe){
#if defined(STYLEOS_TEST_FOUR_GUEST_864) && !defined(STYLEOS_INTEGRATED_RUNTIME_864)
 for(unsigned i=0;i<S846_ROOT_COUNT;i++)if(seen[i]!=s862_loop_budget[i]+2)return false;
#endif
 if(safe!=safe849||cr3_849()!=safe||(flags849()&0x200)||s849_live
#if defined(STYLEOS_INTEGRATED_RUNTIME_864)
 /* Retire the actual bound slots; capture counts are external test criteria. */
#elif defined(STYLEOS_TEST_RUNTIME_LOOP_862)
#ifdef STYLEOS_TEST_RUNTIME_MIXED_863
 ||seen[0]!=s862_loop_budget[0]+2||seen[1]!=s862_loop_budget[1]+2
#else
 ||seen[0]!=s862_loop_budget[0]+1||seen[1]!=s862_loop_budget[1]+1
#endif
#else
 ||seen[0]!=4||seen[1]!=6
#endif
 ||bank.active!=-1)return false;
 for(unsigned i=0;i<S846_ROOT_COUNT;i++)if(!s849_retire(&bank,i,bank.slot[i].root,FX_GEN(i))||!allzero(&bank.slot[i],sizeof bank.slot[i]))return false;
 scrub(&s849_kernel_image,sizeof s849_kernel_image);scrub(&bank,sizeof bank);safe849=0;
 if(!allzero(&s849_kernel_image,sizeof s849_kernel_image)||!allzero(&bank,sizeof bank)||!allzero(&s849_user_capture,sizeof s849_user_capture)||!allzero(&s849_user_restore,sizeof s849_user_restore)||!allzero(&kernel_check,sizeof kernel_check))return false;
 arch_puts("STYLEOS S849 CLEAN slots-zero=yes kernel-copy-zero=yes\n");return true;
}
