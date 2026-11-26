/* Same production-side allocator, detached tree builder, loader, bank,
 * dispatcher and FP metadata with 32 entries. No actual CPL3 execution
 * or hardware FP restore claimed by this host test. */
#include <styleos/runtime_bank_862.h>
#include <styleos/fx_context_849.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define N 32u
#define P 4096u
_Static_assert(S846_ROOT_COUNT==N&&S849_SLOT_COUNT==N&&PS_MAX_SLOTS==N&&S844_PAGES==3*N,"capacity mismatch");
static _Alignas(P) uint64_t tables[N+1][PG_TABLE_PAGES][512];
static _Alignas(P) uint8_t mem[S844_BYTES],scratch[8192],elf[8192];
static struct s844_arena arena;
static struct s844_lease leases[N];
static struct s846_root roots[N];
static struct s842_root base;
static struct s842_cache cache={.pat=0x0007040600070406,.cap=1,.def=0x806,.count=1,.bits=48,.pairs={{0x80000000,0xffff80000800}}};
static struct rt862_bank bank,initial_bank,initial_before;
static struct dpr_service initial_service;
static struct pr_table initial_registry;
static struct ws_service initial_windows;
static struct dpr_service service;
static struct pr_table registry;
static struct ws_service windows;
static struct s849_bank fx;
static struct rt865_launch_request req={.name="native",.source={.sectors=16,.bytes=4224}};
static uint32_t pids[N];
static unsigned checks,admissions,dispatches,refusals;
#define OK(x) do{assert(x);++checks;}while(0)
#define NO(x) do{assert(!(x));++refusals;}while(0)
static uint64_t safe(void){return (uintptr_t)tables[0];}
static uint64_t child(unsigned i){return (uintptr_t)tables[i+1];}
static void put(unsigned off,uint64_t v,unsigned n){for(unsigned j=0;j<n;j++)elf[off+j]=(uint8_t)(v>>(8*j));}
static bool read_sector(void *opaque,uint32_t lba,uint8_t out[512]){
 (void)opaque;if(lba>=16)return false;memcpy(out,elf+lba*512,512);return true;
}
static bool audit(void *unused){
 (void)unused;unsigned live=0,used;
 if(!s844_count(&arena,&used)||!s842_audit(&base,&cache))return false;
 for(unsigned i=0;i<N;i++){
  if(!leases[i].generation){if(roots[i].ready)return false;continue;}
  ++live;
  if(!s865_root_audit(&base,&cache,&arena,leases,i,0,&roots[i]))return false;
  if(roots[i].built.root!=child(i)||roots[i].plan.tables!=tables[i+1])return false;
 }
 return used==3*live;
}
static void admit(unsigned i){
 OK(s844_acquire(&arena,S846_OWNER_A+i,3,&leases[i]));
 OK(s865_root_build(&base,&cache,&arena,leases,i,0,tables[i+1],child(i),&roots[i]));
 OK(audit(0));
 OK(s865_fx_bind(&fx,i,child(i),leases[i].generation));
 req.gpr[14]=leases[i].generation;
 struct nte_loaded loaded={0};uint32_t pid=0;
 OK(rt865_launch_prepared(&bank,&service,safe(),i,&roots[i],&arena,&leases[i],&req,scratch,sizeof scratch,audit,0,&loaded,&pid)==PS_OK);
 OK(loaded.entry==S846_CODE+32&&pid>pids[i]);pids[i]=pid;++admissions;
 OK(!memcmp((void *)(uintptr_t)leases[i].pa,elf+4096,128));
}
static struct rt862_selection choose(unsigned expected){
 unsigned slot;uint32_t pid;
 OK(dpr_dispatch(&service,&slot,&pid)==PS_OK&&slot==expected&&pid==pids[slot]);++dispatches;
 return (struct rt862_selection){slot,service.dispatcher.dispatches,pid,leases[slot].generation,child(slot),safe()};
}
static void observe(const struct rt862_selection *t,bool exit){
 struct up_context c={0};struct s849_image image={0};
 unsigned i=t->slot;
 OK(rt862_bank_enter(&bank,&service,t,safe(),&c)&&c.gpr[14]==leases[i].generation);
 OK(s849_dispatch(&fx,i,child(i),leases[i].generation,&image));
 if(!exit)image.bytes[160]=(uint8_t)(i+1);
 OK(s849_suspend(&fx,i,child(i),leases[i].generation,&image));
 struct rt860_tick r={.slot=i,.dispatch=t->dispatch,.pid=t->pid,.handle=t->handle,.child_root=t->child_root,.safe_root=t->safe_root,
 .frame_va=S846_IST+4096-16-sizeof(struct s847_raw),.vector=128,.safe_flags=2,
 .frame={.vector=128,.rip=c.pc,.rsp=c.sp,.cs=c.cs,.ss=c.ss,.rflags=c.status}};
 for(unsigned j=0;j<15;j++)r.frame.gpr[j]=c.gpr[j];
 r.frame.gpr[0]=exit?S847_CALL_EXIT:S847_CALL_YIELD;
 OK(rt863_bank_event(&bank,&service,t,safe(),&r,0)==PS_OK);
 if(!exit)return;
 OK(pr_find(&registry,pids[i])->state==PR_EXITED);
 struct s844_lease old=leases[i];
 OK(s849_retire(&fx,i,child(i),old.generation));
 memset(&roots[i],0,sizeof roots[i]);memset(tables[i+1],0,sizeof tables[i+1]);
 OK(s844_release(&arena,old.owner,old));memset(&leases[i],0,sizeof leases[i]);
 NO(s844_lease_live(&arena,old));
 OK(rt862_bank_release(&bank,&service,safe(),i,pids[i],old.generation,child(i))==PS_OK);
 OK(audit(0));
}
int main(void){
 /* Exercise whole-arena and cross-word allocations before normal admission. */
 OK(s844_init(&arena,(uintptr_t)mem));
 struct s844_lease whole={0},extra={0},a={0},b={0};
 unsigned used=0;
 OK(s844_acquire(&arena,1,S844_PAGES,&whole));OK(s844_count(&arena,&used)&&used==S844_PAGES);
 NO(s844_acquire(&arena,2,1,&extra));OK(!extra.generation);
 OK(s844_release(&arena,1,whole));
 OK(s844_acquire(&arena,1,31,&a));OK(s844_acquire(&arena,2,34,&b));OK(s844_count(&arena,&used)&&used==65);
 OK(s844_release(&arena,1,a));OK(s844_release(&arena,2,b));
 OK(s844_count(&arena,&used)&&!used);
 struct pg_plan plan={.arch=PG_X86,.pa_bits=48,.tables=tables[0],.tables_pa=safe(),.capacity=PG_TABLE_PAGES,.count=2,.probe=UINT64_C(0x400000001000),
 .maps={{(uintptr_t)tables,(uintptr_t)tables,sizeof tables,PG_WRITE},{(uintptr_t)mem,(uintptr_t)mem,sizeof mem,PG_WRITE}}};
 struct boot_memory_map map={0};
 OK(s842_build(&plan,&map,&cache,0xfee00000,0xfec00000,&base));
 pr_init(&registry);ws_init(&windows);
 OK(dpr_start(&service,&registry,&windows,safe())==DPR_OK);
 NO(rt862_bank_open(&bank,&service,safe(),0x58,0x63));
 OK(!memcmp(&bank,&initial_bank,sizeof bank));
 OK(rt862_bank_open(&bank,&service,safe(),0x5b,0x63));
 pr_init(&initial_registry);ws_init(&initial_windows);
 OK(dpr_start(&initial_service,&initial_registry,&initial_windows,safe())==DPR_OK);
 initial_bank.slots[0].handle=123;initial_before=initial_bank;
 for(unsigned i=0;i<N;i++){
  uint32_t pid;
  NO(rt862_bank_init(&initial_bank,&initial_service,safe(),0x5b,0x63));
  OK(!memcmp(&initial_bank,&initial_before,sizeof initial_bank));
  OK(dpr_admit(&initial_service,i,i+1,child(i),0,"initial",&pid)==PS_OK);
 }
 NO(rt862_bank_init(&initial_bank,&initial_service,safe(),0x5b,0x5b));
 OK(!memcmp(&initial_bank,&initial_before,sizeof initial_bank));
 OK(rt862_bank_init(&initial_bank,&initial_service,safe(),0x5b,0x63));
 for(unsigned i=0;i<N;i++)OK(initial_bank.slots[i].pid==i+1&&initial_bank.slots[i].root==child(i)&&initial_bank.slots[i].context.pc==S846_CODE);
 struct s849_caps caps={.leaf1_edx=1u|(1u<<24)|(1u<<25)|(1u<<26),.cr0=0x23,.cr4=0x600,.mxcsr_mask=0xffbf};
 OK(s865_fx_open(&fx,&caps));
 elf[0]=127;elf[1]='E';elf[2]='L';elf[3]='F';elf[4]=2;elf[5]=1;elf[6]=1;
 put(16,2,2);put(18,62,2);put(20,1,4);put(24,NTE_USER_CODE_VA+32,8);
 put(32,64,8);put(52,64,2);put(54,56,2);put(56,1,2);put(64,1,4);put(68,5,4);
 put(72,4096,8);put(80,NTE_USER_CODE_VA,8);put(96,128,8);put(104,4096,8);put(112,4096,8);
 for(unsigned i=0;i<128;i++)elf[4096+i]=(uint8_t)(i*7+3);
 req.source.read=read_sector;nte_sha256(elf,4224,req.source.sha256);
 for(unsigned i=0;i<N;i++)admit(i);
 OK(pr_snapshot(&registry,0,0,true)==N&&s844_count(&arena,&used)&&used==3*N);
 NO(s844_acquire(&arena,99,1,&extra));
 for(unsigned i=0;i<N;i++){struct rt862_selection t=choose(i);observe(&t,false);}
 /* Fault/exit and replacement at a safe boundary, while 31 peers remain
  * READY. Old lease/generation cannot dispatch or authorize new memory. */
 for(unsigned i=0;i<N;i++){
  struct rt862_selection t=choose(i);
  if(i%2==0){
   struct s844_lease old=leases[i];observe(&t,true);
   admit(i);
   OK(leases[i].pa==old.pa&&leases[i].generation!=old.generation);
   struct up_context rejected={0};NO(rt862_bank_enter(&bank,&service,&t,safe(),&rejected));
   struct s849_image rejected_fx={0};NO(s849_dispatch(&fx,i,child(i),old.generation,&rejected_fx));
   NO(s844_release(&arena,old.owner,old));
  }else observe(&t,false);
 }
 for(unsigned i=0;i<N;i++){
  struct rt862_selection t=choose(i);
  OK(fx.slot[i].image.bytes[160]==(i%2?(uint8_t)(i+1):0));
  observe(&t,true);
 }
 unsigned slot;uint32_t pid;
 OK(dpr_dispatch(&service,&slot,&pid)==PS_EMPTY);
 OK(!pr_snapshot(&registry,0,0,true)&&s844_count(&arena,&used)&&!used&&audit(0));
 for(size_t i=0;i<sizeof mem;i++)assert(!mem[i]);
 OK(admissions==48&&dispatches==96);
 printf("Kernel expanded capacity HOST ONLY: 32 resident processes, %u admissions, %u synthetic dispatches, %u checks, %u refusals PASS\n",admissions,dispatches,checks,refusals);
}
