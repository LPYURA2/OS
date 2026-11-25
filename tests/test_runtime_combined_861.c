#include <styleos/desktop_gop_850.h>
#include <styleos/runtime_roots_861.h>
#include <styleos/runtime_entry_858.h>
#include <styleos/runtime_tick_860.h>
#include <styleos/user_session_runtime.h>
#include <styleos/native_window_channel.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define BASE UINT64_C(0x80000000)
static uint64_t t[4][512];
static unsigned char raw[80];
static struct boot_info b;
static struct boot_memory_map m;
static struct boot_memory_evidence e;
static struct desktop_surface s;
static struct pci794_bar bar;
static struct s842_cache c;
static unsigned count;
static unsigned refused;
#define REJECT(x) do { assert(!(x)); refused++; } while (0)
static bool read_table(void *p,uint64_t pa,uint16_t i,uint64_t *out){(void)p;if(pa<0x1000||pa>0x4000||(pa&4095)||i>=512)return false;*out=t[pa/4096-1][i];return true;}
static void setle(unsigned char *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(unsigned char)(v>>(8*i));}
static void descriptor(unsigned n,unsigned type,uint64_t start,uint64_t pages,uint64_t attr){unsigned char *p=raw+40*n;memset(p,0,40);setle(p,type,4);setle(p+8,start,8);setle(p+24,pages,8);setle(p+32,attr,8);}
static struct s850_gop_request reset(void){
 memset(t,0,sizeof t);memset(&m,0,sizeof m);memset(raw,0,sizeof raw);
 t[0][0]=0x2003;t[1][2]=0x3003;t[2][0]=BASE|0x83;t[2][1]=(BASE+0x200000)|0x83;
 descriptor(0,2,0x100000,0x7000,8);descriptor(1,11,BASE,0x1000,1);
 b=(struct boot_info){.magic=STYLEOS_BOOT_MAGIC,.version=1,.size=sizeof b,.arch=1,.platform=1,.flags=1,.memory_map=(uintptr_t)raw,.memory_map_size=sizeof raw,.descriptor_size=40,.descriptor_version=1};
 m.nr_ram=1;m.ram[0]=(struct boot_range){0x100000,0x7100000};
 e=(struct boot_memory_evidence){.live={{0x200000,0x300000},{0x210000,0x220000},{(uintptr_t)&b,(uintptr_t)&b+sizeof b},{(uintptr_t)raw,(uintptr_t)raw+sizeof raw}},.workspace={0x230000,0x240000},.map=&m};
 s=(struct desktop_surface){BASE,4096000,1280,800,1280,DESKTOP_BGR_BYTES};
 bar=(struct pci794_bar){.base=BASE,.raw_lo=0x80000008,.width=32,.memory=true,.prefetch=true};
 c=(struct s842_cache){.pat=0x0007040600070406,.cap=1,.def=0x806,.count=1,.bits=48,.pairs={{0x80000000,0xffff80000800}}};
 return (struct s850_gop_request){&b,&e,&s,&bar,0xff000008,0,&c,0x1000,read_table,0};
}
#define CHECK(x) do{assert(x);count++;}while(0)
static _Alignas(4096) uint64_t original[PG_TABLE_PAGES][512],dt[PG_TABLE_PAGES][512],ta[PG_TABLE_PAGES][512],tb[PG_TABLE_PAGES][512];
static _Alignas(4096) unsigned char mem[S844_BYTES];
static struct s842_root base;
static struct s850_desktop_root desktop;
static struct s846_root a,broot;
static struct s844_arena arena;
static struct s844_lease leases[2];
static struct s846_guard guard={{0x280000,0x281000,0x282000}};
static struct s850_gop_request req;
int main(void){
 req=reset();uint64_t pa=(uintptr_t)mem;
 assert(s844_init(&arena,pa)&&s844_acquire(&arena,S846_OWNER_A,3,&leases[0])&&s844_acquire(&arena,S846_OWNER_B,3,&leases[1]));
 struct pg_plan p={.arch=PG_X86,.pa_bits=48,.tables=original,.tables_pa=0x200000,.capacity=PG_TABLE_PAGES,.count=3,.probe=0x400000001000,.maps={{0x200000,0x200000,4*PG_TABLE_BYTES+3*4096,PG_WRITE},{0x100000,0x100000,4096,PG_EXEC},{pa,pa,S844_BYTES,PG_WRITE}}};
 assert(s842_build(&p,&m,&c,0xfee00000,0xfec00000,&base));
 assert(s850_desktop_build_guarded(&base,&req,&guard,dt,0x220000,&desktop));
 assert(s846_build(&base,&c,&arena,leases,0,&guard,ta,0x240000,&a));
 assert(s846_build(&base,&c,&arena,leases,1,&guard,tb,0x260000,&broot));

 CHECK(rt861_roots_audit(&base,&req,&arena,leases,&guard,&desktop,&a,&broot));
 const uint64_t safe=desktop.built.root;
 struct rt858_expected expected={.old_root=req.inherited_cr3,.root=safe,.gdt=0x290000,.idt=0x291000,.stack_lo=0x210000,.stack_hi=0x220000,.gdt_limit=0x57,.tr=0x48,.cs=0x38};
 struct rt858_live live={.root=safe,.gdt=expected.gdt,.idt=expected.idt,.rsp=expected.stack_hi-16,.cr0=UINT64_C(0x80010001),.cr4=32,.efer=0xc00,.rflags=2,.gdt_limit=expected.gdt_limit,.idt_limit=4095,.tr=expected.tr,.cs=expected.cs};
 CHECK(rt858_start(&expected,&live));
 struct dpr_service *d=rt861_dispatcher(&live); CHECK(d&&d==dpr_global_state());
 const struct usr_runtime *u=usr_global();const struct nwc_service *n=nwc_global();
 uint32_t pids[2],wins[2],pid;unsigned slot;
 struct s846_root *roots[2]={&a,&broot};
 for(unsigned i=0;i<2;i++){
  CHECK(dpr_admit(d,i,leases[i].generation,roots[i]->built.root,0,i?"B.tea":"A.tea",&pids[i])==PS_OK);
  CHECK(pr_account(pr_global(),pids[i],leases[i].generation,0,3*4096)==PR_OK);
  CHECK(ws_open_app(ws_global(),pr_global(),pids[i],i?"B":"A",(struct ws_bounds){0,0,400,300},&wins[i])==WS_OK);
 }
 CHECK(ws_set_minimized(ws_global(),pids[0],wins[0],true)==WS_OK);
 REJECT(rt858_start(&expected,&live));
 struct rt858_live wrong=live;wrong.root=base.built.root;REJECT(rt861_dispatcher(&wrong));
 wrong.root=a.built.root;REJECT(rt861_dispatcher(&wrong));
 wrong.root=broot.built.root;REJECT(rt861_dispatcher(&wrong));
 struct rt860_bounds bounds={{S846_CODE,S846_CODE+4096},{S846_STACK,S846_STACK+4096},{S846_IST,S846_IST+4096},0x5b,0x63};
 struct s847_raw saved[2]={0};
 const unsigned order[]={0,1,0,1,0,1,0,1,1,1};
 for(unsigned step=0;step<10;step++){
  CHECK(rt861_roots_audit(&base,&req,&arena,leases,&guard,&desktop,&a,&broot));
  CHECK(rt861_dispatcher(&live)==d);
  CHECK(dpr_dispatch(d,&slot,&pid)==PS_OK&&slot==order[step]&&pid==pids[slot]);
  REJECT(rt861_dispatcher(&live));
  struct rt860_tick tick={.slot=slot,.dispatch=d->dispatcher.dispatches,.pid=pid,.handle=leases[slot].generation,.child_root=roots[slot]->built.root,.safe_root=safe,.frame_va=bounds.ist.end-16-sizeof tick.frame,.vector=0x50,.isr=1,.eoi=1,.safe_flags=2,.frame={.vector=0x50,.rip=S846_CODE+2,.rsp=S846_STACK+4096,.cs=bounds.cs,.ss=bounds.ss,.rflags=0x202}};
  if(step==6||step==9){
   CHECK(dpr_observe(d,slot,leases[slot].generation,roots[slot]->built.root,safe,slot?PS_EXIT:PS_FAULT)==PS_OK);
   CHECK(!ws_find(ws_global(),wins[slot]));
   if(!slot)CHECK(ws_find(ws_global(),wins[1]));
  }else{
   struct rt860_tick bad=tick;bad.safe_root=base.built.root;
   REJECT(dpr_tick_860(d,&bounds,&bad,&saved[slot])==PS_OK);
   bad=tick;bad.child_root=roots[1-slot]->built.root;
   REJECT(dpr_tick_860(d,&bounds,&bad,&saved[slot])==PS_OK);
   bad=tick;bad.handle=leases[1-slot].generation;
   REJECT(dpr_tick_860(d,&bounds,&bad,&saved[slot])==PS_OK);
   CHECK(dpr_tick_860(d,&bounds,&tick,&saved[slot])==PS_OK);
   REJECT(dpr_tick_860(d,&bounds,&tick,&saved[slot])==PS_OK);
   CHECK(pr_find(pr_global(),pid)->resident_bytes==3*4096);
   if(step<6)CHECK(ws_find(ws_global(),wins[0])->minimized);
  }
  CHECK(rt861_dispatcher(&live)==d);
 }
 CHECK(pr_find(pr_global(),pids[0])->cpu_ticks==3&&pr_find(pr_global(),pids[1])->cpu_ticks==5);
 CHECK(d->dispatcher.dispatches==10&&d->dispatcher.current==-1&&!ws_snapshot(ws_global(),0,0));
 /* This is a host lifecycle sequence, not an architecture teardown proof. */
 memset(ta,0,sizeof ta);memset(tb,0,sizeof tb);memset(&a,0,sizeof a);memset(&broot,0,sizeof broot);memset(saved,0,sizeof saved);
 for(unsigned i=0;i<2;i++){
  memset((void *)(uintptr_t)leases[i].pa,0xa5,3*4096);
  CHECK(s844_release(&arena,i?S846_OWNER_B:S846_OWNER_A,leases[i]));
  for(unsigned j=0;j<3*4096;j++)assert(!((unsigned char *)(uintptr_t)leases[i].pa)[j]);
  CHECK(dpr_release(d,i,leases[i].generation)==PS_OK);
 }
 unsigned used=99;CHECK(s844_count(&arena,&used)&&used==0);
 CHECK(rt861_dispatcher(&live)==d&&!pr_snapshot(pr_global(),0,0,true)&&usr_global()==u&&nwc_global()==n);
 CHECK(s850_desktop_audit_guarded(&base,&req,&guard,&desktop));
 struct s844_lease fresh;CHECK(s844_acquire(&arena,S846_OWNER_A,3,&fresh));CHECK(fresh.pa==leases[0].pa&&fresh.generation!=leases[0].generation);
 REJECT(s844_release(&arena,S846_OWNER_A,leases[0]));CHECK(s844_release(&arena,S846_OWNER_A,fresh));
 printf("S861 combined host model: %u positive / %u refusals; 8 synthetic ticks, 10 dispatches, 24576 scrubbed bytes PASS\n",count,refused);
 return 0;
}
