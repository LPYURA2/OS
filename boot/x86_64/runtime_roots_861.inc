/* S861 diagnostic only: included after the S851 hardware helpers. No CPL3
 * dispatch or new grants. All objects are image-owned, single CPU, IF=0. */
#include <styleos/runtime_roots_861.h>
#ifndef S863_DISPATCHES_PER_BATCH
#define S863_DISPATCHES_PER_BATCH 10u
#endif
static _Alignas(4096) uint64_t child_tables861[S846_ROOT_COUNT][PG_TABLE_PAGES][512];
#ifndef STYLEOS_OWNED_SHELL_865
static struct s846_root children861[S846_ROOT_COUNT];
#endif
static struct s850_gop_plan frozen861;
static const struct s842_root *base861;
static bool preflight861(struct pg_experiment *p,const struct s842_root *base,const struct s850_gop_request *req){
 if(base861||read_cr3()!=p->old_root||arch_paging_attest_image_writable(p,(uintptr_t)child_tables861,sizeof child_tables861)!=PG_OK||!s850_gop_prepare(req,&frozen861))return false;
 base861=base;return true;
}
static struct rt858_live capture861(void){return rt864_arch_capture();}
static bool boundary861(const struct dpr_service *d,const struct s842_cache *cache){
 struct rt858_live v=capture861();return d&&rt861_dispatcher(&v)==d&&same_cache(cache)&&(
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
#if defined(STYLEOS_TEST_DESKTOP_AUTONOMOUS_863) || defined(STYLEOS_INTEGRATED_RUNTIME_864)
 (d->dispatcher.dispatches&&d->dispatcher.dispatches%S863_DISPATCHES_PER_BATCH==0&&
  d->dispatcher.dispatches<=S863_DISPATCHES_PER_BATCH*S863_LAUNCH_LIMIT&&!pr_snapshot(pr_global(),0,0,true)&&!ws_snapshot(ws_global(),0,0))||
#endif
 ((d->dispatcher.dispatches==
#ifdef STYLEOS_TEST_FOUR_GUEST_864
 26 || d->dispatcher.dispatches==52 || d->dispatcher.dispatches==78 || d->dispatcher.dispatches==104
#elif defined(STYLEOS_TEST_RUNTIME_MIXED_863)
 12
#else
 10
#endif
#ifdef STYLEOS_TEST_RUNTIME_RECYCLE_862
 ||d->dispatcher.dispatches==2*S863_DISPATCHES_PER_BATCH
#endif
 )&&!pr_snapshot(pr_global(),0,0,true)&&!ws_snapshot(ws_global(),0,0))||
#endif
#ifdef STYLEOS_TEST_RUNTIME_USER_861
#ifdef STYLEOS_TEST_RUNTIME_COOP_863
 (d->dispatcher.dispatches==8&&!pr_snapshot(pr_global(),0,0,true)&&!ws_snapshot(ws_global(),0,0))||
#else
 (d->dispatcher.dispatches==4&&!pr_snapshot(pr_global(),0,0,true)&&!ws_snapshot(ws_global(),0,0))||
#endif
#endif
 rt858_ready(v.root));
}
#ifndef STYLEOS_OWNED_SHELL_865
static void zero861(void *p,size_t n){volatile unsigned char *b=p;for(size_t i=0;i<n;i++)b[i]=0;}
static bool iszero861(uint64_t p,size_t n){const volatile unsigned char *b=(const void *)(uintptr_t)p;for(size_t i=0;i<n;i++)if(b[i])return false;return true;}
static __attribute__((noinline)) void roundtrip861(unsigned i,uint64_t root,uint64_t safe,uint64_t *out){
 uint64_t pc,sp;
 __asm__ volatile("mov %0,%%cr3"::"r"(root):"memory");
 if(read_cr3()!=root||flags()&0x200)fail("861-child-live");
 uint64_t value=*(volatile uint64_t *)(uintptr_t)S846_DATA;
 *(volatile uint64_t *)(uintptr_t)S846_DATA=value^UINT64_C(0x86100000);
 s851_frame_address=0;
 __asm__ volatile("leaq s861_bp_return(%%rip),%0;mov %%rsp,%1;int3;.global s861_bp_return;s861_bp_return:":"=&r"(pc),"=&r"(sp)::"memory");
 if(read_cr3()!=root||s851_frame_address!=S846_IST+4096-16-40||s851_frame[0]!=pc||s851_frame[3]!=sp||s851_frame[2]&0x200)fail("861-child-ist");
 __asm__ volatile("mov %0,%%cr3"::"r"(safe):"memory");
 if(read_cr3()!=safe||value!=UINT64_C(0x86110000)+i)fail("861-child-return");
 out[0]=pc;out[1]=sp;out[2]=value;out[3]=s851_frame[1];out[4]=s851_frame[2];
}
#if defined(STYLEOS_TEST_RUNTIME_USER_861) || defined(STYLEOS_TEST_RUNTIME_TIMER_861)
struct user_audit861 {const struct s842_cache *cache;const struct boot_memory_evidence *e;const struct s846_guard *guard;
#ifdef STYLEOS_INTEGRATED_RUNTIME_864
 struct s844_arena *arena;struct s844_lease *leases;
#else
 const struct s844_arena *arena;const struct s844_lease *leases;
#endif
};
static bool user_audit861(void *opaque){
 const struct user_audit861 *a=opaque;
#ifdef STYLEOS_INTEGRATED_RUNTIME_864
 return read_cr3()==composed.built.root&&!(flags()&0x200)&&same_cache(a->cache)&&rt865_roots_audit_sparse_frozen(base861,a->cache,&frozen861,a->e,a->arena,a->leases,a->guard,&composed,children861);
#elif defined(STYLEOS_TEST_FOUR_GUEST_864)
 return read_cr3()==composed.built.root&&!(flags()&0x200)&&same_cache(a->cache)&&rt864_roots_audit_frozen(base861,a->cache,&frozen861,a->e,a->arena,a->leases,a->guard,&composed,children861);
#else
 return read_cr3()==composed.built.root&&!(flags()&0x200)&&same_cache(a->cache)&&rt861_roots_audit_frozen(base861,a->cache,&frozen861,a->e,a->arena,a->leases,a->guard,&composed,&children861[0],&children861[1]);
#endif
}
#ifdef STYLEOS_INTEGRATED_RUNTIME_864
static bool retire865(void *opaque,unsigned i,uint64_t root,uint64_t generation){
 struct user_audit861 *a=opaque;
 unsigned before,after;
 if(i>=S846_ROOT_COUNT||!a||!user_audit861(a)||!s844_count(a->arena,&before)||before<3||
    children861[i].built.root!=root||a->leases[i].generation!=generation)return false;
 const struct s844_lease lease=a->leases[i];
 zero861(&children861[i],sizeof children861[i]);zero861(child_tables861[i],sizeof child_tables861[i]);
 if(!iszero861((uintptr_t)child_tables861[i],sizeof child_tables861[i])||
    !s844_release(a->arena,lease.owner,lease)||!iszero861(lease.pa,3*4096))return false;
 zero861(&a->leases[i],sizeof a->leases[i]);
 if(!user_audit861(a)||!s844_count(a->arena,&after)||after+3!=before)return false;
 arch_puts("STYLEOS S865 ROOT-RETIRE");field("id",i);field("safe",read_cr3());field("used",after);field("gen",generation);arch_putc('\n');
 return true;
}
#endif
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
extern bool s848_trial(
#else
extern void s848_trial(
#endif
const struct s846_root [S846_ROOT_COUNT],const struct s844_lease [S846_ROOT_COUNT],uint64_t,uint64_t,bool (*)(void *),void *
#ifdef STYLEOS_INTEGRATED_RUNTIME_864
 ,bool (*)(void *,unsigned,uint64_t,uint64_t),const struct s844_arena *
#endif
 );
extern void s860_reap(uint64_t);
#else
extern void s847_trial(const struct s846_root [2],const struct s844_lease [2],uint64_t,bool (*)(void *),void *);
#ifdef STYLEOS_TEST_RUNTIME_COOP_863
extern void s863_coop_reap(uint64_t,const struct s844_lease [2]);
#endif
#endif
#endif
#ifdef STYLEOS_TEST_FOUR_GUEST_864
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
static bool
#else
static void
#endif
guest861(struct pg_experiment *p,const struct s842_cache *cache,const struct s846_guard *guard,struct s844_arena *arena,const struct desktop_surface *s,uint64_t before){
 const uint64_t safe=composed.built.root;const struct dpr_service *d=dpr_global_state();
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
 static struct s844_lease l[4],old[4];
 static unsigned round;
 static bool prepared;
 unsigned used;
 if(round>=S863_LAUNCH_LIMIT)fail("864-live-limit");
 const uint64_t gen=S863_INITIAL_ARENA_NEXT+4*round;
 if(!prepared){
#else
 struct s844_lease l[4],old[4]={0};unsigned used;
 for(unsigned round=0;round<4;round++){
 const uint64_t gen=4+4*round;
#endif
 if(d->dispatcher.dispatches!=26*round||pr_global()->next_pid!=1+4*round||ws_global()->next_id!=1+4*round)fail("864-batch-start");
 arch_puts("STYLEOS S864 BATCH-BEGIN");field("round",round);field("dispatches",d->dispatcher.dispatches);field("next",arena->next);arch_putc('\n');
 if(!base861||!arena||!boundary861(d,cache)||!s844_count(arena,&used)||used||arena->next!=gen||!s850_desktop_audit_frozen_861(base861,cache,&frozen861,p->evidence,guard,&composed))fail("864-admit");
#ifdef STYLEOS_INTEGRATED_RUNTIME_864
 struct user_audit861 sparse={cache,p->evidence,guard,arena,l};
 if(!user_audit861(&sparse))fail("865-empty-audit");
 for(unsigned i=0;i<4;i++){
  if(!s844_acquire(arena,S846_OWNER_A+i,3,&l[i])||!iszero861(l[i].pa,3*4096))fail("865-slot-lease");
  *(volatile uint64_t *)(uintptr_t)(l[i].pa+4096)=UINT64_C(0x86110000)+i;
  if(!s865_root_build(base861,cache,arena,l,i,guard,child_tables861[i],(uintptr_t)child_tables861[i],&children861[i])||
     !user_audit861(&sparse)||!s844_count(arena,&used)||used!=3*(i+1))fail("865-slot-build");
  arch_puts("STYLEOS S865 ROOT-ADMIT");field("id",i);field("safe",safe);field("used",used);field("gen",l[i].generation);arch_putc('\n');
#else
 for(unsigned i=0;i<4;i++){
  if(!s844_acquire(arena,S846_OWNER_A+i,3,&l[i])||!iszero861(l[i].pa,3*4096))fail("864-lease");
  *(volatile uint64_t *)(uintptr_t)(l[i].pa+4096)=UINT64_C(0x86110000)+i;
 }
 for(unsigned i=0;i<4;i++){
  if(!s846_build(base861,cache,arena,l,i,guard,child_tables861[i],(uintptr_t)child_tables861[i],&children861[i]))fail("864-build");
#endif
  arch_puts("STYLEOS S864 ROOT");field("id",i);field("safe",safe);field("child",children861[i].built.root);field("pa",l[i].pa);field("gen",l[i].generation);arch_putc('\n');
 }
 struct user_audit861 prep={cache,p->evidence,guard,arena,l};
 if(!user_audit861(&prep))fail("864-joint-audit");
 if(round){
  if(rt864_roots_audit_frozen(base861,cache,&frozen861,p->evidence,arena,old,guard,&composed,children861))fail("864-stale-root");
  for(unsigned i=0;i<4;i++){
   if(old[i].pa!=l[i].pa||old[i].generation+4!=l[i].generation||
      s844_release(arena,old[i].owner,old[i])||!s844_count(arena,&used)||used!=12||
      *(volatile uint64_t *)(uintptr_t)(l[i].pa+4096)!=UINT64_C(0x86110000)+i)fail("864-stale-lease");
   arch_puts("STYLEOS S864 REUSE");field("id",i);field("pa",l[i].pa);field("old",old[i].generation);field("new",l[i].generation);arch_putc('\n');
  }
  if(!user_audit861(&prep))fail("864-reuse-audit");
 }
 for(unsigned i=0;i<4;i++)old[i]=l[i];
 for(unsigned i=0;i<4;i++){
  uint64_t r[5];roundtrip861(i,children861[i].built.root,safe,r);
  for(unsigned j=0;j<4;j++)if(*(volatile uint64_t *)(uintptr_t)(l[j].pa+4096)!=((UINT64_C(0x86110000)+j)^(j<=i?UINT64_C(0x86100000):0)))fail("864-cross-isolation");
  if(!boundary861(d,cache)||!user_audit861(&prep))fail("864-switch-audit");
  arch_puts("STYLEOS S864 SWITCH");field("id",i);field("child",children861[i].built.root);field("safe",safe);field("rip",r[0]);field("rsp",r[1]);field("cs",r[3]);field("flags",r[4]);field("frame",s851_frame_address);arch_putc('\n');
 }
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
 prepared=true;}
#endif
 struct user_audit861 a={cache,p->evidence,guard,arena,l};
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
 if(!s848_trial(children861,l,safe,base861->lapic,user_audit861,&a
#ifdef STYLEOS_INTEGRATED_RUNTIME_864
 ,retire865,arena
#endif
 ))return false;
#else
 s848_trial(children861,l,safe,base861->lapic,user_audit861,&a);
#endif
 if(!user_audit861(&a))fail("864-final-audit");
#ifdef STYLEOS_INTEGRATED_RUNTIME_864
 /* Actual terminal events already revoked each root independently. */
 if(!s844_count(arena,&used)||used||!iszero861((uintptr_t)child_tables861,sizeof child_tables861))fail("865-all-retired");
#else
 zero861(children861,sizeof children861);zero861(child_tables861,sizeof child_tables861);
 if(!iszero861((uintptr_t)child_tables861,sizeof child_tables861))fail("864-table-zero");
 for(unsigned i=0;i<4;i++)if(!s844_release(arena,S846_OWNER_A+i,l[i])||!iszero861(l[i].pa,3*4096))fail("864-release");
#endif
 struct rt858_live v=capture861();
 if(rt861_dispatcher(&v)!=d||!s844_count(arena,&used)||used)fail("864-reap-boundary");
 s860_reap(safe);
 uint64_t after=UINT64_C(14695981039346656037);const volatile uint32_t *fb=(const void *)(uintptr_t)s->base;
 for(size_t i=0;i<(size_t)s->width*s->height;i++){uint32_t pixel=fb[i];for(unsigned j=0;j<4;j++){after^=(pixel>>(8*j))&255u;after*=UINT64_C(1099511628211);}}
 if(after!=before)fail("864-final-frame");
 if(!boundary861(d,cache))fail("864-final-boundary");
 if(arena->next!=gen+4)fail("864-final-generation");
 if(!s850_desktop_audit_frozen_861(base861,cache,&frozen861,p->evidence,guard,&composed))fail("864-final-root");
 arch_puts("STYLEOS S864 CLEAN");field("safe",safe);field("used",used);field("next",arena->next);field("tables-zero",1);field("leases-zero",1);field("fnv",after);arch_putc('\n');
 if(pr_global()->next_pid!=5+4*round||ws_global()->next_id!=5+4*round)fail("864-identities");
 arch_puts("STYLEOS S864 BATCH-END");field("round",round);field("dispatches",d->dispatcher.dispatches);field("next",arena->next);arch_putc('\n');
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
 prepared=false;++round;return true;
#else
 }
#endif
}
#else
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
static __attribute__((noinline)) bool
#else
#ifdef STYLEOS_TEST_DESKTOP_COLD_863
static __attribute__((unused)) void
#else
static void
#endif
#endif
guest861(struct pg_experiment *p,const struct s842_cache *cache,const struct s846_guard *guard,struct s844_arena *arena,const struct desktop_surface *s,uint64_t before){
 const uint64_t safe=composed.built.root;const struct dpr_service *d=dpr_global_state();
#ifdef STYLEOS_TEST_RUNTIME_RECYCLE_862
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
 static struct s844_lease old[2],l[2];
 static unsigned round;static bool prepared;
 unsigned used;
 if(round>=S863_LAUNCH_LIMIT)fail("863-pump-completed");
#else
 struct s844_lease old[2]={0};
 for(unsigned round=0;round<2;round++){
#endif
 const uint64_t expected_generation=S863_INITIAL_ARENA_NEXT+2*round;
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
 if(!prepared){
#endif
 if(d->dispatcher.dispatches!=S863_DISPATCHES_PER_BATCH*round)fail("862-recycle-dispatch-start");
 arch_puts("STYLEOS S862R BEGIN");field("round",round);field("safe",safe);field("dispatches",d->dispatcher.dispatches);field("next",arena->next);arch_putc('\n');
#else
 const uint64_t expected_generation=4;
#endif
#ifndef STYLEOS_TEST_DESKTOP_PUMP_863
 struct s844_lease l[2];unsigned used;
#endif
 if(!base861||!arena||!boundary861(d,cache)||!s844_count(arena,&used)||used||arena->next!=expected_generation||!s850_desktop_audit_frozen_861(base861,cache,&frozen861,p->evidence,guard,&composed))fail("861-admit");
 for(unsigned i=0;i<2;i++){
  if(!s844_acquire(arena,i?S846_OWNER_B:S846_OWNER_A,3,&l[i])||!iszero861(l[i].pa,3*4096))fail("861-lease");
  *(volatile uint64_t *)(uintptr_t)(l[i].pa+4096)=UINT64_C(0x86110000)+i;
 }
 for(unsigned i=0;i<2;i++)if(!s846_build(base861,cache,arena,l,i,guard,child_tables861[i],(uintptr_t)child_tables861[i],&children861[i]))fail("861-build");
 if(!rt861_roots_audit_frozen(base861,cache,&frozen861,p->evidence,arena,l,guard,&composed,&children861[0],&children861[1]))fail("861-audit");
#ifdef STYLEOS_TEST_RUNTIME_RECYCLE_862
 if(round){
  if(l[0].pa!=old[0].pa||l[1].pa!=old[1].pa||l[0].generation!=old[0].generation+2||l[1].generation!=old[1].generation+2||s846_audit_pair(base861,cache,arena,old,guard,&children861[0],&children861[1]))fail("862-recycle-binding");
  for(unsigned i=0;i<2;i++)if(s844_release(arena,old[i].owner,old[i])||!s844_count(arena,&used)||used!=6||*(volatile uint64_t *)(uintptr_t)(l[i].pa+4096)!=UINT64_C(0x86110000)+i)fail("862-recycle-stale-lease");
  if(!rt861_roots_audit_frozen(base861,cache,&frozen861,p->evidence,arena,l,guard,&composed,&children861[0],&children861[1]))fail("862-recycle-audit");
  arch_puts("STYLEOS S862R REUSE");field("pa-a",l[0].pa);field("pa-b",l[1].pa);field("old-a",old[0].generation);field("old-b",old[1].generation);field("new-a",l[0].generation);field("new-b",l[1].generation);field("stale-leases",2);field("stale-root",1);arch_putc('\n');
 }
 old[0]=l[0];old[1]=l[1];
#endif
 arch_puts("STYLEOS S861 ROOTS");field("safe",safe);field("a",children861[0].built.root);field("b",children861[1].built.root);field("pa-a",l[0].pa);field("pa-b",l[1].pa);field("gen-a",l[0].generation);field("gen-b",l[1].generation);arch_putc('\n');
 for(unsigned i=0;i<2;i++){
  if(!boundary861(d,cache)||!rt861_roots_audit_frozen(base861,cache,&frozen861,p->evidence,arena,l,guard,&composed,&children861[0],&children861[1]))fail("861-before-switch");
  uint64_t r[5];roundtrip861(i,children861[i].built.root,safe,r);
  uint64_t own=*(volatile uint64_t *)(uintptr_t)(l[i].pa+4096),other=*(volatile uint64_t *)(uintptr_t)(l[1-i].pa+4096);
  uint64_t wantother=UINT64_C(0x86110000)+(1-i);if(i)wantother^=UINT64_C(0x86100000);
  if(own!=(r[2]^UINT64_C(0x86100000))||other!=wantother||!boundary861(d,cache)||!rt861_roots_audit_frozen(base861,cache,&frozen861,p->evidence,arena,l,guard,&composed,&children861[0],&children861[1]))fail("861-after-switch");
  arch_puts("STYLEOS S861 SWITCH");field("id",i);field("child",children861[i].built.root);field("safe",read_cr3());field("rip",r[0]);field("rsp",r[1]);field("cs",r[3]);field("flags",r[4]);field("frame",s851_frame_address);field("own",own);field("other",other);arch_putc('\n');
 }
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
 prepared=true;}
#endif
 struct user_audit861 a={cache,p->evidence,guard,arena,l};
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
 if(!s848_trial(children861,l,safe,base861->lapic,user_audit861,&a))return false;
#else
 s848_trial(children861,l,safe,base861->lapic,user_audit861,&a);
#endif
 if(!user_audit861(&a))fail("861-timer-final-audit");
#endif
#ifdef STYLEOS_TEST_RUNTIME_USER_861
 struct user_audit861 a={cache,p->evidence,guard,arena,l};
 for(unsigned i=0;i<2;i++)*(volatile uint64_t *)(uintptr_t)(l[i].pa+4096)=UINT64_C(0x846d1000)+i;
 s847_trial(children861,l,safe,user_audit861,&a);
 if(!user_audit861(&a))fail("861-user-final-audit");
#endif
 zero861(children861,sizeof children861);zero861(child_tables861,sizeof child_tables861);
 if(!iszero861((uintptr_t)child_tables861,sizeof child_tables861))fail("861-table-zero");
 for(unsigned i=0;i<2;i++)if(!s844_release(arena,i?S846_OWNER_B:S846_OWNER_A,l[i])||!iszero861(l[i].pa,3*4096))fail("861-release");
#ifdef STYLEOS_TEST_RUNTIME_TIMER_861
 struct rt858_live v=capture861();struct dpr_service *joined=rt861_dispatcher(&v);
 if(!joined||joined!=d)fail("861-timer-reap-boundary");
 if(!s844_count(arena,&used)||used)fail("861-timer-scrub");
 arch_puts("STYLEOS S861T SCRUB");field("safe",safe);field("used",used);field("tables-zero",1);field("leases-zero",1);arch_putc('\n');
 s860_reap(safe);
#endif
#ifdef STYLEOS_TEST_RUNTIME_USER_861
 struct rt858_live v=capture861();struct dpr_service *joined=rt861_dispatcher(&v);
 if(!joined||joined!=d)fail("861-user-reap-boundary");
#ifdef STYLEOS_TEST_RUNTIME_COOP_863
 if(!s844_count(arena,&used)||used)fail("863-coop-scrub");
 s863_coop_reap(safe,l);
 const unsigned expected_dispatches=8;
#else
 for(unsigned i=0;i<2;i++)if(dpr_release(joined,i,l[i].generation)!=PS_OK)fail("861-user-reap");
 const unsigned expected_dispatches=4;
#endif
 if(pr_snapshot(pr_global(),0,0,true)||ws_snapshot(ws_global(),0,0)||joined->dispatcher.dispatches!=expected_dispatches)fail("861-user-clean");
 arch_puts("STYLEOS S861U CLEAN");field("safe",safe);field("dispatches",expected_dispatches);field("processes",0);field("windows",0);arch_putc('\n');
 arch_puts("STYLEOS S861U POLICY cooperative-cpl3=yes existing-dispatcher=yes timer=no fp=no production-cr3=deny\n");
#endif
 if(!s844_count(arena,&used)||used||!boundary861(d,cache)||!s850_desktop_audit_frozen_861(base861,cache,&frozen861,p->evidence,guard,&composed))fail("861-final");
 uint64_t after=UINT64_C(14695981039346656037);const volatile uint32_t *fb=(const void *)(uintptr_t)s->base;
 for(size_t i=0;i<(size_t)s->width*s->height;i++){uint32_t pixel=fb[i];for(unsigned b=0;b<4;b++){after^=(pixel>>(8*b))&255u;after*=UINT64_C(1099511628211);}}
 if(after!=before||!boundary861(d,cache))fail("861-frame");
 arch_puts("STYLEOS S861 CLEAN");field("safe",safe);field("used",used);field("next",arena->next);field("tables-zero",1);field("leases-zero",1);field("fnv",after);arch_putc('\n');
 arch_puts("STYLEOS S861 POLICY cpl0-only=yes shared-desktop=yes inherited-return=no cpl3=no timer=no production-cr3=deny\n");
#ifdef STYLEOS_TEST_RUNTIME_RECYCLE_862
 if(arena->next!=expected_generation+2||d->dispatcher.dispatches!=S863_DISPATCHES_PER_BATCH*(round+1)||pr_global()->next_pid!=3+2*round||ws_global()->next_id!=3+2*round)fail("862-recycle-final");
 arch_puts("STYLEOS S862R DONE");field("round",round);field("safe",safe);field("dispatches",d->dispatcher.dispatches);field("next",arena->next);field("next-pid",pr_global()->next_pid);field("next-window",ws_global()->next_id);field("used",used);arch_putc('\n');
#ifdef STYLEOS_TEST_DESKTOP_PUMP_863
 prepared=false;
#ifdef STYLEOS_TEST_DESKTOP_COLD_TASKS_863
 ++round;return true;
#else
 return ++round==2;
#endif
#else
 }
#endif
#endif
}

#endif /* four guest */

#endif /* exclude diagnostic child execution helpers in owned shell */
