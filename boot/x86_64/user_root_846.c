/* Isolated, terminal QEMU supervisor-only trial of two process roots. */
#include <styleos/user_root_846.h>
#include <styleos/ist_guard_plan_785.h>
#include <styleos/arch.h>
#define P UINT64_C(4096)
#define NX (UINT64_C(1)<<63)
static _Alignas(4096) uint64_t tables[2][PG_TABLE_PAGES][512];
static struct s846_root child[2];
static struct s846_guard guards;
static struct s844_lease leases[2];
static const struct s842_root *safe;
static const struct s842_cache *cache;
static const struct igp785_tables *active;
static uint64_t safe_pa;
struct s844_arena *s845_arena(void);
static uint64_t cr3(void){uint64_t v;__asm__ volatile("mov %%cr3,%0":"=r"(v));return v;}
static uint64_t flags(void){uint64_t v;__asm__ volatile("pushfq;popq %0":"=r"(v));return v;}
static uint64_t cr4(void){uint64_t v;__asm__ volatile("mov %%cr4,%0":"=r"(v));return v;}
static void switch_to(uint64_t v){__asm__ volatile("mov %0,%%cr3"::"r"(v):"memory");}
static void fld(const char *key,uint64_t v){arch_putc(' ');arch_puts(key);arch_puts("0x");for(int i=60;i>=0;i-=4)arch_putc("0123456789abcdef"[(v>>(unsigned)i)&15]);}
static _Noreturn void fail(const char *s){arch_puts("STYLEOS S846 FAIL ");arch_puts(s);arch_putc('\n');__asm__ volatile("cli;outl %0,%1"::"a"(0x25u),"Nd"((uint16_t)0xf4));arch_halt();}
/* Bind only the image-owned source and destination table buffers, while
 * the previous inherited physical-attestation mechanism is still available. */
