#include <styleos/runtime_bank_862.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct rt862_bank bank;
static struct dpr_service d;
static struct pr_table pr;
static struct ws_service ws;
static struct s846_root child;
static struct s844_lease lease={.pa=0x100000,.generation=17,.owner=S846_OWNER_A,.pages=3};
static struct nte_loaded image={.entry=S846_CODE+32,.code_bytes=64,.memory_bytes=4096};
static struct up_context initial={.pc=S846_CODE+32,.sp=S846_STACK+4096,.status=2,.cs=0x5b,.ss=0x63};
static unsigned checks;
static uint32_t pid=0xdeadbeef;
#define OK(x) do{assert(x);++checks;}while(0)
#define REJECT(x) do{struct rt862_bank b=bank;struct dpr_service s=d;struct pr_table p=pr;struct ws_service w=ws;uint32_t old=pid;OK((x)!=PS_OK);OK(!memcmp(&b,&bank,sizeof b)&&!memcmp(&s,&d,sizeof s)&&!memcmp(&p,&pr,sizeof p)&&!memcmp(&w,&ws,sizeof w)&&old==pid);}while(0)
#define ADMIT() rt865_bank_admit_image(&bank,&d,0x1000,0,&child,&lease,&image,"loader image",&initial,&pid)
static _Alignas(4096) uint8_t launch_mem[S844_BYTES],elf[8192],scratch[8192];
static _Alignas(4096) uint64_t launch_tables[2][PG_TABLE_PAGES][512];
static unsigned reads,audits,fail_audit;
static void put(unsigned off,uint64_t value,unsigned count){for(unsigned i=0;i<count;i++)elf[off+i]=(uint8_t)(value>>(8*i));}
static bool read_sector(void *opaque,uint32_t lba,uint8_t out[512]){
 (void)opaque;++reads;if(lba>=16)return false;memcpy(out,elf+512*lba,512);return true;
}
static bool audit_launch(void *opaque){(void)opaque;return ++audits!=fail_audit;}
static void launch_requests(void){
 struct rt862_bank b={0};struct dpr_service service={0};
 struct pr_table registry;struct ws_service windows;
 struct s844_arena arena={0};struct s844_lease leases[2]={0};
 struct s846_root roots[2]={0};
 struct rt865_launch_request request={.source={.read=read_sector,.sectors=16,.bytes=4224},.name="native"};
 struct nte_loaded out={17,18,19};uint32_t assigned=0xdeadbeef;
 pr_init(&registry);ws_init(&windows);
 OK(dpr_start(&service,&registry,&windows,0x1000)==DPR_OK);
 OK(rt862_bank_open(&b,&service,0x1000,0x5b,0x63));
 OK(s844_init(&arena,(uintptr_t)launch_mem));
 for(unsigned i=0;i<2;i++){
  OK(s844_acquire(&arena,S846_OWNER_A+i,3,&leases[i]));
  uint64_t pa=leases[i].pa,table=(uintptr_t)launch_tables[i];
  roots[i].ready=true;roots[i].lease=leases[i];
  roots[i].built=(struct pg_result){.root=table,.used=1,.error=PG_OK};
  roots[i].plan=(struct pg_plan){.arch=PG_X86,.tables=launch_tables[i],.tables_pa=table,
   .capacity=PG_TABLE_PAGES,.count=3,
   .maps={{S846_CODE,pa,4096,PG_EXEC},{S846_DATA,pa+4096,4096,PG_WRITE},{S846_STACK,pa+8192,4096,PG_WRITE}}};
 }
 memset(elf,0,sizeof elf);elf[0]=127;elf[1]='E';elf[2]='L';elf[3]='F';elf[4]=2;elf[5]=1;elf[6]=1;
 put(16,2,2);put(18,62,2);put(20,1,4);put(24,NTE_USER_CODE_VA+32,8);
 put(32,64,8);put(52,64,2);put(54,56,2);put(56,1,2);put(64,1,4);put(68,5,4);
 put(72,4096,8);put(80,NTE_USER_CODE_VA,8);put(96,128,8);put(104,4096,8);put(112,4096,8);
 for(unsigned i=0;i<128;i++)elf[4096+i]=(uint8_t)(i*7+3);
 nte_sha256(elf,4224,request.source.sha256);
 #define LAUNCH(i,buf) rt865_launch_prepared(&b,&service,0x1000,i,&roots[i],&arena,&leases[i],&request,buf,sizeof scratch,audit_launch,0,&out,&assigned)
 #define NO_PUBLISH(expr) do{struct rt862_bank before=b;struct dpr_service ds=service;struct pr_table p=registry;struct ws_service w=windows;uint32_t old=assigned;struct nte_loaded prev=out;OK((expr)!=PS_OK);OK(!memcmp(&before,&b,sizeof b)&&!memcmp(&ds,&service,sizeof ds)&&!memcmp(&p,&registry,sizeof p)&&!memcmp(&w,&windows,sizeof w)&&assigned==old&&!memcmp(&out,&prev,sizeof out));}while(0)
 reads=audits=0;fail_audit=1;NO_PUBLISH(LAUNCH(0,scratch));OK(!reads&&audits==1);
 reads=audits=0;fail_audit=0;NO_PUBLISH(LAUNCH(0,(uint8_t *)launch_tables[0]));OK(!reads&&!audits);
 request.source.sha256[0]^=1;NO_PUBLISH(LAUNCH(0,scratch));OK(reads==9);request.source.sha256[0]^=1;
 for(unsigned i=0;i<sizeof scratch;i++)OK(!scratch[i]);
 reads=audits=0;fail_audit=2;NO_PUBLISH(LAUNCH(0,scratch));OK(reads==9&&audits==2);
 reads=audits=0;fail_audit=0;OK(LAUNCH(0,scratch)==PS_OK&&assigned==1&&reads==9&&audits==2);
 OK(out.entry==S846_CODE+32&&b.slots[0].context.gpr[0]==0&&b.slots[0].context.pc==out.entry);
 struct rt862_bank_slot peer=b.slots[0];
 reads=audits=0;NO_PUBLISH(LAUNCH(0,scratch));OK(!reads&&!audits);
 request.gpr[0]=123;OK(LAUNCH(1,scratch)==PS_OK&&assigned==2);
 OK(!memcmp(&peer,&b.slots[0],sizeof peer)&&b.slots[1].context.gpr[0]==123);
 OK(!memcmp(launch_mem,elf+4096,128));
 unsigned chosen;uint32_t chosen_pid;OK(dpr_dispatch(&service,&chosen,&chosen_pid)==PS_OK&&chosen==0&&chosen_pid==1);
 struct rt862_selection ticket={chosen,service.dispatcher.dispatches,chosen_pid,leases[0].generation,roots[0].built.root,0x1000};
 struct up_context context={0};OK(rt862_bank_enter(&b,&service,&ticket,0x1000,&context)&&context.pc==S846_CODE+32);
 #undef NO_PUBLISH
 #undef LAUNCH
}
int main(void){
 launch_requests();
 pr_init(&pr);ws_init(&ws);OK(dpr_start(&d,&pr,&ws,0x1000)==DPR_OK);
 OK(rt862_bank_open(&bank,&d,0x1000,0x5b,0x63));
 child.ready=true;child.built=(struct pg_result){.root=0x2000,.used=1,.error=PG_OK};
 child.plan=(struct pg_plan){.arch=PG_X86,.tables_pa=0x2000,.capacity=PG_TABLE_PAGES,.count=3,
 .maps={{S846_CODE,0x100000,4096,PG_EXEC},{S846_DATA,0x101000,4096,PG_WRITE},{S846_STACK,0x102000,4096,PG_WRITE}}};
 child.lease=lease;initial.gpr[0]=0xabcdef;initial.gpr[14]=0x123456;
 lease.generation++;REJECT(ADMIT());lease.generation--;
 lease.owner++;REJECT(ADMIT());lease.owner--;
 child.plan.maps[0].flags=PG_WRITE|PG_EXEC;REJECT(ADMIT());child.plan.maps[0].flags=PG_EXEC;
 child.plan.maps[1].pa+=4096;REJECT(ADMIT());child.plan.maps[1].pa-=4096;
 child.built.root=0x1000;REJECT(ADMIT());child.built.root=0x2000;
 image.code_bytes=32;REJECT(ADMIT());image.code_bytes=64;
 image.memory_bytes=4097;REJECT(ADMIT());image.memory_bytes=4096;
 initial.pc++;REJECT(ADMIT());initial.pc--;
 initial.sp--;REJECT(ADMIT());initial.sp++;
 OK(ADMIT()==PS_OK&&pid==1);
 OK(bank.slots[0].handle==lease.generation&&bank.slots[0].root==child.built.root);
 unsigned slot;uint32_t chosen;OK(dpr_dispatch(&d,&slot,&chosen)==PS_OK&&slot==0&&chosen==pid);
 struct rt862_selection t={slot,d.dispatcher.dispatches,pid,lease.generation,child.built.root,0x1000};
 struct up_context c={0};OK(rt862_bank_enter(&bank,&d,&t,0x1000,&c));
 OK(c.pc==image.entry&&c.gpr[0]==initial.gpr[0]&&c.gpr[14]==initial.gpr[14]&&c.status==0x202);
 printf("S865 image admission HOST ONLY: %u checks PASS\n",checks);
}