bool s846_bind(const struct s842_root *r,const struct s842_cache *c,struct pg_experiment *p,const struct igp785_tables *t,uint64_t root,uint64_t g1,uint64_t g2,uint64_t g3){
 if(safe||!p||!t||root!=(uintptr_t)t||!s842_audit(r,c)||
    arch_paging_attest_image_writable(p,(uintptr_t)tables,sizeof tables)!=PG_OK)return false;
 guards=(struct s846_guard){{g1,g2,g3}};safe=r;cache=c;active=t;safe_pa=root;return true;
}
static bool source_ok(void){
 if(!safe||!cache||!active||cr3()!=safe_pa||!s842_audit(safe,cache))return false;
 for(unsigned i=0;i<512;i++){
  uint64_t root=i==0?safe->plan.tables[0][0]&~UINT64_C(0x20):i==128?((uintptr_t)active->pdpt|3u):0;
  uint64_t pdpt=i==0?((uintptr_t)active->pd|3u):0;
  uint64_t pd=i==0?((uintptr_t)active->pt|3u):0;
  uint64_t pt=i==3?(guards.pa[0]|NX|3u):i==6?(guards.pa[1]|NX|3u):i==9?(guards.pa[2]|NX|3u):0;
  if((active->pml4[i]&~(root?UINT64_C(0x20):0))!=root||(active->pdpt[i]&~(pdpt?UINT64_C(0x20):0))!=pdpt||(active->pd[i]&~(pd?UINT64_C(0x20):0))!=pd||(active->pt[i]&~(pt?UINT64_C(0x60):0))!=pt)return false;
 }
 return true;
}
static uint64_t read_word(uint64_t a){return *(volatile uint64_t *)(uintptr_t)a;}
static void write_word(uint64_t a,uint64_t v){*(volatile uint64_t *)(uintptr_t)a=v;}
static bool zero_span(uint64_t pa,uint64_t bytes){const volatile unsigned char *b=(const void *)(uintptr_t)pa;for(uint64_t i=0;i<bytes;i++)if(b[i])return false;return true;}
static void clear_span(void *pa,size_t bytes){volatile unsigned char *b=pa;for(size_t i=0;i<bytes;i++)b[i]=0;}
void s846_guest_run(uint64_t root){
 uint16_t cs;__asm__ volatile("mov %%cs,%0":"=r"(cs));
 /* No PCID/PGE/LA57/SMAP. Do not change policy to force this profile to pass. */
 if(root!=safe_pa||!source_ok()||(flags()&0x200)||(cs&3)||(cr4()&((1u<<7)|(1u<<12)|(1u<<17)|(1u<<21))))fail("source-root-or-mode");
 struct s844_arena *a=s845_arena();unsigned count;
 if(!a||!s844_acquire(a,S846_OWNER_A,3,&leases[0])||!s844_acquire(a,S846_OWNER_B,3,&leases[1]))fail("leases");
 for(unsigned i=0;i<2;i++){
  if(!zero_span(leases[i].pa,3*P))fail("lease-zero");
  write_word(leases[i].pa,UINT64_C(0x846c0000)+i); /* data fixture, never executed */
  write_word(leases[i].pa+P,UINT64_C(0x846d0000)+i);
  write_word(leases[i].pa+2*P,UINT64_C(0x846e0000)+i);
  if(!s846_build(safe,cache,a,leases,i,&guards,tables[i],(uintptr_t)tables[i],&child[i]))fail("build");
 }
 if(!s846_audit_pair(safe,cache,a,leases,&guards,&child[0],&child[1]))fail("pair-audit");
 for(unsigned i=0;i<3;i++)write_word(guards.pa[i]+0x80,UINT64_C(0x84610000)+i);
 arch_puts("STYLEOS S846 READY");fld("safe",root);fld("a",child[0].built.root);fld("b",child[1].built.root);fld("pa-a",leases[0].pa);fld("pa-b",leases[1].pa);fld("gen-a",leases[0].generation);fld("gen-b",leases[1].generation);fld("used-a",child[0].built.used);fld("used-b",child[1].built.used);for(unsigned i=0;i<3;i++)fld(i==0?"ist1":i==1?"ist2":"ist3",guards.pa[i]);arch_putc('\n');
 for(unsigned i=0;i<2;i++){
  if(!source_ok()||!s846_audit_pair(safe,cache,a,leases,&guards,&child[0],&child[1]))fail("before-switch");
  switch_to(child[i].built.root);uint64_t live=cr3();
  uint64_t code=read_word(S846_CODE),data=read_word(S846_DATA),stack=read_word(S846_STACK),g[3];
  for(unsigned j=0;j<3;j++)g[j]=read_word(S846_IST+3*j*P+0x80);
  write_word(S846_DATA,UINT64_C(0x846d1000)+i);write_word(S846_STACK,UINT64_C(0x846e1000)+i);
  switch_to(root);
  if(!source_ok()||live!=child[i].built.root||code!=UINT64_C(0x846c0000)+i||data!=UINT64_C(0x846d0000)+i||stack!=UINT64_C(0x846e0000)+i||read_word(leases[i].pa+P)!=UINT64_C(0x846d1000)+i||read_word(leases[i].pa+2*P)!=UINT64_C(0x846e1000)+i||read_word(leases[1-i].pa+P)!=(i?UINT64_C(0x846d1000):UINT64_C(0x846d0001)))fail("live-alias");
  for(unsigned j=0;j<3;j++)if(g[j]!=UINT64_C(0x84610000)+j)fail("live-guard");
  arch_puts("STYLEOS S846 SWITCH");fld("id",i);fld("live",live);fld("after",cr3());fld("code",code);fld("data",data);fld("stack",stack);fld("write-data",read_word(leases[i].pa+P));fld("write-stack",read_word(leases[i].pa+2*P));fld("foreign-data",read_word(leases[1-i].pa+P));for(unsigned j=0;j<3;j++)fld(j==0?"g1":j==1?"g2":"g3",g[j]);arch_putc('\n');
 }
 if(!s846_audit_pair(safe,cache,a,leases,&guards,&child[0],&child[1]))fail("post-switch-audit");
 /* Invalidate detached contexts before releasing any mapped data. */
 clear_span(child,sizeof child);clear_span(tables,sizeof tables);
 if(!zero_span((uintptr_t)tables,sizeof tables)||!s844_release(a,S846_OWNER_A,leases[0])||!s844_release(a,S846_OWNER_B,leases[1])||!zero_span(leases[0].pa,3*P)||!zero_span(leases[1].pa,3*P)||!s844_count(a,&count)||count||!source_ok())fail("cleanup");
 arch_puts("STYLEOS S846 CLEAN");fld("root",cr3());fld("used",count);fld("tables-zero",1);fld("leases-zero",1);arch_putc('\n');
 arch_puts("STYLEOS S846 POLICY supervisor-switch=yes cpl3=no production-cr3=deny\n");
}
